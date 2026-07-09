#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"

class USRPlanetSurfaceGrid;
struct FSRAssemblyPlacementDragState;

namespace StarRovers::Assembly
{
	class STARROVERS_API FSRAssemblyStructureDragPathBuilder
	{
	public:
		static bool BuildQueuedCellIds(
			const FSRAssemblyPlacementDragState& PlacementDrag,
			USRPlanetSurfaceGrid* SurfaceGrid,
			const FSRPlanetSurfaceGridCellId& TargetCellId,
			TArray<FSRPlanetSurfaceGridCellId>& OutCellIds);

	private:
		static void AppendGridLineCellIds(
			const FSRPlanetSurfaceGridCellId& StartCellId,
			const FSRPlanetSurfaceGridCellId& EndCellId,
			TArray<FSRPlanetSurfaceGridCellId>& OutCellIds);
	};
}
