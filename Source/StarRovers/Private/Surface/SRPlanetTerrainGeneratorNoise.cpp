#include "Surface/SRPlanetTerrainGeneratorSampling.h"

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

FVector FSRPlanetTerrainGenerator::ApplyDomainWarp(const FVector& LocalUnitDirection, const FSRDynamicMeshGeneration& Settings, float Strength)
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
		SampleFractalNoise(Direction, Settings.GenerationSeed + 211, WarpFrequency, 3, 0.5f),
		SampleFractalNoise(Direction, Settings.GenerationSeed + 223, WarpFrequency, 3, 0.5f),
		SampleFractalNoise(Direction, Settings.GenerationSeed + 227, WarpFrequency, 3, 0.5f));

	return (Direction + (Warp * SafeStrength * 0.42f)).GetSafeNormal();
}

float FSRPlanetTerrainGenerator::SampleRiverMask(const FVector& LocalUnitDirection, const FSRDynamicMeshGeneration& Settings, float LandMask, float MountainMask)
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

	const float ChannelA = FMath::Abs(SampleFractalNoise(Direction, Settings.GenerationSeed + 263, Settings.ContinentFrequency * 5.5f, 4, 0.55f));
	const float ChannelB = FMath::Abs(SampleFractalNoise(Direction, Settings.GenerationSeed + 269, Settings.ContinentFrequency * 9.0f, 3, 0.48f));
	const float LargeChannel = 1.0f - SmoothStep(0.018f, 0.082f, ChannelA);
	const float Tributary = 1.0f - SmoothStep(0.012f, 0.052f, ChannelB);
	const float SourceMask = SmoothStep(0.18f, 0.75f, MountainMask);
	const float RiverMask = FMath::Max(LargeChannel, Tributary * 0.55f) * FMath::Lerp(0.45f, 1.0f, SourceMask) * LandMask;
	return FMath::Clamp(RiverMask * Strength, 0.0f, 1.0f);
}

float FSRPlanetTerrainGenerator::SampleLakeMask(const FVector& LocalUnitDirection, const FSRDynamicMeshGeneration& Settings, float LandMask, float HeightAlpha)
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

	const float BasinNoiseA = (SampleFractalNoise(Direction, Settings.GenerationSeed + 277, Settings.ContinentFrequency * 5.2f, 3, 0.42f) + 1.0f) * 0.5f;
	const float BasinNoiseB = FMath::Abs(SampleFractalNoise(Direction, Settings.GenerationSeed + 283, Settings.ContinentFrequency * 8.5f, 2, 0.36f));
	const float BasinMask = SmoothStep(0.84f, 0.98f, BasinNoiseA) * SmoothStep(0.08f, 0.28f, BasinNoiseB);
	const float LowlandMask = 1.0f - SmoothStep(0.04f, 0.30f, HeightAlpha);
	const float InlandMask = SmoothStep(0.72f, 0.94f, LandMask);
	return FMath::Clamp(BasinMask * LowlandMask * InlandMask * Strength * 0.45f, 0.0f, 1.0f);
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
