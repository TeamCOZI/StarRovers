#include "Celestial/SRPlanetDataAsset.h"

#include "Celestial/SRCelestialBodyCategory.h"

USRPlanetDataAsset::USRPlanetDataAsset()
{
	DisplayName = FText::FromString(TEXT("Planet"));
	BodyCategory = ESRCelestialBodyCategory::Planet;
	BodyScale = 20.0f;
	bHasOcean = true;
	OceanScaleMultiplier = 0.97f;
	SurfaceGridSurfaceOffset = 0.0f;
	TerrainSettings = FSRPlanetTerrainSettings();
	TerrainSettings.TerrainProfile = ESRPlanetTerrainProfile::EarthLike;
	TerrainSettings.bUseProceduralTerrain = true;
	TerrainSettings.TerrainHeight = 120.0f;
	OrbitSpeed = 1.0f;
	Mass = 200.0f;
	GravityRatio = 1.0f;
	GravityRadiusRatio = 100.0f;
}

FSRCelestialBodyBiomeSpec USRPlanetDataAsset::BuildBiomeSpec() const
{
	FSRCelestialBodyBiomeSpec Result;
	Result.DisplayName = DisplayName;
	Result.bUseProceduralTerrain = TerrainSettings.bUseProceduralTerrain;
	Result.TerrainSeed = TerrainSettings.TerrainSeed;
	Result.TerrainHeight = TerrainSettings.TerrainHeight;
	Result.TerrainFrequency = TerrainSettings.DetailFrequency;
	Result.TerrainOctaves = TerrainSettings.TerrainOctaves;
	Result.TerrainPersistence = TerrainSettings.TerrainPersistence;
	Result.TerrainSettings = TerrainSettings;
	Result.bHasOcean = bHasOcean;
	Result.OceanMesh = OceanMesh;
	Result.OceanMaterial = OceanMaterial;
	Result.OceanScaleMultiplier = FMath::Max(0.01f, OceanScaleMultiplier);
	Result.SurfaceGridSurfaceOffset = FMath::Clamp(SurfaceGridSurfaceOffset, 0.0f, 1.0f);
	Result.OrbitSpeed = FMath::Max(0.0f, OrbitSpeed);
	return Result;
}
