#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetTerrainTypes.h"

class STARROVERS_API FSRPlanetTerrainGenerator
{
public:
	static FSRPlanetTerrainSample SampleTerrain(const FVector& LocalUnitDirection, const FSRPlanetTerrainSettings& Settings);

private:
	static FSRPlanetTerrainSample SampleMinecraftOverworldTerrain(const FVector& LocalUnitDirection, const FSRPlanetTerrainSettings& Settings);
	static float ComputeMinecraftPeaksAndValleys(float Weirdness);
	static float SampleFractalNoise(const FVector& LocalUnitDirection, int32 Seed, float Frequency, int32 Octaves, float Persistence);
	static float SampleRidgedNoise(const FVector& LocalUnitDirection, int32 Seed, float Frequency, int32 Octaves);
	static FVector ApplyDomainWarp(const FVector& LocalUnitDirection, const FSRPlanetTerrainSettings& Settings, float Strength);
	static float SampleRiverMask(const FVector& LocalUnitDirection, const FSRPlanetTerrainSettings& Settings, float LandMask, float MountainMask);
	static float SampleLakeMask(const FVector& LocalUnitDirection, const FSRPlanetTerrainSettings& Settings, float LandMask, float HeightAlpha);
	static FLinearColor GetBiomeColor(ESRPlanetBiome Biome, float HeightAlpha, float Moisture, float Temperature);
	static FVector BuildSeedOffset(int32 Seed);
	static float SmoothStep(float Edge0, float Edge1, float Value);
};
