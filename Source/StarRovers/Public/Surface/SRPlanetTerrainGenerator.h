#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetTerrainTypes.h"

class STARROVERS_API FSRPlanetTerrainGenerator
{
public:
	static FSRPlanetTerrainSample SampleTerrain(const FVector& LocalUnitDirection, const FSRDynamicMeshGeneration& Settings);
	static FSRPlanetTerrainSample SampleTerrain(const FSRBiomeSampleContext& Context, const FSRDynamicMeshGeneration& Settings);
	static FSRPlanetTerrainSample SampleTerrain(const FSRBiomeSampleContext& Context, const FSRDynamicMeshGenerationSnapshot& Settings);
	static FSRPlanetTerrainSample SampleDefaultTerrain(const FSRBiomeSampleContext& Context, const FSRDynamicMeshGeneration& Settings);
	static FSRPlanetTerrainSample SampleDefaultTerrain(const FSRBiomeSampleContext& Context, const FSRDynamicMeshGenerationSnapshot& Settings);
	static float SampleFractalNoise(const FVector& LocalUnitDirection, int32 Seed, float Frequency, int32 Octaves, float Persistence);
	static float SampleRidgedNoise(const FVector& LocalUnitDirection, int32 Seed, float Frequency, int32 Octaves);
	static FLinearColor GetBiomeColor(ESRPlanetBiome Biome, float HeightAlpha, float Moisture, float Temperature);
	static float SmoothStep(float Edge0, float Edge1, float Value);
	static bool IsOceanLevelWaterSample(const FSRPlanetTerrainSample& Sample);
	static bool TryResolveOceanLevelHeightOffset(const FSRDynamicMeshGeneration& Settings, int32 SampleCount, float HeightStep, float& OutHeightOffset);
	static bool TryResolveOceanLevelHeightOffset(const FSRDynamicMeshGenerationSnapshot& Settings, int32 SampleCount, float HeightStep, float& OutHeightOffset);
	static void ClampSampleHeightToOceanLevel(FSRPlanetTerrainSample& Sample, float OceanLevelHeightOffset);

private:
	static FSRBiomeSampleContext BuildSampleContextFromDirection(const FVector& LocalUnitDirection);
	static float ComputeMinecraftPeaksAndValleys(float Weirdness);
	static FVector BuildOceanLevelSampleDirection(int32 Index, int32 Count);
	static FVector ApplyDomainWarp(const FVector& LocalUnitDirection, const FSRDynamicMeshGeneration& Settings, float Strength);
	static float SampleRiverMask(const FVector& LocalUnitDirection, const FSRDynamicMeshGeneration& Settings, float LandMask, float MountainMask);
	static float SampleLakeMask(const FVector& LocalUnitDirection, const FSRDynamicMeshGeneration& Settings, float LandMask, float HeightAlpha);
	static FVector BuildSeedOffset(int32 Seed);
};
