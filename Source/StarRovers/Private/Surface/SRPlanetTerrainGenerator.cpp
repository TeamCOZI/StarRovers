#include "Surface/SRPlanetTerrainGenerator.h"

FSRPlanetTerrainSample FSRPlanetTerrainGenerator::SampleTerrain(const FVector& LocalUnitDirection, const FSRPlanetTerrainSettings& Settings)
{
	if (!Settings.bUseProceduralTerrain || Settings.TerrainHeight <= KINDA_SMALL_NUMBER)
	{
		FSRPlanetTerrainSample Sample;
		Sample.Biome = ESRPlanetBiome::Plains;
		return Sample;
	}

	switch (Settings.TerrainProfile)
	{
	case ESRPlanetTerrainProfile::None:
	case ESRPlanetTerrainProfile::GasGiant:
		return FSRPlanetTerrainSample();
	default:
		return SampleMinecraftOverworldTerrain(LocalUnitDirection, Settings);
	}
}

FSRPlanetTerrainSample FSRPlanetTerrainGenerator::SampleMinecraftOverworldTerrain(const FVector& LocalUnitDirection, const FSRPlanetTerrainSettings& Settings)
{
	FSRPlanetTerrainSample Sample;

	const FVector Direction = LocalUnitDirection.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return Sample;
	}

	const int32 SafeOctaves = FMath::Clamp(Settings.TerrainOctaves, 1, 8);
	const float SafePersistence = FMath::Clamp(Settings.TerrainPersistence, 0.0f, 1.0f);
	const float SafeTerrainHeight = FMath::Max(0.0f, Settings.TerrainHeight);
	const FVector ClimateDirection = ApplyDomainWarp(Direction, Settings, Settings.DomainWarpStrength * 0.55f);
	const FVector TerrainDirection = ApplyDomainWarp(Direction, Settings, Settings.DomainWarpStrength);

	const float Continentalness = SampleFractalNoise(
		ClimateDirection,
		Settings.TerrainSeed + 10001,
		FMath::Max(0.01f, Settings.ContinentFrequency * 0.58f),
		FMath::Max(3, SafeOctaves),
		0.56f);
	const float ErosionNoise = SampleFractalNoise(
		ClimateDirection,
		Settings.TerrainSeed + 10037,
		FMath::Max(0.01f, Settings.ContinentFrequency * 1.18f),
		FMath::Max(3, SafeOctaves - 1),
		0.52f);
	const float Weirdness = SampleFractalNoise(
		TerrainDirection,
		Settings.TerrainSeed + 10061,
		FMath::Max(0.01f, Settings.MountainFrequency * 0.42f),
		FMath::Max(3, SafeOctaves),
		0.50f);
	const float Ridges = SampleRidgedNoise(
		TerrainDirection,
		Settings.TerrainSeed + 10091,
		FMath::Max(0.01f, Settings.MountainFrequency * 0.72f),
		FMath::Max(3, SafeOctaves - 1));
	const float Detail = SampleFractalNoise(
		TerrainDirection,
		Settings.TerrainSeed + 10111,
		FMath::Max(0.01f, Settings.DetailFrequency),
		FMath::Max(2, SafeOctaves - 2),
		SafePersistence);

	const float TemperatureNoise = SampleFractalNoise(
		ClimateDirection,
		Settings.TerrainSeed + 10141,
		FMath::Max(0.01f, Settings.TemperatureNoiseFrequency),
		3,
		0.5f);
	const float HumidityNoise = SampleFractalNoise(
		ClimateDirection,
		Settings.TerrainSeed + 10163,
		FMath::Max(0.01f, Settings.MoistureFrequency),
		3,
		0.5f);

	const float ContinentalnessBias = Settings.TerrainProfile == ESRPlanetTerrainProfile::OceanWorld ? -0.18f : 0.18f;
	const float EffectiveContinentalness = FMath::Clamp(Continentalness + ContinentalnessBias - (Settings.SeaLevel * 0.62f), -1.0f, 1.0f);
	const float Erosion = FMath::Clamp((ErosionNoise + 1.0f) * 0.5f, 0.0f, 1.0f);
	const float PeaksAndValleys = ComputeMinecraftPeaksAndValleys(Weirdness);
	const float LandMask = SmoothStep(-0.12f, 0.02f, EffectiveContinentalness);
	const float CoastMask = 1.0f - FMath::Abs(FMath::Clamp(EffectiveContinentalness / 0.14f, -1.0f, 1.0f));
	const float OceanDepthMask = 1.0f - SmoothStep(-0.58f, -0.12f, EffectiveContinentalness);
	const float InlandMask = SmoothStep(0.00f, 0.36f, EffectiveContinentalness);
	const float MountainSuppressionByErosion = 1.0f - SmoothStep(0.36f, 0.86f, Erosion);
	const float MountainPotential = FMath::Clamp(
		FMath::Max(SmoothStep(0.18f, 0.90f, PeaksAndValleys), Ridges * 0.72f)
		* MountainSuppressionByErosion
		* LandMask,
		0.0f,
		1.0f);
	const float ValleyMask = SmoothStep(-0.95f, -0.18f, -PeaksAndValleys) * LandMask;
	const float PlateauMask = SmoothStep(0.18f, 0.72f, EffectiveContinentalness) * SmoothStep(0.28f, 0.68f, Erosion);

	const float OceanFloorHeight = -OceanDepthMask * OceanDepthMask * SafeTerrainHeight * 0.42f;
	const float LandLift = LandMask * SafeTerrainHeight * 0.10f;
	const float CoastalShelfHeight = CoastMask * SafeTerrainHeight * 0.08f;
	const float PlainsHeight = LandMask * SafeTerrainHeight * FMath::Lerp(0.14f, 0.28f, InlandMask) * SmoothStep(0.12f, 0.78f, Erosion);
	const float PlateauHeight = PlateauMask * SafeTerrainHeight * 0.24f;
	const float MountainHeight = MountainPotential
		* SafeTerrainHeight
		* FMath::Lerp(0.48f, 0.92f, FMath::Clamp(PeaksAndValleys, 0.0f, 1.0f))
		* FMath::Pow(FMath::Clamp(Settings.MountainSharpness / 2.0f, 0.25f, 2.0f), 0.45f);
	const float ValleyCarve = ValleyMask * SafeTerrainHeight * FMath::Lerp(0.10f, 0.31f, 1.0f - Erosion) * FMath::Clamp(Settings.ValleyStrength, 0.0f, 1.0f);
	const float DetailHeight = Detail * SafeTerrainHeight * 0.035f * FMath::Clamp(Settings.MicroDetailStrength, 0.0f, 1.0f) * FMath::Lerp(0.35f, 1.0f, LandMask);
	const float RidgeBonus = FMath::Square(Ridges) * MountainPotential * SafeTerrainHeight * 0.16f;

	const float HeightBeforeSurfaceRules = OceanFloorHeight + LandLift + CoastalShelfHeight + PlainsHeight + PlateauHeight + MountainHeight + RidgeBonus + DetailHeight - ValleyCarve;
	const float HeightAlphaBeforeRules = FMath::Clamp(HeightBeforeSurfaceRules / FMath::Max(SafeTerrainHeight, KINDA_SMALL_NUMBER), -1.0f, 1.0f);
	const float RiverMask = SampleRiverMask(TerrainDirection, Settings, LandMask, MountainPotential);
	const float LakeMask = SampleLakeMask(TerrainDirection, Settings, LandMask, HeightAlphaBeforeRules);
	const float SurfaceRuleCarve = ((RiverMask * 0.13f) + (LakeMask * 0.08f)) * SafeTerrainHeight;

	Sample.HeightOffset = HeightBeforeSurfaceRules - SurfaceRuleCarve;
	Sample.Continent = EffectiveContinentalness;
	Sample.MountainMask = MountainPotential;
	Sample.RiverMask = RiverMask;
	Sample.LakeMask = LakeMask;
	Sample.PlateBeltMask = Ridges * MountainPotential;

	const float LatitudeTemperature = 1.0f - FMath::Abs(Direction.Z);
	const float HeightTemperaturePenalty = FMath::Max(0.0f, Sample.HeightOffset / FMath::Max(SafeTerrainHeight, KINDA_SMALL_NUMBER)) * 0.28f;
	Sample.Temperature = FMath::Clamp((LatitudeTemperature * 0.78f) + (TemperatureNoise * 0.18f) + 0.11f - HeightTemperaturePenalty, 0.0f, 1.0f);
	Sample.Moisture = FMath::Clamp((HumidityNoise + 1.0f) * 0.5f, 0.0f, 1.0f);

	const float HeightAlpha = FMath::Clamp(Sample.HeightOffset / FMath::Max(SafeTerrainHeight, KINDA_SMALL_NUMBER), -1.0f, 1.0f);
	const bool bOcean = EffectiveContinentalness < -0.24f || LandMask < 0.18f;
	const bool bCoast = !bOcean && EffectiveContinentalness < 0.07f;
	const bool bPeak = MountainPotential > 0.58f && HeightAlpha > 0.24f;
	const bool bSnowLine = Sample.Temperature < 0.24f || HeightAlpha > 0.48f;

	if (bOcean)
	{
		Sample.Biome = OceanDepthMask > 0.62f ? ESRPlanetBiome::Ocean : ESRPlanetBiome::Coast;
	}
	else if (bCoast || RiverMask > 0.58f || LakeMask > 0.38f)
	{
		Sample.Biome = ESRPlanetBiome::Coast;
	}
	else if (bPeak)
	{
		Sample.Biome = bSnowLine ? ESRPlanetBiome::Snow : ESRPlanetBiome::Mountain;
	}
	else if (Sample.Temperature < 0.16f)
	{
		Sample.Biome = ESRPlanetBiome::Ice;
	}
	else if (Sample.Temperature > 0.62f && Sample.Moisture < 0.30f)
	{
		Sample.Biome = ESRPlanetBiome::Desert;
	}
	else if (Sample.Moisture > 0.58f)
	{
		Sample.Biome = ESRPlanetBiome::Forest;
	}
	else
	{
		Sample.Biome = ESRPlanetBiome::Plains;
	}

	Sample.SurfaceColor = GetBiomeColor(Sample.Biome, HeightAlpha, Sample.Moisture, Sample.Temperature);
	if (Sample.Biome == ESRPlanetBiome::Coast && (RiverMask > 0.58f || LakeMask > 0.38f))
	{
		const float WaterMask = FMath::Clamp(FMath::Max(RiverMask, LakeMask), 0.0f, 1.0f);
		Sample.SurfaceColor = FLinearColor::LerpUsingHSV(Sample.SurfaceColor, FLinearColor(0.03f, 0.18f, 0.34f, 1.0f), WaterMask * 0.74f);
	}

	Sample.SurfaceColor.A = 1.0f;
	return Sample;
}

