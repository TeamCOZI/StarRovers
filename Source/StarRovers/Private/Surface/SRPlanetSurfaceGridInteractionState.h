#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "Templates/Function.h"

namespace StarRovers::SurfaceGridInteractionState
{
	using FCellIdValidator = TFunctionRef<bool(const FSRPlanetSurfaceGridCellId& CellId)>;

	bool SetFocusedCell(
		const FSRPlanetSurfaceGridCellId& CellId,
		bool& bHasCell,
		FSRPlanetSurfaceGridCellId& StoredCellId,
		FCellIdValidator IsValidCell,
		bool& bOutChanged);

	bool ClearFocusedCell(bool& bHasCell, FSRPlanetSurfaceGridCellId& StoredCellId);

	bool SetValidatedUniqueCellIds(
		const TArray<FSRPlanetSurfaceGridCellId>& CellIds,
		TArray<FSRPlanetSurfaceGridCellId>& StoredCellIds,
		FCellIdValidator IsValidCell);

	bool ClearCellIds(TArray<FSRPlanetSurfaceGridCellId>& StoredCellIds);

	bool SetValidatedPortPreviewCellIds(
		const TArray<FSRPlanetSurfaceGridCellId>& InputConnectionCellIds,
		const TArray<FSRPlanetSurfaceGridCellId>& OutputConnectionCellIds,
		TArray<FSRPlanetSurfaceGridCellId>& StoredInputCellIds,
		TArray<FSRPlanetSurfaceGridCellId>& StoredOutputCellIds,
		FCellIdValidator IsValidCell);

	bool ClearPortPreviewCellIds(
		TArray<FSRPlanetSurfaceGridCellId>& StoredInputCellIds,
		TArray<FSRPlanetSurfaceGridCellId>& StoredOutputCellIds);

	bool HasInteractionOverlayContent(
		bool bHasHoveredCell,
		bool bHasSelectedCell,
		const TArray<FSRPlanetSurfaceGridCellId>& AreaSelectionCellIds,
		const TArray<FSRPlanetSurfaceGridCellId>& OccupiedPreviewCellIds,
		const TArray<FSRPlanetSurfaceGridCellId>& InputPortPreviewCellIds,
		const TArray<FSRPlanetSurfaceGridCellId>& OutputPortPreviewCellIds,
		const TArray<FSRPlanetSurfaceGridCellId>& DeletionPreviewCellIds,
		const TArray<FSRPlanetSurfaceGridCellId>& InvalidPreviewCellIds);

	void ResetInteractionState(
		bool& bHasHoveredCell,
		FSRPlanetSurfaceGridCellId& HoveredCellId,
		bool& bHoveredInteractionGridPatchVisible,
		bool& bHasSelectedCell,
		FSRPlanetSurfaceGridCellId& SelectedCellId,
		TArray<FSRPlanetSurfaceGridCellId>& AreaSelectionCellIds,
		TArray<FSRPlanetSurfaceGridCellId>& OccupiedPreviewCellIds,
		TArray<FSRPlanetSurfaceGridCellId>& InputPortPreviewCellIds,
		TArray<FSRPlanetSurfaceGridCellId>& OutputPortPreviewCellIds,
		TArray<FSRPlanetSurfaceGridCellId>& DeletionPreviewCellIds,
		TArray<FSRPlanetSurfaceGridCellId>& InvalidPreviewCellIds);
}
