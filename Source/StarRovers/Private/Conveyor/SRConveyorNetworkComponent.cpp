#include "Conveyor/SRConveyorNetworkComponent.h"

#include "Components/DynamicMeshComponent.h"
#include "Components/LineBatchComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Conveyor/SRConveyorBeltActor.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "PCGComponent.h"
#include "Structure/SRStructureDataAsset.h"
#include "Surface/SRPlanetSurfaceGrid.h"
#include "Utility/SRMemoryDiagnostics.h"

namespace
{
	constexpr uint8 ConveyorPathDebugLineDepthPriority = SDPG_Foreground;
	const FName ConveyorVisualSplineNameBase(TEXT("ConveyorVisualSpline"));

	TAutoConsoleVariable<int32> CVarSRMemoryDiagnosticsConveyorDelete(
		TEXT("sr.MemoryDiagnostics.ConveyorDelete"),
		0,
		TEXT("Logs memory diagnostics after conveyor deletion refreshes. 0=disabled, 1=enabled."));

	TAutoConsoleVariable<int32> CVarSRMemoryDiagnosticsConveyorPlacement(
		TEXT("sr.MemoryDiagnostics.ConveyorPlacement"),
		0,
		TEXT("Logs memory diagnostics after conveyor placement refreshes. 0=disabled, 1=enabled."));

	TAutoConsoleVariable<int32> CVarSRMemoryDiagnosticsForceGCOnConveyorDelete(
		TEXT("sr.MemoryDiagnostics.ForceGCOnConveyorDelete"),
		0,
		TEXT("Requests garbage collection after conveyor deletion diagnostics. 0=log next tick only, 1=force GC and log AfterGCTick."));
}

USRConveyorNetworkComponent::USRConveyorNetworkComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	DefaultLayerHeight = 160.0f;
	BeltWidth = 260.0f;
	BeltThickness = 80.0f;
	BeltSurfaceOffset = 0.0f;
	bBuildDynamicMeshVisuals = false;
	bSpawnConveyorBeltActors = true;
	MaxConveyorActorGroupsRefreshedPerFrame = 1;
	bBuildPCGSplineInputs = false;
	PCGSplineComponentTag = TEXT("ConveyorVisualSpline");
	PCGSplineHeightOffset = 0.0f;
	bAutoGeneratePCG = false;
	bShowPathDebugLine = false;
	PathDebugLineColor = FLinearColor(1.0f, 0.1f, 0.0f, 1.0f);
	PathDebugLineThickness = 8.0f;
	BeltMeshComponent = nullptr;
	PathDebugLineBatchComponent = nullptr;
}

void USRConveyorNetworkComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return;
	}

	TArray<UPCGComponent*> PCGComponents;
	OwnerActor->GetComponents<UPCGComponent>(PCGComponents);
	for (UPCGComponent* PCGComponent : PCGComponents)
	{
		if (IsValid(PCGComponent))
		{
			PCGComponent->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
		}
	}
	BindPCGGenerationDelegates();
}

void USRConveyorNetworkComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	USRPlanetSurfaceGrid* SurfaceGrid = PendingConveyorActorRefreshSurfaceGrid.Get();
	if (!IsValid(SurfaceGrid))
	{
		if (AActor* OwnerActor = GetOwner())
		{
			SurfaceGrid = OwnerActor->FindComponentByClass<USRPlanetSurfaceGrid>();
			PendingConveyorActorRefreshSurfaceGrid = SurfaceGrid;
		}
	}

	RefreshDirtyConveyorActorGroups(SurfaceGrid, FMath::Max(1, MaxConveyorActorGroupsRefreshedPerFrame));
	if (!HasDirtyConveyorActorGroups())
	{
		PendingConveyorActorRefreshSurfaceGrid.Reset();
		SetComponentTickEnabled(false);
	}
}

#if WITH_EDITOR
void USRConveyorNetworkComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (AActor* OwnerActor = GetOwner(); IsValid(OwnerActor) && OwnerActor->HasActorBegunPlay())
	{
		USRPlanetSurfaceGrid* SurfaceGrid = OwnerActor->FindComponentByClass<USRPlanetSurfaceGrid>();
		if (bSpawnConveyorBeltActors)
		{
			RebuildPlacedConveyorActors(SurfaceGrid);
		}
		else
		{
			DestroyPlacedConveyorActors();
		}
		RefreshConveyorVisuals(SurfaceGrid);
		RefreshPathDebugLines(SurfaceGrid);
	}
}
#endif

bool USRConveyorNetworkComponent::HasConveyorSegment(const FSRConveyorLaneKey& LaneKey) const
{
	return Segments.Contains(LaneKey);
}

bool USRConveyorNetworkComponent::GetConveyorSegment(const FSRConveyorLaneKey& LaneKey, FSRConveyorSegment& OutSegment) const
{
	if (const FSRConveyorSegment* Segment = Segments.Find(LaneKey))
	{
		OutSegment = *Segment;
		return true;
	}

	OutSegment = FSRConveyorSegment();
	return false;
}

void USRConveyorNetworkComponent::ClearConveyors()
{
	if (AActor* OwnerActor = GetOwner())
	{
		if (USRPlanetSurfaceGrid* SurfaceGrid = OwnerActor->FindComponentByClass<USRPlanetSurfaceGrid>())
		{
			TArray<FSRPlanetSurfaceGridCellId> SurfaceLayerCellIds;
			for (const TPair<FSRConveyorLaneKey, FSRConveyorSegment>& SegmentPair : Segments)
			{
				if (SegmentPair.Key.Layer == 0)
				{
					SurfaceLayerCellIds.Add(SegmentPair.Key.CellId);
				}
			}
			if (!SurfaceLayerCellIds.IsEmpty())
			{
				SurfaceGrid->SetCellsOccupied(SurfaceLayerCellIds, false, NAME_None);
			}
		}
	}

	Segments.Reset();
	VisualPaths.Reset();
	DestroyPlacedConveyorActors();
	PendingConveyorActorRefreshSurfaceGrid.Reset();
	SetComponentTickEnabled(false);
	if (IsValid(BeltMeshComponent))
	{
		UE::Geometry::FDynamicMesh3 EmptyMesh;
		EmptyMesh.EnableAttributes();
		EmptyMesh.Attributes()->EnablePrimaryColors();
		EmptyMesh.Attributes()->SetNumUVLayers(1);
		BeltMeshComponent->SetMesh(MoveTemp(EmptyMesh));
	}
	if (IsValid(PathDebugLineBatchComponent))
	{
		PathDebugLineBatchComponent->Flush();
	}
	ClearUnusedPCGSplineComponents(0);
}

void USRConveyorNetworkComponent::SetPathDebugLineVisible(bool bNewPathDebugLineVisible)
{
	if (bShowPathDebugLine == bNewPathDebugLineVisible)
	{
		return;
	}

	bShowPathDebugLine = bNewPathDebugLineVisible;
	if (AActor* OwnerActor = GetOwner())
	{
		RefreshPathDebugLines(OwnerActor->FindComponentByClass<USRPlanetSurfaceGrid>());
	}
}

bool USRConveyorNetworkComponent::IsPathDebugLineVisible() const
{
	return bShowPathDebugLine;
}

bool USRConveyorNetworkComponent::FindConveyorPath(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& StartCellId,
	const FSRPlanetSurfaceGridCellId& EndCellId,
	int32 Layer,
	TArray<FSRPlanetSurfaceGridCellId>& OutPath) const
{
	OutPath.Reset();
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	FSRPlanetSurfaceGridCell StartCell;
	FSRPlanetSurfaceGridCell EndCell;
	if (!SurfaceGrid->GetCellById(StartCellId, StartCell) || !SurfaceGrid->GetCellById(EndCellId, EndCell))
	{
		return false;
	}

	const int32 SafeLayer = FMath::Max(0, Layer);
	TArray<FSRPlanetSurfaceGridCellId> OpenSet;
	TSet<FSRPlanetSurfaceGridCellId> Visited;
	TMap<FSRPlanetSurfaceGridCellId, FSRPlanetSurfaceGridCellId> CameFrom;
	OpenSet.Add(StartCellId);
	Visited.Add(StartCellId);

	for (int32 OpenIndex = 0; OpenIndex < OpenSet.Num(); ++OpenIndex)
	{
		const FSRPlanetSurfaceGridCellId CurrentCellId = OpenSet[OpenIndex];
		if (CurrentCellId == EndCellId)
		{
			TArray<FSRPlanetSurfaceGridCellId> ReversedPath;
			FSRPlanetSurfaceGridCellId TraceCellId = EndCellId;
			ReversedPath.Add(TraceCellId);
			while (!(TraceCellId == StartCellId))
			{
				const FSRPlanetSurfaceGridCellId* PreviousCellId = CameFrom.Find(TraceCellId);
				if (!PreviousCellId)
				{
					return false;
				}
				TraceCellId = *PreviousCellId;
				ReversedPath.Add(TraceCellId);
			}

			OutPath.Reserve(ReversedPath.Num());
			for (int32 PathIndex = ReversedPath.Num() - 1; PathIndex >= 0; --PathIndex)
			{
				OutPath.Add(ReversedPath[PathIndex]);
			}
			return OutPath.Num() > 0;
		}

		FSRPlanetSurfaceGridCellNeighbors Neighbors;
		if (!SurfaceGrid->GetCellNeighbors(CurrentCellId, Neighbors))
		{
			continue;
		}

		const ESRConveyorGridDirection Directions[] =
		{
			ESRConveyorGridDirection::NegativeU,
			ESRConveyorGridDirection::PositiveU,
			ESRConveyorGridDirection::NegativeV,
			ESRConveyorGridDirection::PositiveV,
		};

		for (const ESRConveyorGridDirection Direction : Directions)
		{
			FSRPlanetSurfaceGridCellId NeighborCellId;
			if (!GetNeighborCellIdByDirection(Neighbors, Direction, NeighborCellId) || Visited.Contains(NeighborCellId))
			{
				continue;
			}

			const FSRConveyorLaneKey NeighborLaneKey = MakeLaneKey(NeighborCellId, SafeLayer);
			if (!(NeighborCellId == EndCellId) && !CanPlaceConveyorSegment(SurfaceGrid, NeighborLaneKey))
			{
				continue;
			}

			Visited.Add(NeighborCellId);
			CameFrom.Add(NeighborCellId, CurrentCellId);
			OpenSet.Add(NeighborCellId);
		}
	}

	return false;
}

