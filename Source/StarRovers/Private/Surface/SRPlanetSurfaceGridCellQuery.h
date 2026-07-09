#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "Templates/Function.h"

namespace StarRovers::SurfaceGridCellQuery
{
	using FCellIndexLookup = TFunctionRef<bool(const FSRPlanetSurfaceGridCellId& CellId, int32& OutIndex)>;
	using FStoredCellInfoLookup = TFunctionRef<bool(const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCellInfo& OutCellInfo)>;
	using FRuntimeCellInfoResolver = TFunctionRef<FSRPlanetSurfaceGridCellInfo(const FSRPlanetSurfaceGridCellInfo& CellInfo)>;

	bool GetCellById(
		const TArray<FSRPlanetSurfaceGridCell>& Cells,
		const FSRPlanetSurfaceGridCellId& CellId,
		FCellIndexLookup GetCellIndex,
		FSRPlanetSurfaceGridCell& OutCell);

	bool GetCellInfoById(
		const FSRPlanetSurfaceGridCellId& CellId,
		FStoredCellInfoLookup GetStoredCellInfoById,
		FRuntimeCellInfoResolver ResolveRuntimeCellInfo,
		FSRPlanetSurfaceGridCellInfo& OutCellInfo);

	bool GetCellNeighbors(
		const FSRPlanetSurfaceGridCellId& CellId,
		int32 FaceResolution,
		FCellIndexLookup GetCellIndex,
		FSRPlanetSurfaceGridCellNeighbors& OutNeighbors);
}
