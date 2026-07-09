#include "Conveyor/SRConveyorNetworkComponent.h"

#include "SRConveyorDeletionDiagnostics.h"
#include "Conveyor/SRConveyorMutationFinalizer.h"
#include "Conveyor/SRConveyorPlacementPlanner.h"
#include "Conveyor/SRConveyorPlacementValidator.h"
#include "Conveyor/SRConveyorRemovalPlanner.h"
#include "Conveyor/SRConveyorTickCoordinator.h"
#include "Surface/SRPlanetSurfaceGrid.h"

bool USRConveyorNetworkComponent::CanPlaceConveyorPath(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TArray<FSRPlanetSurfaceGridCellId>& PathCellIds,
	int32 Layer,
	const TSet<FSRPlanetSurfaceGridCellId>& IgnoredOccupiedCellIds) const
{
	return StarRovers::Conveyor::FSRConveyorPlacementPlanner::CanPlacePath(
		SurfaceGrid,
		PathCellIds,
		Layer,
		Segments,
		IgnoredOccupiedCellIds);
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

	StarRovers::Conveyor::FSRConveyorPlacementPlan PlacementPlan;
	if (!StarRovers::Conveyor::FSRConveyorPlacementPlanner::BuildPlacementPlan(
		SurfaceGrid,
		PathCellIds,
		Layer,
		LayerHeight,
		DefaultLayerHeight,
		StructureDataAsset,
		NetworkId,
		Segments,
		BeltPaths.Num(),
		PlacementPlan))
	{
		return false;
	}

	if (!PlacementPlan.DestructibleStructureCellIds.IsEmpty())
	{
		if (!StarRovers::Conveyor::FSRConveyorPlacementValidator::TryRemoveDestructibleStructuresAtCells(SurfaceGrid, PlacementPlan.DestructibleStructureCellIds))
		{
			return false;
		}
	}

	StarRovers::Conveyor::FSRConveyorPlacementPlanner::ApplyPlacementPlan(PlacementPlan, Segments, BeltPaths);

	if (PlacementPlan.Layer == 0)
	{
		TArray<FSRPlanetSurfaceGridCellId> OccupiedCellIds = PathCellIds;
		if (!SurfaceGrid->SetCellsOccupied(OccupiedCellIds, true, NetworkId.IsNone() ? FName(TEXT("Conveyor")) : NetworkId))
		{
			StarRovers::Conveyor::FSRConveyorPlacementPlanner::RollbackPlacementPlan(PlacementPlan, Segments, BeltPaths);
			return false;
		}
	}

	if (bSpawnConveyorBeltActors)
	{
		StarRovers::Conveyor::FSRConveyorMutationFinalizer::MarkPlacementActorGroup(ActorGroupState, StructureDataAsset, PlacementPlan.Layer);
		ScheduleDirtyConveyorActorGroupRefresh(SurfaceGrid);
	}

	RefreshConveyorRibbonMesh(SurfaceGrid);
	RefreshPCGSplineInputs(SurfaceGrid);
	RequestPCGGeneration();
	RefreshPathDebugLines(SurfaceGrid);
	SetComponentTickEnabled(true);
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
	StarRovers::Conveyor::FSRConveyorRemovalResult RemovalResult;
	if (!StarRovers::Conveyor::FSRConveyorRemovalPlanner::RemoveConveyorAtCell(
		SurfaceGrid,
		CellId,
		SafeLayer,
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
			TEXT("ConveyorDelete.DestroyPlacedActors"),
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

bool USRConveyorNetworkComponent::TryRemoveConveyorBeltPath(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRConveyorBeltPath& BeltPath,
	const TArray<FSRPlanetSurfaceGridCellId>& PlacedCellIds)
{
	if (!IsValid(SurfaceGrid) || BeltPath.CellIds.IsEmpty())
	{
		return false;
	}

	StarRovers::Conveyor::FSRConveyorRemovalResult RemovalResult;
	if (!StarRovers::Conveyor::FSRConveyorRemovalPlanner::RemoveBeltPath(
		SurfaceGrid,
		BeltPath,
		PlacedCellIds,
		BeltPaths,
		Segments,
		RemovalResult))
	{
		return false;
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
			TEXT("ConveyorDelete.DestroyPlacedActors"),
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

bool USRConveyorNetworkComponent::TryRemoveConveyorsAtCells(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TArray<FSRPlanetSurfaceGridCellId>& CellIds)
{
	if (!IsValid(SurfaceGrid) || CellIds.IsEmpty())
	{
		return false;
	}

	TSet<FSRPlanetSurfaceGridCellId> SelectedCellIds;
	SelectedCellIds.Reserve(CellIds.Num());
	for (const FSRPlanetSurfaceGridCellId& CellId : CellIds)
	{
		SelectedCellIds.Add(CellId);
	}

	TArray<FSRConveyorLaneKey> LaneKeysToRemove;
	StarRovers::Conveyor::FSRConveyorRemovalPlanner::CollectLaneKeysInCells(Segments, SelectedCellIds, LaneKeysToRemove);

	bool bRemovedAny = false;
	for (const FSRConveyorLaneKey& LaneKey : LaneKeysToRemove)
	{
		bRemovedAny |= TryRemoveConveyorAtCell(SurfaceGrid, LaneKey.CellId, LaneKey.Layer);
	}

	return bRemovedAny;
}
