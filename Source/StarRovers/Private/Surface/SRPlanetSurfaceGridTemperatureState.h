#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "Templates/Function.h"

namespace StarRovers::SurfaceGridTemperatureState
{
	using FCellIndexQuery = TFunctionRef<bool(const FSRPlanetSurfaceGridCellId& CellId, int32& OutIndex)>;
	using FCellInfoQuery = TFunctionRef<bool(const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCellInfo& OutCellInfo)>;
	using FCellInfoBuilder = TFunctionRef<FSRPlanetSurfaceGridCellInfo(const FSRPlanetSurfaceGridCell& Cell)>;
	using FCellInfoStore = TFunctionRef<void(const FSRPlanetSurfaceGridCellInfo& CellInfo)>;

	float GetRepresentativeSurfaceTemperature(ESRFacilityTemperatureState TemperatureState);
	ESRFacilityTemperatureState ResolveTemperatureStateFromSurfaceTemperature(float SurfaceTemperature);

	bool GetCellTemperatureState(
		const TArray<FSRPlanetSurfaceGridCell>& Cells,
		const FSRPlanetSurfaceGridCellId& CellId,
		ESRFacilityTemperatureState& OutTemperatureState,
		FCellIndexQuery GetCellIndex);

	bool SetCellTemperatureState(
		TArray<FSRPlanetSurfaceGridCell>& Cells,
		const FSRPlanetSurfaceGridCellId& CellId,
		ESRFacilityTemperatureState TemperatureState,
		FCellIndexQuery GetCellIndex,
		FCellInfoBuilder BuildCellInfo,
		FCellInfoQuery GetStoredCellInfoById,
		FCellInfoStore StoreCellInfo);

	bool SetCellSurfaceTemperature(
		TArray<FSRPlanetSurfaceGridCell>& Cells,
		const FSRPlanetSurfaceGridCellId& CellId,
		float SurfaceTemperature,
		FCellIndexQuery GetCellIndex,
		FCellInfoBuilder BuildCellInfo,
		FCellInfoQuery GetStoredCellInfoById,
		FCellInfoStore StoreCellInfo);
}
