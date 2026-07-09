#include "Assembly/SRAssemblyComponent.h"

#include "Assembly/SRAssemblySingleCellDeletion.h"
#include "Assembly/SRAssemblySurfaceCursorQuery.h"
#include "Camera/SRPlayerController.h"
#include "Conveyor/SRConveyorNetworkComponent.h"
#include "GameFramework/Actor.h"
#include "Structure/SRStructureDataAsset.h"
#include "Surface/SRPlanetSurfaceGrid.h"

bool USRAssemblyComponent::UpdateConveyorBulkDeletionPreview()
{
	ASRPlayerController* PlayerController = GetOwnerController();
	if (!ModeState.bAssemblyModeActive
		|| !IsValid(PlayerController)
		|| !PlayerController->IsConveyorBulkDeleteModifierActive()
		|| PlayerController->IsPointerOverBlockingUI())
	{
		return false;
	}

	StarRovers::Assembly::FSRAssemblySurfaceCursorTarget CursorTarget;
	if (!StarRovers::Assembly::FSRAssemblySurfaceCursorQuery::TryResolveSurfaceCell(PlayerController, CursorTarget))
	{
		return false;
	}

	AActor* FocusedActor = CursorTarget.FocusedActor;
	USRPlanetSurfaceGrid* SurfaceGrid = CursorTarget.SurfaceGrid;
	const FSRPlanetSurfaceGridCell& HoveredCell = CursorTarget.Cell;
	USRConveyorNetworkComponent* ConveyorNetwork = FocusedActor->FindComponentByClass<USRConveyorNetworkComponent>();
	if (!IsValid(ConveyorNetwork))
	{
		return false;
	}

	TArray<int32> CandidateConveyorLayers;
	StarRovers::Assembly::FSRAssemblySingleCellDeletion::BuildCandidateConveyorLayers(
		PlayerController->GetSelectedStructureDataAsset(),
		CandidateConveyorLayers);
	for (const int32 CandidateLayer : CandidateConveyorLayers)
	{
		TArray<FSRPlanetSurfaceGridCellId> ConnectedCellIds;
		if (!ConveyorNetwork->GetConnectedConveyorCellIdsAtCell(SurfaceGrid, HoveredCell.CellId, CandidateLayer, ConnectedCellIds))
		{
			continue;
		}

		TArray<FSRConveyorBeltPath> ConnectedBeltPaths;
		ConveyorNetwork->GetConnectedConveyorBeltPathsAtCell(SurfaceGrid, HoveredCell.CellId, CandidateLayer, ConnectedBeltPaths);
		ConveyorPreview.SetBulkDeletionPreview(SurfaceGrid, ConnectedCellIds);
		UpdateConveyorDeletionGhostPreview(SurfaceGrid, ConveyorNetwork, HoveredCell.CellId, CandidateLayer, ConnectedBeltPaths);
		return true;
	}

	return false;
}

bool USRAssemblyComponent::UpdateConveyorDeletionGhostPreview(
	USRPlanetSurfaceGrid* SurfaceGrid,
	USRConveyorNetworkComponent* ConveyorNetwork,
	const FSRPlanetSurfaceGridCellId& TargetCellId,
	int32 Layer,
	const TArray<FSRConveyorBeltPath>& BeltPaths)
{
	if (!IsValid(SurfaceGrid) || !IsValid(ConveyorNetwork) || BeltPaths.IsEmpty())
	{
		ConveyorPreview.DestroyDeletionGhostActor();
		return false;
	}

	USRStructureDataAsset* ConveyorDataAsset = BeltPaths[0].StructureDataAsset.Get();
	if (!IsValid(ConveyorDataAsset))
	{
		ConveyorPreview.DestroyDeletionGhostActor();
		return false;
	}

	const FSRStructureData ConveyorData = ConveyorDataAsset->BuildData();
	if (ConveyorPreview.IsDeletionGhostActorCurrent(SurfaceGrid, ConveyorDataAsset, TargetCellId, Layer))
	{
		return true;
	}

	return ConveyorPreview.UpdateDeletionGhostActor(
		SurfaceGrid,
		ConveyorDataAsset,
		ConveyorData,
		BeltPaths,
		ConveyorNetwork->GetConveyorActorSplineComponentTag(),
		ConveyorNetwork->GetConveyorActorSurfaceOffset(),
		TargetCellId,
		Layer);
}
