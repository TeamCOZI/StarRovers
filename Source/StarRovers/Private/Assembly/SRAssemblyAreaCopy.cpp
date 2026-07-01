#include "Assembly/SRAssemblyAreaCopy.h"

namespace StarRovers::Assembly
{
	bool FSRAssemblyAreaCopy::IsPlacementActive() const
	{
		return bIsPlacementActive;
	}

	bool FSRAssemblyAreaCopy::HasPayload() const
	{
		return !CopiedStructures.IsEmpty() || !CopiedConveyorPaths.IsEmpty();
	}

	bool FSRAssemblyAreaCopy::HasCachedPreviewForHover(const FSRPlanetSurfaceGridCellId& HoverCellId) const
	{
		return bHasLastPreviewHoverCell && LastPreviewHoverCellId == HoverCellId;
	}

	void FSRAssemblyAreaCopy::BeginPlacement(
		TArray<FSRAreaCopiedStructure>&& NewCopiedStructures,
		TArray<FSRAreaCopiedConveyorPath>&& NewCopiedConveyorPaths)
	{
		CopiedStructures = MoveTemp(NewCopiedStructures);
		CopiedConveyorPaths = MoveTemp(NewCopiedConveyorPaths);
		bIsPlacementActive = true;
		ResetPreviewCache();
	}

	void FSRAssemblyAreaCopy::Cancel()
	{
		bIsPlacementActive = false;
		ResetPreviewCache();
		CopiedStructures.Reset();
		CopiedConveyorPaths.Reset();
	}

	void FSRAssemblyAreaCopy::ResetPreviewCache()
	{
		ClearPreviewHoverCache();
		LastPreviewState = ESRAreaCopyPlacementPreviewState::Blocked;
		LastPreviewHoverCellId = FSRPlanetSurfaceGridCellId();
	}

	void FSRAssemblyAreaCopy::ClearPreviewHoverCache()
	{
		bHasLastPreviewHoverCell = false;
		LastReplaceableOccupantIds.Reset();
	}

	void FSRAssemblyAreaCopy::StorePreviewEvaluation(
		const FSRPlanetSurfaceGridCellId& HoverCellId,
		const FSRAreaCopyPlacementEvaluation& Evaluation)
	{
		LastPreviewHoverCellId = HoverCellId;
		bHasLastPreviewHoverCell = true;
		LastPreviewState = Evaluation.PreviewState;
		LastReplaceableOccupantIds = Evaluation.ReplaceableOccupantIds;
	}

	void FSRAssemblyAreaCopy::SetPreviewState(ESRAreaCopyPlacementPreviewState PreviewState)
	{
		LastPreviewState = PreviewState;
	}
}
