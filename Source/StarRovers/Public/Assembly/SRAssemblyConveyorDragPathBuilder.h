#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"

class USRConveyorNetworkComponent;
class USRPlanetSurfaceGrid;
struct FSRAssemblyPlacementDragState;
struct FSRStructureData;

namespace StarRovers::Assembly
{
	class STARROVERS_API FSRAssemblyConveyorDragPathBuilder
	{
	public:
		static const FSRPlanetSurfaceGridCellId& ResolveAnchorCellId(
			const FSRPlanetSurfaceGridCellId& StartCellId,
			const TArray<FSRPlanetSurfaceGridCellId>& WaypointCellIds);

		static bool IsSegmentWithinExtent(
			const FSRPlanetSurfaceGridCellId& StartCellId,
			const FSRPlanetSurfaceGridCellId& EndCellId);

		static bool BuildPath(
			USRPlanetSurfaceGrid* SurfaceGrid,
			USRConveyorNetworkComponent* ConveyorNetwork,
			const FSRAssemblyPlacementDragState& PlacementDrag,
			const FSRStructureData& ConveyorData,
			const FSRPlanetSurfaceGridCellId& TargetCellId,
			TArray<FSRPlanetSurfaceGridCellId>& OutPathCellIds);
	};
}
