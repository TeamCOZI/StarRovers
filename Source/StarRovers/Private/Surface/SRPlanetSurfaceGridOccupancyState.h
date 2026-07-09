#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "Templates/Function.h"

namespace StarRovers::SurfaceGridOccupancyState
{
	using FCellIndexQuery = TFunctionRef<bool(const FSRPlanetSurfaceGridCellId& CellId, int32& OutIndex)>;
	using FCellInfoQuery = TFunctionRef<bool(const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCellInfo& OutCellInfo)>;
	using FCellInfoBuilder = TFunctionRef<FSRPlanetSurfaceGridCellInfo(const FSRPlanetSurfaceGridCell& Cell)>;
	using FCellInfoStore = TFunctionRef<void(const FSRPlanetSurfaceGridCellInfo& CellInfo)>;

	void ClearOccupancy(
		TArray<FSRPlanetSurfaceGridCell>& Cells,
		FCellInfoBuilder BuildCellInfo,
		FCellInfoQuery GetStoredCellInfoById,
		FCellInfoStore StoreCellInfo);

	bool SetCellOccupied(
		TArray<FSRPlanetSurfaceGridCell>& Cells,
		const FSRPlanetSurfaceGridCellId& CellId,
		bool bOccupied,
		FName OccupantId,
		FCellIndexQuery GetCellIndex,
		FCellInfoBuilder BuildCellInfo,
		FCellInfoQuery GetStoredCellInfoById,
		FCellInfoStore StoreCellInfo);

	bool CanOccupyCells(
		const TArray<FSRPlanetSurfaceGridCellId>& CellIds,
		FCellInfoQuery GetCellInfoById);

	bool SetCellsOccupied(
		TArray<FSRPlanetSurfaceGridCell>& Cells,
		const TArray<FSRPlanetSurfaceGridCellId>& CellIds,
		bool bOccupied,
		FName OccupantId,
		FCellIndexQuery GetCellIndex,
		FCellInfoBuilder BuildCellInfo,
		FCellInfoQuery GetStoredCellInfoById,
		FCellInfoStore StoreCellInfo);
}
