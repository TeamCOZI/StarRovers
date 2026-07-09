#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Celestial/SRCelestialBodyData.h"
#include "SRStarDataAsset.generated.h"

class UMaterialInterface;
class UStaticMesh;

UCLASS(BlueprintType)
class STARROVERS_API USRStarDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	USRStarDataAsset();

	FSRCelestialBodyData BuildData() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity", meta = (DisplayName = "VariableName"))
	FText VariableName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity", meta = (DisplayName = "BodyCategory"))
	ESRCelestialBodyCategory BodyCategory = ESRCelestialBodyCategory::Star;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|CelestialBody", meta = (DisplayName = "Scale", ClampMin = "0.0"))
	float Scale = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|CelestialBody", meta = (DisplayName = "StaticMesh"))
	TObjectPtr<UStaticMesh> StaticMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|CelestialBody", meta = (DisplayName = "Material"))
	TObjectPtr<UMaterialInterface> Material = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "Mass", ClampMin = "0.0"))
	float Mass = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "GravityRatio", ClampMin = "0.0"))
	float GravityRatio = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Gravity", meta = (DisplayName = "GravityRadiusRatio", ClampMin = "0.0"))
	float GravityRadiusRatio = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|GenerationSeed", meta = (DisplayName = "GenerationSeed", ClampMin = "0"))
	int32 GenerationSeed = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|GenerationSeed", meta = (DisplayName = "bRandomizeGenerationSeedEachRun"))
	bool bRandomizeGenerationSeedEachRun = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Star", meta = (DisplayName = "StarPointLightIntensity", ClampMin = "0.0"))
	float StarPointLightIntensity = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Star", meta = (DisplayName = "StarPointLightColor"))
	FLinearColor StarPointLightColor = FLinearColor(1.0f, 0.956f, 0.84f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Fuel", meta = (DisplayName = "InitialStoredStellarFuel", ClampMin = "0.0", ToolTip = "Fuel health for each stellar evolution stage. The star starts as a main sequence star with this fuel, then refills to this amount when it becomes a red giant."))
	double InitialStoredStellarFuel = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Star|Fuel", meta = (DisplayName = "InitialStellarFuelDecreasePerSecond", ClampMin = "0.0", ToolTip = "Fuel health removed on the first one-second stellar fuel tick. Later ticks multiply the previous decrease by (200 + previous second index) percent."))
	double InitialStellarFuelDecreasePerSecond = 50.0;
};
