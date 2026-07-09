#pragma once

#include "CoreMinimal.h"
#include "Assembly/SRAssemblyPlacementHistory.h"
#include "Structure/SRStructureInstanceManagerComponent.h"

class USRPlanetSurfaceGrid;
class USRStructureInstanceManagerComponent;

namespace StarRovers::Assembly
{
	STARROVERS_API void AppendRestorableStructures(
		const TArray<FSRPlacedStructureInstance>& RemovedStructures,
		TArray<FSRRestorableNaturalStructure>& OutRestorableStructures);

	STARROVERS_API void RestoreRemovedStructures(
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRStructureInstanceManagerComponent* StructureInstanceManager,
		const TArray<FSRPlacedStructureInstance>& RemovedStructures);
}
