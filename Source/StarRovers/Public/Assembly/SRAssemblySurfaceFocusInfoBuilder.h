#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "UI/SRCelestialBodyFocusInfo.h"

class AActor;
class USRPlanetSurfaceGrid;
struct FSRStructureData;

namespace StarRovers::Assembly
{
	class STARROVERS_API FSRAssemblySurfaceFocusInfoBuilder
	{
	public:
		static bool TryBuildSelectedStructureInfo(
			AActor* FocusedActor,
			USRPlanetSurfaceGrid* SurfaceGrid,
			const FSRPlanetSurfaceGridCell& ClickedCell,
			FSRFocusedSurfaceStructureInfo& OutStructureInfo);

		static void BuildFocusedFacilityPortInfo(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const FSRFocusedSurfaceStructureInfo& StructureInfo,
			const FSRStructureData& StructureData,
			int32 FootprintCellsX,
			int32 FootprintCellsY,
			int32 PlacementRotationSteps,
			TArray<FSRFocusedFacilityPortInfo>& OutFacilityPorts);

		static void GatherFacilityPortPreviewCells(
			const TArray<FSRFocusedFacilityPortInfo>& FacilityPorts,
			TArray<FSRPlanetSurfaceGridCellId>& OutInputConnectionCellIds,
			TArray<FSRPlanetSurfaceGridCellId>& OutOutputConnectionCellIds);
	};
}