bool USRConveyorNetworkComponent::TryPlaceConveyorPath(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TArray<FSRPlanetSurfaceGridCellId>& PathCellIds,
	int32 Layer,
	float LayerHeight,
	USRStructureDataAsset* StructureDataAsset,
	FName NetworkId)
{
	if (!IsValid(SurfaceGrid) || !IsValid(StructureDataAsset) || PathCellIds.IsEmpty())
	{
		return false;
	}

	const int32 SafeLayer = FMath::Max(0, Layer);
	const float SafeLayerHeight = ResolveConveyorLayerHeight(SurfaceGrid, LayerHeight);
	const int32 PreviousVisualPathCount = VisualPaths.Num();
	for (const FSRPlanetSurfaceGridCellId& CellId : PathCellIds)
	{
		const FSRConveyorLaneKey LaneKey = MakeLaneKey(CellId, SafeLayer);
		if (!Segments.Contains(LaneKey) && !CanPlaceConveyorSegment(SurfaceGrid, LaneKey))
		{
			return false;
		}
	}

	TMap<FSRConveyorLaneKey, FSRConveyorSegment> PreviousSegments;
	for (const FSRPlanetSurfaceGridCellId& CellId : PathCellIds)
	{
		const FSRConveyorLaneKey LaneKey = MakeLaneKey(CellId, SafeLayer);
		if (const FSRConveyorSegment* ExistingSegment = Segments.Find(LaneKey))
		{
			PreviousSegments.Add(LaneKey, *ExistingSegment);
		}
	}

	auto RollbackConveyorData = [&]()
	{
		for (const FSRPlanetSurfaceGridCellId& CellId : PathCellIds)
		{
			const FSRConveyorLaneKey LaneKey = MakeLaneKey(CellId, SafeLayer);
			if (const FSRConveyorSegment* PreviousSegment = PreviousSegments.Find(LaneKey))
			{
				Segments.Add(LaneKey, *PreviousSegment);
			}
			else
			{
				Segments.Remove(LaneKey);
			}
		}
		VisualPaths.SetNum(PreviousVisualPathCount, EAllowShrinking::No);
	};

	for (int32 PathIndex = 0; PathIndex < PathCellIds.Num(); ++PathIndex)
	{
		const FSRPlanetSurfaceGridCellId& CellId = PathCellIds[PathIndex];
		ESRConveyorGridDirection InputDirection = ESRConveyorGridDirection::None;
		ESRConveyorGridDirection OutputDirection = ESRConveyorGridDirection::None;
		if (PathIndex > 0)
		{
			ESRConveyorGridDirection PreviousDirection = ESRConveyorGridDirection::None;
			if (FindDirectionBetweenCells(SurfaceGrid, CellId, PathCellIds[PathIndex - 1], PreviousDirection))
			{
				InputDirection = PreviousDirection;
			}
		}
		if (PathIndex + 1 < PathCellIds.Num())
		{
			FindDirectionBetweenCells(SurfaceGrid, CellId, PathCellIds[PathIndex + 1], OutputDirection);
		}

		FSRConveyorSegment Segment;
		Segment.Lane = MakeLaneKey(CellId, SafeLayer);
		Segment.InputDirection = InputDirection;
		Segment.OutputDirection = OutputDirection;
		Segment.Shape = ResolveSegmentShape(InputDirection, OutputDirection);
		Segment.NetworkId = NetworkId;
		Segment.StructureDataAsset = StructureDataAsset;
		Segments.Add(Segment.Lane, Segment);
	}

	FSRConveyorVisualPath VisualPath;
	VisualPath.CellIds = PathCellIds;
	VisualPath.Layer = SafeLayer;
	VisualPath.LayerHeight = SafeLayerHeight;
	VisualPath.NetworkId = NetworkId;
	VisualPath.StructureDataAsset = StructureDataAsset;
	VisualPaths.Add(VisualPath);

	if (SafeLayer == 0)
	{
		TArray<FSRPlanetSurfaceGridCellId> OccupiedCellIds = PathCellIds;
		if (!SurfaceGrid->SetCellsOccupied(OccupiedCellIds, true, NetworkId.IsNone() ? FName(TEXT("Conveyor")) : NetworkId))
		{
			RollbackConveyorData();
			return false;
		}
	}

	if (bSpawnConveyorBeltActors)
	{
		MarkConveyorActorGroupDirty(StructureDataAsset, SafeLayer);
		MarkConveyorActorGroupPlacementDiagnosticPending(StructureDataAsset, SafeLayer);
		ScheduleDirtyConveyorActorGroupRefresh(SurfaceGrid);
	}

	RefreshConveyorVisuals(SurfaceGrid);
	RefreshPCGSplineInputs(SurfaceGrid);
	RequestPCGGeneration();
	RefreshPathDebugLines(SurfaceGrid);
	return true;
}

bool USRConveyorNetworkComponent::TryRemoveConveyorAtCell(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& CellId,
	int32 Layer)
{
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	const int32 SafeLayer = FMath::Max(0, Layer);
	const FSRConveyorLaneKey TargetLaneKey = MakeLaneKey(CellId, SafeLayer);
	const FSRConveyorSegment* RemovedSegment = Segments.Find(TargetLaneKey);
	if (!RemovedSegment)
	{
		return false;
	}
	USRStructureDataAsset* RemovedStructureDataAsset = RemovedSegment->StructureDataAsset.Get();

	TSet<FSRPlanetSurfaceGridCellId> OldAffectedCellIds;
	TSet<FSRPlanetSurfaceGridCellId> RetainedAffectedCellIds;
	TArray<FSRConveyorVisualPath> NewVisualPaths;
	NewVisualPaths.Reserve(VisualPaths.Num() + 1);

	bool bRemovedFromVisualPath = false;
	for (const FSRConveyorVisualPath& VisualPath : VisualPaths)
	{
		if (VisualPath.Layer != SafeLayer || !VisualPath.CellIds.Contains(CellId))
		{
			NewVisualPaths.Add(VisualPath);
			continue;
		}

		bRemovedFromVisualPath = true;
		for (const FSRPlanetSurfaceGridCellId& PathCellId : VisualPath.CellIds)
		{
			OldAffectedCellIds.Add(PathCellId);
		}

		TArray<FSRPlanetSurfaceGridCellId> CurrentSubPath;
		auto FlushCurrentSubPath = [&]()
		{
			if (CurrentSubPath.IsEmpty())
			{
				return;
			}

			FSRConveyorVisualPath SplitVisualPath = VisualPath;
			SplitVisualPath.CellIds = CurrentSubPath;
			NewVisualPaths.Add(SplitVisualPath);
			for (const FSRPlanetSurfaceGridCellId& RetainedCellId : CurrentSubPath)
			{
				RetainedAffectedCellIds.Add(RetainedCellId);
			}
			CurrentSubPath.Reset();
		};

		for (const FSRPlanetSurfaceGridCellId& PathCellId : VisualPath.CellIds)
		{
			if (PathCellId == CellId)
			{
				FlushCurrentSubPath();
				continue;
			}

			CurrentSubPath.Add(PathCellId);
		}
		FlushCurrentSubPath();
	}

	if (!bRemovedFromVisualPath)
	{
		Segments.Remove(TargetLaneKey);
		OldAffectedCellIds.Add(CellId);
	}

	VisualPaths = MoveTemp(NewVisualPaths);
	RebuildSegmentsFromVisualPaths(SurfaceGrid);

	if (SafeLayer == 0)
	{
		TArray<FSRPlanetSurfaceGridCellId> ClearedCellIds;
		for (const FSRPlanetSurfaceGridCellId& OldCellId : OldAffectedCellIds)
		{
			if (!RetainedAffectedCellIds.Contains(OldCellId))
			{
				ClearedCellIds.Add(OldCellId);
			}
		}

		if (ClearedCellIds.IsEmpty())
		{
			ClearedCellIds.Add(CellId);
		}
		SurfaceGrid->SetCellsOccupied(ClearedCellIds, false, NAME_None);
	}

	if (bSpawnConveyorBeltActors)
	{
		MarkConveyorActorGroupDirty(RemovedStructureDataAsset, SafeLayer);
		MarkConveyorActorGroupDeletionDiagnosticPending(RemovedStructureDataAsset, SafeLayer);
		ScheduleDirtyConveyorActorGroupRefresh(SurfaceGrid);
	}
	else
	{
		DestroyPlacedConveyorActors();
		LogConveyorMutationMemoryDiagnostics(TEXT("ConveyorDelete.DestroyPlacedActors"), MakeActorGroupKey(RemovedStructureDataAsset, SafeLayer), CVarSRMemoryDiagnosticsForceGCOnConveyorDelete.GetValueOnGameThread() != 0);
	}
	RefreshConveyorVisuals(SurfaceGrid);
	RefreshPCGSplineInputs(SurfaceGrid);
	RequestPCGGeneration();
	RefreshPathDebugLines(SurfaceGrid);
	return true;
}

FSRConveyorLaneKey USRConveyorNetworkComponent::MakeLaneKey(const FSRPlanetSurfaceGridCellId& CellId, int32 Layer)
{
	FSRConveyorLaneKey LaneKey;
	LaneKey.CellId = CellId;
	LaneKey.Layer = FMath::Max(0, Layer);
	return LaneKey;
}

FName USRConveyorNetworkComponent::MakeActorGroupKey(USRStructureDataAsset* StructureDataAsset, int32 Layer)
{
	if (!IsValid(StructureDataAsset))
	{
		return NAME_None;
	}

	return FName(*FString::Printf(
		TEXT("%s|Layer_%d"),
		*StructureDataAsset->GetPathName(),
		FMath::Max(0, Layer)));
}

ESRConveyorGridDirection USRConveyorNetworkComponent::GetOppositeDirection(ESRConveyorGridDirection Direction)
{
	switch (Direction)
	{
	case ESRConveyorGridDirection::NegativeU:
		return ESRConveyorGridDirection::PositiveU;
	case ESRConveyorGridDirection::PositiveU:
		return ESRConveyorGridDirection::NegativeU;
	case ESRConveyorGridDirection::NegativeV:
		return ESRConveyorGridDirection::PositiveV;
	case ESRConveyorGridDirection::PositiveV:
		return ESRConveyorGridDirection::NegativeV;
	default:
		return ESRConveyorGridDirection::None;
	}
}

ESRConveyorSegmentShape USRConveyorNetworkComponent::ResolveSegmentShape(ESRConveyorGridDirection InputDirection, ESRConveyorGridDirection OutputDirection)
{
	if (InputDirection == ESRConveyorGridDirection::None || OutputDirection == ESRConveyorGridDirection::None)
	{
		return ESRConveyorSegmentShape::End;
	}

	return GetOppositeDirection(InputDirection) == OutputDirection
		? ESRConveyorSegmentShape::Straight
		: ESRConveyorSegmentShape::Corner;
}