float FSRPlanetTerrainGenerator::ComputeMinecraftPeaksAndValleys(float Weirdness)
{
	const float AbsWeirdness = FMath::Abs(FMath::Clamp(Weirdness, -1.0f, 1.0f));
	return -(FMath::Abs(AbsWeirdness - (2.0f / 3.0f)) - (1.0f / 3.0f)) * 3.0f;
}

float FSRPlanetTerrainGenerator::SampleFractalNoise(const FVector& LocalUnitDirection, int32 Seed, float Frequency, int32 Octaves, float Persistence)
{
	const FVector Direction = LocalUnitDirection.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return 0.0f;
	}

	float CurrentFrequency = FMath::Max(0.01f, Frequency);
	float CurrentAmplitude = 1.0f;
	float NoiseSum = 0.0f;
	float AmplitudeSum = 0.0f;
	const FVector SeedOffset = BuildSeedOffset(Seed);
	const int32 SafeOctaves = FMath::Clamp(Octaves, 1, 8);
	const float SafePersistence = FMath::Clamp(Persistence, 0.0f, 1.0f);

	for (int32 OctaveIndex = 0; OctaveIndex < SafeOctaves; ++OctaveIndex)
	{
		const float NoiseValue = FMath::PerlinNoise3D((Direction * CurrentFrequency) + SeedOffset);
		NoiseSum += NoiseValue * CurrentAmplitude;
		AmplitudeSum += CurrentAmplitude;
		CurrentFrequency *= 2.0f;
		CurrentAmplitude *= SafePersistence;
	}

	return AmplitudeSum > KINDA_SMALL_NUMBER ? NoiseSum / AmplitudeSum : 0.0f;
}

