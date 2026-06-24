#include "Assembly/SRAssemblyComponent.h"

#include "Algo/Reverse.h"
#include "Camera/SRPlayerController.h"
#include "Conveyor/SRConveyorBeltActor.h"
#include "Conveyor/SRConveyorNetworkComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	enum class ESRConveyorPlacementEndpointRole : uint8
	{
		None,
		Source,
		Sink,
		Ambiguous,
	};

	FName MakeConveyorNetworkId(AActor* FocusedActor, int32 Layer)
	{
		return FName(*FString::Printf(TEXT("Conveyor_%s_%d"), *GetNameSafe(FocusedActor), FMath::Max(0, Layer)));
	}

	void AppendConveyorPathSegment(
		const TArray<FSRPlanetSurfaceGridCellId>& SegmentCellIds,
		TArray<FSRPlanetSurfaceGridCellId>& OutPathCellIds)
	{
		for (const FSRPlanetSurfaceGridCellId& CellId : SegmentCellIds)
		{
			if (!OutPathCellIds.IsEmpty() && OutPathCellIds.Last() == CellId)
			{
				continue;
			}

			OutPathCellIds.Add(CellId);
		}
	}

	ESRConveyorPlacementEndpointRole CombineEndpointRole(
		ESRConveyorPlacementEndpointRole CurrentRole,
		ESRConveyorPlacementEndpointRole IncomingRole)
	{
		if (IncomingRole == ESRConveyorPlacementEndpointRole::None)
		{
			return CurrentRole;
		}
		if (CurrentRole == ESRConveyorPlacementEndpointRole::None)
		{
			return IncomingRole;
		}
		if (CurrentRole == IncomingRole)
		{
			return CurrentRole;
		}

		return ESRConveyorPlacementEndpointRole::Ambiguous;
	}

	bool GetNeighborCellIdByStructurePortDirection(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& CellId,
		ESRStructurePortDirection Direction,
		FSRPlanetSurfaceGridCellId& OutNeighborCellId)
	{
		OutNeighborCellId = FSRPlanetSurfaceGridCellId();
		if (!IsValid(SurfaceGrid))
		{
			return false;
		}

		FSRPlanetSurfaceGridCellNeighbors Neighbors;
		if (!SurfaceGrid->GetCellNeighbors(CellId, Neighbors))
		{
			return false;
		}

		switch (Direction)
		{
		case ESRStructurePortDirection::Left:
			OutNeighborCellId = Neighbors.NegativeU;
			break;
		case ESRStructurePortDirection::Right:
			OutNeighborCellId = Neighbors.PositiveU;
			break;
		case ESRStructurePortDirection::Top:
			OutNeighborCellId = Neighbors.NegativeV;
			break;
		case ESRStructurePortDirection::Bottom:
			OutNeighborCellId = Neighbors.PositiveV;
			break;
		default:
			return false;
		}

		return true;
	}

	bool DoesPortConnectToConveyorCell(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlacedStructureInstance& PlacedStructure,
		const FSRStructureData& StructureData,
		const FSRStructurePortSpec& PortSpec,
		const FSRPlanetSurfaceGridCellId& ConveyorCellId)
	{
		if (!IsValid(SurfaceGrid) || PlacedStructure.FootprintCellIds.IsEmpty())
		{
			return false;
		}

		const int32 FootprintCellsX = StarRovers::Structure::GetRotatedFootprintCellsX(StructureData, PlacedStructure.PlacementRotationSteps);
		const int32 FootprintCellsY = StarRovers::Structure::GetRotatedFootprintCellsY(StructureData, PlacedStructure.PlacementRotationSteps);
		if (PortSpec.CellOffsetX < 0
			|| PortSpec.CellOffsetY < 0
			|| PortSpec.CellOffsetX >= FootprintCellsX
			|| PortSpec.CellOffsetY >= FootprintCellsY)
		{
			return false;
		}

		const int32 FootprintIndex = PortSpec.CellOffsetY * FootprintCellsX + PortSpec.CellOffsetX;
		if (!PlacedStructure.FootprintCellIds.IsValidIndex(FootprintIndex))
		{
			return false;
		}

		FSRPlanetSurfaceGridCellId ConnectionCellId;
		if (!GetNeighborCellIdByStructurePortDirection(
			SurfaceGrid,
			PlacedStructure.FootprintCellIds[FootprintIndex],
			PortSpec.Direction,
			ConnectionCellId))
		{
			return false;
		}

		return ConnectionCellId == ConveyorCellId && !PlacedStructure.FootprintCellIds.Contains(ConnectionCellId);
	}

	ESRConveyorPlacementEndpointRole ResolveFacilityEndpointRole(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& CellId)
	{
		AActor* SurfaceOwner = IsValid(SurfaceGrid) ? SurfaceGrid->GetOwner() : nullptr;
		USRStructureInstanceManagerComponent* StructureInstanceManager = IsValid(SurfaceOwner)
			? SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>()
			: nullptr;
		if (!IsValid(StructureInstanceManager))
		{
			return ESRConveyorPlacementEndpointRole::None;
		}

		TArray<FSRPlacedStructureInstance> PlacedStructures;
		StructureInstanceManager->GetPlacedStructures(PlacedStructures);

		ESRConveyorPlacementEndpointRole ResolvedRole = ESRConveyorPlacementEndpointRole::None;
		for (const FSRPlacedStructureInstance& PlacedStructure : PlacedStructures)
		{
			USRStructureDataAsset* StructureDataAsset = PlacedStructure.StructureDataAsset.Get();
			if (PlacedStructure.bNaturalStructure
				|| !IsValid(StructureDataAsset)
				|| PlacedStructure.FootprintCellIds.IsEmpty())
			{
				continue;
			}

			const FSRStructureData StructureData = StructureDataAsset->BuildData();
			if (StructureData.BuildKind != ESRStructureBuildKind::Structure)
			{
				continue;
			}

			for (const FSRStructurePortSpec& OutputPort : StructureData.OutputPorts)
			{
				const FSRStructurePortSpec RotatedOutputPort = StarRovers::Structure::RotateStructurePortSpec(
					OutputPort,
					StructureData,
					PlacedStructure.PlacementRotationSteps);
				if (DoesPortConnectToConveyorCell(SurfaceGrid, PlacedStructure, StructureData, RotatedOutputPort, CellId))
				{
					ResolvedRole = CombineEndpointRole(ResolvedRole, ESRConveyorPlacementEndpointRole::Source);
				}
			}

			for (const FSRStructurePortSpec& InputPort : StructureData.InputPorts)
			{
				const FSRStructurePortSpec RotatedInputPort = StarRovers::Structure::RotateStructurePortSpec(
					InputPort,
					StructureData,
					PlacedStructure.PlacementRotationSteps);
				if (DoesPortConnectToConveyorCell(SurfaceGrid, PlacedStructure, StructureData, RotatedInputPort, CellId))
				{
					ResolvedRole = CombineEndpointRole(ResolvedRole, ESRConveyorPlacementEndpointRole::Sink);
				}
			}
		}

		return ResolvedRole;
	}

	ESRConveyorPlacementEndpointRole ResolveExistingConveyorEndpointRole(
		const USRConveyorNetworkComponent* ConveyorNetwork,
		const FSRPlanetSurfaceGridCellId& CellId,
		int32 Layer)
	{
		if (!IsValid(ConveyorNetwork))
		{
			return ESRConveyorPlacementEndpointRole::None;
		}

		FSRConveyorLaneKey LaneKey;
		LaneKey.CellId = CellId;
		LaneKey.Layer = FMath::Max(0, Layer);

		FSRConveyorSegment Segment;
		if (!ConveyorNetwork->GetConveyorSegment(LaneKey, Segment))
		{
			return ESRConveyorPlacementEndpointRole::None;
		}

		const bool bHasInput = Segment.InputDirection != ESRConveyorGridDirection::None;
		const bool bHasOutput = Segment.OutputDirection != ESRConveyorGridDirection::None
			|| Segment.BranchOutputDirection != ESRConveyorGridDirection::None;

		if (!bHasInput && bHasOutput)
		{
			return ESRConveyorPlacementEndpointRole::Sink;
		}
		if (bHasInput)
		{
			return ESRConveyorPlacementEndpointRole::Source;
		}

		return ESRConveyorPlacementEndpointRole::None;
	}

	ESRConveyorPlacementEndpointRole ResolveConveyorPlacementEndpointRole(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const USRConveyorNetworkComponent* ConveyorNetwork,
		const FSRPlanetSurfaceGridCellId& CellId,
		int32 Layer)
	{
		const ESRConveyorPlacementEndpointRole ConveyorRole = ResolveExistingConveyorEndpointRole(ConveyorNetwork, CellId, Layer);
		return ConveyorRole != ESRConveyorPlacementEndpointRole::None
			? ConveyorRole
			: ResolveFacilityEndpointRole(SurfaceGrid, CellId);
	}

	int32 ScoreEndpointRoleAsPathStart(ESRConveyorPlacementEndpointRole Role)
	{
		switch (Role)
		{
		case ESRConveyorPlacementEndpointRole::Source:
			return 2;
		case ESRConveyorPlacementEndpointRole::Sink:
			return -2;
		default:
			return 0;
		}
	}

	int32 ScoreEndpointRoleAsPathEnd(ESRConveyorPlacementEndpointRole Role)
	{
		switch (Role)
		{
		case ESRConveyorPlacementEndpointRole::Sink:
			return 2;
		case ESRConveyorPlacementEndpointRole::Source:
			return -2;
		default:
			return 0;
		}
	}

	void OrientConveyorPathToConnectedEndpoints(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const USRConveyorNetworkComponent* ConveyorNetwork,
		int32 Layer,
		TArray<FSRPlanetSurfaceGridCellId>& PathCellIds)
	{
		if (PathCellIds.Num() < 2)
		{
			return;
		}

		const ESRConveyorPlacementEndpointRole FirstEndpointRole = ResolveConveyorPlacementEndpointRole(
			SurfaceGrid,
			ConveyorNetwork,
			PathCellIds[0],
			Layer);
		const ESRConveyorPlacementEndpointRole LastEndpointRole = ResolveConveyorPlacementEndpointRole(
			SurfaceGrid,
			ConveyorNetwork,
			PathCellIds.Last(),
			Layer);

		const int32 KeepScore = ScoreEndpointRoleAsPathStart(FirstEndpointRole)
			+ ScoreEndpointRoleAsPathEnd(LastEndpointRole);
		const int32 ReverseScore = ScoreEndpointRoleAsPathStart(LastEndpointRole)
			+ ScoreEndpointRoleAsPathEnd(FirstEndpointRole);
		if (ReverseScore > KeepScore)
		{
			Algo::Reverse(PathCellIds);
		}
	}
}