bool USRConveyorNetworkComponent::GetNeighborCellIdByDirection(const FSRPlanetSurfaceGridCellNeighbors& Neighbors, ESRConveyorGridDirection Direction, FSRPlanetSurfaceGridCellId& OutCellId)
{
	switch (Direction)
	{
	case ESRConveyorGridDirection::NegativeU:
		OutCellId = Neighbors.NegativeU;
		return true;
	case ESRConveyorGridDirection::PositiveU:
		OutCellId = Neighbors.PositiveU;
		return true;
	case ESRConveyorGridDirection::NegativeV:
		OutCellId = Neighbors.NegativeV;
		return true;
	case ESRConveyorGridDirection::PositiveV:
		OutCellId = Neighbors.PositiveV;
		return true;
	default:
		OutCellId = FSRPlanetSurfaceGridCellId();
		return false;
	}
}

bool USRConveyorNetworkComponent::FindDirectionBetweenCells(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& FromCellId, const FSRPlanetSurfaceGridCellId& ToCellId, ESRConveyorGridDirection& OutDirection)
{
	OutDirection = ESRConveyorGridDirection::None;
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	FSRPlanetSurfaceGridCellNeighbors Neighbors;
	if (!SurfaceGrid->GetCellNeighbors(FromCellId, Neighbors))
	{
		return false;
	}

	const ESRConveyorGridDirection Directions[] =
	{
		ESRConveyorGridDirection::NegativeU,
		ESRConveyorGridDirection::PositiveU,
		ESRConveyorGridDirection::NegativeV,
		ESRConveyorGridDirection::PositiveV,
	};
	for (const ESRConveyorGridDirection Direction : Directions)
	{
		FSRPlanetSurfaceGridCellId NeighborCellId;
		if (GetNeighborCellIdByDirection(Neighbors, Direction, NeighborCellId) && NeighborCellId == ToCellId)
		{
			OutDirection = Direction;
			return true;
		}
	}

	return false;
}

bool USRConveyorNetworkComponent::CanPlaceConveyorSegment(USRPlanetSurfaceGrid* SurfaceGrid, const FSRConveyorLaneKey& LaneKey) const
{
	if (!IsValid(SurfaceGrid) || Segments.Contains(LaneKey))
	{
		return false;
	}

	FSRPlanetSurfaceGridCellInfo CellInfo;
	if (!SurfaceGrid->GetCellInfoById(LaneKey.CellId, CellInfo))
	{
		return false;
	}

	return LaneKey.Layer > 0 || (CellInfo.bCanConstruct && !CellInfo.bOccupied);
}

void USRConveyorNetworkComponent::EnsureBeltMeshComponent()
{
	if (IsValid(BeltMeshComponent))
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	const FName BeltMeshComponentName = MakeUniqueObjectName(OwnerActor, UDynamicMeshComponent::StaticClass(), FName(TEXT("ConveyorBeltMesh")));
	BeltMeshComponent = NewObject<UDynamicMeshComponent>(OwnerActor, BeltMeshComponentName);
	if (!IsValid(BeltMeshComponent))
	{
		return;
	}

	BeltMeshComponent->SetupAttachment(this);
	BeltMeshComponent->SetRelativeTransform(FTransform::Identity);
	BeltMeshComponent->SetMobility(EComponentMobility::Movable);
	BeltMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BeltMeshComponent->SetGenerateOverlapEvents(false);
	BeltMeshComponent->SetCastShadow(false);
	BeltMeshComponent->SetVisibility(true);
	BeltMeshComponent->SetHiddenInGame(false);
	OwnerActor->AddInstanceComponent(BeltMeshComponent);
	BeltMeshComponent->RegisterComponent();
}

void USRConveyorNetworkComponent::EnsurePathDebugLineBatchComponent()
{
	if (IsValid(PathDebugLineBatchComponent))
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	const FName DebugLineBatchName = MakeUniqueObjectName(OwnerActor, ULineBatchComponent::StaticClass(), FName(TEXT("ConveyorPathDebugLineBatch")));
	PathDebugLineBatchComponent = NewObject<ULineBatchComponent>(OwnerActor, DebugLineBatchName);
	if (!IsValid(PathDebugLineBatchComponent))
	{
		return;
	}

	PathDebugLineBatchComponent->SetupAttachment(this);
	PathDebugLineBatchComponent->SetMobility(EComponentMobility::Movable);
	PathDebugLineBatchComponent->SetUsingAbsoluteLocation(true);
	PathDebugLineBatchComponent->SetUsingAbsoluteRotation(true);
	PathDebugLineBatchComponent->SetUsingAbsoluteScale(true);
	PathDebugLineBatchComponent->ComponentTags.AddUnique(TEXT("StarRovers.ConveyorPathDebugLine"));
	OwnerActor->AddInstanceComponent(PathDebugLineBatchComponent);
	PathDebugLineBatchComponent->RegisterComponent();
}

USplineComponent* USRConveyorNetworkComponent::EnsurePCGSplineComponent(int32 SplineIndex)
{
	if (SplineIndex < 0)
	{
		return nullptr;
	}

	while (PCGSplineComponents.Num() <= SplineIndex)
	{
		PCGSplineComponents.Add(nullptr);
	}

	if (IsValid(PCGSplineComponents[SplineIndex]))
	{
		return PCGSplineComponents[SplineIndex];
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return nullptr;
	}

	const FName RequestedName(*FString::Printf(TEXT("%s_%d"), *ConveyorVisualSplineNameBase.ToString(), SplineIndex));
	const FName SplineComponentName = MakeUniqueObjectName(OwnerActor, USplineComponent::StaticClass(), RequestedName);
	USplineComponent* SplineComponent = NewObject<USplineComponent>(OwnerActor, SplineComponentName);
	if (!IsValid(SplineComponent))
	{
		return nullptr;
	}

	SplineComponent->SetupAttachment(this);
	SplineComponent->SetRelativeTransform(FTransform::Identity);
	SplineComponent->SetMobility(EComponentMobility::Movable);
	SplineComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SplineComponent->SetGenerateOverlapEvents(false);
	SplineComponent->SetHiddenInGame(true);
	SplineComponent->SetVisibility(false);
	if (!PCGSplineComponentTag.IsNone())
	{
		SplineComponent->ComponentTags.AddUnique(PCGSplineComponentTag);
	}
	SplineComponent->ComponentTags.AddUnique(TEXT("StarRovers.ConveyorVisualSpline"));

	OwnerActor->AddInstanceComponent(SplineComponent);
	SplineComponent->RegisterComponent();
	PCGSplineComponents[SplineIndex] = SplineComponent;
	return SplineComponent;
}

void USRConveyorNetworkComponent::ClearUnusedPCGSplineComponents(int32 FirstUnusedSplineIndex)
{
	for (int32 SplineIndex = FMath::Max(0, FirstUnusedSplineIndex); SplineIndex < PCGSplineComponents.Num(); ++SplineIndex)
	{
		USplineComponent* SplineComponent = PCGSplineComponents[SplineIndex];
		if (!IsValid(SplineComponent))
		{
			continue;
		}

		SplineComponent->ClearSplinePoints(false);
		SplineComponent->SetVisibility(false);
		SplineComponent->SetHiddenInGame(true);
		SplineComponent->UpdateSpline();
	}
}

bool USRConveyorNetworkComponent::BuildConveyorPathSplinePoints(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRConveyorVisualPath& VisualPath,
	TArray<FVector>& OutWorldPoints,
	TArray<FVector>& OutWorldNormals) const
{
	OutWorldPoints.Reset();
	OutWorldNormals.Reset();
	if (!IsValid(SurfaceGrid) || VisualPath.CellIds.IsEmpty())
	{
		return false;
	}

	const FVector PlanetCenter = SurfaceGrid->GetComponentTransform().GetLocation();
	const float LayerOffset = static_cast<float>(FMath::Max(0, VisualPath.Layer)) * FMath::Max(0.0f, VisualPath.LayerHeight);
	const float HeightOffset = LayerOffset + FMath::Max(0.0f, BeltSurfaceOffset) + PCGSplineHeightOffset;
	OutWorldPoints.Reserve(VisualPath.CellIds.Num());
	OutWorldNormals.Reserve(VisualPath.CellIds.Num());

	for (const FSRPlanetSurfaceGridCellId& CellId : VisualPath.CellIds)
	{
		FSRPlanetSurfaceGridCellInfo CellInfo;
		if (!SurfaceGrid->GetCellInfoById(CellId, CellInfo))
		{
			continue;
		}

		FVector OutwardNormal = CellInfo.WorldNormal.GetSafeNormal();
		if (OutwardNormal.IsNearlyZero())
		{
			OutwardNormal = (CellInfo.WorldCenter - PlanetCenter).GetSafeNormal();
		}
		else if (FVector::DotProduct(OutwardNormal, CellInfo.WorldCenter - PlanetCenter) < 0.0f)
		{
			OutwardNormal *= -1.0f;
		}

		OutWorldPoints.Add(CellInfo.WorldCenter + OutwardNormal * HeightOffset);
		OutWorldNormals.Add(OutwardNormal);
	}

	if (OutWorldPoints.Num() == 1)
	{
		const FSRPlanetSurfaceGridCellId& CellId = VisualPath.CellIds[0];
		FSRPlanetSurfaceGridCell Cell;
		if (SurfaceGrid->GetCellById(CellId, Cell))
		{
			const FTransform SurfaceGridTransform = SurfaceGrid->GetComponentTransform();
			FVector SingleTangent = SurfaceGridTransform.TransformPosition(Cell.Corner10) - SurfaceGridTransform.TransformPosition(Cell.Corner00);
			SingleTangent = SingleTangent - OutWorldNormals[0] * FVector::DotProduct(SingleTangent, OutWorldNormals[0]);
			if (SingleTangent.Normalize())
			{
				const FVector CenterPoint = OutWorldPoints[0];
				const FVector CenterNormal = OutWorldNormals[0];
				const float CellEdgeLength = FVector::Distance(
					SurfaceGridTransform.TransformPosition(Cell.Corner00),
					SurfaceGridTransform.TransformPosition(Cell.Corner10));
				const float HalfLength = FMath::Clamp(BeltWidth * 0.5f, 1.0f, FMath::Max(1.0f, CellEdgeLength * 0.35f));
				OutWorldPoints.Reset();
				OutWorldNormals.Reset();
				OutWorldPoints.Add(CenterPoint - SingleTangent * HalfLength);
				OutWorldPoints.Add(CenterPoint + SingleTangent * HalfLength);
				OutWorldNormals.Add(CenterNormal);
				OutWorldNormals.Add(CenterNormal);
			}
		}
	}

	return OutWorldPoints.Num() >= 2 && OutWorldPoints.Num() == OutWorldNormals.Num();
}

