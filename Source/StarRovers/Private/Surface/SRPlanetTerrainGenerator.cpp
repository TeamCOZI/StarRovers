#include "Surface/SRPlanetTerrainGenerator.h"

FSRPlanetTerrainSample FSRPlanetTerrainGenerator::SampleTerrain(const FVector& LocalUnitDirection, const FSRDynamicMeshGeneration& Settings)
{
	return SampleTerrain(BuildSampleContextFromDirection(LocalUnitDirection), Settings);
}

FSRPlanetTerrainSample FSRPlanetTerrainGenerator::SampleTerrain(const FSRBiomeSampleContext& Context, const FSRDynamicMeshGeneration& Settings)
{
	if (!Settings.bDynamicMeshGeneration || Settings.DynamicMeshHeight <= KINDA_SMALL_NUMBER)
	{
		FSRPlanetTerrainSample Sample;
		Sample.Biome = ESRPlanetBiome::Plains;
		Sample.BiomeId = FName(TEXT("Plains"));
		return Sample;
	}

	return SampleDefaultTerrain(Context, Settings);
}

FSRPlanetTerrainSample FSRPlanetTerrainGenerator::SampleTerrain(const FSRBiomeSampleContext& Context, const FSRDynamicMeshGenerationSnapshot& Settings)
{
	if (!Settings.bDynamicMeshGeneration || Settings.DynamicMeshHeight <= KINDA_SMALL_NUMBER)
	{
		FSRPlanetTerrainSample Sample;
		Sample.Biome = ESRPlanetBiome::Plains;
		Sample.BiomeId = FName(TEXT("Plains"));
		return Sample;
	}

	return SampleDefaultTerrain(Context, Settings);
}

bool FSRPlanetTerrainGenerator::IsOceanLevelWaterSample(const FSRPlanetTerrainSample& Sample)
{
	return Sample.WaterRole == ESRBiomeWaterRole::Ocean
		|| Sample.WaterRole == ESRBiomeWaterRole::River
		|| Sample.WaterRole == ESRBiomeWaterRole::Lake
		|| (Sample.WaterRole == ESRBiomeWaterRole::Coast && (Sample.RiverMask > 0.58f || Sample.LakeMask > 0.38f));
}

bool FSRPlanetTerrainGenerator::TryResolveOceanLevelHeightOffset(
	const FSRDynamicMeshGeneration& Settings,
	int32 SampleCount,
	float HeightStep,
	float& OutHeightOffset)
{
	OutHeightOffset = 0.0f;
	if (!Settings.bDynamicMeshGeneration || Settings.DynamicMeshHeight <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	float HighestWaterHeightOffset = -BIG_NUMBER;
	const int32 SafeSampleCount = FMath::Max(1, SampleCount);
	for (int32 SampleIndex = 0; SampleIndex < SafeSampleCount; ++SampleIndex)
	{
		const FVector Direction = BuildOceanLevelSampleDirection(SampleIndex, SafeSampleCount);
		FSRPlanetTerrainSample Sample = SampleTerrain(Direction, Settings);
		if (HeightStep > KINDA_SMALL_NUMBER)
		{
			Sample.HeightOffset = FMath::RoundToFloat(Sample.HeightOffset / HeightStep) * HeightStep;
		}
		if (IsOceanLevelWaterSample(Sample))
		{
			HighestWaterHeightOffset = FMath::Max(HighestWaterHeightOffset, Sample.HeightOffset);
		}
	}

	if (HighestWaterHeightOffset <= -BIG_NUMBER * 0.5f)
	{
		return false;
	}

	OutHeightOffset = HighestWaterHeightOffset;
	return true;
}

bool FSRPlanetTerrainGenerator::TryResolveOceanLevelHeightOffset(
	const FSRDynamicMeshGenerationSnapshot& Settings,
	int32 SampleCount,
	float HeightStep,
	float& OutHeightOffset)
{
	OutHeightOffset = 0.0f;
	if (!Settings.bDynamicMeshGeneration || Settings.DynamicMeshHeight <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	float HighestWaterHeightOffset = -BIG_NUMBER;
	const int32 SafeSampleCount = FMath::Max(1, SampleCount);
	for (int32 SampleIndex = 0; SampleIndex < SafeSampleCount; ++SampleIndex)
	{
		const FVector Direction = BuildOceanLevelSampleDirection(SampleIndex, SafeSampleCount);
		FSRBiomeSampleContext Context = BuildSampleContextFromDirection(Direction);
		FSRPlanetTerrainSample Sample = SampleTerrain(Context, Settings);
		if (HeightStep > KINDA_SMALL_NUMBER)
		{
			Sample.HeightOffset = FMath::RoundToFloat(Sample.HeightOffset / HeightStep) * HeightStep;
		}
		if (IsOceanLevelWaterSample(Sample))
		{
			HighestWaterHeightOffset = FMath::Max(HighestWaterHeightOffset, Sample.HeightOffset);
		}
	}

	if (HighestWaterHeightOffset <= -BIG_NUMBER * 0.5f)
	{
		return false;
	}

	OutHeightOffset = HighestWaterHeightOffset;
	return true;
}

void FSRPlanetTerrainGenerator::ClampSampleHeightToOceanLevel(FSRPlanetTerrainSample& Sample, float OceanLevelHeightOffset)
{
	if (Sample.HeightOffset > OceanLevelHeightOffset)
	{
		Sample.HeightOffset = OceanLevelHeightOffset;
	}
}

FSRBiomeSampleContext FSRPlanetTerrainGenerator::BuildSampleContextFromDirection(const FVector& LocalUnitDirection)
{
	FSRBiomeSampleContext Context;
	Context.LocalUnitDirection = LocalUnitDirection.GetSafeNormal();
	if (Context.LocalUnitDirection.IsNearlyZero())
	{
		Context.LocalUnitDirection = FVector::UpVector;
		Context.Face = ESRCubeSphereFace::PositiveZ;
		return Context;
	}

	const FVector AbsDirection = Context.LocalUnitDirection.GetAbs();
	if (AbsDirection.X >= AbsDirection.Y && AbsDirection.X >= AbsDirection.Z)
	{
		Context.Face = Context.LocalUnitDirection.X >= 0.0f ? ESRCubeSphereFace::PositiveX : ESRCubeSphereFace::NegativeX;
	}
	else if (AbsDirection.Y >= AbsDirection.X && AbsDirection.Y >= AbsDirection.Z)
	{
		Context.Face = Context.LocalUnitDirection.Y >= 0.0f ? ESRCubeSphereFace::PositiveY : ESRCubeSphereFace::NegativeY;
	}
	else
	{
		Context.Face = Context.LocalUnitDirection.Z >= 0.0f ? ESRCubeSphereFace::PositiveZ : ESRCubeSphereFace::NegativeZ;
	}

	return Context;
}

FVector FSRPlanetTerrainGenerator::BuildOceanLevelSampleDirection(int32 Index, int32 Count)
{
	constexpr float GoldenAngle = PI * (3.0f - 2.2360679775f);
	const float SafeCount = FMath::Max(1.0f, static_cast<float>(Count));
	const float Z = 1.0f - ((static_cast<float>(Index) + 0.5f) * 2.0f / SafeCount);
	const float Radius = FMath::Sqrt(FMath::Max(0.0f, 1.0f - (Z * Z)));
	const float Theta = GoldenAngle * static_cast<float>(Index);
	return FVector(FMath::Cos(Theta) * Radius, FMath::Sin(Theta) * Radius, Z).GetSafeNormal();
}
