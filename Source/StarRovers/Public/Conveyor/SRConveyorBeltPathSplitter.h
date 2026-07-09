#pragma once

#include "CoreMinimal.h"
#include "Conveyor/SRConveyorTypes.h"
#include "Templates/Function.h"

namespace StarRovers::Conveyor
{
	struct STARROVERS_API FSRConveyorBeltPathSplitter
	{
		static bool AppendMatchingSubPaths(
			const FSRConveyorBeltPath& BeltPath,
			TFunctionRef<bool(const FSRPlanetSurfaceGridCellId& CellId)> ShouldKeepCell,
			TArray<FSRConveyorBeltPath>& OutBeltPaths);
	};
}
