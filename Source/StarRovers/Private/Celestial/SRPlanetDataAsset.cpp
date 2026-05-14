#include "Celestial/SRPlanetDataAsset.h"

#include "Celestial/SRCelestialBodyCategory.h"

USRPlanetDataAsset::USRPlanetDataAsset()
{
	DisplayName = FText::FromString(TEXT("Planet"));
	BodyCategory = ESRCelestialBodyCategory::Planet;
	BodyScale = 20.0f;
	bHasOcean = true;
	OceanScaleMultiplier = 0.97f;
	SurfaceGridSurfaceOffset = 8.0f;
	CastShadowScaleMultiplier = 0.95f;
	OrbitSpeed = 1.0f;
	Mass = 200.0f;
	GravityRatio = 1.0f;
	GravityRadiusRatio = 100.0f;
}

FSRCelestialBodyBiomeSpec USRPlanetDataAsset::BuildBiomeSpec() const
{
	FSRCelestialBodyBiomeSpec Result;
	Result.DisplayName = DisplayName;
	Result.bUseProceduralTerrain = false;
	Result.TerrainHeight = 0.0f;
	Result.bHasOcean = bHasOcean;
	Result.OceanMesh = OceanMesh;
	Result.OceanMaterial = OceanMaterial;
	Result.OceanScaleMultiplier = FMath::Max(0.01f, OceanScaleMultiplier);
	Result.SurfaceGridSurfaceOffset = FMath::Max(0.0f, SurfaceGridSurfaceOffset);
	Result.ShadowCasterScaleMultiplier = FMath::Max(0.01f, CastShadowScaleMultiplier);
	Result.OrbitSpeed = FMath::Max(0.0f, OrbitSpeed);
	return Result;
}
