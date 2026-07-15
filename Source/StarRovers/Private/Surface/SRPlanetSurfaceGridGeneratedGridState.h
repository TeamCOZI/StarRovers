#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"

class AActor;

namespace StarRovers::SurfaceGridGeneratedGridState
{
	void AssignGeneratedCells(
		TArray<FSRPlanetSurfaceGridCell>& TargetCells,
		TArray<FSRPlanetSurfaceGridCell>&& NewCells,
		int32& FaceResolution,
		bool& bUsingGeneratedGridCells);

	bool TryLoadOwnerCachedCells(
		const AActor* Owner,
		TArray<FSRPlanetSurfaceGridCell>& TargetCells,
		bool& bUsingGeneratedGridCells);

	void ResetGeneratedGridInteractionState(
		bool& bHasHoveredCell,
		FSRPlanetSurfaceGridCellId& HoveredCellId,
		bool& bHasSelectedCell,
		FSRPlanetSurfaceGridCellId& SelectedCellId,
		TArray<FSRPlanetSurfaceGridCellId>& InputPortPreviewCellIds,
		TArray<FSRPlanetSurfaceGridCellId>& OutputPortPreviewCellIds,
		TArray<FSRPlanetSurfaceGridCellId>& DeletionPreviewCellIds,
		TArray<FSRPlanetSurfaceGridCellId>& ConstructionReplacementPreviewCellIds,
		TArray<FSRPlanetSurfaceGridCellId>& InvalidPreviewCellIds);
}
