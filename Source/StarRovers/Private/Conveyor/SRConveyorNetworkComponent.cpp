#include "Conveyor/SRConveyorNetworkComponent.h"

#include "Components/DynamicMeshComponent.h"
#include "Components/LineBatchComponent.h"
#include "Conveyor/SRConveyorComponentPool.h"
#include "Conveyor/SRConveyorMutationFinalizer.h"
#include "Conveyor/SRConveyorPCGGenerationCoordinator.h"
#include "Conveyor/SRConveyorSegmentQuery.h"
#include "Conveyor/SRConveyorTickCoordinator.h"
#include "GameFramework/Actor.h"
#include "Surface/SRPlanetSurfaceGrid.h"

USRConveyorNetworkComponent::USRConveyorNetworkComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	DefaultLayerHeight = 160.0f;
	BeltWidth = 260.0f;
	BeltThickness = 80.0f;
	BeltSurfaceOffset = 0.0f;
	bBuildBeltRibbonMesh = false;
	bSpawnConveyorBeltActors = true;
	MaxConveyorActorGroupsRefreshedPerFrame = 1;
	bBuildPCGSplineInputs = false;
	PCGSplineComponentTag = TEXT("ConveyorVisualSpline");
	PCGSplineHeightOffset = 0.0f;
	bAutoGeneratePCG = false;
	bShowPathDebugLine = false;
	PathDebugLineColor = FLinearColor(1.0f, 0.1f, 0.0f, 1.0f);
	PathDebugLineThickness = 8.0f;
	bShowConnectionDebugLine = false;
	ConnectionDebugLineColor = FLinearColor(0.1f, 1.0f, 0.25f, 1.0f);
	BrokenConnectionDebugLineColor = FLinearColor(1.0f, 0.15f, 0.05f, 1.0f);
	EndpointDebugLineColor = FLinearColor(1.0f, 0.9f, 0.1f, 1.0f);
	ConnectionDebugLineThickness = 6.0f;
	ConnectionDebugLineHeightOffset = 110.0f;
	bAutoTransportItems = true;
	ItemSpeedCellsPerSecond = 1.0f;
	MaxItemTransfersPerTick = 128;
	bShowTransportItemLabels = true;
	ItemLabelHeightOffset = 180.0f;
	ItemPatternLabelWorldSize = 120.0f;
	ItemPatternLabelMaxScale = 2.5f;
	ItemPatternSparseColor = FLinearColor(0.1f, 0.75f, 1.0f, 1.0f);
	ItemPatternDenseColor = FLinearColor(1.0f, 0.55f, 0.05f, 1.0f);
	ItemPatternSpecialColor = FLinearColor(0.85f, 0.1f, 1.0f, 1.0f);
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

	StarRovers::Conveyor::FSRConveyorPCGGenerationCoordinator::ConfigureGenerationTriggers(OwnerActor);
	BindPCGGenerationDelegates();

	if (bShowPathDebugLine || bShowConnectionDebugLine)
	{
		USRPlanetSurfaceGrid* SurfaceGrid = OwnerActor->FindComponentByClass<USRPlanetSurfaceGrid>();
		RefreshPathDebugLines(SurfaceGrid);
		SetComponentTickEnabled(true);
	}
}

void USRConveyorNetworkComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	USRPlanetSurfaceGrid* SurfaceGrid = StarRovers::Conveyor::FSRConveyorTickCoordinator::ResolveSurfaceGrid(GetOwner(), PendingConveyorActorRefreshSurfaceGrid);
	const float TransportDeltaTime = StarRovers::Conveyor::FSRConveyorTickCoordinator::ResolveTransportDeltaTime(GetWorld(), DeltaTime);

	if (bAutoTransportItems && IsValid(SurfaceGrid) && TransportDeltaTime > 0.0f)
	{
		ProcessConveyorTransport(SurfaceGrid, TransportDeltaTime);
	}

	if (bShowTransportItemLabels && IsValid(SurfaceGrid))
	{
		RefreshConveyorItemLabels(SurfaceGrid, DeltaTime);
	}
	else
	{
		DestroyConveyorItemLabels();
	}

	if ((bShowPathDebugLine || bShowConnectionDebugLine) && IsValid(SurfaceGrid))
	{
		RefreshPathDebugLines(SurfaceGrid);
	}

	RefreshDirtyConveyorActorGroups(SurfaceGrid, FMath::Max(1, MaxConveyorActorGroupsRefreshedPerFrame));
	if (!StarRovers::Conveyor::FSRConveyorTickCoordinator::ShouldKeepTickEnabled(
		HasDirtyConveyorActorGroups(),
		ShouldKeepTransportTickEnabled(),
		bShowPathDebugLine,
		bShowConnectionDebugLine))
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
		RefreshConveyorRibbonMesh(SurfaceGrid);
		RefreshPathDebugLines(SurfaceGrid);
		SetComponentTickEnabled(StarRovers::Conveyor::FSRConveyorTickCoordinator::ShouldKeepTickEnabled(
			HasDirtyConveyorActorGroups(),
			ShouldKeepTransportTickEnabled(),
			bShowPathDebugLine,
			bShowConnectionDebugLine));
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

bool USRConveyorNetworkComponent::HasConveyorSegmentAtCell(const FSRPlanetSurfaceGridCellId& CellId) const
{
	return StarRovers::Conveyor::FSRConveyorSegmentQuery::HasSegmentAtCell(Segments, CellId);
}

void USRConveyorNetworkComponent::ClearConveyors()
{
	if (AActor* OwnerActor = GetOwner())
	{
		if (USRPlanetSurfaceGrid* SurfaceGrid = OwnerActor->FindComponentByClass<USRPlanetSurfaceGrid>())
		{
			TArray<FSRPlanetSurfaceGridCellId> SurfaceLayerCellIds;
			StarRovers::Conveyor::FSRConveyorSegmentQuery::GatherCellIdsAtLayer(Segments, 0, SurfaceLayerCellIds);
			StarRovers::Conveyor::FSRConveyorMutationFinalizer::ClearSurfaceCells(SurfaceGrid, SurfaceLayerCellIds);
		}
	}

	Segments.Reset();
	BeltPaths.Reset();
	TransportState.ResetItems();
	DestroyConveyorItemLabels();
	DestroyPlacedConveyorActors();
	PendingConveyorActorRefreshSurfaceGrid.Reset();
	SetComponentTickEnabled(false);
	StarRovers::Conveyor::FSRConveyorComponentPool::ClearBeltMeshComponent(BeltMeshComponent, false);
	if (IsValid(PathDebugLineBatchComponent))
	{
		PathDebugLineBatchComponent->Flush();
	}
	StarRovers::Conveyor::FSRConveyorComponentPool::ClearUnusedPCGSplineComponents(PCGSplineComponents, 0);
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
	SetComponentTickEnabled(StarRovers::Conveyor::FSRConveyorTickCoordinator::ShouldKeepTickEnabled(
		HasDirtyConveyorActorGroups(),
		ShouldKeepTransportTickEnabled(),
		bShowPathDebugLine,
		bShowConnectionDebugLine));
}

bool USRConveyorNetworkComponent::IsPathDebugLineVisible() const
{
	return bShowPathDebugLine;
}

void USRConveyorNetworkComponent::SetConnectionDebugLineVisible(bool bNewConnectionDebugLineVisible)
{
	if (bShowConnectionDebugLine == bNewConnectionDebugLineVisible)
	{
		return;
	}

	bShowConnectionDebugLine = bNewConnectionDebugLineVisible;
	if (AActor* OwnerActor = GetOwner())
	{
		RefreshPathDebugLines(OwnerActor->FindComponentByClass<USRPlanetSurfaceGrid>());
	}
	SetComponentTickEnabled(StarRovers::Conveyor::FSRConveyorTickCoordinator::ShouldKeepTickEnabled(
		HasDirtyConveyorActorGroups(),
		ShouldKeepTransportTickEnabled(),
		bShowPathDebugLine,
		bShowConnectionDebugLine));
}

bool USRConveyorNetworkComponent::IsConnectionDebugLineVisible() const
{
	return bShowConnectionDebugLine;
}
