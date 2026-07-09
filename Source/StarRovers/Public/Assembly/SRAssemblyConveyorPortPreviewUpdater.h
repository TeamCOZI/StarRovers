#pragma once

#include "CoreMinimal.h"

class USRPlanetSurfaceGrid;
class USRStructureDataAsset;
struct FSRAssemblyConveyorPreviewState;

namespace StarRovers::Assembly
{
	class STARROVERS_API FSRAssemblyConveyorPortPreviewUpdater
	{
	public:
		static bool Update(
			USRPlanetSurfaceGrid* HoveredSurfaceGrid,
			USRStructureDataAsset* SelectedStructureDataAsset,
			FSRAssemblyConveyorPreviewState& ConveyorPreview);
	};
}