float FSRPlanetTerrainGenerator::SampleRidgedNoise(const FVector& LocalUnitDirection, int32 Seed, float Frequency, int32 Octaves)
{
	const FVector Direction = LocalUnitDirection.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return 0.0f;
	}

	float CurrentFrequency = FMath::Max(0.01f, Frequency);
	float CurrentAmplitude = 1.0f;
	float NoiseSum = 0.0f;
	float AmplitudeSum = 0.0f;
	const FVector SeedOffset = BuildSeedOffset(Seed);
	const int32 SafeOctaves = FMath::Clamp(Octaves, 1, 8);

	for (int32 OctaveIndex = 0; OctaveIndex < SafeOctaves; ++OctaveIndex)
	{
		const float NoiseValue = FMath::PerlinNoise3D((Direction * CurrentFrequency) + SeedOffset);
		const float RidgedValue = 1.0f - FMath::Abs(NoiseValue);
		NoiseSum += FMath::Clamp(RidgedValue, 0.0f, 1.0f) * CurrentAmplitude;
		AmplitudeSum += CurrentAmplitude;
		CurrentFrequency *= 2.0f;
		CurrentAmplitude *= 0.5f;
	}

	return AmplitudeSum > KINDA_SMALL_NUMBER ? NoiseSum / AmplitudeSum : 0.0f;
}

