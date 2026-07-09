#include "Surface/SRPlanetTerrainGeneratorSampling.h"

namespace StarRovers::Terrain
{
	template <typename TSettings>
	FVector ApplyDomainWarpForSettings(const FVector& LocalUnitDirection, const TSettings& Settings, float Strength)
	{
		const float SafeStrength = FMath::Clamp(Strength, 0.0f, 1.0f);
		if (SafeStrength <= KINDA_SMALL_NUMBER)
		{
			return LocalUnitDirection;
		}

		const float WarpFrequency = FMath::Max(0.01f, Settings.ContinentFrequency * 2.0f);
		const FVector Warp(
			SampleFractalNoiseUnitDirection(LocalUnitDirection, Settings.GenerationSeed + 211, WarpFrequency, 3, 0.5f),
			SampleFractalNoiseUnitDirection(LocalUnitDirection, Settings.GenerationSeed + 223, WarpFrequency, 3, 0.5f),
			SampleFractalNoiseUnitDirection(LocalUnitDirection, Settings.GenerationSeed + 227, WarpFrequency, 3, 0.5f));

		return (LocalUnitDirection + (Warp * SafeStrength * 0.42f)).GetSafeNormal();
	}

	FVector ApplyCompiledDomainWarpForSettings(
		const FVector& LocalUnitDirection,
		const FSRCompiledTerrainNoiseDescriptor (&WarpNoise)[3],
		float SafeStrength)
	{
		if (SafeStrength <= KINDA_SMALL_NUMBER)
		{
			return LocalUnitDirection;
		}

		const FVector Warp(
			SampleFractalNoiseUnitDirection(LocalUnitDirection, WarpNoise[0]),
			SampleFractalNoiseUnitDirection(LocalUnitDirection, WarpNoise[1]),
			SampleFractalNoiseUnitDirection(LocalUnitDirection, WarpNoise[2]));

		return (LocalUnitDirection + (Warp * SafeStrength * 0.42f)).GetSafeNormal();
	}

	template <typename TSettings>
	FVector ApplyClimateDomainWarpForSettings(const FVector& LocalUnitDirection, const TSettings& Settings)
	{
		return ApplyDomainWarpForSettings(LocalUnitDirection, Settings, Settings.NoiseStrength * 0.55f);
	}

	FVector ApplyClimateDomainWarpForSettings(const FVector& LocalUnitDirection, const FSRDynamicMeshGenerationSnapshot& Settings)
	{
		return ApplyCompiledDomainWarpForSettings(LocalUnitDirection, Settings.ClimateWarpNoise, Settings.ClimateWarpStrength);
	}

	template <typename TSettings>
	FVector ApplyTerrainDomainWarpForSettings(const FVector& LocalUnitDirection, const TSettings& Settings)
	{
		return ApplyDomainWarpForSettings(LocalUnitDirection, Settings, Settings.NoiseStrength);
	}

	FVector ApplyTerrainDomainWarpForSettings(const FVector& LocalUnitDirection, const FSRDynamicMeshGenerationSnapshot& Settings)
	{
		return ApplyCompiledDomainWarpForSettings(LocalUnitDirection, Settings.TerrainWarpNoise, Settings.TerrainWarpStrength);
	}

	template <typename TSettings>
	float SampleRiverMaskForSettings(const FVector& LocalUnitDirection, const TSettings& Settings, float LandMask, float MountainMask)
	{
		const float Strength = FMath::Clamp(Settings.RiverStrength, 0.0f, 1.0f);
		if (Strength <= KINDA_SMALL_NUMBER || LandMask <= KINDA_SMALL_NUMBER)
		{
			return 0.0f;
		}

		const float ChannelA = FMath::Abs(SampleFractalNoiseUnitDirection(LocalUnitDirection, Settings.GenerationSeed + 263, Settings.ContinentFrequency * 5.5f, 4, 0.55f));
		const float ChannelB = FMath::Abs(SampleFractalNoiseUnitDirection(LocalUnitDirection, Settings.GenerationSeed + 269, Settings.ContinentFrequency * 9.0f, 3, 0.48f));
		const float LargeChannel = 1.0f - FSRPlanetTerrainGenerator::SmoothStep(0.018f, 0.082f, ChannelA);
		const float Tributary = 1.0f - FSRPlanetTerrainGenerator::SmoothStep(0.012f, 0.052f, ChannelB);
		const float SourceMask = FSRPlanetTerrainGenerator::SmoothStep(0.18f, 0.75f, MountainMask);
		const float RiverMask = FMath::Max(LargeChannel, Tributary * 0.55f) * FMath::Lerp(0.45f, 1.0f, SourceMask) * LandMask;
		return FMath::Clamp(RiverMask * Strength, 0.0f, 1.0f);
	}

