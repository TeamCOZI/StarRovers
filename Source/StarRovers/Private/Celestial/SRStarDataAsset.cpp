#include "Celestial/SRStarDataAsset.h"

#include "Celestial/SRCelestialBodyCategory.h"

USRStarDataAsset::USRStarDataAsset()
{
	DisplayName = FText::FromString(TEXT("Star"));
	BodyCategory = ESRCelestialBodyCategory::Star;
	BodyScale = 100.0f;
	Mass = 2000.0f;
	GravityRatio = 1.0f;
	GravityRadiusRatio = 100.0f;
	StarMaterialEmissiveStrength = 30.0f;
	StarPointLightIntensity = 100.0f;
	StarPointLightColor = FLinearColor(1.0f, 0.956f, 0.84f, 1.0f);
}

FSRCelestialBodyBiomeSpec USRStarDataAsset::BuildBiomeSpec() const
{
	FSRCelestialBodyBiomeSpec Result;
	Result.DisplayName = DisplayName;
	Result.bUseProceduralTerrain = false;
	Result.bHasOcean = false;
	Result.StarMaterialEmissiveStrength = FMath::Max(0.0f, StarMaterialEmissiveStrength);
	Result.StarPointLightIntensity = FMath::Max(0.0f, StarPointLightIntensity);
	Result.StarPointLightColor = StarPointLightColor;
	return Result;
}
