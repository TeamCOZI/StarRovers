#pragma once

#include "CoreMinimal.h"
#include "Celestial/SRCelestialBodyCategory.h"
#include "Celestial/SRStellarEvolutionTypes.h"
#include "Simulation/SRNaturalStructureSpawnTypes.h"
#include "Surface/SRPlanetTerrainTypes.h"
#include "SRCelestialBodyData.generated.h"

class AActor;
class UMaterialInterface;
class UStaticMesh;
class USRDynamicMeshBaseDataAsset;
class USRPlanetTerrainProfileDataAsset;

USTRUCT(BlueprintType)
struct STARROVERS_API FSRToonOutlineSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Visual|Toon Outline", meta = (DisplayName = "bEnableToonOutline"))
	bool bEnableToonOutline = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Visual|Toon Outline", meta = (DisplayName = "ToonOutlineStencilValue", ClampMin = "1", ClampMax = "255", EditCondition = "bEnableToonOutline"))
	int32 ToonOutlineStencilValue = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Visual|Toon Outline", meta = (DisplayName = "ToonLineColor", EditCondition = "bEnableToonOutline"))
	FLinearColor ToonLineColor = FLinearColor::Black;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Visual|Toon Outline", meta = (DisplayName = "ToonLineThickness", ClampMin = "0.0", ClampMax = "0.25", EditCondition = "bEnableToonOutline"))
	float ToonLineThickness = 0.035f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Visual|Toon Outline", meta = (DisplayName = "bUseFeatureEdgeToonOutline", EditCondition = "bEnableToonOutline"))
	bool bUseFeatureEdgeToonOutline = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Visual|Toon Outline", meta = (DisplayName = "bDrawMaterialCellGridToonOutline", EditCondition = "bEnableToonOutline"))
	bool bDrawMaterialCellGridToonOutline = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Visual|Toon Outline", meta = (DisplayName = "FeatureEdgeAngleThresholdDegrees", ClampMin = "0.0", ClampMax = "90.0", EditCondition = "bEnableToonOutline && bUseFeatureEdgeToonOutline"))
	float FeatureEdgeAngleThresholdDegrees = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Visual|Toon Outline", meta = (DisplayName = "bApplyToonOutlineToOcean", EditCondition = "bEnableToonOutline"))
	bool bApplyToonOutlineToOcean = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Visual|Toon Outline", meta = (DisplayName = "bApplyToonOutlineToAtmosphere", EditCondition = "bEnableToonOutline"))
	bool bApplyToonOutlineToAtmosphere = false;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRCelestialBodyData
{
	GENERATED_BODY()

	FSRCelestialBodyData();

	UPROPERTY()
	FText VariableName;

	UPROPERTY()
	ESRCelestialBodyCategory BodyCategory = ESRCelestialBodyCategory::Unknown;

	UPROPERTY()
	TObjectPtr<AActor> ParentBody = nullptr;

	UPROPERTY()
	float OrbitRadius = 0.0f;

	UPROPERTY()
	float OrbitPeriod = 0.0f;

	UPROPERTY()
	float InitialAngle = 0.0f;

	UPROPERTY()
	float FocusZoomMultiplier = 3.0f;

	UPROPERTY()
	bool bCanConstruct = false;

	UPROPERTY()
	float GridLineThickness = 1.0f;

	UPROPERTY()
	FLinearColor GridLineColor = FLinearColor(0.15f, 0.85f, 1.0f, 1.0f);

	UPROPERTY()
	float GridLineOpacity = 1.0f;

	UPROPERTY()
	FLinearColor HoveredCellColor = FLinearColor(1.0f, 0.85f, 0.2f, 1.0f);

	UPROPERTY()
	FLinearColor SelectedCellColor = FLinearColor(0.25f, 1.0f, 0.35f, 1.0f);

	UPROPERTY()
	FLinearColor OccupiedCellColor = FLinearColor(1.0f, 0.35f, 0.35f, 1.0f);

	UPROPERTY()
	float SurfaceGridHeightOffset = 0.0f;

	UPROPERTY()
	float Scale = 1000.0f;

	UPROPERTY()
	TObjectPtr<UStaticMesh> StaticMesh = nullptr;

	UPROPERTY()
	TObjectPtr<USRDynamicMeshBaseDataAsset> DynamicMeshBaseDataAsset = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> Material = nullptr;

	UPROPERTY()
	FSRToonOutlineSettings ToonOutlineSettings;

	UPROPERTY()
	float Mass = 2000.0f;

	UPROPERTY()
	float StarPointLightIntensity = -1.0f;

	UPROPERTY()
	FLinearColor StarPointLightColor = FLinearColor(1.0f, 0.956f, 0.84f, 1.0f);

	UPROPERTY()
	double InitialStoredStellarFuel = 0.0;

	UPROPERTY()
	double InitialStellarFuelDecreasePerSecond = 50.0;

	UPROPERTY()
	double RequiredStellarFuelPerCycle = 10.0;

	UPROPERTY()
	double StellarFuelRequirementGrowthPerCycle = 1.0;

	UPROPERTY()
	double InitialRedGiantPressure = 0.0;

	UPROPERTY()
	double RedGiantPressurePerMissingFuel = 1.0;

	UPROPERTY()
	int32 GenerationSeed = 1000;

	UPROPERTY()
	bool bRandomizeGenerationSeedEachRun = false;

	UPROPERTY()
	FSRDynamicMeshGeneration DynamicMeshGeneration;

	UPROPERTY()
	TObjectPtr<USRPlanetTerrainProfileDataAsset> TerrainProfileDataAsset = nullptr;

	UPROPERTY()
	TArray<FSRNaturalStructureSpawnRuleOverride> ProfileNaturalStructureSpawnRuleOverrides;

	UPROPERTY()
	bool bHasOcean = false;

	UPROPERTY()
	TObjectPtr<USRDynamicMeshBaseDataAsset> OceanDynamicMeshBaseDataAsset = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> OceanMaterial = nullptr;

	UPROPERTY()
	float OceanScaleMultiplier = 1.0f;

	UPROPERTY()
	bool bHasAtmosphere = false;

	UPROPERTY()
	TObjectPtr<USRDynamicMeshBaseDataAsset> AtmosphereDynamicMeshBaseDataAsset = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> AtmosphereMaterial = nullptr;

	UPROPERTY()
	float AtmosphereScaleMultiplier = 1.0f;

	UPROPERTY()
	bool ShowOrbitLine = true;

	UPROPERTY()
	FLinearColor OrbitLineColor = FLinearColor(0.2f, 0.75f, 1.0f, 1.0f);

	UPROPERTY()
	float OrbitLineOpacity = 0.85f;

	UPROPERTY()
	int32 OrbitLineSegments = 96;

	UPROPERTY()
	float OrbitLineThickness = 20.0f;

	UPROPERTY()
	float GravityRatio = 1.0f;

	UPROPERTY()
	float GravityRadiusRatio = 10.0f;

	UPROPERTY()
	bool ShowGravityLine = true;

	UPROPERTY()
	FLinearColor GravityLineColor = FLinearColor(0.45f, 1.0f, 0.45f, 1.0f);

	UPROPERTY()
	float GravityLineOpacity = 0.85f;

	UPROPERTY()
	int32 GravityLineSegments = 96;

	UPROPERTY()
	float GravityLineThickness = 20.0f;

	UPROPERTY()
	bool ShowRotationAxisLine = true;

	UPROPERTY()
	FLinearColor RotationAxisLineColor = FLinearColor(1.0f, 0.9f, 0.2f, 1.0f);

	UPROPERTY()
	float RotationAxisLineOpacity = 0.95f;

	UPROPERTY()
	float RotationAxisLineThickness = 18.0f;

	UPROPERTY()
	float RotationAxisLineLengthMultiplier = 1.25f;
};
