#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetTerrainTypes.h"

namespace StarRovers::SurfaceGridTerrainState
{
	FSRDynamicMeshGeneration BuildProceduralTerrainConfig(
		const FSRDynamicMeshGeneration& CurrentSettings,
		bool bNewDynamicMeshGeneration,
		int32 NewGenerationSeed,
		float NewDynamicMeshHeight,
		float NewDetailFrequency,
		int32 NewNoiseOctaves,
		float NewNoisePersistence);

	void ApplyTerrainConfig(
		const FSRDynamicMeshGeneration& NewSettings,
		FSRDynamicMeshGeneration& Settings,
		bool& bCellsDirty,
		bool& bGridMeshDirty);

	float CalculateTerrainHeightStep(
		const FSRDynamicMeshGeneration& Settings,
		float PlanetRadius,
		int32 FaceResolution);

	FSRPlanetTerrainSample SampleTerrainAtDirection(
		FVector LocalUnitDirection,
		const FSRDynamicMeshGeneration& Settings,
		float PlanetRadius,
		int32 FaceResolution);
}
