#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "Surface/SRPlanetTerrainTypes.h"
#include "Templates/Function.h"

namespace StarRovers::SurfaceGridDefaultCells
{
	using FTerrainSampleQuery = TFunctionRef<FSRPlanetTerrainSample(const FVector& LocalUnitDirection)>;
	using FTemperatureStateResolver = TFunctionRef<ESRFacilityTemperatureState(float SurfaceTemperature)>;

	TArray<FSRPlanetSurfaceGridCell> BuildCubeSphereCells(
		int32 FaceResolution,
		float PlanetRadius,
		FTerrainSampleQuery GetTerrainSample,
		FTemperatureStateResolver ResolveTemperatureState);
}
