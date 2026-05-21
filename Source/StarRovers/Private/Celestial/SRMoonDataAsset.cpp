#include "Celestial/SRMoonDataAsset.h"

#include "Celestial/SRCelestialBodyCategory.h"

USRMoonDataAsset::USRMoonDataAsset()
{
	DisplayName = FText::FromString(TEXT("Moon"));
	BodyCategory = ESRCelestialBodyCategory::Moon;
	BodyScale = 5.0f;
	bHasOcean = false;
	OceanScaleMultiplier = 0.97f;
	SurfaceGridSurfaceOffset = 0.0f;
	TerrainSettings = FSRPlanetTerrainSettings();
	TerrainSettings.TerrainProfile = ESRPlanetTerrainProfile::RockyMoon;
	TerrainSettings.bUseProceduralTerrain = true;
	TerrainSettings.TerrainHeight = 45.0f;
	TerrainSettings.ValleyStrength = 0.12f;
	TerrainSettings.RiverStrength = 0.0f;
	TerrainSettings.LakeStrength = 0.0f;
	TerrainSettings.MicroDetailStrength = 0.55f;
	OrbitSpeed = 1.0f;
	Mass = 50.0f;
	GravityRatio = 1.0f;
	GravityRadiusRatio = 100.0f;
}

FSRCelestialBodyBiomeSpec USRMoonDataAsset::BuildBiomeSpec() const
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
