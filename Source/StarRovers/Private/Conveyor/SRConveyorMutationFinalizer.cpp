#include "Conveyor/SRConveyorMutationFinalizer.h"

#include "SRConveyorDeletionDiagnostics.h"
#include "Conveyor/SRConveyorActorGroupCoordinator.h"
#include "Surface/SRPlanetSurfaceGrid.h"

void StarRovers::Conveyor::FSRConveyorMutationFinalizer::RemoveTransportItems(
	FSRConveyorTransportRuntimeState& TransportState,
	const FSRConveyorRemovalResult& RemovalResult)
{
	for (const FSRConveyorLaneKey& RemovedLaneKey : RemovalResult.RemovedLaneKeys)
	{
		TransportState.ItemsByLane.Remove(RemovedLaneKey);
	}
}

void StarRovers::Conveyor::FSRConveyorMutationFinalizer::ClearSurfaceCells(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TArray<FSRPlanetSurfaceGridCellId>& CellIds)
{
	if (IsValid(SurfaceGrid) && !CellIds.IsEmpty())
	{
		SurfaceGrid->SetCellsOccupied(CellIds, false, NAME_None);
	}
}

void StarRovers::Conveyor::FSRConveyorMutationFinalizer::MarkPlacementActorGroup(
	FSRConveyorActorGroupRuntimeState& ActorGroupState,
	USRStructureDataAsset* StructureDataAsset,
	int32 Layer)
{
	FSRConveyorActorGroupCoordinator::MarkGroupDirty(ActorGroupState, StructureDataAsset, Layer);
	FSRConveyorActorGroupCoordinator::MarkPlacementDiagnosticPending(ActorGroupState, StructureDataAsset, Layer);
}

void StarRovers::Conveyor::FSRConveyorMutationFinalizer::MarkDeletionActorGroups(
	FSRConveyorActorGroupRuntimeState& ActorGroupState,
	const FSRConveyorRemovalResult& RemovalResult)
{
	for (USRStructureDataAsset* StructureDataAsset : RemovalResult.AffectedStructureDataAssets)
	{
		FSRConveyorActorGroupCoordinator::MarkGroupDirty(ActorGroupState, StructureDataAsset, RemovalResult.Layer);
		FSRConveyorActorGroupCoordinator::MarkDeletionDiagnosticPending(ActorGroupState, StructureDataAsset, RemovalResult.Layer);
	}
}

void StarRovers::Conveyor::FSRConveyorMutationFinalizer::LogDeletionDestroyedActorGroups(
	const FSRConveyorRemovalResult& RemovalResult,
	const TCHAR* Label,
	TFunctionRef<void(const TCHAR* LogLabel, FName ActorGroupKey, bool bRequestGarbageCollection)> LogMutationDiagnostics)
{
	for (USRStructureDataAsset* StructureDataAsset : RemovalResult.AffectedStructureDataAssets)
	{
		LogMutationDiagnostics(
			Label,
			FSRConveyorActorGroupCoordinator::MakeGroupKey(StructureDataAsset, RemovalResult.Layer),
			ShouldForceGCOnConveyorDelete());
	}
}
