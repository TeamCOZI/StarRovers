#include "Celestial/SRPlanetDataAsset.h"

#include "Celestial/SRCelestialBodyCategory.h"
#include "Surface/SRPlanetTerrainProfileDataAsset.h"

USRPlanetDataAsset::USRPlanetDataAsset()
{
	VariableName = FText::FromString(TEXT("Planet"));
	BodyCategory = ESRCelestialBodyCategory::Planet;
	Scale = 20.0f;
	bHasOcean = true;
	OceanScaleMultiplier = 1.0f;
	bHasAtmosphere = true;
	AtmosphereScaleMultiplier = 1.0f;
	SurfaceGridHeightOffset = 0.0f;
	DynamicMeshGeneration = FSRDynamicMeshGeneration();
	DynamicMeshGeneration.bDynamicMeshGeneration = true;
	DynamicMeshGeneration.DynamicMeshHeight = 120.0f;
	OrbitPeriod = 1.0f;
	Mass = 200.0f;
	GravityRatio = 1.0f;
	GravityRadiusRatio = 100.0f;
}

void USRPlanetDataAsset::PostLoad()
{
	Super::PostLoad();
	if (IsValid(TerrainProfileDataAsset.Get()))
	{
		TerrainProfileDataAsset->ApplyToDynamicMeshGeneration(DynamicMeshGeneration);
	}
}

#if WITH_EDITOR
void USRPlanetDataAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (IsValid(TerrainProfileDataAsset.Get()))
	{
		TerrainProfileDataAsset->ApplyToDynamicMeshGeneration(DynamicMeshGeneration);
	}
}
#endif

FSRCelestialBodyData USRPlanetDataAsset::BuildData() const
{
	FSRCelestialBodyData Result;
	Result.VariableName = VariableName;
	Result.BodyCategory = BodyCategory;
	Result.Scale = FMath::Max(0.0f, Scale);
	Result.DynamicMeshBaseDataAsset = DynamicMeshBaseDataAsset;
	Result.Material = Material;
	Result.Mass = FMath::Max(0.0f, Mass);
	Result.GravityRatio = FMath::Max(0.0f, GravityRatio);
	Result.GravityRadiusRatio = FMath::Max(0.0f, GravityRadiusRatio);
	Result.DynamicMeshGeneration = DynamicMeshGeneration;
	if (IsValid(TerrainProfileDataAsset.Get()))
	{
		TerrainProfileDataAsset->ApplyToDynamicMeshGeneration(Result.DynamicMeshGeneration);
	}
	Result.TerrainProfileDataAsset = TerrainProfileDataAsset;
	Result.ProfileNaturalStructureSpawnRuleOverrides = ProfileNaturalStructureSpawnRuleOverrides;
	Result.GenerationSeed = Result.DynamicMeshGeneration.GenerationSeed;
	Result.bRandomizeGenerationSeedEachRun = Result.DynamicMeshGeneration.bRandomizeGenerationSeedEachRun;
	Result.bHasOcean = bHasOcean;
	Result.OceanMesh = OceanMesh;
	Result.OceanMaterial = OceanMaterial;
	Result.OceanScaleMultiplier = FMath::Max(0.01f, OceanScaleMultiplier);
	Result.bHasAtmosphere = bHasAtmosphere;
	Result.AtmosphereMesh = AtmosphereMesh;
	Result.AtmosphereMaterial = AtmosphereMaterial;
	Result.AtmosphereScaleMultiplier = FMath::Max(0.01f, AtmosphereScaleMultiplier);
	Result.SurfaceGridHeightOffset = FMath::Clamp(SurfaceGridHeightOffset, 0.0f, 1.0f);
	Result.OrbitPeriod = FMath::Max(0.0f, OrbitPeriod);
	return Result;
}
