#include "SRPlanetSurfaceGridTerrainState.h"

#include "Surface/SRPlanetSurfaceGridLibrary.h"
#include "Surface/SRPlanetTerrainGenerator.h"

namespace
{
	FSRDynamicMeshGeneration SanitizeTerrainConfig(FSRDynamicMeshGeneration Settings)
	{
		Settings.DynamicMeshHeight = FMath::Max(0.0f, Settings.DynamicMeshHeight);
		Settings.ContinentFrequency = FMath::Max(0.01f, Settings.ContinentFrequency);
		Settings.MountainFrequency = FMath::Max(0.01f, Settings.MountainFrequency);
		Settings.DetailFrequency = FMath::Max(0.01f, Settings.DetailFrequency);
		Settings.ValleyStrength = FMath::Clamp(Settings.ValleyStrength, 0.0f, 1.0f);
		Settings.MountainStrength = FMath::Clamp(Settings.MountainStrength, 0.5f, 4.0f);
		Settings.NoiseStrength = FMath::Clamp(Settings.NoiseStrength, 0.0f, 1.0f);
		Settings.RiverStrength = FMath::Clamp(Settings.RiverStrength, 0.0f, 1.0f);
		Settings.LakeStrength = FMath::Clamp(Settings.LakeStrength, 0.0f, 1.0f);
		Settings.DetailStrength = FMath::Clamp(Settings.DetailStrength, 0.0f, 1.0f);
		Settings.MoistureFrequency = FMath::Max(0.01f, Settings.MoistureFrequency);
		Settings.TemperatureFrequency = FMath::Max(0.01f, Settings.TemperatureFrequency);
		Settings.NoiseOctaves = FMath::Max(1, Settings.NoiseOctaves);
		Settings.NoisePersistence = FMath::Clamp(Settings.NoisePersistence, 0.0f, 1.0f);
		Settings.OceanThreshold = FMath::Clamp(Settings.OceanThreshold, -1.0f, 1.0f);
		Settings.AtmosphereThreshold = FMath::Max(0.01f, Settings.AtmosphereThreshold);
		return Settings;
	}

	FSRBiomeSampleContext BuildTerrainSampleContext(FVector LocalUnitDirection, int32 FaceResolution)
	{
		FSRBiomeSampleContext SampleContext;
		SampleContext.LocalUnitDirection = LocalUnitDirection.GetSafeNormal();
		if (SampleContext.LocalUnitDirection.IsNearlyZero())
		{
			SampleContext.LocalUnitDirection = FVector::UpVector;
		}

		FSRPlanetSurfaceGridCellId CellId;
		FVector2D FaceCoordinates = FVector2D::ZeroVector;
		const int32 SafeFaceResolution = FMath::Max(1, FaceResolution);
		if (USRPlanetSurfaceGridLibrary::ProjectDirectionToCubeSphereCellId(
			SampleContext.LocalUnitDirection,
			SafeFaceResolution,
			CellId,
			FaceCoordinates))
		{
			SampleContext.Face = CellId.Face;
			SampleContext.CellX = CellId.CellX;
			SampleContext.CellY = CellId.CellY;
			SampleContext.FaceResolution = SafeFaceResolution;
			SampleContext.FaceUV = FaceCoordinates;
		}

		return SampleContext;
	}

	void ApplyTerrainHeightPostProcessing(
		FSRPlanetTerrainSample& Sample,
		const FSRDynamicMeshGeneration& Settings,
		float HeightStep)
	{
		if (HeightStep > KINDA_SMALL_NUMBER)
		{
			Sample.HeightOffset = FMath::RoundToFloat(Sample.HeightOffset / HeightStep) * HeightStep;
		}
		if (Settings.bClampTerrainHeightToOceanLevel)
		{
			float OceanLevelHeightOffset = 0.0f;
			if (FSRPlanetTerrainGenerator::TryResolveOceanLevelHeightOffset(Settings, 512, HeightStep, OceanLevelHeightOffset))
			{
				FSRPlanetTerrainGenerator::ClampSampleHeightToOceanLevel(Sample, OceanLevelHeightOffset + HeightStep);
			}
		}
	}
}

FSRDynamicMeshGeneration StarRovers::SurfaceGridTerrainState::BuildProceduralTerrainConfig(
	const FSRDynamicMeshGeneration& CurrentSettings,
	bool bNewDynamicMeshGeneration,
	int32 NewGenerationSeed,
	float NewDynamicMeshHeight,
	float NewDetailFrequency,
	int32 NewNoiseOctaves,
	float NewNoisePersistence)
{
	FSRDynamicMeshGeneration NewSettings = CurrentSettings;
	NewSettings.bDynamicMeshGeneration = bNewDynamicMeshGeneration;
	NewSettings.GenerationSeed = NewGenerationSeed;
	NewSettings.DynamicMeshHeight = NewDynamicMeshHeight;
	NewSettings.DetailFrequency = NewDetailFrequency;
	NewSettings.NoiseOctaves = NewNoiseOctaves;
	NewSettings.NoisePersistence = NewNoisePersistence;
	return SanitizeTerrainConfig(MoveTemp(NewSettings));
}

void StarRovers::SurfaceGridTerrainState::ApplyTerrainConfig(
	const FSRDynamicMeshGeneration& NewSettings,
	FSRDynamicMeshGeneration& Settings,
	bool& bCellsDirty,
	bool& bGridMeshDirty)
{
	Settings = SanitizeTerrainConfig(NewSettings);
	if (Settings.BiomeDataAssets.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Planet surface grid terrain requires Profile BiomeDataAssets."));
	}
	else
	{
		Settings.NormalizeBiomeMaterials(Settings.BiomeDataAssets);
	}

	bCellsDirty = true;
	bGridMeshDirty = true;
}

float StarRovers::SurfaceGridTerrainState::CalculateTerrainHeightStep(
	const FSRDynamicMeshGeneration& Settings,
	float PlanetRadius,
	int32 FaceResolution)
{
	const float SafeDynamicMeshHeight = FMath::Max(0.0f, Settings.DynamicMeshHeight);
	return Settings.bMinecraft && Settings.bDynamicMeshGeneration && SafeDynamicMeshHeight > KINDA_SMALL_NUMBER
		? (2.0f * FMath::Max(1.0f, PlanetRadius)) / static_cast<float>(FMath::Max(1, FaceResolution))
		: 0.0f;
}

FSRPlanetTerrainSample StarRovers::SurfaceGridTerrainState::SampleTerrainAtDirection(
	FVector LocalUnitDirection,
	const FSRDynamicMeshGeneration& Settings,
	float PlanetRadius,
	int32 FaceResolution)
{
	FSRPlanetTerrainSample Sample = FSRPlanetTerrainGenerator::SampleTerrain(
		BuildTerrainSampleContext(LocalUnitDirection, FaceResolution),
		Settings);
	const float HeightStep = CalculateTerrainHeightStep(Settings, PlanetRadius, FaceResolution);
	ApplyTerrainHeightPostProcessing(Sample, Settings, HeightStep);
	return Sample;
}
