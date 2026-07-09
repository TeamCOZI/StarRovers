#include "Surface/SRPlanetTerrainGeneratorSampling.h"

namespace StarRovers::Terrain
{
	FVector BuildNoiseSeedOffset(int32 Seed)
	{
		const int64 Seed64 = static_cast<int64>(Seed);
		return FVector(
			static_cast<float>((Seed64 * 15731LL) % 10007LL),
			static_cast<float>((Seed64 * 789221LL) % 10009LL),
			static_cast<float>((Seed64 * 1376312589LL) % 10037LL));
	}

	float SampleFractalNoiseUnitDirection(const FVector& UnitDirection, int32 Seed, float Frequency, int32 Octaves, float Persistence)
	{
		float CurrentFrequency = FMath::Max(0.01f, Frequency);
		float CurrentAmplitude = 1.0f;
		float NoiseSum = 0.0f;
		float AmplitudeSum = 0.0f;
		const FVector SeedOffset = BuildNoiseSeedOffset(Seed);
		const int32 SafeOctaves = FMath::Clamp(Octaves, 1, 8);
		const float SafePersistence = FMath::Clamp(Persistence, 0.0f, 1.0f);

		for (int32 OctaveIndex = 0; OctaveIndex < SafeOctaves; ++OctaveIndex)
		{
			const float NoiseValue = FMath::PerlinNoise3D((UnitDirection * CurrentFrequency) + SeedOffset);
			NoiseSum += NoiseValue * CurrentAmplitude;
			AmplitudeSum += CurrentAmplitude;
			CurrentFrequency *= 2.0f;
			CurrentAmplitude *= SafePersistence;
		}

		return AmplitudeSum > KINDA_SMALL_NUMBER ? NoiseSum / AmplitudeSum : 0.0f;
	}

	float SampleFractalNoiseUnitDirection(const FVector& UnitDirection, const FSRCompiledTerrainNoiseDescriptor& Noise)
	{
		float CurrentFrequency = Noise.Frequency;
		float CurrentAmplitude = 1.0f;
		float NoiseSum = 0.0f;
		float AmplitudeSum = 0.0f;

		for (int32 OctaveIndex = 0; OctaveIndex < Noise.Octaves; ++OctaveIndex)
		{
			const float NoiseValue = FMath::PerlinNoise3D((UnitDirection * CurrentFrequency) + Noise.SeedOffset);
			NoiseSum += NoiseValue * CurrentAmplitude;
			AmplitudeSum += CurrentAmplitude;
			CurrentFrequency *= 2.0f;
			CurrentAmplitude *= Noise.Persistence;
		}

		return AmplitudeSum > KINDA_SMALL_NUMBER ? NoiseSum / AmplitudeSum : 0.0f;
	}

	float SampleFractalNoiseUnitDirection(
		const FVector& UnitDirection,
		const FVector& SeedOffset,
		float Frequency,
		int32 Octaves,
		float Persistence)
	{
		float CurrentFrequency = Frequency;
		float CurrentAmplitude = 1.0f;
		float NoiseSum = 0.0f;
		float AmplitudeSum = 0.0f;

		for (int32 OctaveIndex = 0; OctaveIndex < Octaves; ++OctaveIndex)
		{
			const float NoiseValue = FMath::PerlinNoise3D((UnitDirection * CurrentFrequency) + SeedOffset);
			NoiseSum += NoiseValue * CurrentAmplitude;
			AmplitudeSum += CurrentAmplitude;
			CurrentFrequency *= 2.0f;
			CurrentAmplitude *= Persistence;
		}

		return AmplitudeSum > KINDA_SMALL_NUMBER ? NoiseSum / AmplitudeSum : 0.0f;
	}

