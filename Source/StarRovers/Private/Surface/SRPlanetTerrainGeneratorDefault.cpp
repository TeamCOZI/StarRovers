#include "Surface/SRPlanetTerrainGeneratorSampling.h"

#include "Surface/SRPlanetBiomeDataAsset.h"

using namespace StarRovers::Terrain;

namespace StarRovers::Terrain
{
	template <typename TSettings, typename TSelectBiome>
	FSRPlanetTerrainSample SampleDefaultTerrainForSettings(
	const FSRBiomeSampleContext& Context,
	const TSettings& Settings,
	TSelectBiome SelectBiome)
	{
		FSRPlanetTerrainSample Sample;

		const FVector Direction = Context.LocalUnitDirection.GetSafeNormal();
		if (Direction.IsNearlyZero())
		{
			return Sample;
		}

		const int32 SafeOctaves = GetSafeNoiseOctavesForSettings(Settings);
		const float SafePersistence = GetSafeNoisePersistenceForSettings(Settings);
		const float SafeDynamicMeshHeight = GetSafeDynamicMeshHeightForSettings(Settings);
		const float InvSafeDynamicMeshHeight = GetInvSafeDynamicMeshHeightForSettings(Settings);
		const FVector ClimateDirection = ApplyClimateDomainWarpForSettings(Direction, Settings);
		const FVector TerrainDirection = ApplyTerrainDomainWarpForSettings(Direction, Settings);

		const float Continentalness = SampleContinentalnessForSettings(ClimateDirection, Settings, SafeOctaves);
		const float ErosionNoise = SampleErosionForSettings(ClimateDirection, Settings, SafeOctaves);
		const float Weirdness = SampleWeirdnessForSettings(TerrainDirection, Settings, SafeOctaves);
		const float Ridges = SampleRidgesForSettings(TerrainDirection, Settings, SafeOctaves);
		const float Detail = SampleDetailForSettings(TerrainDirection, Settings, SafeOctaves, SafePersistence);
		const float TemperatureNoise = SampleTemperatureForSettings(ClimateDirection, Settings);
		const float HumidityNoise = SampleHumidityForSettings(ClimateDirection, Settings);

		const float ContinentalnessBias = 0.18f;
		const float EffectiveContinentalness = FMath::Clamp(Continentalness + ContinentalnessBias - (Settings.OceanThreshold * 0.62f), -1.0f, 1.0f);
		const float Erosion = FMath::Clamp((ErosionNoise + 1.0f) * 0.5f, 0.0f, 1.0f);
		const float PeaksAndValleys = ComputeMinecraftPeaksAndValleysForSettings(Weirdness);
		const float LandMask = FSRPlanetTerrainGenerator::SmoothStep(-0.12f, 0.02f, EffectiveContinentalness);
		const float CoastMask = 1.0f - FMath::Abs(FMath::Clamp(EffectiveContinentalness / 0.14f, -1.0f, 1.0f));
		const float OceanDepthMask = 1.0f - FSRPlanetTerrainGenerator::SmoothStep(-0.58f, -0.12f, EffectiveContinentalness);
		const float InlandMask = FSRPlanetTerrainGenerator::SmoothStep(0.00f, 0.36f, EffectiveContinentalness);
		const float MountainSuppressionByErosion = 1.0f - FSRPlanetTerrainGenerator::SmoothStep(0.36f, 0.86f, Erosion);
		const float MountainPotential = FMath::Clamp(
		FMath::Max(FSRPlanetTerrainGenerator::SmoothStep(0.18f, 0.90f, PeaksAndValleys), Ridges * 0.72f)
		* MountainSuppressionByErosion
		* LandMask,
		0.0f,
		1.0f);
		const float ValleyMask = FSRPlanetTerrainGenerator::SmoothStep(-0.95f, -0.18f, -PeaksAndValleys) * LandMask;
		const float PlateauMask = FSRPlanetTerrainGenerator::SmoothStep(0.18f, 0.72f, EffectiveContinentalness) * FSRPlanetTerrainGenerator::SmoothStep(0.28f, 0.68f, Erosion);

		const float OceanFloorHeight = -OceanDepthMask * OceanDepthMask * SafeDynamicMeshHeight * 0.42f;
		const float LandLift = LandMask * SafeDynamicMeshHeight * 0.10f;
		const float CoastalShelfHeight = CoastMask * SafeDynamicMeshHeight * 0.08f;
		const float PlainsHeight = LandMask * SafeDynamicMeshHeight * FMath::Lerp(0.14f, 0.28f, InlandMask) * FSRPlanetTerrainGenerator::SmoothStep(0.12f, 0.78f, Erosion);
		const float PlateauHeight = PlateauMask * SafeDynamicMeshHeight * 0.24f;
		const float MountainHeight = MountainPotential
		* SafeDynamicMeshHeight
		* FMath::Lerp(0.48f, 0.92f, FMath::Clamp(PeaksAndValleys, 0.0f, 1.0f))
		* GetMountainHeightStrengthScaleForSettings(Settings);
		const float ValleyCarve = ValleyMask * SafeDynamicMeshHeight * FMath::Lerp(0.10f, 0.31f, 1.0f - Erosion) * GetClampedValleyStrengthForSettings(Settings);
		const float DetailHeight = Detail * SafeDynamicMeshHeight * 0.035f * GetClampedDetailStrengthForSettings(Settings) * FMath::Lerp(0.35f, 1.0f, LandMask);
		const float RidgeBonus = FMath::Square(Ridges) * MountainPotential * SafeDynamicMeshHeight * 0.16f;

		const float HeightBeforeSurfaceRules = OceanFloorHeight + LandLift + CoastalShelfHeight + PlainsHeight + PlateauHeight + MountainHeight + RidgeBonus + DetailHeight - ValleyCarve;
		const float HeightAlphaBeforeRules = FMath::Clamp(HeightBeforeSurfaceRules * InvSafeDynamicMeshHeight, -1.0f, 1.0f);
		const float RiverMask = SampleRiverMaskForSettings(TerrainDirection, Settings, LandMask, MountainPotential);
		const float LakeMask = SampleLakeMaskForSettings(TerrainDirection, Settings, LandMask, HeightAlphaBeforeRules);
		const float SurfaceRuleCarve = ((RiverMask * 0.13f) + (LakeMask * 0.08f)) * SafeDynamicMeshHeight;

		Sample.HeightOffset = HeightBeforeSurfaceRules - SurfaceRuleCarve;
		Sample.Continent = EffectiveContinentalness;
		Sample.MountainMask = MountainPotential;
		Sample.RiverMask = RiverMask;
		Sample.LakeMask = LakeMask;
		Sample.PlateBeltMask = Ridges * MountainPotential;

		const float LatitudeTemperature = 1.0f - FMath::Abs(Direction.Z);
		const float HeightTemperaturePenalty = FMath::Max(0.0f, Sample.HeightOffset * InvSafeDynamicMeshHeight) * 0.28f;
		Sample.Temperature = FMath::Clamp((LatitudeTemperature * 0.78f) + (TemperatureNoise * 0.18f) + 0.11f - HeightTemperaturePenalty, 0.0f, 1.0f);
		Sample.Moisture = FMath::Clamp((HumidityNoise + 1.0f) * 0.5f, 0.0f, 1.0f);

		const float HeightAlpha = FMath::Clamp(Sample.HeightOffset * InvSafeDynamicMeshHeight, -1.0f, 1.0f);
		const float AbsLatitudeSin = static_cast<float>(FMath::Clamp(FMath::Abs(Direction.Z), 0.0, 1.0));
		FSRDefaultBiomeMetrics Metrics;
		Metrics.HeightAlpha = HeightAlpha;
		Metrics.Continentalness = EffectiveContinentalness;
		Metrics.Erosion = Erosion;
		Metrics.Weirdness = Weirdness;
		Metrics.LandMask = LandMask;
		Metrics.CoastMask = CoastMask;
		Metrics.OceanDepthMask = OceanDepthMask;
		Metrics.InlandMask = InlandMask;
		Metrics.MountainMask = MountainPotential;
		Metrics.RiverMask = RiverMask;
		Metrics.LakeMask = LakeMask;
		Metrics.Temperature = Sample.Temperature;
		Metrics.Moisture = Sample.Moisture;
		Metrics.AbsLatitudeDegrees = FMath::RadiansToDegrees(FMath::Asin(AbsLatitudeSin));
		Metrics.RareRegionNoise = ShouldSampleRareRegionNoise(Settings)
		? FMath::Clamp((SampleRareRegionForSettings(Direction, Settings) + 1.0f) * 0.5f, 0.0f, 1.0f)
		: 0.0f;

		if (const auto* BiomeDataAsset = SelectBiome(Context, Settings, Metrics, Direction))
		{
			Sample.Biome = GetRuntimeBiomeForBiomeDataAsset(*BiomeDataAsset);
			Sample.BiomeId = BiomeDataAsset->BiomeId;
			Sample.WaterRole = GetWaterRoleForBiomeDataAsset(*BiomeDataAsset);
			Sample.SurfaceColor = GetBiomeDataAssetColor(*BiomeDataAsset, HeightAlpha, Sample.Moisture, Sample.Temperature);
		}
		else
		{
			Sample.Biome = ESRPlanetBiome::Plains;
			Sample.BiomeId = FName(TEXT("Plains"));
			Sample.WaterRole = ESRBiomeWaterRole::None;
			Sample.SurfaceColor = FSRPlanetTerrainGenerator::GetBiomeColor(ESRPlanetBiome::Plains, HeightAlpha, Sample.Moisture, Sample.Temperature);
			LogNoMatchingBiomeOnce(Settings, Context);
		}

		if (Sample.WaterRole == ESRBiomeWaterRole::Coast && (RiverMask > 0.58f || LakeMask > 0.38f))
		{
			const float WaterMask = FMath::Clamp(FMath::Max(RiverMask, LakeMask), 0.0f, 1.0f);
			Sample.SurfaceColor = FLinearColor::LerpUsingHSV(Sample.SurfaceColor, FLinearColor(0.03f, 0.18f, 0.34f, 1.0f), WaterMask * 0.74f);
		}

		Sample.SurfaceColor.A = 1.0f;
		return Sample;
	}
}

FSRPlanetTerrainSample FSRPlanetTerrainGenerator::SampleDefaultTerrain(const FSRBiomeSampleContext& Context, const FSRDynamicMeshGeneration& Settings)
{
	return SampleDefaultTerrainForSettings(Context, Settings, SelectBiomeDataAsset);
}

FSRPlanetTerrainSample FSRPlanetTerrainGenerator::SampleDefaultTerrain(const FSRBiomeSampleContext& Context, const FSRDynamicMeshGenerationSnapshot& Settings)
{
	return SampleDefaultTerrainForSettings(Context, Settings, SelectBiomeDataSnapshot);
}
