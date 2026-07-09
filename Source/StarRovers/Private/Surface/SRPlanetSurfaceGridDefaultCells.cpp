#include "SRPlanetSurfaceGridDefaultCells.h"

#include "Surface/SRPlanetSurfaceGridLibrary.h"

TArray<FSRPlanetSurfaceGridCell> StarRovers::SurfaceGridDefaultCells::BuildCubeSphereCells(
	int32 FaceResolution,
	float PlanetRadius,
	FTerrainSampleQuery GetTerrainSample,
	FTemperatureStateResolver ResolveTemperatureState)
{
	TArray<FSRPlanetSurfaceGridCell> Cells = USRPlanetSurfaceGridLibrary::GenerateCubeSphereCells(
		FMath::Max(1, FaceResolution),
		FMath::Max(1.0f, PlanetRadius));

	for (FSRPlanetSurfaceGridCell& Cell : Cells)
	{
		const FSRPlanetTerrainSample TerrainSample = GetTerrainSample(Cell.LocalCenter.GetSafeNormal());
		Cell.Biome = TerrainSample.Biome;
		Cell.BiomeId = TerrainSample.BiomeId;
		Cell.WaterRole = TerrainSample.WaterRole;
		Cell.SurfaceTemperature = TerrainSample.Temperature;
		Cell.TemperatureState = ResolveTemperatureState(TerrainSample.Temperature);
	}

	return Cells;
}