	float SampleRidgedNoiseUnitDirection(const FVector& UnitDirection, int32 Seed, float Frequency, int32 Octaves)
	{
		float CurrentFrequency = FMath::Max(0.01f, Frequency);
		float CurrentAmplitude = 1.0f;
		float NoiseSum = 0.0f;
		float AmplitudeSum = 0.0f;
		const FVector SeedOffset = BuildNoiseSeedOffset(Seed);
		const int32 SafeOctaves = FMath::Clamp(Octaves, 1, 8);

		for (int32 OctaveIndex = 0; OctaveIndex < SafeOctaves; ++OctaveIndex)
		{
			const float NoiseValue = FMath::PerlinNoise3D((UnitDirection * CurrentFrequency) + SeedOffset);
			const float RidgedValue = 1.0f - FMath::Abs(NoiseValue);
			NoiseSum += FMath::Clamp(RidgedValue, 0.0f, 1.0f) * CurrentAmplitude;
			AmplitudeSum += CurrentAmplitude;
			CurrentFrequency *= 2.0f;
			CurrentAmplitude *= 0.5f;
		}

		return AmplitudeSum > KINDA_SMALL_NUMBER ? NoiseSum / AmplitudeSum : 0.0f;
	}

	float SampleRidgedNoiseUnitDirection(const FVector& UnitDirection, const FSRCompiledTerrainNoiseDescriptor& Noise)
	{
		float CurrentFrequency = Noise.Frequency;
		float CurrentAmplitude = 1.0f;
		float NoiseSum = 0.0f;
		float AmplitudeSum = 0.0f;

		for (int32 OctaveIndex = 0; OctaveIndex < Noise.Octaves; ++OctaveIndex)
		{
			const float NoiseValue = FMath::PerlinNoise3D((UnitDirection * CurrentFrequency) + Noise.SeedOffset);
			const float RidgedValue = 1.0f - FMath::Abs(NoiseValue);
			NoiseSum += FMath::Clamp(RidgedValue, 0.0f, 1.0f) * CurrentAmplitude;
			AmplitudeSum += CurrentAmplitude;
			CurrentFrequency *= 2.0f;
			CurrentAmplitude *= 0.5f;
		}

		return AmplitudeSum > KINDA_SMALL_NUMBER ? NoiseSum / AmplitudeSum : 0.0f;
	}

	float ComputeMinecraftPeaksAndValleysForSettings(float Weirdness)
	{
		const float AbsWeirdness = FMath::Abs(FMath::Clamp(Weirdness, -1.0f, 1.0f));
		return -(FMath::Abs(AbsWeirdness - (2.0f / 3.0f)) - (1.0f / 3.0f)) * 3.0f;
	}

	bool ShouldSampleRareRegionNoise(const FSRDynamicMeshGenerationSnapshot& Settings)
	{
		return Settings.bUsesRareRegionPlacementMetric;
	}

	bool ShouldSampleRareRegionNoise(const FSRDynamicMeshGeneration& Settings)
	{
		(void)Settings;
		return true;
	}

	float SampleContinentalnessForSettings(const FVector& Direction, const FSRDynamicMeshGeneration& Settings, int32 SafeOctaves)
	{
		return SampleFractalNoiseUnitDirection(Direction, Settings.GenerationSeed + 10001, Settings.ContinentFrequency * 0.58f, FMath::Max(3, SafeOctaves), 0.56f);
	}

	float SampleContinentalnessForSettings(const FVector& Direction, const FSRDynamicMeshGenerationSnapshot& Settings, int32 SafeOctaves)
	{
		(void)SafeOctaves;
		return SampleFractalNoiseUnitDirection(Direction, Settings.ContinentalnessNoise);
	}

	float SampleErosionForSettings(const FVector& Direction, const FSRDynamicMeshGeneration& Settings, int32 SafeOctaves)
	{
		return SampleFractalNoiseUnitDirection(Direction, Settings.GenerationSeed + 10037, Settings.ContinentFrequency * 1.18f, FMath::Max(3, SafeOctaves - 1), 0.52f);
	}

