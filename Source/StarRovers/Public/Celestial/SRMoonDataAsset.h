#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Celestial/SRCelestialBody.h"
#include "SRMoonDataAsset.generated.h"

class UMaterialInterface;
class UStaticMesh;

UCLASS(BlueprintType)
class STARROVERS_API USRMoonDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	USRMoonDataAsset();

	FSRCelestialBodyData BuildData() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity", meta = (DisplayName = "VariableName"))
	FText VariableName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity", meta = (DisplayName = "BodyCategory"))
	ESRCelestialBodyCategory BodyCategory = ESRCelestialBodyCategory::Moon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|CelestialBody", meta = (DisplayName = "Scale", ClampMin = "0.0"))
	float BodyScale = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|CelestialBody", meta = (DisplayName = "StaticMesh"))
	TObjectPtr<UStaticMesh> StaticMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|CelestialBody", meta = (DisplayName = "Material"))
	TObjectPtr<UMaterialInterface> Material = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "Mass", ClampMin = "0.0"))
	float Mass = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "GravityRatio", ClampMin = "0.0"))
	float GravityRatio = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "GravityRadiusRatio", ClampMin = "0.0"))
	float GravityRadiusRatio = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Orbit", meta = (DisplayName = "OrbitPeriod", ClampMin = "0.0", ToolTip = "Orbit period measured in simulation periods. Zero disables orbit movement."))
	float OrbitPeriod = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Surface", meta = (DisplayName = "SurfaceGridHeightOffset", ClampMin = "0.0"))
	float SurfaceGridHeightOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Surface", meta = (DisplayName = "ConstructionHeightOffset", ClampMin = "0.0"))
	float ConstructionHeightOffset = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Terrain", meta = (DisplayName = "DynamicMeshGeneration"))
	FSRDynamicMeshGeneration DynamicMeshGeneration;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Ocean", meta = (DisplayName = "bHasOcean"))
	bool bHasOcean = true;

	UPROPERTY()
	float OceanScaleMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Ocean", meta = (DisplayName = "OceanMesh", EditCondition = "bHasOcean"))
	TObjectPtr<UStaticMesh> OceanMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Ocean", meta = (DisplayName = "OceanMaterial", EditCondition = "bHasOcean"))
	TObjectPtr<UMaterialInterface> OceanMaterial = nullptr;

};