FVector FSRPlanetTerrainGenerator::ApplyDomainWarp(const FVector& LocalUnitDirection, const FSRPlanetTerrainSettings& Settings, float Strength)
{
	const FVector Direction = LocalUnitDirection.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return FVector::ForwardVector;
	}

	const float SafeStrength = FMath::Clamp(Strength, 0.0f, 1.0f);
	if (SafeStrength <= KINDA_SMALL_NUMBER)
	{
		return Direction;
	}

	const float WarpFrequency = FMath::Max(0.01f, Settings.ContinentFrequency * 2.0f);
	const FVector Warp(
		SampleFractalNoise(Direction, Settings.TerrainSeed + 211, WarpFrequency, 3, 0.5f),
		SampleFractalNoise(Direction, Settings.TerrainSeed + 223, WarpFrequency, 3, 0.5f),
		SampleFractalNoise(Direction, Settings.TerrainSeed + 227, WarpFrequency, 3, 0.5f));

	return (Direction + (Warp * SafeStrength * 0.42f)).GetSafeNormal();
}

float FSRPlanetTerrainGenerator::SampleRiverMask(const FVector& LocalUnitDirection, const FSRPlanetTerrainSettings& Settings, float LandMask, float MountainMask)
{
	const float Strength = FMath::Clamp(Settings.RiverStrength, 0.0f, 1.0f);
	if (Strength <= KINDA_SMALL_NUMBER || LandMask <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const FVector Direction = LocalUnitDirection.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return 0.0f;
	}

	const float ChannelA = FMath::Abs(SampleFractalNoise(Direction, Settings.TerrainSeed + 263, Settings.ContinentFrequency * 5.5f, 4, 0.55f));
	const float ChannelB = FMath::Abs(SampleFractalNoise(Direction, Settings.TerrainSeed + 269, Settings.ContinentFrequency * 9.0f, 3, 0.48f));
	const float LargeChannel = 1.0f - SmoothStep(0.018f, 0.082f, ChannelA);
	const float Tributary = 1.0f - SmoothStep(0.012f, 0.052f, ChannelB);
	const float SourceMask = SmoothStep(0.18f, 0.75f, MountainMask);
	const float RiverMask = FMath::Max(LargeChannel, Tributary * 0.55f) * FMath::Lerp(0.45f, 1.0f, SourceMask) * LandMask;
	return FMath::Clamp(RiverMask * Strength, 0.0f, 1.0f);
}

float FSRPlanetTerrainGenerator::SampleLakeMask(const FVector& LocalUnitDirection, const FSRPlanetTerrainSettings& Settings, float LandMask, float HeightAlpha)
{
	const float Strength = FMath::Clamp(Settings.LakeStrength, 0.0f, 1.0f);
	if (Strength <= KINDA_SMALL_NUMBER || LandMask <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const FVector Direction = LocalUnitDirection.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return 0.0f;
	}

	const float BasinNoiseA = (SampleFractalNoise(Direction, Settings.TerrainSeed + 277, Settings.ContinentFrequency * 5.2f, 3, 0.42f) + 1.0f) * 0.5f;
	const float BasinNoiseB = FMath::Abs(SampleFractalNoise(Direction, Settings.TerrainSeed + 283, Settings.ContinentFrequency * 8.5f, 2, 0.36f));
	const float BasinMask = SmoothStep(0.84f, 0.98f, BasinNoiseA) * SmoothStep(0.08f, 0.28f, BasinNoiseB);
	const float LowlandMask = 1.0f - SmoothStep(0.04f, 0.30f, HeightAlpha);
	const float InlandMask = SmoothStep(0.72f, 0.94f, LandMask);
	return FMath::Clamp(BasinMask * LowlandMask * InlandMask * Strength * 0.45f, 0.0f, 1.0f);
}