bool USRAssemblyComponent::BuildConveyorPlacementDragPath(
	USRPlanetSurfaceGrid* SurfaceGrid,
	USRConveyorNetworkComponent* ConveyorNetwork,
	const FSRStructureData& ConveyorData,
	const FSRPlanetSurfaceGridCellId& TargetCellId,
	TArray<FSRPlanetSurfaceGridCellId>& OutPathCellIds) const
{
	OutPathCellIds.Reset();
	if (!bIsConveyorPlacementDragActive
		|| !bHasConveyorDragStartCell
		|| !IsValid(ConveyorDragStartSurfaceGrid)
		|| ConveyorDragStartSurfaceGrid != SurfaceGrid
		|| !IsValid(SurfaceGrid)
		|| !IsValid(ConveyorNetwork)
		|| ConveyorData.BuildKind != ESRStructureBuildKind::Conveyor)
	{
		return false;
	}

	TArray<FSRPlanetSurfaceGridCellId> ControlCellIds;
	ControlCellIds.Reserve(ConveyorDragWaypointCellIds.Num() + 2);
	ControlCellIds.Add(ConveyorDragStartCellId);
	for (const FSRPlanetSurfaceGridCellId& WaypointCellId : ConveyorDragWaypointCellIds)
	{
		if (ControlCellIds.Last() == WaypointCellId)
		{
			continue;
		}

		ControlCellIds.Add(WaypointCellId);
	}
	if (!(ControlCellIds.Last() == TargetCellId))
	{
		ControlCellIds.Add(TargetCellId);
	}

	for (int32 ControlIndex = 1; ControlIndex < ControlCellIds.Num(); ++ControlIndex)
	{
		TArray<FSRPlanetSurfaceGridCellId> SegmentCellIds;
		if (!ConveyorNetwork->FindConveyorPath(
			SurfaceGrid,
			ControlCellIds[ControlIndex - 1],
			ControlCellIds[ControlIndex],
			ConveyorData.ConveyorLayer,
			SegmentCellIds))
		{
			OutPathCellIds.Reset();
			return false;
		}

		AppendConveyorPathSegment(SegmentCellIds, OutPathCellIds);
	}

	if (OutPathCellIds.IsEmpty() && !ControlCellIds.IsEmpty())
	{
		OutPathCellIds.Add(ControlCellIds[0]);
	}
	OrientConveyorPathToConnectedEndpoints(SurfaceGrid, ConveyorNetwork, ConveyorData.ConveyorLayer, OutPathCellIds);
	return !OutPathCellIds.IsEmpty();
}

