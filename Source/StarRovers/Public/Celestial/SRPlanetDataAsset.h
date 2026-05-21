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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity", meta = (DisplayName = "DisplayName"))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity", meta = (DisplayName = "BodyCategory"))
	ESRCelestialBodyCategory BodyCategory = ESRCelestialBodyCategory::Planet;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|CelestialBody", meta = (DisplayName = "Scale", ClampMin = "0.0"))
	float BodyScale = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|CelestialBody", meta = (DisplayName = "BodyMesh"))
	TObjectPtr<UStaticMesh> BodyMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|CelestialBody", meta = (DisplayName = "Material"))
	TObjectPtr<UMaterialInterface> BodyMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "Mass", ClampMin = "0.0"))
	float Mass = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "GravityRatio", ClampMin = "0.0"))
	float GravityRatio = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "GravityRadiusRatio", ClampMin = "0.0"))
	float GravityRadiusRatio = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Orbit", meta = (DisplayName = "OrbitSpeed", ClampMin = "0.0", ToolTip = "Orbit revolutions per simulation period. Zero disables orbit movement."))
	float OrbitSpeed = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Surface", meta = (DisplayName = "SurfaceGridSurfaceOffset", ClampMin = "0.0"))
	float SurfaceGridSurfaceOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Terrain", meta = (DisplayName = "TerrainSettings"))
	FSRPlanetTerrainSettings TerrainSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Ocean", meta = (DisplayName = "bHasOcean"))
	bool bHasOcean = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Ocean", meta = (DisplayName = "Scale", EditCondition = "bHasOcean", ClampMin = "0.01"))
	float OceanScaleMultiplier = 0.97f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Ocean", meta = (DisplayName = "BodyMesh", EditCondition = "bHasOcean"))
	TObjectPtr<UStaticMesh> OceanMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Ocean", meta = (DisplayName = "Material", EditCondition = "bHasOcean"))
	TObjectPtr<UMaterialInterface> OceanMaterial = nullptr;

};
