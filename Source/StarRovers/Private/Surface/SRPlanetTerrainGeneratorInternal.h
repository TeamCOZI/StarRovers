#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetTerrainGenerator.h"

class USRPlanetBiomeDataAsset;

namespace StarRovers::Terrain
{
	struct FSRDefaultBiomeMetrics
	{
		float HeightAlpha = 0.0f;
		float Continentalness = 0.0f;
		float Erosion = 0.0f;
		float Weirdness = 0.0f;
		float LandMask = 0.0f;
		float CoastMask = 0.0f;
		float OceanDepthMask = 0.0f;
		float InlandMask = 0.0f;
		float MountainMask = 0.0f;
		float RiverMask = 0.0f;
		float LakeMask = 0.0f;
		float Temperature = 0.0f;
		float Moisture = 0.0f;
		float AbsLatitudeDegrees = 0.0f;
		float RareRegionNoise = 0.0f;
	};

	uint32 HashBiomeValue(FName BiomeId, int32 Salt);
	float HashBiomeUnit(FName BiomeId, int32 Salt);

	void LogNoMatchingBiomeOnce(const FSRDynamicMeshGeneration& Settings, const FSRBiomeSampleContext& Context);
	void LogNoMatchingBiomeOnce(const FSRDynamicMeshGenerationSnapshot& Settings, const FSRBiomeSampleContext& Context);

	FVector BuildNoiseSeedOffset(int32 Seed);
	float SampleFractalNoiseUnitDirection(const FVector& UnitDirection, int32 Seed, float Frequency, int32 Octaves, float Persistence);
	float SampleFractalNoiseUnitDirection(const FVector& UnitDirection, const FSRCompiledTerrainNoiseDescriptor& Noise);
	float SampleFractalNoiseUnitDirection(const FVector& UnitDirection, const FVector& SeedOffset, float Frequency, int32 Octaves, float Persistence);
	float SampleRidgedNoiseUnitDirection(const FVector& UnitDirection, int32 Seed, float Frequency, int32 Octaves);
	float SampleRidgedNoiseUnitDirection(const FVector& UnitDirection, const FSRCompiledTerrainNoiseDescriptor& Noise);

	int32 GetSafeNoiseOctavesForSettings(const FSRDynamicMeshGeneration& Settings);
	int32 GetSafeNoiseOctavesForSettings(const FSRDynamicMeshGenerationSnapshot& Settings);
	float GetSafeNoisePersistenceForSettings(const FSRDynamicMeshGeneration& Settings);
	float GetSafeNoisePersistenceForSettings(const FSRDynamicMeshGenerationSnapshot& Settings);
	float GetSafeDynamicMeshHeightForSettings(const FSRDynamicMeshGeneration& Settings);
	float GetSafeDynamicMeshHeightForSettings(const FSRDynamicMeshGenerationSnapshot& Settings);
	float GetInvSafeDynamicMeshHeightForSettings(const FSRDynamicMeshGeneration& Settings);
	float GetInvSafeDynamicMeshHeightForSettings(const FSRDynamicMeshGenerationSnapshot& Settings);
	float GetMountainHeightStrengthScaleForSettings(const FSRDynamicMeshGeneration& Settings);
	float GetMountainHeightStrengthScaleForSettings(const FSRDynamicMeshGenerationSnapshot& Settings);
	float GetClampedValleyStrengthForSettings(const FSRDynamicMeshGeneration& Settings);
	float GetClampedValleyStrengthForSettings(const FSRDynamicMeshGenerationSnapshot& Settings);
	float GetClampedDetailStrengthForSettings(const FSRDynamicMeshGeneration& Settings);
	float GetClampedDetailStrengthForSettings(const FSRDynamicMeshGenerationSnapshot& Settings);

	FVector ApplyClimateDomainWarpForSettings(const FVector& LocalUnitDirection, const FSRDynamicMeshGeneration& Settings);
	FVector ApplyClimateDomainWarpForSettings(const FVector& LocalUnitDirection, const FSRDynamicMeshGenerationSnapshot& Settings);
	FVector ApplyTerrainDomainWarpForSettings(const FVector& LocalUnitDirection, const FSRDynamicMeshGeneration& Settings);
	FVector ApplyTerrainDomainWarpForSettings(const FVector& LocalUnitDirection, const FSRDynamicMeshGenerationSnapshot& Settings);
	float SampleRiverMaskForSettings(const FVector& LocalUnitDirection, const FSRDynamicMeshGeneration& Settings, float LandMask, float MountainMask);
	float SampleRiverMaskForSettings(const FVector& LocalUnitDirection, const FSRDynamicMeshGenerationSnapshot& Settings, float LandMask, float MountainMask);
	float SampleLakeMaskForSettings(const FVector& LocalUnitDirection, const FSRDynamicMeshGeneration& Settings, float LandMask, float HeightAlpha);
	float SampleLakeMaskForSettings(const FVector& LocalUnitDirection, const FSRDynamicMeshGenerationSnapshot& Settings, float LandMask, float HeightAlpha);

