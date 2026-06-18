#include "Surface/SRPlanetTerrainGeneratorInternal.h"

namespace StarRovers::Terrain
{
	int32 GetSafeNoiseOctavesForSettings(const FSRDynamicMeshGeneration& Settings)
	{
		return FMath::Clamp(Settings.NoiseOctaves, 1, 8);
	}

	int32 GetSafeNoiseOctavesForSettings(const FSRDynamicMeshGenerationSnapshot& Settings)
	{
		return Settings.SafeNoiseOctaves;
	}

	float GetSafeNoisePersistenceForSettings(const FSRDynamicMeshGeneration& Settings)
	{
		return FMath::Clamp(Settings.NoisePersistence, 0.0f, 1.0f);
	}

	float GetSafeNoisePersistenceForSettings(const FSRDynamicMeshGenerationSnapshot& Settings)
	{
		return Settings.SafeNoisePersistence;
	}

	float GetSafeDynamicMeshHeightForSettings(const FSRDynamicMeshGeneration& Settings)
	{
		return FMath::Max(0.0f, Settings.DynamicMeshHeight);
	}

	float GetSafeDynamicMeshHeightForSettings(const FSRDynamicMeshGenerationSnapshot& Settings)
	{
		return Settings.SafeDynamicMeshHeight;
	}

	float GetInvSafeDynamicMeshHeightForSettings(const FSRDynamicMeshGeneration& Settings)
	{
		const float SafeDynamicMeshHeight = GetSafeDynamicMeshHeightForSettings(Settings);
		return SafeDynamicMeshHeight > KINDA_SMALL_NUMBER ? 1.0f / SafeDynamicMeshHeight : 0.0f;
	}

	float GetInvSafeDynamicMeshHeightForSettings(const FSRDynamicMeshGenerationSnapshot& Settings)
	{
		return Settings.InvSafeDynamicMeshHeight;
	}

	float GetMountainHeightStrengthScaleForSettings(const FSRDynamicMeshGeneration& Settings)
	{
		return FMath::Pow(FMath::Clamp(Settings.MountainStrength / 2.0f, 0.25f, 2.0f), 0.45f);
	}

	float GetMountainHeightStrengthScaleForSettings(const FSRDynamicMeshGenerationSnapshot& Settings)
	{
		return Settings.MountainHeightStrengthScale;
	}

	float GetClampedValleyStrengthForSettings(const FSRDynamicMeshGeneration& Settings)
	{
		return FMath::Clamp(Settings.ValleyStrength, 0.0f, 1.0f);
	}

	float GetClampedValleyStrengthForSettings(const FSRDynamicMeshGenerationSnapshot& Settings)
	{
		return Settings.ClampedValleyStrength;
	}

	float GetClampedDetailStrengthForSettings(const FSRDynamicMeshGeneration& Settings)
	{
		return FMath::Clamp(Settings.DetailStrength, 0.0f, 1.0f);
	}

	float GetClampedDetailStrengthForSettings(const FSRDynamicMeshGenerationSnapshot& Settings)
	{
		return Settings.ClampedDetailStrength;
	}
}
