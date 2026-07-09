#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "Templates/Function.h"

namespace StarRovers::SurfaceGridFootprint
{
	using FCellIndexQuery = TFunctionRef<bool(const FSRPlanetSurfaceGridCellId& CellId, int32& OutIndex)>;

	bool BuildFootprintCellIds(
		const FSRPlanetSurfaceGridCellId& OriginCellId,
		int32 FootprintCellsX,
		int32 FootprintCellsY,
		int32 FaceResolution,
		FCellIndexQuery GetCellIndex,
		TArray<FSRPlanetSurfaceGridCellId>& OutCellIds);
}
