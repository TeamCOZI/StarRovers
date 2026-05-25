#include "Celestial/SRStarDataAsset.h"

#include "Celestial/SRCelestialBodyCategory.h"

USRStarDataAsset::USRStarDataAsset()
{
	VariableName = FText::FromString(TEXT("Star"));
	BodyCategory = ESRCelestialBodyCategory::Star;
	Scale = 100.0f;
	Mass = 2000.0f;
	GravityRatio = 1.0f;
	GravityRadiusRatio = 100.0f;
	StarPointLightIntensity = 100.0f;
	StarPointLightColor = FLinearColor(1.0f, 0.956f, 0.84f, 1.0f);
}

FSRCelestialBodyData USRStarDataAsset::BuildData() const
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
	Result.DynamicMeshGeneration.bDynamicMeshGeneration = false;
	Result.GenerationSeed = Result.DynamicMeshGeneration.GenerationSeed;
	Result.bHasOcean = false;
	Result.StarPointLightIntensity = FMath::Max(0.0f, StarPointLightIntensity);
	Result.StarPointLightColor = StarPointLightColor;
	return Result;
}