float USRConveyorNetworkComponent::ResolveBeltHalfWidth(const TArray<FVector>& WorldPoints) const
{
	float TotalSegmentLength = 0.0f;
	int32 SegmentCount = 0;
	for (int32 PointIndex = 1; PointIndex < WorldPoints.Num(); ++PointIndex)
	{
		const float SegmentLength = FVector::Distance(WorldPoints[PointIndex - 1], WorldPoints[PointIndex]);
		if (SegmentLength > KINDA_SMALL_NUMBER)
		{
			TotalSegmentLength += SegmentLength;
			++SegmentCount;
		}
	}

	const float DesiredHalfWidth = FMath::Max(1.0f, BeltWidth * 0.5f);
	if (SegmentCount <= 0)
	{
		return DesiredHalfWidth;
	}

	const float AverageSegmentLength = TotalSegmentLength / static_cast<float>(SegmentCount);
	return FMath::Clamp(DesiredHalfWidth, 1.0f, FMath::Max(1.0f, AverageSegmentLength * 0.35f));
}

float USRConveyorNetworkComponent::ResolveBeltHalfThickness(float HalfWidth, float LayerHeight) const
{
	const float DesiredHalfThickness = FMath::Max(1.0f, BeltThickness * 0.5f);
	const float LayerLimitedHalfThickness = LayerHeight > KINDA_SMALL_NUMBER
		? FMath::Max(1.0f, LayerHeight * 0.35f)
		: DesiredHalfThickness;
	return FMath::Clamp(DesiredHalfThickness, 1.0f, FMath::Max(1.0f, FMath::Min(HalfWidth * 0.5f, LayerLimitedHalfThickness)));
}

float USRConveyorNetworkComponent::ResolveConveyorLayerHeight(USRPlanetSurfaceGrid* SurfaceGrid, float RequestedLayerHeight) const
{
	const float TerrainHeightStep = IsValid(SurfaceGrid) ? SurfaceGrid->GetTerrainHeightStep() : 0.0f;
	if (TerrainHeightStep > KINDA_SMALL_NUMBER)
	{
		return TerrainHeightStep;
	}

	if (RequestedLayerHeight > KINDA_SMALL_NUMBER)
	{
		return RequestedLayerHeight;
	}

	return DefaultLayerHeight;
}

bool USRConveyorNetworkComponent::BuildConveyorSegmentRibbon(
	UE::Geometry::FDynamicMesh3& BeltMesh,
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRConveyorSegment& Segment,
	float LayerHeight) const
{
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	FTransform CurrentTransform;
	if (!SurfaceGrid->GetCellWorldTransform(Segment.Lane.CellId, 0.0f, CurrentTransform))
	{
		return false;
	}

	auto ResolveNeighborTransform = [SurfaceGrid, &Segment](ESRConveyorGridDirection Direction, FTransform& OutTransform)
	{
		if (Direction == ESRConveyorGridDirection::None)
		{
			return false;
		}

		FSRPlanetSurfaceGridCellNeighbors Neighbors;
		FSRPlanetSurfaceGridCellId NeighborCellId;
		if (!SurfaceGrid->GetCellNeighbors(Segment.Lane.CellId, Neighbors)
			|| !GetNeighborCellIdByDirection(Neighbors, Direction, NeighborCellId))
		{
			return false;
		}

		return SurfaceGrid->GetCellWorldTransform(NeighborCellId, 0.0f, OutTransform);
	};

	const float HeightOffset = static_cast<float>(FMath::Max(0, Segment.Lane.Layer)) * FMath::Max(0.0f, LayerHeight) + BeltSurfaceOffset;
	const FVector CurrentNormal = CurrentTransform.GetRotation().GetAxisZ().GetSafeNormal();
	const FVector CurrentWorldPosition = CurrentTransform.GetLocation() + (CurrentNormal * HeightOffset);

	FTransform InputTransform;
	FTransform OutputTransform;
	const bool bHasInput = ResolveNeighborTransform(Segment.InputDirection, InputTransform);
	const bool bHasOutput = ResolveNeighborTransform(Segment.OutputDirection, OutputTransform);
	const FVector InputWorldPosition = bHasInput
		? InputTransform.GetLocation() + (InputTransform.GetRotation().GetAxisZ().GetSafeNormal() * HeightOffset)
		: FVector::ZeroVector;
	const FVector OutputWorldPosition = bHasOutput
		? OutputTransform.GetLocation() + (OutputTransform.GetRotation().GetAxisZ().GetSafeNormal() * HeightOffset)
		: FVector::ZeroVector;

	FVector Forward = FVector::ZeroVector;
	if (bHasOutput)
	{
		Forward = OutputWorldPosition - CurrentWorldPosition;
	}
	else if (bHasInput)
	{
		Forward = CurrentWorldPosition - InputWorldPosition;
	}
	else
	{
		Forward = CurrentTransform.GetRotation().GetAxisX();
	}
	Forward = Forward - CurrentNormal * FVector::DotProduct(Forward, CurrentNormal);
	if (!Forward.Normalize())
	{
		Forward = CurrentTransform.GetRotation().GetAxisX();
	}

	const float HalfSegmentLength = bHasOutput
		? FVector::Distance(CurrentWorldPosition, OutputWorldPosition) * 0.5f
		: (bHasInput ? FVector::Distance(CurrentWorldPosition, InputWorldPosition) * 0.5f : BeltWidth);
	const FVector StartWorldPosition = bHasInput
		? (InputWorldPosition + CurrentWorldPosition) * 0.5f
		: CurrentWorldPosition - Forward * HalfSegmentLength;
	const FVector EndWorldPosition = bHasOutput
		? (CurrentWorldPosition + OutputWorldPosition) * 0.5f
		: CurrentWorldPosition + Forward * HalfSegmentLength;

	auto AppendRibbonQuad = [this, &BeltMesh](const FVector& WorldPointA, const FVector& WorldPointB, const FVector& WorldNormal)
	{
		FVector SegmentTangent = WorldPointB - WorldPointA;
		SegmentTangent = SegmentTangent - WorldNormal * FVector::DotProduct(SegmentTangent, WorldNormal);
		if (!SegmentTangent.Normalize())
		{
			return;
		}

		FVector Side = FVector::CrossProduct(WorldNormal, SegmentTangent).GetSafeNormal();
		if (Side.IsNearlyZero())
		{
			return;
		}

		const float HalfWidth = FMath::Max(1.0f, BeltWidth * 0.5f);
		const FTransform ComponentTransform = GetComponentTransform();
		const FVector LocalPoint0 = ComponentTransform.InverseTransformPosition(WorldPointA - Side * HalfWidth);
		const FVector LocalPoint1 = ComponentTransform.InverseTransformPosition(WorldPointA + Side * HalfWidth);
		const FVector LocalPoint2 = ComponentTransform.InverseTransformPosition(WorldPointB + Side * HalfWidth);
		const FVector LocalPoint3 = ComponentTransform.InverseTransformPosition(WorldPointB - Side * HalfWidth);
		const FVector LocalNormal = ComponentTransform.InverseTransformVectorNoScale(WorldNormal).GetSafeNormal();

		const int32 Vertex0 = BeltMesh.AppendVertex(FVector3d(LocalPoint0));
		const int32 Vertex1 = BeltMesh.AppendVertex(FVector3d(LocalPoint1));
		const int32 Vertex2 = BeltMesh.AppendVertex(FVector3d(LocalPoint2));
		const int32 Vertex3 = BeltMesh.AppendVertex(FVector3d(LocalPoint3));
		const int32 Triangle0 = BeltMesh.AppendTriangle(Vertex0, Vertex2, Vertex1);
		const int32 Triangle1 = BeltMesh.AppendTriangle(Vertex0, Vertex3, Vertex2);
		const int32 BackTriangle0 = BeltMesh.AppendTriangle(Vertex0, Vertex1, Vertex2);
		const int32 BackTriangle1 = BeltMesh.AppendTriangle(Vertex0, Vertex2, Vertex3);

		UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay = BeltMesh.Attributes()->PrimaryNormals();
		UE::Geometry::FDynamicMeshUVOverlay* UVOverlay = BeltMesh.Attributes()->PrimaryUV();
		auto* ColorOverlay = BeltMesh.Attributes()->PrimaryColors();
		if (!NormalOverlay || !UVOverlay || !ColorOverlay)
		{
			return;
		}

		const int32 Normal0 = NormalOverlay->AppendElement(FVector3f(LocalNormal));
		const int32 Normal1 = NormalOverlay->AppendElement(FVector3f(LocalNormal));
		const int32 Normal2 = NormalOverlay->AppendElement(FVector3f(LocalNormal));
		const int32 Normal3 = NormalOverlay->AppendElement(FVector3f(LocalNormal));
		const FLinearColor BeltColor = FLinearColor::White;
		const int32 Color0 = ColorOverlay->AppendElement(FVector4f(BeltColor.R, BeltColor.G, BeltColor.B, BeltColor.A));
		const int32 Color1 = ColorOverlay->AppendElement(FVector4f(BeltColor.R, BeltColor.G, BeltColor.B, BeltColor.A));
		const int32 Color2 = ColorOverlay->AppendElement(FVector4f(BeltColor.R, BeltColor.G, BeltColor.B, BeltColor.A));
		const int32 Color3 = ColorOverlay->AppendElement(FVector4f(BeltColor.R, BeltColor.G, BeltColor.B, BeltColor.A));
		const float VLength = FVector::Distance(WorldPointA, WorldPointB) / FMath::Max(1.0f, BeltWidth);
		const int32 UV0 = UVOverlay->AppendElement(FVector2f(0.0f, 0.0f));
		const int32 UV1 = UVOverlay->AppendElement(FVector2f(1.0f, 0.0f));
		const int32 UV2 = UVOverlay->AppendElement(FVector2f(1.0f, VLength));
		const int32 UV3 = UVOverlay->AppendElement(FVector2f(0.0f, VLength));

		if (Triangle0 >= 0)
		{
			NormalOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(Normal0, Normal2, Normal1));
			UVOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(UV0, UV2, UV1));
			ColorOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(Color0, Color2, Color1));
		}
		if (Triangle1 >= 0)
		{
			NormalOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(Normal0, Normal3, Normal2));
			UVOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(UV0, UV3, UV2));
			ColorOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(Color0, Color3, Color2));
		}
		if (BackTriangle0 >= 0)
		{
			const int32 BackNormal0 = NormalOverlay->AppendElement(FVector3f(-LocalNormal));
			const int32 BackNormal1 = NormalOverlay->AppendElement(FVector3f(-LocalNormal));
			const int32 BackNormal2 = NormalOverlay->AppendElement(FVector3f(-LocalNormal));
			NormalOverlay->SetTriangle(BackTriangle0, UE::Geometry::FIndex3i(BackNormal0, BackNormal1, BackNormal2));
			UVOverlay->SetTriangle(BackTriangle0, UE::Geometry::FIndex3i(UV0, UV1, UV2));
			ColorOverlay->SetTriangle(BackTriangle0, UE::Geometry::FIndex3i(Color0, Color1, Color2));
		}
		if (BackTriangle1 >= 0)
		{
			const int32 BackNormal0 = NormalOverlay->AppendElement(FVector3f(-LocalNormal));
			const int32 BackNormal2 = NormalOverlay->AppendElement(FVector3f(-LocalNormal));
			const int32 BackNormal3 = NormalOverlay->AppendElement(FVector3f(-LocalNormal));
			NormalOverlay->SetTriangle(BackTriangle1, UE::Geometry::FIndex3i(BackNormal0, BackNormal2, BackNormal3));
			UVOverlay->SetTriangle(BackTriangle1, UE::Geometry::FIndex3i(UV0, UV2, UV3));
			ColorOverlay->SetTriangle(BackTriangle1, UE::Geometry::FIndex3i(Color0, Color2, Color3));
		}
	};

	if (bHasInput && bHasOutput && Segment.Shape == ESRConveyorSegmentShape::Corner)
	{
		AppendRibbonQuad(StartWorldPosition, CurrentWorldPosition, CurrentNormal);
		AppendRibbonQuad(CurrentWorldPosition, EndWorldPosition, CurrentNormal);
	}
	else
	{
		AppendRibbonQuad(StartWorldPosition, EndWorldPosition, CurrentNormal);
	}

	return true;
}

