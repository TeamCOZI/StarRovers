#include "Celestial/SRMoonDataAsset.h"

#include "Utility/SRLog.h"
#include "Celestial/SRCelestialBodyCategory.h"
#include "Celestial/SRPlanetShapeDataAsset.h"
#include "Surface/SRPlanetTerrainProfileDataAsset.h"

USRMoonDataAsset::USRMoonDataAsset()
{
	VariableName = FText::FromString(TEXT("Moon"));
	BodyCategory = ESRCelestialBodyCategory::Moon;
	Scale = 5.0f;
	bHasOcean = false;
	OceanScaleMultiplier = 1.0f;
	bHasAtmosphere = false;
	AtmosphereScaleMultiplier = 1.0f;
	SurfaceGridHeightOffset = 0.0f;
	DynamicMeshGeneration = FSRDynamicMeshGeneration();
	DynamicMeshGeneration.bDynamicMeshGeneration = true;
	DynamicMeshGeneration.DynamicMeshHeight = 45.0f;
	DynamicMeshGeneration.ValleyStrength = 0.12f;
	DynamicMeshGeneration.RiverStrength = 0.0f;
	DynamicMeshGeneration.LakeStrength = 0.0f;
	DynamicMeshGeneration.DetailStrength = 0.55f;
	Mass = 50.0f;
	GravityRatio = 1.0f;
	GravityRadiusRatio = 100.0f;
}

void USRMoonDataAsset::PostLoad()
{
	Super::PostLoad();
	if (IsValid(TerrainProfileDataAsset.Get()))
	{
		TerrainProfileDataAsset->ApplyToDynamicMeshGeneration(DynamicMeshGeneration);
	}
}

#if WITH_EDITOR
void USRMoonDataAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (IsValid(TerrainProfileDataAsset.Get()))
	{
		TerrainProfileDataAsset->ApplyToDynamicMeshGeneration(DynamicMeshGeneration);
	}
	if (IsValid(ShapeDataAsset.Get()) && !ShapeDataAsset->IsDynamicMeshBaseShapeCompatible())
	{
		SR_LOG(Celestial, LogTemp,
			Warning,
			TEXT("MoonDataAsset '%s' uses ShapeDataAsset '%s' with an incompatible DynamicMeshBaseDataAsset shape."),
			*GetName(),
			*GetNameSafe(ShapeDataAsset.Get()));
	}
}
#endif

FSRCelestialBodyData USRMoonDataAsset::BuildData() const
{
	const USRPlanetShapeDataAsset* ResolvedShapeDataAsset = ShapeDataAsset.Get();
	USRDynamicMeshBaseDataAsset* ShapeDynamicMeshBaseDataAsset = nullptr;
	USRDynamicMeshBaseDataAsset* ShapeOceanDynamicMeshBaseDataAsset = nullptr;
	USRDynamicMeshBaseDataAsset* ShapeAtmosphereDynamicMeshBaseDataAsset = nullptr;
	if (IsValid(ResolvedShapeDataAsset))
	{
		ShapeDynamicMeshBaseDataAsset = ResolvedShapeDataAsset->GetDynamicMeshBaseDataAsset();
		ShapeOceanDynamicMeshBaseDataAsset = ResolvedShapeDataAsset->GetOceanDynamicMeshBaseDataAsset();
		ShapeAtmosphereDynamicMeshBaseDataAsset = ResolvedShapeDataAsset->GetAtmosphereDynamicMeshBaseDataAsset();
	}

	FSRCelestialBodyData Result;
	Result.VariableName = VariableName;
	Result.BodyCategory = BodyCategory;
	Result.Scale = FMath::Max(0.0f, Scale);
	Result.DynamicMeshBaseDataAsset = ShapeDynamicMeshBaseDataAsset;
	Result.Material = Material;
	Result.ToonOutlineSettings = ToonOutlineSettings;
	Result.Mass = FMath::Max(0.0f, Mass);
	Result.GravityRatio = FMath::Max(0.0f, GravityRatio);
	Result.GravityRadiusRatio = FMath::Max(0.0f, GravityRadiusRatio);
	Result.bCanConstruct = true;
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
	Result.OceanDynamicMeshBaseDataAsset = ShapeOceanDynamicMeshBaseDataAsset;
	Result.OceanMaterial = OceanMaterial;
	Result.OceanScaleMultiplier = FMath::Max(0.01f, OceanScaleMultiplier);
	Result.bHasAtmosphere = bHasAtmosphere;
	Result.AtmosphereDynamicMeshBaseDataAsset = ShapeAtmosphereDynamicMeshBaseDataAsset;
	Result.AtmosphereMaterial = AtmosphereMaterial;
	Result.AtmosphereScaleMultiplier = FMath::Max(0.01f, AtmosphereScaleMultiplier);
	Result.SurfaceGridHeightOffset = FMath::Clamp(SurfaceGridHeightOffset, 0.0f, 1.0f);
	return Result;
}
