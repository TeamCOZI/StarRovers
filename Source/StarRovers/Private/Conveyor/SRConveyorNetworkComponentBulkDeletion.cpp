#include "Conveyor/SRConveyorNetworkComponent.h"

#include "SRConveyorDeletionDiagnostics.h"
#include "Conveyor/SRConveyorConnectionQuery.h"
#include "Conveyor/SRConveyorMutationFinalizer.h"
#include "Conveyor/SRConveyorRemovalPlanner.h"
#include "Conveyor/SRConveyorTickCoordinator.h"
#include "Conveyor/SRConveyorBeltPathQuery.h"
#include "Structure/SRStructureDataAsset.h"
#include "Surface/SRPlanetSurfaceGrid.h"

bool USRConveyorNetworkComponent::GetConnectedConveyorCellIdsAtCell(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& CellId,
	int32 Layer,
	TArray<FSRPlanetSurfaceGridCellId>& OutCellIds) const
{
	return StarRovers::Conveyor::FSRConveyorBeltPathQuery::GatherConnectedCellIds(
		SurfaceGrid,
		Segments,
		CellId,
		Layer,
		OutCellIds);
}

bool USRConveyorNetworkComponent::GetConnectedConveyorBeltPathsAtCell(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& CellId,
	int32 Layer,
	TArray<FSRConveyorBeltPath>& OutBeltPaths) const
{
	return StarRovers::Conveyor::FSRConveyorBeltPathQuery::GatherConnectedBeltPaths(
		SurfaceGrid,
		Segments,
		BeltPaths,
		CellId,
		Layer,
		OutBeltPaths);
}

bool USRConveyorNetworkComponent::GetConveyorBeltPathsInCells(
	const TSet<FSRPlanetSurfaceGridCellId>& CellIds,
	TArray<FSRConveyorBeltPath>& OutBeltPaths) const
{
	return StarRovers::Conveyor::FSRConveyorBeltPathQuery::GatherBeltPathsInCells(
		BeltPaths,
		CellIds,
		OutBeltPaths);
}

bool USRConveyorNetworkComponent::TryRemoveConnectedConveyorsAtCell(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& CellId,
	int32 Layer)
{
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	TArray<FSRConveyorLaneKey> ConnectedLaneKeys;
	if (!StarRovers::Conveyor::FSRConveyorConnectionQuery::GatherConnectedLaneKeysAtCell(SurfaceGrid, Segments, CellId, Layer, ConnectedLaneKeys))
	{
		return false;
	}

	StarRovers::Conveyor::FSRConveyorRemovalResult RemovalResult;
	if (!StarRovers::Conveyor::FSRConveyorRemovalPlanner::RemoveLaneKeys(
		SurfaceGrid,
		ConnectedLaneKeys,
		Layer,
		BeltPaths,
		Segments,
		RemovalResult))
	{
		return false;
	}

	StarRovers::Conveyor::FSRConveyorMutationFinalizer::RemoveTransportItems(TransportState, RemovalResult);

	if (bShowTransportItemLabels)
	{
		RefreshConveyorItemLabels(SurfaceGrid, 0.0f);
	}

	StarRovers::Conveyor::FSRConveyorMutationFinalizer::ClearSurfaceCells(SurfaceGrid, RemovalResult.ClearedCellIds);

	if (bSpawnConveyorBeltActors)
	{
		StarRovers::Conveyor::FSRConveyorMutationFinalizer::MarkDeletionActorGroups(ActorGroupState, RemovalResult);
		ScheduleDirtyConveyorActorGroupRefresh(SurfaceGrid);
	}
	else
	{
		DestroyPlacedConveyorActors();
		StarRovers::Conveyor::FSRConveyorMutationFinalizer::LogDeletionDestroyedActorGroups(
			RemovalResult,
			TEXT("ConveyorBulkDelete.DestroyPlacedActors"),
			[this](const TCHAR* Label, FName ActorGroupKey, bool bRequestGarbageCollection)
		{
			LogConveyorMutationMemoryDiagnostics(
				Label,
				ActorGroupKey,
				bRequestGarbageCollection);
		});
	}

	RefreshConveyorRibbonMesh(SurfaceGrid);
	RefreshPCGSplineInputs(SurfaceGrid);
	RequestPCGGeneration();
	RefreshPathDebugLines(SurfaceGrid);
	SetComponentTickEnabled(StarRovers::Conveyor::FSRConveyorTickCoordinator::ShouldKeepTickEnabled(
		HasDirtyConveyorActorGroups(),
		ShouldKeepTransportTickEnabled(),
		bShowPathDebugLine,
		bShowConnectionDebugLine));
	return true;
}
