#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SRSimulationSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Star Rovers Simulation"))
class STARROVERS_API USRSimulationSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Time Control", meta = (DisplayName = "Seconds Per Period", ClampMin = "0.01", UIMin = "0.01"))
	float SecondsPerPeriod = 20.0f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Augment", meta = (DisplayName = "Augment Interval Cycles", ClampMin = "1", UIMin = "1"))
	int32 AugmentIntervalCycles = 3;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Augment", meta = (DisplayName = "Choices Per Offer", ClampMin = "1", UIMin = "1"))
	int32 AugmentChoicesPerOffer = 3;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Augment|Rarity", meta = (DisplayName = "Basic Chance Percent", ClampMin = "0.0", UIMin = "0.0"))
	float AugmentBasicChancePercent = 60.0f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Augment|Rarity", meta = (DisplayName = "Advanced Chance Percent", ClampMin = "0.0", UIMin = "0.0"))
	float AugmentAdvancedChancePercent = 35.0f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Augment|Rarity", meta = (DisplayName = "High Tech Base Chance Percent", ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0"))
	float AugmentHighTechBaseChancePercent = 5.0f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Augment|Rarity", meta = (DisplayName = "High Tech Pity Increase Percent", ClampMin = "0.0", UIMin = "0.0"))
	float AugmentHighTechPityIncreasePercent = 0.5f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Augment|Rarity", meta = (DisplayName = "High Tech Pity Cap Percent", ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0"))
	float AugmentHighTechPityCapPercent = 30.0f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Augment", meta = (DisplayName = "Pause Simulation During Choice"))
	bool bPauseSimulationDuringAugmentChoice = true;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Augment", meta = (DisplayName = "Augment Random Seed"))
	int32 AugmentRandomSeed = 47219;
};