	float ComputeMinecraftPeaksAndValleysForSettings(float Weirdness);
	bool ShouldSampleRareRegionNoise(const FSRDynamicMeshGeneration& Settings);
	bool ShouldSampleRareRegionNoise(const FSRDynamicMeshGenerationSnapshot& Settings);
	float SampleContinentalnessForSettings(const FVector& Direction, const FSRDynamicMeshGeneration& Settings, int32 SafeOctaves);
	float SampleContinentalnessForSettings(const FVector& Direction, const FSRDynamicMeshGenerationSnapshot& Settings, int32 SafeOctaves);
	float SampleErosionForSettings(const FVector& Direction, const FSRDynamicMeshGeneration& Settings, int32 SafeOctaves);
	float SampleErosionForSettings(const FVector& Direction, const FSRDynamicMeshGenerationSnapshot& Settings, int32 SafeOctaves);
	float SampleWeirdnessForSettings(const FVector& Direction, const FSRDynamicMeshGeneration& Settings, int32 SafeOctaves);
	float SampleWeirdnessForSettings(const FVector& Direction, const FSRDynamicMeshGenerationSnapshot& Settings, int32 SafeOctaves);
	float SampleRidgesForSettings(const FVector& Direction, const FSRDynamicMeshGeneration& Settings, int32 SafeOctaves);
	float SampleRidgesForSettings(const FVector& Direction, const FSRDynamicMeshGenerationSnapshot& Settings, int32 SafeOctaves);
	float SampleDetailForSettings(const FVector& Direction, const FSRDynamicMeshGeneration& Settings, int32 SafeOctaves, float SafePersistence);
	float SampleDetailForSettings(const FVector& Direction, const FSRDynamicMeshGenerationSnapshot& Settings, int32 SafeOctaves, float SafePersistence);
	float SampleTemperatureForSettings(const FVector& Direction, const FSRDynamicMeshGeneration& Settings);
	float SampleTemperatureForSettings(const FVector& Direction, const FSRDynamicMeshGenerationSnapshot& Settings);
	float SampleHumidityForSettings(const FVector& Direction, const FSRDynamicMeshGeneration& Settings);
	float SampleHumidityForSettings(const FVector& Direction, const FSRDynamicMeshGenerationSnapshot& Settings);
	float SampleRareRegionForSettings(const FVector& Direction, const FSRDynamicMeshGeneration& Settings);
	float SampleRareRegionForSettings(const FVector& Direction, const FSRDynamicMeshGenerationSnapshot& Settings);

	const USRPlanetBiomeDataAsset* SelectBiomeDataAsset(
	const FSRBiomeSampleContext& Context,
	const FSRDynamicMeshGeneration& Settings,
	const FSRDefaultBiomeMetrics& Metrics,
	const FVector& UnitDirection);
	const FSRCompiledPlanetBiomeGenerationSnapshot* SelectBiomeDataSnapshot(
	const FSRBiomeSampleContext& Context,
	const FSRDynamicMeshGenerationSnapshot& Settings,
	const FSRDefaultBiomeMetrics& Metrics,
	const FVector& UnitDirection);

	ESRBiomeWaterRole GetWaterRoleForBiomeDataAsset(const USRPlanetBiomeDataAsset& BiomeDataAsset);
	ESRBiomeWaterRole GetWaterRoleForBiomeDataAsset(const FSRCompiledPlanetBiomeGenerationSnapshot& BiomeDataAsset);
	ESRPlanetBiome GetRuntimeBiomeForBiomeDataAsset(const USRPlanetBiomeDataAsset& BiomeDataAsset);
	ESRPlanetBiome GetRuntimeBiomeForBiomeDataAsset(const FSRCompiledPlanetBiomeGenerationSnapshot& BiomeDataAsset);
	FLinearColor GetBiomeDataAssetColor(const USRPlanetBiomeDataAsset& BiomeDataAsset, float HeightAlpha, float Moisture, float Temperature);
	FLinearColor GetBiomeDataAssetColor(const FSRCompiledPlanetBiomeGenerationSnapshot& BiomeDataAsset, float HeightAlpha, float Moisture, float Temperature);
}