bool USRConveyorNetworkComponent::BuildConveyorPathRibbon(
	UE::Geometry::FDynamicMesh3& BeltMesh,
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRConveyorVisualPath& VisualPath) const
{
	if (!IsValid(SurfaceGrid) || VisualPath.CellIds.IsEmpty())
	{
		return false;
	}

	const AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return false;
	}

	const FVector PlanetCenter = SurfaceGrid->GetComponentTransform().GetLocation();
	const float LayerOffset = static_cast<float>(FMath::Max(0, VisualPath.Layer)) * FMath::Max(0.0f, VisualPath.LayerHeight);
	TArray<FVector> WorldPoints;
	TArray<FVector> WorldNormals;
	WorldPoints.Reserve(VisualPath.CellIds.Num());
	WorldNormals.Reserve(VisualPath.CellIds.Num());

	for (const FSRPlanetSurfaceGridCellId& CellId : VisualPath.CellIds)
	{
		FSRPlanetSurfaceGridCellInfo CellInfo;
		if (!SurfaceGrid->GetCellInfoById(CellId, CellInfo))
		{
			continue;
		}

		FVector OutwardNormal = CellInfo.WorldNormal.GetSafeNormal();
		if (OutwardNormal.IsNearlyZero())
		{
			OutwardNormal = (CellInfo.WorldCenter - PlanetCenter).GetSafeNormal();
		}
		else if (FVector::DotProduct(OutwardNormal, CellInfo.WorldCenter - PlanetCenter) < 0.0f)
		{
			OutwardNormal *= -1.0f;
		}

		WorldPoints.Add(CellInfo.WorldCenter + OutwardNormal * LayerOffset);
		WorldNormals.Add(OutwardNormal);
	}

	if (WorldPoints.IsEmpty())
	{
		return false;
	}

	if (WorldPoints.Num() == 1)
	{
		const FSRPlanetSurfaceGridCellId& CellId = VisualPath.CellIds[0];
		FSRPlanetSurfaceGridCell Cell;
		if (SurfaceGrid->GetCellById(CellId, Cell))
		{
			const FTransform SurfaceGridTransform = SurfaceGrid->GetComponentTransform();
			FVector SingleTangent = SurfaceGridTransform.TransformPosition(Cell.Corner10) - SurfaceGridTransform.TransformPosition(Cell.Corner00);
			SingleTangent = SingleTangent - WorldNormals[0] * FVector::DotProduct(SingleTangent, WorldNormals[0]);
			if (SingleTangent.Normalize())
			{
				const FVector CenterPoint = WorldPoints[0];
				const FVector CenterNormal = WorldNormals[0];
				const float CellEdgeLength = FVector::Distance(
					SurfaceGridTransform.TransformPosition(Cell.Corner00),
					SurfaceGridTransform.TransformPosition(Cell.Corner10));
				const float HalfLength = FMath::Clamp(BeltWidth * 0.5f, 1.0f, FMath::Max(1.0f, CellEdgeLength * 0.35f));
				WorldPoints.Reset();
				WorldNormals.Reset();
				WorldPoints.Add(CenterPoint - SingleTangent * HalfLength);
				WorldPoints.Add(CenterPoint + SingleTangent * HalfLength);
				WorldNormals.Add(CenterNormal);
				WorldNormals.Add(CenterNormal);
			}
		}
	}

	if (WorldPoints.Num() < 2)
	{
		return false;
	}

	UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay = BeltMesh.Attributes()->PrimaryNormals();
	UE::Geometry::FDynamicMeshUVOverlay* UVOverlay = BeltMesh.Attributes()->PrimaryUV();
	auto* ColorOverlay = BeltMesh.Attributes()->PrimaryColors();
	if (!NormalOverlay || !UVOverlay || !ColorOverlay)
	{
		return false;
	}

	const FTransform ComponentTransform = GetComponentTransform();
	const float HalfWidth = ResolveBeltHalfWidth(WorldPoints);
	const float HalfThickness = ResolveBeltHalfThickness(HalfWidth, VisualPath.LayerHeight);
	const float CenterSurfaceOffset = FMath::Max(0.0f, BeltSurfaceOffset) + HalfThickness;
	for (int32 PointIndex = 0; PointIndex < WorldPoints.Num(); ++PointIndex)
	{
		WorldPoints[PointIndex] += WorldNormals[PointIndex].GetSafeNormal() * CenterSurfaceOffset;
	}
	const FLinearColor BeltColor = FLinearColor::White;

	auto AppendQuad = [&BeltMesh, NormalOverlay, UVOverlay, ColorOverlay, &BeltColor](
		int32 Vertex0,
		int32 Vertex1,
		int32 Vertex2,
		int32 Vertex3,
		const FVector& LocalNormal,
		const FVector2f& UV0,
		const FVector2f& UV1,
		const FVector2f& UV2,
		const FVector2f& UV3)
	{
		const int32 Triangle0 = BeltMesh.AppendTriangle(Vertex0, Vertex1, Vertex2);
		const int32 Triangle1 = BeltMesh.AppendTriangle(Vertex0, Vertex2, Vertex3);
		if (Triangle0 < 0 || Triangle1 < 0)
		{
			return;
		}

		const int32 Normal0 = NormalOverlay->AppendElement(FVector3f(LocalNormal));
		const int32 Normal1 = NormalOverlay->AppendElement(FVector3f(LocalNormal));
		const int32 Normal2 = NormalOverlay->AppendElement(FVector3f(LocalNormal));
		const int32 Normal3 = NormalOverlay->AppendElement(FVector3f(LocalNormal));
		const int32 Color0 = ColorOverlay->AppendElement(FVector4f(BeltColor.R, BeltColor.G, BeltColor.B, BeltColor.A));
		const int32 Color1 = ColorOverlay->AppendElement(FVector4f(BeltColor.R, BeltColor.G, BeltColor.B, BeltColor.A));
		const int32 Color2 = ColorOverlay->AppendElement(FVector4f(BeltColor.R, BeltColor.G, BeltColor.B, BeltColor.A));
		const int32 Color3 = ColorOverlay->AppendElement(FVector4f(BeltColor.R, BeltColor.G, BeltColor.B, BeltColor.A));
		const int32 UVElement0 = UVOverlay->AppendElement(UV0);
		const int32 UVElement1 = UVOverlay->AppendElement(UV1);
		const int32 UVElement2 = UVOverlay->AppendElement(UV2);
		const int32 UVElement3 = UVOverlay->AppendElement(UV3);

		NormalOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(Normal0, Normal1, Normal2));
		NormalOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(Normal0, Normal2, Normal3));
		UVOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(UVElement0, UVElement1, UVElement2));
		UVOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(UVElement0, UVElement2, UVElement3));
		ColorOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(Color0, Color1, Color2));
		ColorOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(Color0, Color2, Color3));
	};

	float AccumulatedDistance = 0.0f;
	for (int32 SegmentIndex = 0; SegmentIndex + 1 < WorldPoints.Num(); ++SegmentIndex)
	{
		const FVector SegmentStart = WorldPoints[SegmentIndex];
		const FVector SegmentEnd = WorldPoints[SegmentIndex + 1];
		const float SegmentLength = FVector::Distance(SegmentStart, SegmentEnd);
		if (SegmentLength <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		FVector Normal = (WorldNormals[SegmentIndex] + WorldNormals[SegmentIndex + 1]).GetSafeNormal();
		if (Normal.IsNearlyZero())
		{
			Normal = WorldNormals[SegmentIndex].GetSafeNormal();
		}

		FVector Tangent = SegmentEnd - SegmentStart;
		Tangent = Tangent - Normal * FVector::DotProduct(Tangent, Normal);
		if (!Tangent.Normalize())
		{
			continue;
		}

		const FVector Side = FVector::CrossProduct(Normal, Tangent).GetSafeNormal();
		if (Side.IsNearlyZero())
		{
			continue;
		}

		const FVector TopOffset = Normal * HalfThickness;
		const FVector BottomOffset = -TopOffset;
		const FVector WidthOffset = Side * HalfWidth;
		const FVector WorldTopLeft0 = SegmentStart - WidthOffset + TopOffset;
		const FVector WorldTopRight0 = SegmentStart + WidthOffset + TopOffset;
		const FVector WorldTopRight1 = SegmentEnd + WidthOffset + TopOffset;
		const FVector WorldTopLeft1 = SegmentEnd - WidthOffset + TopOffset;
		const FVector WorldBottomLeft0 = SegmentStart - WidthOffset + BottomOffset;
		const FVector WorldBottomRight0 = SegmentStart + WidthOffset + BottomOffset;
		const FVector WorldBottomRight1 = SegmentEnd + WidthOffset + BottomOffset;
		const FVector WorldBottomLeft1 = SegmentEnd - WidthOffset + BottomOffset;

		const int32 TopLeft0 = BeltMesh.AppendVertex(FVector3d(ComponentTransform.InverseTransformPosition(WorldTopLeft0)));
		const int32 TopRight0 = BeltMesh.AppendVertex(FVector3d(ComponentTransform.InverseTransformPosition(WorldTopRight0)));
		const int32 TopRight1 = BeltMesh.AppendVertex(FVector3d(ComponentTransform.InverseTransformPosition(WorldTopRight1)));
		const int32 TopLeft1 = BeltMesh.AppendVertex(FVector3d(ComponentTransform.InverseTransformPosition(WorldTopLeft1)));
		const int32 BottomLeft0 = BeltMesh.AppendVertex(FVector3d(ComponentTransform.InverseTransformPosition(WorldBottomLeft0)));
		const int32 BottomRight0 = BeltMesh.AppendVertex(FVector3d(ComponentTransform.InverseTransformPosition(WorldBottomRight0)));
		const int32 BottomRight1 = BeltMesh.AppendVertex(FVector3d(ComponentTransform.InverseTransformPosition(WorldBottomRight1)));
		const int32 BottomLeft1 = BeltMesh.AppendVertex(FVector3d(ComponentTransform.InverseTransformPosition(WorldBottomLeft1)));
		const FVector LocalNormal = ComponentTransform.InverseTransformVectorNoScale(Normal).GetSafeNormal();
		const float TextureWidth = FMath::Max(1.0f, HalfWidth * 2.0f);
		const float V0 = AccumulatedDistance / TextureWidth;
		const float V1 = (AccumulatedDistance + SegmentLength) / TextureWidth;
		const FVector LocalSideNormal = ComponentTransform.InverseTransformVectorNoScale(Side).GetSafeNormal();
		const FVector LocalTangentNormal = ComponentTransform.InverseTransformVectorNoScale(Tangent).GetSafeNormal();

		AppendQuad(TopLeft0, TopLeft1, TopRight1, TopRight0, LocalNormal, FVector2f(0.0f, V0), FVector2f(0.0f, V1), FVector2f(1.0f, V1), FVector2f(1.0f, V0));
		AppendQuad(BottomLeft0, BottomRight0, BottomRight1, BottomLeft1, -LocalNormal, FVector2f(0.0f, V0), FVector2f(1.0f, V0), FVector2f(1.0f, V1), FVector2f(0.0f, V1));
		AppendQuad(TopRight0, TopRight1, BottomRight1, BottomRight0, LocalSideNormal, FVector2f(0.0f, V0), FVector2f(0.0f, V1), FVector2f(1.0f, V1), FVector2f(1.0f, V0));
		AppendQuad(TopLeft0, BottomLeft0, BottomLeft1, TopLeft1, -LocalSideNormal, FVector2f(0.0f, V0), FVector2f(1.0f, V0), FVector2f(1.0f, V1), FVector2f(0.0f, V1));
		AppendQuad(TopLeft0, TopRight0, BottomRight0, BottomLeft0, -LocalTangentNormal, FVector2f(0.0f, 0.0f), FVector2f(1.0f, 0.0f), FVector2f(1.0f, 1.0f), FVector2f(0.0f, 1.0f));
		AppendQuad(TopLeft1, BottomLeft1, BottomRight1, TopRight1, LocalTangentNormal, FVector2f(0.0f, 0.0f), FVector2f(0.0f, 1.0f), FVector2f(1.0f, 1.0f), FVector2f(1.0f, 0.0f));

		AccumulatedDistance += SegmentLength;
	}

	return true;
}

