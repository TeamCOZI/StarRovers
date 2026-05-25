#include "Celestial/SRPlanetDataAsset.h"

#include "Celestial/SRCelestialBodyCategory.h"

USRPlanetDataAsset::USRPlanetDataAsset()
{
	VariableName = FText::FromString(TEXT("Planet"));
	BodyCategory = ESRCelestialBodyCategory::Planet;
	Scale = 20.0f;
	bHasOcean = true;
	OceanScaleMultiplier = 1.0f;
	SurfaceGridHeightOffset = 0.0f;
	ConstructionHeightOffset = 15.0f;
	DynamicMeshGeneration = FSRDynamicMeshGeneration();
	DynamicMeshGeneration.BiomeProfile = ESRPlanetBiomeProfile::EarthLike;
	DynamicMeshGeneration.bDynamicMeshGeneration = true;
	DynamicMeshGeneration.DynamicMeshHeight = 120.0f;
	OrbitPeriod = 1.0f;
	Mass = 200.0f;
	GravityRatio = 1.0f;
	GravityRadiusRatio = 100.0f;
}

FSRCelestialBodyData USRPlanetDataAsset::BuildData() const
{
	FSRCelestialBodyData Result;
	Result.VariableName = VariableName;
	Result.BodyCategory = BodyCategory;
	Result.Scale = FMath::Max(0.0f, Scale);
	Result.StaticMesh = StaticMesh;
	Result.Material = Material;
	Result.Mass = FMath::Max(0.0f, Mass);
	Result.GravityRatio = FMath::Max(0.0f, GravityRatio);
	Result.GravityRadiusRatio = FMath::Max(0.0f, GravityRadiusRatio);
	Result.DynamicMeshGeneration = DynamicMeshGeneration;
	Result.GenerationSeed = Result.DynamicMeshGeneration.GenerationSeed;
	Result.bHasOcean = bHasOcean;
	Result.OceanMesh = OceanMesh;
	Result.OceanMaterial = OceanMaterial;
	Result.OceanScaleMultiplier = FMath::Max(0.01f, OceanScaleMultiplier);
	Result.SurfaceGridHeightOffset = FMath::Clamp(SurfaceGridHeightOffset, 0.0f, 1.0f);
	Result.ConstructionHeightOffset = FMath::Max(0.0f, ConstructionHeightOffset);
	Result.OrbitPeriod = FMath::Max(0.0f, OrbitPeriod);
	return Result;
}
