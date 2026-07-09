#include "Conveyor/SRConveyorNetworkComponent.h"

#include "Conveyor/SRConveyorActorGroupCoordinator.h"
#include "Conveyor/SRConveyorTickCoordinator.h"
#include "Surface/SRPlanetSurfaceGrid.h"

void USRConveyorNetworkComponent::DestroyPlacedConveyorActors()
{
	StarRovers::Conveyor::FSRConveyorActorGroupCoordinator::DestroyActors(ActorGroupState, PlacedConveyorActors);
	PendingConveyorActorRefreshSurfaceGrid.Reset();
	SetComponentTickEnabled(StarRovers::Conveyor::FSRConveyorTickCoordinator::ShouldKeepTickEnabled(
		false,
		ShouldKeepTransportTickEnabled(),
		bShowPathDebugLine,
		bShowConnectionDebugLine));
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

bool USRConveyorNetworkComponent::RefreshDirtyConveyorActorGroups(USRPlanetSurfaceGrid* SurfaceGrid, int32 MaxGroupCount)
{
	if (!bSpawnConveyorBeltActors)
	{
		return true;
	}

	StarRovers::Conveyor::FSRConveyorActorGroupRefreshSettings Settings;
	Settings.PCGSplineComponentTag = PCGSplineComponentTag;
	Settings.ConveyorActorSurfaceOffset = BeltSurfaceOffset + PCGSplineHeightOffset;
	return StarRovers::Conveyor::FSRConveyorActorGroupCoordinator::RefreshDirtyGroups(
		SurfaceGrid,
		BeltPaths,
		ActorGroupState,
		PlacedConveyorActors,
		Settings,
		MaxGroupCount,
		[this](const TCHAR* Label, FName ActorGroupKey, bool bRequestGarbageCollection)
	{
		LogConveyorMutationMemoryDiagnostics(Label, ActorGroupKey, bRequestGarbageCollection);
	});
}

bool USRConveyorNetworkComponent::HasDirtyConveyorActorGroups() const
{
	return ActorGroupState.HasDirtyGroups();
}

bool USRConveyorNetworkComponent::RebuildPlacedConveyorActors(USRPlanetSurfaceGrid* SurfaceGrid)
{
	DestroyPlacedConveyorActors();
	StarRovers::Conveyor::FSRConveyorActorGroupCoordinator::MarkGroupsDirtyForBeltPaths(ActorGroupState, BeltPaths);
	return RefreshDirtyConveyorActorGroups(SurfaceGrid);
}
