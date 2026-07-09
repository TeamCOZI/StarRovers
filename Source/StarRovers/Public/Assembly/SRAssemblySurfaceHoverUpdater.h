#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"

class ASRPlayerController;
class USRPlanetSurfaceGrid;
struct FSRAssemblySurfaceState;

namespace StarRovers::Assembly
{
	enum class ESRAssemblySurfaceHoverUpdateResult : uint8
	{
		ClearHover,
		ClearHoverPreview,
		NoChange,
		Updated,
	};

	struct FSRAssemblySurfaceHoverUpdate
	{
		USRPlanetSurfaceGrid* SurfaceGrid = nullptr;
		FSRPlanetSurfaceGridCell HoveredCell;
	};

	class STARROVERS_API FSRAssemblySurfaceHoverUpdater
	{
	public:
		static ESRAssemblySurfaceHoverUpdateResult Update(
			ASRPlayerController* PlayerController,
			bool bAssemblyModeActive,
			USRPlanetSurfaceGrid*& HoveredSurfaceGrid,
			FSRAssemblySurfaceState& SurfaceState,
			FSRAssemblySurfaceHoverUpdate& OutUpdate);
	};
}