	float SampleErosionForSettings(const FVector& Direction, const FSRDynamicMeshGenerationSnapshot& Settings, int32 SafeOctaves)
	{
		(void)SafeOctaves;
		return SampleFractalNoiseUnitDirection(Direction, Settings.ErosionNoise);
	}

	float SampleWeirdnessForSettings(const FVector& Direction, const FSRDynamicMeshGeneration& Settings, int32 SafeOctaves)
	{
		return SampleFractalNoiseUnitDirection(Direction, Settings.GenerationSeed + 10061, Settings.MountainFrequency * 0.42f, FMath::Max(3, SafeOctaves), 0.50f);
	}

	float SampleWeirdnessForSettings(const FVector& Direction, const FSRDynamicMeshGenerationSnapshot& Settings, int32 SafeOctaves)
	{
		(void)SafeOctaves;
		return SampleFractalNoiseUnitDirection(Direction, Settings.WeirdnessNoise);
	}

	float SampleRidgesForSettings(const FVector& Direction, const FSRDynamicMeshGeneration& Settings, int32 SafeOctaves)
	{
		return SampleRidgedNoiseUnitDirection(Direction, Settings.GenerationSeed + 10091, Settings.MountainFrequency * 0.72f, FMath::Max(3, SafeOctaves - 1));
	}

	float SampleRidgesForSettings(const FVector& Direction, const FSRDynamicMeshGenerationSnapshot& Settings, int32 SafeOctaves)
	{
		(void)SafeOctaves;
		return SampleRidgedNoiseUnitDirection(Direction, Settings.RidgesNoise);
	}

	float SampleDetailForSettings(const FVector& Direction, const FSRDynamicMeshGeneration& Settings, int32 SafeOctaves, float SafePersistence)
	{
		return SampleFractalNoiseUnitDirection(Direction, Settings.GenerationSeed + 10111, Settings.DetailFrequency, FMath::Max(2, SafeOctaves - 2), SafePersistence);
	}

	float SampleDetailForSettings(const FVector& Direction, const FSRDynamicMeshGenerationSnapshot& Settings, int32 SafeOctaves, float SafePersistence)
	{
		(void)SafeOctaves;
		(void)SafePersistence;
		return SampleFractalNoiseUnitDirection(Direction, Settings.DetailNoise);
	}

	float SampleTemperatureForSettings(const FVector& Direction, const FSRDynamicMeshGeneration& Settings)
	{
		return SampleFractalNoiseUnitDirection(Direction, Settings.GenerationSeed + 10141, Settings.TemperatureFrequency, 3, 0.5f);
	}

	float SampleTemperatureForSettings(const FVector& Direction, const FSRDynamicMeshGenerationSnapshot& Settings)
	{
		return SampleFractalNoiseUnitDirection(Direction, Settings.TemperatureNoise);
	}

	float SampleHumidityForSettings(const FVector& Direction, const FSRDynamicMeshGeneration& Settings)
	{
		return SampleFractalNoiseUnitDirection(Direction, Settings.GenerationSeed + 10163, Settings.MoistureFrequency, 3, 0.5f);
	}

	float SampleHumidityForSettings(const FVector& Direction, const FSRDynamicMeshGenerationSnapshot& Settings)
	{
		return SampleFractalNoiseUnitDirection(Direction, Settings.HumidityNoise);
	}

	float SampleRareRegionForSettings(const FVector& Direction, const FSRDynamicMeshGeneration& Settings)
	{
		return SampleFractalNoiseUnitDirection(Direction, Settings.GenerationSeed + 503, Settings.ContinentFrequency * 3.75f, 3, 0.54f);
	}

	float SampleRareRegionForSettings(const FVector& Direction, const FSRDynamicMeshGenerationSnapshot& Settings)
	{
		return SampleFractalNoiseUnitDirection(Direction, Settings.RareRegionNoise);
	}
}