FLinearColor FSRPlanetTerrainGenerator::GetBiomeColor(ESRPlanetBiome Biome, float HeightAlpha, float Moisture, float Temperature)
{
	FLinearColor BaseColor;
	switch (Biome)
	{
	case ESRPlanetBiome::Ocean:
		BaseColor = FLinearColor(0.018f, 0.105f, 0.255f, 1.0f);
		break;
	case ESRPlanetBiome::Coast:
		BaseColor = FLinearColor(0.62f, 0.56f, 0.38f, 1.0f);
		break;
	case ESRPlanetBiome::Forest:
		BaseColor = FLinearColor(0.10f, 0.34f, 0.15f, 1.0f);
		break;
	case ESRPlanetBiome::Desert:
		BaseColor = FLinearColor(0.58f, 0.48f, 0.30f, 1.0f);
		break;
	case ESRPlanetBiome::Mountain:
		BaseColor = FLinearColor(0.42f, 0.36f, 0.29f, 1.0f);
		break;
	case ESRPlanetBiome::Snow:
		BaseColor = FLinearColor(0.78f, 0.80f, 0.77f, 1.0f);
		break;
	case ESRPlanetBiome::Ice:
		BaseColor = FLinearColor(0.66f, 0.76f, 0.82f, 1.0f);
		break;
	default:
		BaseColor = FLinearColor(0.28f, 0.46f, 0.23f, 1.0f);
		break;
	}

	const float HeightShade = FMath::Lerp(0.92f, 1.08f, FMath::Clamp((HeightAlpha + 1.0f) * 0.5f, 0.0f, 1.0f));
	const float MoistureShade = FMath::Lerp(0.96f, 1.06f, FMath::Clamp(Moisture, 0.0f, 1.0f));
	const float TemperatureShade = FMath::Lerp(0.97f, 1.03f, FMath::Clamp(Temperature, 0.0f, 1.0f));
	BaseColor *= HeightShade * MoistureShade * TemperatureShade;
	if (Biome == ESRPlanetBiome::Ocean)
	{
		const float ShallowWater = FMath::Clamp(HeightAlpha + 0.42f, 0.0f, 1.0f);
		BaseColor = FLinearColor::LerpUsingHSV(BaseColor, FLinearColor(0.04f, 0.30f, 0.46f, 1.0f), ShallowWater * 0.45f);
	}
	else if (Biome == ESRPlanetBiome::Coast)
	{
		const float WetCoast = FMath::Clamp(Moisture, 0.0f, 1.0f);
		BaseColor = FLinearColor::LerpUsingHSV(BaseColor, FLinearColor(0.20f, 0.46f, 0.24f, 1.0f), WetCoast * 0.35f);
	}
	BaseColor.A = 1.0f;
	return BaseColor;
}

FVector FSRPlanetTerrainGenerator::BuildSeedOffset(int32 Seed)
{
	const int64 Seed64 = static_cast<int64>(Seed);
	return FVector(
		static_cast<float>((Seed64 * 15731LL) % 10007LL),
		static_cast<float>((Seed64 * 789221LL) % 10009LL),
		static_cast<float>((Seed64 * 1376312589LL) % 10037LL));
}

float FSRPlanetTerrainGenerator::SmoothStep(float Edge0, float Edge1, float Value)
{
	if (FMath::IsNearlyEqual(Edge0, Edge1))
	{
		return Value >= Edge1 ? 1.0f : 0.0f;
	}

	const float Alpha = FMath::Clamp((Value - Edge0) / (Edge1 - Edge0), 0.0f, 1.0f);
	return Alpha * Alpha * (3.0f - (2.0f * Alpha));
}