bool USRAssemblyComponent::UpdateConveyorGhostPreview(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCell& TargetCell,
	USRStructureDataAsset* ConveyorDataAsset)
{
	if (!bIsConveyorPlacementDragActive
		|| !bHasConveyorDragStartCell
		|| !IsValid(ConveyorDragStartSurfaceGrid)
		|| ConveyorDragStartSurfaceGrid != SurfaceGrid
		|| !IsValid(SurfaceGrid)
		|| !IsValid(ConveyorDataAsset))
	{
		DestroyConveyorGhostPreview();
		return false;
	}

	AActor* FocusedActor = nullptr;
	USRConveyorNetworkComponent* ConveyorNetwork = nullptr;
	if (!TryGetFocusedConveyorNetwork(FocusedActor, ConveyorNetwork))
	{
		DestroyConveyorGhostPreview();
		return false;
	}

	const FSRStructureData ConveyorData = ConveyorDataAsset->BuildData();
	if (ConveyorData.BuildKind != ESRStructureBuildKind::Conveyor)
	{
		DestroyConveyorGhostPreview();
		return false;
	}

	SurfaceGrid->SetHoveredCell(TargetCell.CellId);
	PublishHoveredCellInfo(SurfaceGrid, TargetCell);

	if (IsValid(ConveyorGhostActor)
		&& ConveyorGhostDataAsset == ConveyorDataAsset
		&& ConveyorGhostSurfaceGrid == SurfaceGrid
		&& bHasConveyorGhostTargetCell
		&& ConveyorGhostTargetCellId == TargetCell.CellId)
	{
		return true;
	}

	TArray<FSRPlanetSurfaceGridCellId> PathCellIds;
	if (!BuildConveyorPlacementDragPath(
		SurfaceGrid,
		ConveyorNetwork,
		ConveyorData,
		TargetCell.CellId,
		PathCellIds))
	{
		DestroyConveyorGhostPreview();
		return true;
	}

	UClass* ConveyorActorClass = ConveyorData.StructureActorClass.Get();
	if (!IsValid(ConveyorActorClass) || !ConveyorActorClass->IsChildOf(ASRConveyorBeltActor::StaticClass()))
	{
		DestroyConveyorGhostPreview();
		return false;
	}

	const bool bNeedsNewGhostActor = !IsValid(ConveyorGhostActor)
		|| ConveyorGhostDataAsset != ConveyorDataAsset
		|| ConveyorGhostSurfaceGrid != SurfaceGrid
		|| ConveyorGhostActor->GetClass() != ConveyorActorClass;
	if (bNeedsNewGhostActor)
	{
		DestroyConveyorGhostPreview();

		AActor* SurfaceOwner = SurfaceGrid->GetOwner();
		UWorld* World = IsValid(SurfaceOwner) ? SurfaceOwner->GetWorld() : nullptr;
		if (!World)
		{
			return false;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = SurfaceOwner;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParameters.ObjectFlags |= RF_Transient;
		ConveyorGhostActor = World->SpawnActor<ASRConveyorBeltActor>(
			ConveyorActorClass,
			SurfaceOwner->GetActorTransform(),
			SpawnParameters);
		if (!IsValid(ConveyorGhostActor))
		{
			return false;
		}

		ConveyorGhostActor->SetOwner(SurfaceOwner);
		ConveyorGhostActor->AttachToActor(SurfaceOwner, FAttachmentTransformRules::KeepWorldTransform);
		ConveyorGhostActor->SetActorHiddenInGame(false);
		ConveyorGhostActor->SetConveyorGhostMode(true, ConveyorData.GhostMaterial);
		ConveyorGhostDataAsset = ConveyorDataAsset;
		ConveyorGhostSurfaceGrid = SurfaceGrid;
	}

	FSRConveyorVisualPath VisualPath;
	VisualPath.CellIds = MoveTemp(PathCellIds);
	VisualPath.Layer = FMath::Max(0, ConveyorData.ConveyorLayer);
	VisualPath.LayerHeight = ConveyorData.ConveyorLayerHeight;
	VisualPath.NetworkId = MakeConveyorNetworkId(FocusedActor, ConveyorData.ConveyorLayer);
	VisualPath.StructureDataAsset = ConveyorDataAsset;

	TArray<FSRConveyorVisualPath> VisualPaths;
	VisualPaths.Add(VisualPath);
	if (!ConveyorGhostActor->InitializeConveyorPaths(
		SurfaceGrid,
		VisualPaths,
		ConveyorNetwork->GetConveyorActorSplineComponentTag(),
		ConveyorNetwork->GetConveyorActorSurfaceOffset()))
	{
		DestroyConveyorGhostPreview();
		return true;
	}

	ConveyorGhostActor->SetConveyorGhostMode(true, ConveyorData.GhostMaterial);
	ConveyorGhostActor->SetActorHiddenInGame(false);
	ConveyorGhostTargetCellId = TargetCell.CellId;
	bHasConveyorGhostTargetCell = true;
	return true;
}

bool USRAssemblyComponent::TryAddConveyorPlacementDragWaypoint()
{
	if (!bIsConveyorPlacementDragActive
		|| !bHasConveyorDragStartCell
		|| !IsValid(ConveyorDragStartSurfaceGrid))
	{
		return false;
	}

	ASRPlayerController* PlayerController = GetOwnerController();
	USRStructureDataAsset* ConveyorDataAsset = PlayerController ? PlayerController->GetSelectedStructureDataAsset() : nullptr;
	if (!IsValid(ConveyorDataAsset))
	{
		return false;
	}

	const FSRStructureData ConveyorData = ConveyorDataAsset->BuildData();
	if (ConveyorData.BuildKind != ESRStructureBuildKind::Conveyor)
	{
		return false;
	}

	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* SurfaceGrid = nullptr;
	FSRPlanetSurfaceGridCell TargetCell;
	if (!TryResolveStructurePlacementDragTarget(FocusedActor, SurfaceGrid, TargetCell)
		|| SurfaceGrid != ConveyorDragStartSurfaceGrid)
	{
		return false;
	}

	AActor* ConveyorActor = nullptr;
	USRConveyorNetworkComponent* ConveyorNetwork = nullptr;
	if (!TryGetFocusedConveyorNetwork(ConveyorActor, ConveyorNetwork))
	{
		return false;
	}

	TArray<FSRPlanetSurfaceGridCellId> PathCellIds;
	if (!BuildConveyorPlacementDragPath(SurfaceGrid, ConveyorNetwork, ConveyorData, TargetCell.CellId, PathCellIds))
	{
		return false;
	}

	const FSRPlanetSurfaceGridCellId& PreviousAnchorCellId = ConveyorDragWaypointCellIds.IsEmpty()
		? ConveyorDragStartCellId
		: ConveyorDragWaypointCellIds.Last();
	if (!(PreviousAnchorCellId == TargetCell.CellId))
	{
		ConveyorDragWaypointCellIds.Add(TargetCell.CellId);
	}

	bHasConveyorGhostTargetCell = false;
	SurfaceGrid->SetSelectedCell(TargetCell.CellId);
	return UpdateConveyorGhostPreview(SurfaceGrid, TargetCell, ConveyorDataAsset);
}

void USRAssemblyComponent::DestroyConveyorGhostPreview()
{
	if (IsValid(ConveyorGhostActor))
	{
		ConveyorGhostActor->Destroy();
	}

	ConveyorGhostActor = nullptr;
	ConveyorGhostDataAsset = nullptr;
	ConveyorGhostSurfaceGrid = nullptr;
	ConveyorGhostTargetCellId = FSRPlanetSurfaceGridCellId();
	bHasConveyorGhostTargetCell = false;
}

bool USRAssemblyComponent::CommitConveyorPlacementDrag()
{
	if (!bIsConveyorPlacementDragActive || !bHasConveyorDragStartCell || !IsValid(ConveyorDragStartSurfaceGrid))
	{
		return false;
	}

	ASRPlayerController* PlayerController = GetOwnerController();
	USRStructureDataAsset* ConveyorDataAsset = PlayerController ? PlayerController->GetSelectedStructureDataAsset() : nullptr;
	if (!IsValid(ConveyorDataAsset) || ConveyorDataAsset->BuildData().BuildKind != ESRStructureBuildKind::Conveyor)
	{
		return false;
	}

	USRPlanetSurfaceGrid* StartSurfaceGrid = ConveyorDragStartSurfaceGrid;
	FSRPlanetSurfaceGridCell TargetCell;
	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* CurrentSurfaceGrid = nullptr;
	FSRPlanetSurfaceGridCell CurrentTargetCell;
	if (TryResolveStructurePlacementDragTarget(FocusedActor, CurrentSurfaceGrid, CurrentTargetCell)
		&& CurrentSurfaceGrid == StartSurfaceGrid)
	{
		TargetCell = CurrentTargetCell;
	}
	else if (!bHasConveyorGhostTargetCell || !StartSurfaceGrid->GetCellById(ConveyorGhostTargetCellId, TargetCell))
	{
		return false;
	}

	AActor* SurfaceOwner = StartSurfaceGrid->GetOwner();
	USRConveyorNetworkComponent* ConveyorNetwork = IsValid(SurfaceOwner)
		? SurfaceOwner->FindComponentByClass<USRConveyorNetworkComponent>()
		: nullptr;
	if (!IsValid(ConveyorNetwork))
	{
		return false;
	}

	const FSRStructureData ConveyorData = ConveyorDataAsset->BuildData();
	TArray<FSRPlanetSurfaceGridCellId> PathCellIds;
	if (!BuildConveyorPlacementDragPath(StartSurfaceGrid, ConveyorNetwork, ConveyorData, TargetCell.CellId, PathCellIds))
	{
		return false;
	}

	const bool bPlaced = ConveyorNetwork->TryPlaceConveyorPath(
		StartSurfaceGrid,
		PathCellIds,
		ConveyorData.ConveyorLayer,
		ConveyorData.ConveyorLayerHeight,
		ConveyorDataAsset,
		MakeConveyorNetworkId(SurfaceOwner, ConveyorData.ConveyorLayer));
	StartSurfaceGrid->ClearSelectedCell();
	return bPlaced;
}
