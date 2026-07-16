#include "Surface/SRPlanetSurfaceGrid.h"

#include "SRPlanetSurfaceGridInteractionState.h"

namespace SurfaceGridInteractionState = StarRovers::SurfaceGridInteractionState;

bool USRPlanetSurfaceGrid::IsInteractionCellIdValid(const FSRPlanetSurfaceGridCellId& CellId) const
{
	int32 CellIndex = INDEX_NONE;
	return GetCellIndex(CellId, CellIndex);
}

bool USRPlanetSurfaceGrid::SetFocusedInteractionCell(
	const FSRPlanetSurfaceGridCellId& CellId,
	bool& bHasCell,
	FSRPlanetSurfaceGridCellId& StoredCellId)
{
	const auto IsValidCellId = [this](const FSRPlanetSurfaceGridCellId& CandidateCellId)
	{
		return IsInteractionCellIdValid(CandidateCellId);
	};

	bool bChanged = false;
	if (!SurfaceGridInteractionState::SetFocusedCell(CellId, bHasCell, StoredCellId, IsValidCellId, bChanged))
	{
		return false;
	}

	if (bChanged)
	{
		NotifyInteractionStateChanged();
	}
	return true;
}

bool USRPlanetSurfaceGrid::ClearFocusedInteractionCell(bool& bHasCell, FSRPlanetSurfaceGridCellId& StoredCellId)
{
	if (!SurfaceGridInteractionState::ClearFocusedCell(bHasCell, StoredCellId))
	{
		return false;
	}

	NotifyInteractionStateChanged();
	return true;
}

bool USRPlanetSurfaceGrid::SetInteractionPreviewCellIds(
	const TArray<FSRPlanetSurfaceGridCellId>& CellIds,
	TArray<FSRPlanetSurfaceGridCellId>& StoredCellIds)
{
	const auto IsValidCellId = [this](const FSRPlanetSurfaceGridCellId& CandidateCellId)
	{
		return IsInteractionCellIdValid(CandidateCellId);
	};

	if (!SurfaceGridInteractionState::SetValidatedUniqueCellIds(CellIds, StoredCellIds, IsValidCellId))
	{
		return false;
	}

	NotifyInteractionStateChanged();
	return true;
}

bool USRPlanetSurfaceGrid::ClearInteractionPreviewCellIds(TArray<FSRPlanetSurfaceGridCellId>& StoredCellIds)
{
	if (!SurfaceGridInteractionState::ClearCellIds(StoredCellIds))
	{
		return false;
	}

	NotifyInteractionStateChanged();
	return true;
}

bool USRPlanetSurfaceGrid::SetInteractionPortPreviewCellIds(
	const TArray<FSRPlanetSurfaceGridCellId>& InputConnectionCellIds,
	const TArray<FSRPlanetSurfaceGridCellId>& OutputConnectionCellIds)
{
	const auto IsValidCellId = [this](const FSRPlanetSurfaceGridCellId& CandidateCellId)
	{
		return IsInteractionCellIdValid(CandidateCellId);
	};

	if (!SurfaceGridInteractionState::SetValidatedPortPreviewCellIds(
		InputConnectionCellIds,
		OutputConnectionCellIds,
		InputPortPreviewCellIds,
		OutputPortPreviewCellIds,
		IsValidCellId))
	{
		return false;
	}

	NotifyInteractionStateChanged();
	return true;
}

bool USRPlanetSurfaceGrid::ClearInteractionPortPreviewCellIds()
{
	if (!SurfaceGridInteractionState::ClearPortPreviewCellIds(InputPortPreviewCellIds, OutputPortPreviewCellIds))
	{
		return false;
	}

	NotifyInteractionStateChanged();
	return true;
}

bool USRPlanetSurfaceGrid::SetInteractionHoverGridHighlightCellIds(
	const TArray<FSRPlanetSurfaceGridCellId>& OccupiedCellIds,
	const TArray<FSRPlanetSurfaceGridCellId>& InputPortCellIds,
	const TArray<FSRPlanetSurfaceGridCellId>& OutputPortCellIds)
{
	const auto IsValidCellId = [this](const FSRPlanetSurfaceGridCellId& CandidateCellId)
	{
		return IsInteractionCellIdValid(CandidateCellId);
	};

	const bool bOccupiedChanged = SurfaceGridInteractionState::SetValidatedUniqueCellIds(
		OccupiedCellIds,
		HoverGridOccupiedCellIds,
		IsValidCellId);
	const bool bInputChanged = SurfaceGridInteractionState::SetValidatedUniqueCellIds(
		InputPortCellIds,
		HoverGridInputPortCellIds,
		IsValidCellId);
	const bool bOutputChanged = SurfaceGridInteractionState::SetValidatedUniqueCellIds(
		OutputPortCellIds,
		HoverGridOutputPortCellIds,
		IsValidCellId);
	if (!bOccupiedChanged && !bInputChanged && !bOutputChanged)
	{
		return false;
	}

	NotifyInteractionStateChanged();
	return true;
}

bool USRPlanetSurfaceGrid::ClearInteractionHoverGridHighlightCellIds()
{
	const bool bOccupiedChanged = SurfaceGridInteractionState::ClearCellIds(HoverGridOccupiedCellIds);
	const bool bInputChanged = SurfaceGridInteractionState::ClearCellIds(HoverGridInputPortCellIds);
	const bool bOutputChanged = SurfaceGridInteractionState::ClearCellIds(HoverGridOutputPortCellIds);
	if (!bOccupiedChanged && !bInputChanged && !bOutputChanged)
	{
		return false;
	}

	NotifyInteractionStateChanged();
	return true;
}
