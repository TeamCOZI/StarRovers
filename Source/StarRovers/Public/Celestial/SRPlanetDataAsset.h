#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Celestial/SRCelestialBodyData.h"
#include "SRPlanetDataAsset.generated.h"

class UMaterialInterface;
class USRPlanetShapeDataAsset;
class USRPlanetTerrainProfileDataAsset;

UCLASS(BlueprintType)
class STARROVERS_API USRPlanetDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	USRPlanetDataAsset();

	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	FSRCelestialBodyData BuildData() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity", meta = (DisplayName = "VariableName"))
	FText VariableName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity", meta = (DisplayName = "BodyCategory"))
	ESRCelestialBodyCategory BodyCategory = ESRCelestialBodyCategory::Planet;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|CelestialBody", meta = (DisplayName = "Scale", ClampMin = "0.0"))
	float Scale = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Shape", meta = (DisplayName = "ShapeDataAsset", ToolTip = "Required shared shape source for terrain, ocean, and atmosphere meshes. Referenced assets should be editor-baked and only assigned at runtime."))
	TObjectPtr<USRPlanetShapeDataAsset> ShapeDataAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|CelestialBody", meta = (DisplayName = "Material"))
	TObjectPtr<UMaterialInterface> Material = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Visual", meta = (DisplayName = "ToonOutlineSettings"))
	FSRToonOutlineSettings ToonOutlineSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "Mass", ClampMin = "0.0"))
	float Mass = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "GravityRatio", ClampMin = "0.0"))
	float GravityRatio = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "GravityRadiusRatio", ClampMin = "0.0"))
	float GravityRadiusRatio = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Surface", meta = (DisplayName = "SurfaceGridHeightOffset", ClampMin = "0.0"))
	float SurfaceGridHeightOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "Dynamic Mesh Generation", ShowOnlyInnerProperties))
	FSRDynamicMeshGeneration DynamicMeshGeneration;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Dynamic Mesh Generation", meta = (DisplayName = "TerrainProfileDataAsset"))
	TObjectPtr<USRPlanetTerrainProfileDataAsset> TerrainProfileDataAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Natural Structures", meta = (DisplayName = "ProfileNaturalStructureSpawnRuleOverrides"))
	TArray<FSRNaturalStructureSpawnRuleOverride> ProfileNaturalStructureSpawnRuleOverrides;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Ocean", meta = (DisplayName = "bHasOcean"))
	bool bHasOcean = true;

	UPROPERTY()
	float OceanScaleMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Ocean", meta = (DisplayName = "OceanMaterial", EditCondition = "bHasOcean"))
	TObjectPtr<UMaterialInterface> OceanMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Atmosphere", meta = (DisplayName = "bHasAtmosphere"))
	bool bHasAtmosphere = true;

	UPROPERTY()
	float AtmosphereScaleMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Atmosphere", meta = (DisplayName = "AtmosphereMaterial", EditCondition = "bHasAtmosphere"))
	TObjectPtr<UMaterialInterface> AtmosphereMaterial = nullptr;

};
