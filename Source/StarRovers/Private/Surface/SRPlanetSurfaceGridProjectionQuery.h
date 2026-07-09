#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "Templates/Function.h"

namespace StarRovers::SurfaceGridProjectionQuery
{
	using FCellByIdQuery = TFunctionRef<bool(const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCell& OutCell)>;

	bool ProjectWorldLocationToCell(
		const TArray<FSRPlanetSurfaceGridCell>& Cells,
		const FVector& WorldLocation,
		const FTransform& ComponentTransform,
		int32 FaceResolution,
		FCellByIdQuery GetCellById,
		FSRPlanetSurfaceGridCell& OutCell);
}