	float SampleRiverMaskForSettings(const FVector& LocalUnitDirection, const FSRDynamicMeshGenerationSnapshot& Settings, float LandMask, float MountainMask)
	{
		if (Settings.ClampedRiverStrength <= KINDA_SMALL_NUMBER || LandMask <= KINDA_SMALL_NUMBER)
		{
			return 0.0f;
		}

		const float ChannelA = FMath::Abs(SampleFractalNoiseUnitDirection(LocalUnitDirection, Settings.RiverNoise[0]));
		const float ChannelB = FMath::Abs(SampleFractalNoiseUnitDirection(LocalUnitDirection, Settings.RiverNoise[1]));
		const float LargeChannel = 1.0f - FSRPlanetTerrainGenerator::SmoothStep(0.018f, 0.082f, ChannelA);
		const float Tributary = 1.0f - FSRPlanetTerrainGenerator::SmoothStep(0.012f, 0.052f, ChannelB);
		const float SourceMask = FSRPlanetTerrainGenerator::SmoothStep(0.18f, 0.75f, MountainMask);
		const float RiverMask = FMath::Max(LargeChannel, Tributary * 0.55f) * FMath::Lerp(0.45f, 1.0f, SourceMask) * LandMask;
		return FMath::Clamp(RiverMask * Settings.ClampedRiverStrength, 0.0f, 1.0f);
	}

	template <typename TSettings>
	float SampleLakeMaskForSettings(const FVector& LocalUnitDirection, const TSettings& Settings, float LandMask, float HeightAlpha)
	{
		const float Strength = FMath::Clamp(Settings.LakeStrength, 0.0f, 1.0f);
		if (Strength <= KINDA_SMALL_NUMBER || LandMask <= KINDA_SMALL_NUMBER)
		{
			return 0.0f;
		}

		const float BasinNoiseA = (SampleFractalNoiseUnitDirection(LocalUnitDirection, Settings.GenerationSeed + 277, Settings.ContinentFrequency * 5.2f, 3, 0.42f) + 1.0f) * 0.5f;
		const float BasinNoiseB = FMath::Abs(SampleFractalNoiseUnitDirection(LocalUnitDirection, Settings.GenerationSeed + 283, Settings.ContinentFrequency * 8.5f, 2, 0.36f));
		const float BasinMask = FSRPlanetTerrainGenerator::SmoothStep(0.84f, 0.98f, BasinNoiseA) * FSRPlanetTerrainGenerator::SmoothStep(0.08f, 0.28f, BasinNoiseB);
		const float LowlandMask = 1.0f - FSRPlanetTerrainGenerator::SmoothStep(0.04f, 0.30f, HeightAlpha);
		const float InlandMask = FSRPlanetTerrainGenerator::SmoothStep(0.72f, 0.94f, LandMask);
		return FMath::Clamp(BasinMask * LowlandMask * InlandMask * Strength * 0.45f, 0.0f, 1.0f);
	}

	float SampleLakeMaskForSettings(const FVector& LocalUnitDirection, const FSRDynamicMeshGenerationSnapshot& Settings, float LandMask, float HeightAlpha)
	{
		if (Settings.ClampedLakeStrength <= KINDA_SMALL_NUMBER || LandMask <= KINDA_SMALL_NUMBER)
		{
			return 0.0f;
		}

		const float BasinNoiseA = (SampleFractalNoiseUnitDirection(LocalUnitDirection, Settings.LakeNoise[0]) + 1.0f) * 0.5f;
		const float BasinNoiseB = FMath::Abs(SampleFractalNoiseUnitDirection(LocalUnitDirection, Settings.LakeNoise[1]));
		const float BasinMask = FSRPlanetTerrainGenerator::SmoothStep(0.84f, 0.98f, BasinNoiseA) * FSRPlanetTerrainGenerator::SmoothStep(0.08f, 0.28f, BasinNoiseB);
		const float LowlandMask = 1.0f - FSRPlanetTerrainGenerator::SmoothStep(0.04f, 0.30f, HeightAlpha);
		const float InlandMask = FSRPlanetTerrainGenerator::SmoothStep(0.72f, 0.94f, LandMask);
		return FMath::Clamp(BasinMask * LowlandMask * InlandMask * Settings.ClampedLakeStrength * 0.45f, 0.0f, 1.0f);
	}

	FVector ApplyClimateDomainWarpForSettings(const FVector& LocalUnitDirection, const FSRDynamicMeshGeneration& Settings)
	{
		return ApplyDomainWarpForSettings<FSRDynamicMeshGeneration>(LocalUnitDirection, Settings, Settings.NoiseStrength * 0.55f);
	}

	FVector ApplyTerrainDomainWarpForSettings(const FVector& LocalUnitDirection, const FSRDynamicMeshGeneration& Settings)
	{
		return ApplyDomainWarpForSettings<FSRDynamicMeshGeneration>(LocalUnitDirection, Settings, Settings.NoiseStrength);
	}

	float SampleRiverMaskForSettings(const FVector& LocalUnitDirection, const FSRDynamicMeshGeneration& Settings, float LandMask, float MountainMask)
	{
		return SampleRiverMaskForSettings<FSRDynamicMeshGeneration>(LocalUnitDirection, Settings, LandMask, MountainMask);
	}

	float SampleLakeMaskForSettings(const FVector& LocalUnitDirection, const FSRDynamicMeshGeneration& Settings, float LandMask, float HeightAlpha)
	{
		return SampleLakeMaskForSettings<FSRDynamicMeshGeneration>(LocalUnitDirection, Settings, LandMask, HeightAlpha);
	}
}
