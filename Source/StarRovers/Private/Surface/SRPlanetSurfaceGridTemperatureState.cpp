#include "SRPlanetSurfaceGridTemperatureState.h"

namespace
{
	void StoreUpdatedCellInfo(
		const FSRPlanetSurfaceGridCell& Cell,
		StarRovers::SurfaceGridTemperatureState::FCellInfoBuilder BuildCellInfo,
		StarRovers::SurfaceGridTemperatureState::FCellInfoQuery GetStoredCellInfoById,
		StarRovers::SurfaceGridTemperatureState::FCellInfoStore StoreCellInfo)
	{
		FSRPlanetSurfaceGridCellInfo UpdatedCellInfo = BuildCellInfo(Cell);
		FSRPlanetSurfaceGridCellInfo ExistingCellInfo;
		if (GetStoredCellInfoById(Cell.CellId, ExistingCellInfo))
		{
			UpdatedCellInfo.FaceCellIndex = ExistingCellInfo.FaceCellIndex;
		}
		StoreCellInfo(UpdatedCellInfo);
	}
}

float StarRovers::SurfaceGridTemperatureState::GetRepresentativeSurfaceTemperature(ESRFacilityTemperatureState TemperatureState)
{
	switch (TemperatureState)
	{
	case ESRFacilityTemperatureState::Frozen:
		return 0.0f;
	case ESRFacilityTemperatureState::Cold:
		return 0.25f;
	case ESRFacilityTemperatureState::Hot:
		return 0.78f;
	case ESRFacilityTemperatureState::Overheated:
		return 1.0f;
	case ESRFacilityTemperatureState::Normal:
	default:
		return 0.5f;
	}
}

ESRFacilityTemperatureState StarRovers::SurfaceGridTemperatureState::ResolveTemperatureStateFromSurfaceTemperature(float SurfaceTemperature)
{
	const float ClampedSurfaceTemperature = FMath::Clamp(SurfaceTemperature, 0.0f, 1.0f);
	if (ClampedSurfaceTemperature <= 0.12f)
	{
		return ESRFacilityTemperatureState::Frozen;
	}
	if (ClampedSurfaceTemperature <= 0.35f)
	{
		return ESRFacilityTemperatureState::Cold;
	}
	if (ClampedSurfaceTemperature < 0.70f)
	{
		return ESRFacilityTemperatureState::Normal;
	}
	if (ClampedSurfaceTemperature < 0.88f)
	{
		return ESRFacilityTemperatureState::Hot;
	}
	return ESRFacilityTemperatureState::Overheated;
}

bool StarRovers::SurfaceGridTemperatureState::GetCellTemperatureState(
	const TArray<FSRPlanetSurfaceGridCell>& Cells,
	const FSRPlanetSurfaceGridCellId& CellId,
	ESRFacilityTemperatureState& OutTemperatureState,
	FCellIndexQuery GetCellIndex)
{
	int32 CellIndex = INDEX_NONE;
	if (!GetCellIndex(CellId, CellIndex) || !Cells.IsValidIndex(CellIndex))
	{
		OutTemperatureState = ESRFacilityTemperatureState::Normal;
		return false;
	}

	OutTemperatureState = Cells[CellIndex].TemperatureState;
	return true;
}

bool StarRovers::SurfaceGridTemperatureState::SetCellTemperatureState(
	TArray<FSRPlanetSurfaceGridCell>& Cells,
	const FSRPlanetSurfaceGridCellId& CellId,
	ESRFacilityTemperatureState TemperatureState,
	FCellIndexQuery GetCellIndex,
	FCellInfoBuilder BuildCellInfo,
	FCellInfoQuery GetStoredCellInfoById,
	FCellInfoStore StoreCellInfo)
{
	int32 CellIndex = INDEX_NONE;
	if (!GetCellIndex(CellId, CellIndex) || !Cells.IsValidIndex(CellIndex))
	{
		return false;
	}

	FSRPlanetSurfaceGridCell& Cell = Cells[CellIndex];
	Cell.TemperatureState = TemperatureState;
	Cell.SurfaceTemperature = GetRepresentativeSurfaceTemperature(TemperatureState);
	StoreUpdatedCellInfo(Cell, BuildCellInfo, GetStoredCellInfoById, StoreCellInfo);
	return true;
}

bool StarRovers::SurfaceGridTemperatureState::SetCellSurfaceTemperature(
	TArray<FSRPlanetSurfaceGridCell>& Cells,
	const FSRPlanetSurfaceGridCellId& CellId,
	float SurfaceTemperature,
	FCellIndexQuery GetCellIndex,
	FCellInfoBuilder BuildCellInfo,
	FCellInfoQuery GetStoredCellInfoById,
	FCellInfoStore StoreCellInfo)
{
	int32 CellIndex = INDEX_NONE;
	if (!GetCellIndex(CellId, CellIndex) || !Cells.IsValidIndex(CellIndex))
	{
		return false;
	}

	FSRPlanetSurfaceGridCell& Cell = Cells[CellIndex];
	Cell.SurfaceTemperature = FMath::Clamp(SurfaceTemperature, 0.0f, 1.0f);
	Cell.TemperatureState = ResolveTemperatureStateFromSurfaceTemperature(Cell.SurfaceTemperature);
	StoreUpdatedCellInfo(Cell, BuildCellInfo, GetStoredCellInfoById, StoreCellInfo);
	return true;
}
