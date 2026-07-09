#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "Templates/Function.h"

class USRPlanetSurfaceGrid;

namespace StarRovers::Conveyor
{
	struct STARROVERS_API FSRConveyorPathfinder
	{
		static bool FindPath(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const FSRPlanetSurfaceGridCellId& StartCellId,
			const FSRPlanetSurfaceGridCellId& EndCellId,
			TFunctionRef<bool(const FSRPlanetSurfaceGridCellId& CellId)> CanUseCell,
			TArray<FSRPlanetSurfaceGridCellId>& OutPath);
	};
}