ASRConveyorBeltActor* USRConveyorNetworkComponent::SpawnConveyorActorForVisualPaths(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TArray<FSRConveyorVisualPath>& GroupedVisualPaths)
{
	if (!IsValid(SurfaceGrid) || GroupedVisualPaths.IsEmpty())
	{
		return nullptr;
	}

	const FSRConveyorVisualPath& FirstVisualPath = GroupedVisualPaths[0];
	if (!IsValid(FirstVisualPath.StructureDataAsset) || FirstVisualPath.CellIds.IsEmpty())
	{
		return nullptr;
	}

	AActor* SurfaceOwner = SurfaceGrid->GetOwner();
	UWorld* World = SurfaceOwner ? SurfaceOwner->GetWorld() : nullptr;
	const FSRStructureData StructureData = FirstVisualPath.StructureDataAsset->BuildData();
	UClass* ConveyorActorClass = StructureData.StructureActorClass.Get();
	if (!IsValid(SurfaceOwner) || !World || !IsValid(ConveyorActorClass) || !ConveyorActorClass->IsChildOf(ASRConveyorBeltActor::StaticClass()))
	{
		UE_LOG(LogTemp, Error, TEXT("Cannot place conveyor from '%s': StructureActorClass must be set to a subclass of ASRConveyorBeltActor."), *GetNameSafe(FirstVisualPath.StructureDataAsset));
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = SurfaceOwner;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASRConveyorBeltActor* PlacedConveyorActor = World->SpawnActor<ASRConveyorBeltActor>(ConveyorActorClass, SurfaceOwner->GetActorTransform(), SpawnParameters);
	if (!IsValid(PlacedConveyorActor))
	{
		return nullptr;
	}

	PlacedConveyorActor->SetOwner(SurfaceOwner);
	PlacedConveyorActor->SetActorHiddenInGame(false);
	if (!PlacedConveyorActor->AttachToActor(SurfaceOwner, FAttachmentTransformRules::KeepWorldTransform))
	{
		PlacedConveyorActor->Destroy();
		return nullptr;
	}

	if (!PlacedConveyorActor->InitializeConveyorPaths(SurfaceGrid, GroupedVisualPaths, PCGSplineComponentTag, BeltSurfaceOffset + PCGSplineHeightOffset))
	{
		PlacedConveyorActor->Destroy();
		return nullptr;
	}

	return PlacedConveyorActor;
}

void USRConveyorNetworkComponent::DestroyPlacedConveyorActors()
{
	for (ASRConveyorBeltActor* ConveyorActor : PlacedConveyorActors)
	{
		if (IsValid(ConveyorActor))
		{
			ConveyorActor->Destroy();
		}
	}

	PlacedConveyorActors.Reset();
	ConveyorActorGroupsByKey.Reset();
	PendingPlacementDiagnosticActorGroupKeys.Reset();
	PendingDeletionDiagnosticActorGroupKeys.Reset();
	PendingConveyorActorRefreshSurfaceGrid.Reset();
	SetComponentTickEnabled(false);
}

void USRConveyorNetworkComponent::MarkConveyorActorGroupDirty(USRStructureDataAsset* StructureDataAsset, int32 Layer)
{
	const FName ActorGroupKey = MakeActorGroupKey(StructureDataAsset, Layer);
	if (ActorGroupKey.IsNone())
	{
		return;
	}

	FSRConveyorActorGroupState& ActorGroup = ConveyorActorGroupsByKey.FindOrAdd(ActorGroupKey);
	ActorGroup.bDirty = true;
}

void USRConveyorNetworkComponent::MarkConveyorActorGroupPlacementDiagnosticPending(USRStructureDataAsset* StructureDataAsset, int32 Layer)
{
	const FName ActorGroupKey = MakeActorGroupKey(StructureDataAsset, Layer);
	if (!ActorGroupKey.IsNone())
	{
		PendingPlacementDiagnosticActorGroupKeys.Add(ActorGroupKey);
	}
}

void USRConveyorNetworkComponent::MarkConveyorActorGroupDeletionDiagnosticPending(USRStructureDataAsset* StructureDataAsset, int32 Layer)
{
	const FName ActorGroupKey = MakeActorGroupKey(StructureDataAsset, Layer);
	if (!ActorGroupKey.IsNone())
	{
		PendingDeletionDiagnosticActorGroupKeys.Add(ActorGroupKey);
	}
}

void USRConveyorNetworkComponent::ScheduleDirtyConveyorActorGroupRefresh(USRPlanetSurfaceGrid* SurfaceGrid)
{
	if (!bSpawnConveyorBeltActors || !HasDirtyConveyorActorGroups())
	{
		return;
	}

	if (IsValid(SurfaceGrid))
	{
		PendingConveyorActorRefreshSurfaceGrid = SurfaceGrid;
	}
	SetComponentTickEnabled(true);
}

bool USRConveyorNetworkComponent::RefreshConveyorActorGroup(USRPlanetSurfaceGrid* SurfaceGrid, FName ActorGroupKey)
{
	if (ActorGroupKey.IsNone())
	{
		return true;
	}

	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	FSRConveyorActorGroupState& ActorGroup = ConveyorActorGroupsByKey.FindOrAdd(ActorGroupKey);
	const bool bLogPlacementDiagnostics = PendingPlacementDiagnosticActorGroupKeys.Remove(ActorGroupKey) > 0;
	const bool bLogDeletionDiagnostics = PendingDeletionDiagnosticActorGroupKeys.Remove(ActorGroupKey) > 0;
	ActorGroup.VisualPaths.Reset();
	for (const FSRConveyorVisualPath& VisualPath : VisualPaths)
	{
		if (MakeActorGroupKey(VisualPath.StructureDataAsset.Get(), VisualPath.Layer) == ActorGroupKey)
		{
			ActorGroup.VisualPaths.Add(VisualPath);
		}
	}

	if (ActorGroup.VisualPaths.IsEmpty())
	{
		if (IsValid(ActorGroup.Actor))
		{
			PlacedConveyorActors.RemoveAll([&ActorGroup](const auto& PlacedActor)
			{
				return PlacedActor.Get() == ActorGroup.Actor;
			});
			ActorGroup.Actor->Destroy();
		}

		ConveyorActorGroupsByKey.Remove(ActorGroupKey);
		if (bLogDeletionDiagnostics)
		{
			LogConveyorMutationMemoryDiagnostics(TEXT("ConveyorDelete.ActorGroupRemoved"), ActorGroupKey, CVarSRMemoryDiagnosticsForceGCOnConveyorDelete.GetValueOnGameThread() != 0);
		}
		return true;
	}

	if (!IsValid(ActorGroup.Actor))
	{
		ActorGroup.Actor = SpawnConveyorActorForVisualPaths(SurfaceGrid, ActorGroup.VisualPaths);
		if (IsValid(ActorGroup.Actor))
		{
			PlacedConveyorActors.AddUnique(ActorGroup.Actor);
		}
	}
	else if (!ActorGroup.Actor->InitializeConveyorPaths(SurfaceGrid, ActorGroup.VisualPaths, PCGSplineComponentTag, BeltSurfaceOffset + PCGSplineHeightOffset))
	{
		PlacedConveyorActors.RemoveAll([&ActorGroup](const auto& PlacedActor)
		{
			return PlacedActor.Get() == ActorGroup.Actor;
		});
		ActorGroup.Actor->Destroy();
		ActorGroup.Actor = nullptr;
	}

	ActorGroup.bDirty = false;
	if (bLogPlacementDiagnostics && CVarSRMemoryDiagnosticsConveyorPlacement.GetValueOnGameThread() != 0)
	{
		LogConveyorMutationMemoryDiagnostics(TEXT("ConveyorPlace.ActorGroupRefreshed"), ActorGroupKey, false);
	}
	if (bLogDeletionDiagnostics)
	{
		LogConveyorMutationMemoryDiagnostics(TEXT("ConveyorDelete.ActorGroupRefreshed"), ActorGroupKey, CVarSRMemoryDiagnosticsForceGCOnConveyorDelete.GetValueOnGameThread() != 0);
	}
	return IsValid(ActorGroup.Actor);
}

bool USRConveyorNetworkComponent::RefreshDirtyConveyorActorGroups(USRPlanetSurfaceGrid* SurfaceGrid, int32 MaxGroupCount)
{
	if (!bSpawnConveyorBeltActors)
	{
		return true;
	}

	TArray<FName> DirtyActorGroupKeys;
	const int32 GroupBudget = MaxGroupCount == INDEX_NONE
		? TNumericLimits<int32>::Max()
		: FMath::Max(1, MaxGroupCount);
	for (const TPair<FName, FSRConveyorActorGroupState>& ActorGroupPair : ConveyorActorGroupsByKey)
	{
		if (ActorGroupPair.Value.bDirty)
		{
			DirtyActorGroupKeys.Add(ActorGroupPair.Key);
			if (DirtyActorGroupKeys.Num() >= GroupBudget)
			{
				break;
			}
		}
	}

	bool bAllGroupsRefreshed = true;
	for (const FName ActorGroupKey : DirtyActorGroupKeys)
	{
		if (!RefreshConveyorActorGroup(SurfaceGrid, ActorGroupKey))
		{
			bAllGroupsRefreshed = false;
		}
	}

	return bAllGroupsRefreshed;
}

bool USRConveyorNetworkComponent::HasDirtyConveyorActorGroups() const
{
	for (const TPair<FName, FSRConveyorActorGroupState>& ActorGroupPair : ConveyorActorGroupsByKey)
	{
		if (ActorGroupPair.Value.bDirty)
		{
			return true;
		}
	}

	return false;
}

void USRConveyorNetworkComponent::LogConveyorMutationMemoryDiagnostics(const TCHAR* Label, FName ActorGroupKey, bool bRequestGarbageCollection) const
{
	const FString LabelString(Label);
	const bool bIsPlacementLog = LabelString.StartsWith(TEXT("ConveyorPlace"));
	const bool bIsDeletionLog = LabelString.StartsWith(TEXT("ConveyorDelete"));
	if ((bIsPlacementLog && CVarSRMemoryDiagnosticsConveyorPlacement.GetValueOnGameThread() == 0)
		|| (bIsDeletionLog && CVarSRMemoryDiagnosticsConveyorDelete.GetValueOnGameThread() == 0))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	TArray<FString> ExtraLines;
	ExtraLines.Add(FString::Printf(
		TEXT("ConveyorNetwork Owner=%s ActorGroup=%s Segments=%d VisualPaths=%d PlacedActors=%d ActorGroups=%d PendingDeletionDiagnostics=%d ForceGC=%s"),
		*GetNameSafe(GetOwner()),
		*ActorGroupKey.ToString(),
		Segments.Num(),
		VisualPaths.Num(),
		PlacedConveyorActors.Num(),
		ConveyorActorGroupsByKey.Num(),
		PendingDeletionDiagnosticActorGroupKeys.Num(),
		bRequestGarbageCollection ? TEXT("true") : TEXT("false")));

	if (bRequestGarbageCollection)
	{
		FSRMemoryDiagnostics::RequestGarbageCollectionAndLogNextTick(World, Label, ExtraLines);
		return;
	}

	FSRMemoryDiagnostics::LogSnapshot(World, FString::Printf(TEXT("%s.AfterRefresh"), Label), ExtraLines);
	FSRMemoryDiagnostics::LogSnapshotNextTick(World, FString::Printf(TEXT("%s.AfterTick"), Label), ExtraLines);
}

void USRConveyorNetworkComponent::RebuildSegmentsFromVisualPaths(USRPlanetSurfaceGrid* SurfaceGrid)
{
	Segments.Reset();

	for (const FSRConveyorVisualPath& VisualPath : VisualPaths)
	{
		for (int32 PathIndex = 0; PathIndex < VisualPath.CellIds.Num(); ++PathIndex)
		{
			const FSRPlanetSurfaceGridCellId& CellId = VisualPath.CellIds[PathIndex];
			ESRConveyorGridDirection InputDirection = ESRConveyorGridDirection::None;
			ESRConveyorGridDirection OutputDirection = ESRConveyorGridDirection::None;
			if (PathIndex > 0)
			{
				FindDirectionBetweenCells(SurfaceGrid, CellId, VisualPath.CellIds[PathIndex - 1], InputDirection);
			}
			if (PathIndex + 1 < VisualPath.CellIds.Num())
			{
				FindDirectionBetweenCells(SurfaceGrid, CellId, VisualPath.CellIds[PathIndex + 1], OutputDirection);
			}

			FSRConveyorSegment Segment;
			Segment.Lane = MakeLaneKey(CellId, VisualPath.Layer);
			Segment.InputDirection = InputDirection;
			Segment.OutputDirection = OutputDirection;
			Segment.Shape = ResolveSegmentShape(InputDirection, OutputDirection);
			Segment.NetworkId = VisualPath.NetworkId;
			Segment.StructureDataAsset = VisualPath.StructureDataAsset;
			Segments.Add(Segment.Lane, Segment);
		}
	}
}

bool USRConveyorNetworkComponent::RebuildPlacedConveyorActors(USRPlanetSurfaceGrid* SurfaceGrid)
{
	DestroyPlacedConveyorActors();

	for (const FSRConveyorVisualPath& VisualPath : VisualPaths)
	{
		if (!IsValid(VisualPath.StructureDataAsset) || VisualPath.CellIds.IsEmpty())
		{
			continue;
		}

		MarkConveyorActorGroupDirty(VisualPath.StructureDataAsset.Get(), VisualPath.Layer);
	}

	return RefreshDirtyConveyorActorGroups(SurfaceGrid);
}

void USRConveyorNetworkComponent::RefreshConveyorVisuals(USRPlanetSurfaceGrid* SurfaceGrid)
{
	if (!bBuildDynamicMeshVisuals)
	{
		if (IsValid(BeltMeshComponent))
		{
			UE::Geometry::FDynamicMesh3 EmptyMesh;
			EmptyMesh.EnableAttributes();
			EmptyMesh.Attributes()->EnablePrimaryColors();
			EmptyMesh.Attributes()->SetNumUVLayers(1);
			BeltMeshComponent->SetMesh(MoveTemp(EmptyMesh));
			BeltMeshComponent->SetVisibility(false);
			BeltMeshComponent->SetHiddenInGame(true);
		}
		return;
	}

	EnsureBeltMeshComponent();
	if (!IsValid(BeltMeshComponent))
	{
		return;
	}

	UE::Geometry::FDynamicMesh3 BeltMesh;
	BeltMesh.EnableAttributes();
	BeltMesh.Attributes()->EnablePrimaryColors();
	BeltMesh.Attributes()->SetNumUVLayers(1);

	UMaterialInterface* BeltMaterial = nullptr;
	for (const FSRConveyorVisualPath& VisualPath : VisualPaths)
	{
		if (!IsValid(VisualPath.StructureDataAsset))
		{
			continue;
		}

		const FSRStructureData StructureData = VisualPath.StructureDataAsset->BuildData();
		if (!IsValid(BeltMaterial) && IsValid(StructureData.Material.Get()))
		{
			BeltMaterial = StructureData.Material.Get();
		}
		BuildConveyorPathRibbon(BeltMesh, SurfaceGrid, VisualPath);
	}

	if (IsValid(BeltMaterial))
	{
		BeltMeshComponent->SetMaterial(0, BeltMaterial);
	}
	BeltMeshComponent->SetMesh(MoveTemp(BeltMesh));
	BeltMeshComponent->SetVisibility(true);
	BeltMeshComponent->SetHiddenInGame(false);
}

void USRConveyorNetworkComponent::RefreshPCGSplineInputs(USRPlanetSurfaceGrid* SurfaceGrid)
{
	if (!bBuildPCGSplineInputs || !IsValid(SurfaceGrid))
	{
		ClearUnusedPCGSplineComponents(0);
		return;
	}

	int32 UsedSplineCount = 0;
	TArray<FVector> WorldPoints;
	TArray<FVector> WorldNormals;
	for (const FSRConveyorVisualPath& VisualPath : VisualPaths)
	{
		if (!BuildConveyorPathSplinePoints(SurfaceGrid, VisualPath, WorldPoints, WorldNormals))
		{
			continue;
		}

		for (int32 SegmentIndex = 0; SegmentIndex + 1 < WorldPoints.Num(); ++SegmentIndex)
		{
			const FVector SegmentStart = WorldPoints[SegmentIndex];
			const FVector SegmentEnd = WorldPoints[SegmentIndex + 1];
			const FVector SegmentVector = SegmentEnd - SegmentStart;
			if (SegmentVector.SizeSquared() <= FMath::Square(KINDA_SMALL_NUMBER))
			{
				continue;
			}

			USplineComponent* SplineComponent = EnsurePCGSplineComponent(UsedSplineCount);
			if (!IsValid(SplineComponent))
			{
				continue;
			}

			const FTransform SplineTransform = SplineComponent->GetComponentTransform();
			const FVector LocalSegmentStart = SplineTransform.InverseTransformPosition(SegmentStart);
			const FVector LocalSegmentEnd = SplineTransform.InverseTransformPosition(SegmentEnd);
			const FVector LocalSegmentVector = SplineTransform.InverseTransformVectorNoScale(SegmentVector);
			const FVector LocalStartNormal = SplineTransform.InverseTransformVectorNoScale(WorldNormals[SegmentIndex].GetSafeNormal()).GetSafeNormal();
			const FVector LocalEndNormal = SplineTransform.InverseTransformVectorNoScale(WorldNormals[SegmentIndex + 1].GetSafeNormal()).GetSafeNormal();

			SplineComponent->ClearSplinePoints(false);
			SplineComponent->AddSplinePoint(LocalSegmentStart, ESplineCoordinateSpace::Local, false);
			SplineComponent->AddSplinePoint(LocalSegmentEnd, ESplineCoordinateSpace::Local, false);
			SplineComponent->SetSplinePointType(0, ESplinePointType::Linear, false);
			SplineComponent->SetSplinePointType(1, ESplinePointType::Linear, false);
			SplineComponent->SetTangentAtSplinePoint(0, LocalSegmentVector, ESplineCoordinateSpace::Local, false);
			SplineComponent->SetTangentAtSplinePoint(1, LocalSegmentVector, ESplineCoordinateSpace::Local, false);
			SplineComponent->SetUpVectorAtSplinePoint(0, LocalStartNormal, ESplineCoordinateSpace::Local, false);
			SplineComponent->SetUpVectorAtSplinePoint(1, LocalEndNormal, ESplineCoordinateSpace::Local, false);
			SplineComponent->SetClosedLoop(false, false);
			SplineComponent->UpdateSpline();
			SplineComponent->SetVisibility(false);
			SplineComponent->SetHiddenInGame(true);
			++UsedSplineCount;
		}
	}

	ClearUnusedPCGSplineComponents(UsedSplineCount);
}

void USRConveyorNetworkComponent::RefreshPathDebugLines(USRPlanetSurfaceGrid* SurfaceGrid)
{
	if (IsValid(PathDebugLineBatchComponent))
	{
		PathDebugLineBatchComponent->Flush();
	}

	if (!bShowPathDebugLine || !IsValid(SurfaceGrid) || VisualPaths.IsEmpty())
	{
		return;
	}

	EnsurePathDebugLineBatchComponent();
	if (!IsValid(PathDebugLineBatchComponent))
	{
		return;
	}

	const FVector SurfaceCenter = SurfaceGrid->GetComponentTransform().GetLocation();
	const FColor LineColor = PathDebugLineColor.ToFColor(true);
	for (const FSRConveyorVisualPath& VisualPath : VisualPaths)
	{
		if (VisualPath.CellIds.IsEmpty())
		{
			continue;
		}

		FSRPlanetSurfaceGridCellInfo StartCellInfo;
		FSRPlanetSurfaceGridCellInfo EndCellInfo;
		if (!SurfaceGrid->GetCellInfoById(VisualPath.CellIds[0], StartCellInfo)
			|| !SurfaceGrid->GetCellInfoById(VisualPath.CellIds.Last(), EndCellInfo))
		{
			continue;
		}

		FVector StartNormal = StartCellInfo.WorldNormal.GetSafeNormal();
		FVector EndNormal = EndCellInfo.WorldNormal.GetSafeNormal();
		if (StartNormal.IsNearlyZero())
		{
			StartNormal = (StartCellInfo.WorldCenter - SurfaceCenter).GetSafeNormal();
		}
		else if (FVector::DotProduct(StartNormal, StartCellInfo.WorldCenter - SurfaceCenter) < 0.0f)
		{
			StartNormal *= -1.0f;
		}
		if (EndNormal.IsNearlyZero())
		{
			EndNormal = (EndCellInfo.WorldCenter - SurfaceCenter).GetSafeNormal();
		}
		else if (FVector::DotProduct(EndNormal, EndCellInfo.WorldCenter - SurfaceCenter) < 0.0f)
		{
			EndNormal *= -1.0f;
		}

		const float HeightOffset = static_cast<float>(FMath::Max(0, VisualPath.Layer)) * FMath::Max(0.0f, VisualPath.LayerHeight) + BeltSurfaceOffset + 40.0f;
		const FVector StartPoint = StartCellInfo.WorldCenter + StartNormal * HeightOffset;
		const FVector EndPoint = EndCellInfo.WorldCenter + EndNormal * HeightOffset;
		PathDebugLineBatchComponent->DrawLine(
			StartPoint,
			EndPoint,
			LineColor,
			ConveyorPathDebugLineDepthPriority,
			FMath::Max(0.0f, PathDebugLineThickness),
			0.0f);
	}
}

void USRConveyorNetworkComponent::RequestPCGGeneration()
{
	if (!bAutoGeneratePCG)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return;
	}

	TArray<UPCGComponent*> PCGComponents;
	OwnerActor->GetComponents<UPCGComponent>(PCGComponents);
	for (UPCGComponent* PCGComponent : PCGComponents)
	{
		if (IsValid(PCGComponent))
		{
			PCGComponent->OnPCGGraphGeneratedDelegate.RemoveAll(this);
			PCGComponent->OnPCGGraphGeneratedDelegate.AddUObject(this, &USRConveyorNetworkComponent::HandlePCGGraphGenerated);
			PCGComponent->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
			PCGComponent->Generate(true);
		}
	}
}

