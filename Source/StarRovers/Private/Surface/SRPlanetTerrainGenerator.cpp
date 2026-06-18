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
