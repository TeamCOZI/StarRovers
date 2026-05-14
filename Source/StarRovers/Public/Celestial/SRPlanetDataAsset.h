#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Celestial/SRCelestialBody.h"
#include "SRPlanetDataAsset.generated.h"

class UMaterialInterface;
class UStaticMesh;

UCLASS(BlueprintType)
class STARROVERS_API USRPlanetDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	USRPlanetDataAsset();

	FSRCelestialBodyBiomeSpec BuildBiomeSpec() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity")
	ESRCelestialBodyCategory BodyCategory = ESRCelestialBodyCategory::Planet;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Celestial Body", meta = (DisplayName = "Scale", ClampMin = "0.0"))
	float BodyScale = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Celestial Body", meta = (DisplayName = "Body Mesh"))
	TObjectPtr<UStaticMesh> BodyMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Celestial Body", meta = (DisplayName = "Material"))
	TObjectPtr<UMaterialInterface> BodyMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "Mass", ClampMin = "0.0"))
	float Mass = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "Gravity Ratio", ClampMin = "0.0"))
	float GravityRatio = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "Gravity Radius Ratio", ClampMin = "0.0"))
	float GravityRadiusRatio = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Orbit", meta = (DisplayName = "Orbit Speed", ClampMin = "0.0", ToolTip = "Orbit revolutions per simulation period. Zero disables orbit movement."))
	float OrbitSpeed = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Surface", meta = (DisplayName = "Surface Grid Surface Offset", ClampMin = "0.0"))
	float SurfaceGridSurfaceOffset = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Cast Shadow", meta = (DisplayName = "Scale", ClampMin = "0.01"))
	float CastShadowScaleMultiplier = 0.95f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Ocean")
	bool bHasOcean = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Ocean", meta = (DisplayName = "Scale", EditCondition = "bHasOcean", ClampMin = "0.01"))
	float OceanScaleMultiplier = 0.97f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Ocean", meta = (DisplayName = "Body Mesh", EditCondition = "bHasOcean"))
	TObjectPtr<UStaticMesh> OceanMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Ocean", meta = (DisplayName = "Material", EditCondition = "bHasOcean"))
	TObjectPtr<UMaterialInterface> OceanMaterial = nullptr;

};