void USRConveyorNetworkComponent::BindPCGGenerationDelegates()
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return;
	}

	TArray<UPCGComponent*> PCGComponents;
	OwnerActor->GetComponents<UPCGComponent>(PCGComponents);
	for (UPCGComponent* PCGComponent : PCGComponents)
	{
		if (IsValid(PCGComponent))
		{
			PCGComponent->OnPCGGraphGeneratedDelegate.RemoveAll(this);
			PCGComponent->OnPCGGraphGeneratedDelegate.AddUObject(this, &USRConveyorNetworkComponent::HandlePCGGraphGenerated);
		}
	}
}

void USRConveyorNetworkComponent::HandlePCGGraphGenerated(UPCGComponent* PCGComponent)
{
	RebaseGeneratedPCGSplineMeshes(PCGComponent);
}

void USRConveyorNetworkComponent::RebaseGeneratedPCGSplineMeshes(UPCGComponent* PCGComponent)
{
	if (!IsValid(PCGComponent))
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || PCGComponent->GetOwner() != OwnerActor)
	{
		return;
	}

	USRPlanetSurfaceGrid* SurfaceGrid = OwnerActor->FindComponentByClass<USRPlanetSurfaceGrid>();
	if (!IsValid(SurfaceGrid))
	{
		return;
	}

	struct FConveyorSplineMeshSegment
	{
		FVector LocalStart = FVector::ZeroVector;
		FVector LocalEnd = FVector::ZeroVector;
		FVector LocalTangent = FVector::ZeroVector;
		FVector LocalUpDirection = FVector::UpVector;
	};

	const FTransform ComponentTransform = GetComponentTransform();
	TArray<FConveyorSplineMeshSegment> ExpectedSegments;
	TArray<FVector> WorldPoints;
	TArray<FVector> WorldNormals;
	for (const FSRConveyorVisualPath& VisualPath : VisualPaths)
	{
		if (!BuildConveyorPathSplinePoints(SurfaceGrid, VisualPath, WorldPoints, WorldNormals))
		{
			continue;
		}

		for (int32 SegmentIndex = 0; SegmentIndex + 1 < WorldPoints.Num(); ++SegmentIndex)
		{
			const FVector SegmentStart = WorldPoints[SegmentIndex];
			const FVector SegmentEnd = WorldPoints[SegmentIndex + 1];
			const FVector SegmentVector = SegmentEnd - SegmentStart;
			if (SegmentVector.SizeSquared() <= FMath::Square(KINDA_SMALL_NUMBER))
			{
				continue;
			}

			FVector WorldUpDirection = (WorldNormals[SegmentIndex] + WorldNormals[SegmentIndex + 1]).GetSafeNormal();
			if (WorldUpDirection.IsNearlyZero())
			{
				WorldUpDirection = WorldNormals[SegmentIndex].GetSafeNormal();
			}

			FConveyorSplineMeshSegment Segment;
			Segment.LocalStart = ComponentTransform.InverseTransformPosition(SegmentStart);
			Segment.LocalEnd = ComponentTransform.InverseTransformPosition(SegmentEnd);
			Segment.LocalTangent = ComponentTransform.InverseTransformVectorNoScale(SegmentVector);
			Segment.LocalUpDirection = ComponentTransform.InverseTransformVectorNoScale(WorldUpDirection).GetSafeNormal();
			if (Segment.LocalUpDirection.IsNearlyZero())
			{
				Segment.LocalUpDirection = FVector::UpVector;
			}
			ExpectedSegments.Add(Segment);
		}
	}

	if (ExpectedSegments.IsEmpty())
	{
		return;
	}

	TArray<USplineMeshComponent*> GeneratedSplineMeshes;
	OwnerActor->GetComponents<USplineMeshComponent>(GeneratedSplineMeshes);
	GeneratedSplineMeshes.RemoveAll([PCGComponent](const USplineMeshComponent* SplineMeshComponent)
	{
		return !IsValid(SplineMeshComponent)
			|| !SplineMeshComponent->ComponentTags.Contains(PCGComponent->GetFName());
	});
	GeneratedSplineMeshes.Sort([](const USplineMeshComponent& Left, const USplineMeshComponent& Right)
	{
		return Left.GetFName().LexicalLess(Right.GetFName());
	});

	const int32 SegmentCount = FMath::Min(ExpectedSegments.Num(), GeneratedSplineMeshes.Num());
	for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
	{
		USplineMeshComponent* SplineMeshComponent = GeneratedSplineMeshes[SegmentIndex];
		if (!IsValid(SplineMeshComponent))
		{
			continue;
		}

		const FConveyorSplineMeshSegment& Segment = ExpectedSegments[SegmentIndex];
		SplineMeshComponent->SetMobility(EComponentMobility::Movable);
		SplineMeshComponent->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
		SplineMeshComponent->SetRelativeTransform(FTransform::Identity);
		SplineMeshComponent->SetSplineUpDir(Segment.LocalUpDirection, false);
		SplineMeshComponent->SetStartAndEnd(Segment.LocalStart, Segment.LocalTangent, Segment.LocalEnd, Segment.LocalTangent, false);
		SplineMeshComponent->SetStartRollDegrees(0.0f, false);
		SplineMeshComponent->SetEndRollDegrees(0.0f, false);
		SplineMeshComponent->SetVisibility(true);
		SplineMeshComponent->SetHiddenInGame(false);
		SplineMeshComponent->UpdateMesh();
	}
}
