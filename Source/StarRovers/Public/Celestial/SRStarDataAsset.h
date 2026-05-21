#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Celestial/SRCelestialBody.h"
#include "SRStarDataAsset.generated.h"

class UMaterialInterface;
class UStaticMesh;

UCLASS(BlueprintType)
class STARROVERS_API USRStarDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	USRStarDataAsset();

	FSRCelestialBodyBiomeSpec BuildBiomeSpec() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity", meta = (DisplayName = "DisplayName"))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity", meta = (DisplayName = "BodyCategory"))
	ESRCelestialBodyCategory BodyCategory = ESRCelestialBodyCategory::Star;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|CelestialBody", meta = (DisplayName = "Scale", ClampMin = "0.0"))
	float BodyScale = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|CelestialBody", meta = (DisplayName = "BodyMesh"))
	TObjectPtr<UStaticMesh> BodyMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|CelestialBody", meta = (DisplayName = "Material"))
	TObjectPtr<UMaterialInterface> BodyMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "Mass", ClampMin = "0.0"))
	float Mass = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "GravityRatio", ClampMin = "0.0"))
	float GravityRatio = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "GravityRadiusRatio", ClampMin = "0.0"))
	float GravityRadiusRatio = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Star", meta = (DisplayName = "StarMaterialEmissiveStrength", ClampMin = "0.0"))
	float StarMaterialEmissiveStrength = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Star", meta = (DisplayName = "StarPointLightIntensity", ClampMin = "0.0"))
	float StarPointLightIntensity = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Star", meta = (DisplayName = "StarPointLightColor"))
	FLinearColor StarPointLightColor = FLinearColor(1.0f, 0.956f, 0.84f, 1.0f);

};
