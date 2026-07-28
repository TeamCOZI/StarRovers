#pragma once

#include "CoreMinimal.h"
#include "Celestial/SRStar.h"
#include "UI/SRUITheme.h"
#include "SRStellarSurvivalPresentation.generated.h"

class UWorld;

USTRUCT(BlueprintType)
struct STARROVERS_API FSRStellarFuelInboundProjection
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Stellar Survival")
	double FuelAmount = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Stellar Survival")
	float SecondsUntilArrival = 0.0f;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRStellarSurvivalSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	bool bIsValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	ESRStellarEvolutionStage EvolutionStage = ESRStellarEvolutionStage::MainSequence;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	double StoredFuel = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	double ReferenceFuelCapacity = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	float FuelProgressRatio = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	ESRStellarDemandPhaseV2 DemandPhase = ESRStellarDemandPhaseV2::Grace;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	float FuelPressureRatio = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	double RecentIncomePerSecond = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	double IncomeWindowSeconds = 30.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	double ConsumptionPerSecond = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	double NetFuelPerSecond = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	bool bHasFiniteRunway = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	double CurrentFuelRunwaySeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	double SecuredFuelRunwaySeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	int32 InboundMissileCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	double TotalInboundFuel = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	double NextInboundFuel = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	float NextInboundSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	bool bNextInboundArrivesBeforeDepletion = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	int32 CurrentCycleIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	float SecondsUntilNextCycle = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	double NextCycleConsumptionPerSecond = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	bool bNextCycleCreatesDeficit = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	bool bSimulationPaused = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	FSRStellarRunProgress RunProgress;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRStellarSurvivalPresentation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	FText SurvivalText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	FText ObjectiveText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	FText IncomeText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	FText ConsumptionText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	FText NetText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	FText InboundText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	FText CycleText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	FText DetailToolTipText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	ESRUIVisualState SurvivalVisualState = ESRUIVisualState::Disabled;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	ESRUIVisualState ObjectiveVisualState = ESRUIVisualState::Disabled;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	ESRUIVisualState IncomeVisualState = ESRUIVisualState::Neutral;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	ESRUIVisualState ConsumptionVisualState = ESRUIVisualState::Warning;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	ESRUIVisualState NetVisualState = ESRUIVisualState::Neutral;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	ESRUIVisualState InboundVisualState = ESRUIVisualState::Neutral;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Stellar Survival")
	ESRUIVisualState CycleVisualState = ESRUIVisualState::Info;
};

/** Pure survival math plus the small world query used by the always-visible Run rail. */
class STARROVERS_API FSRStellarSurvivalPresentationBuilder final
{
public:
	static FSRStellarSurvivalSnapshot BuildSnapshot(
		const FSRStellarFuelState& FuelState,
		const TArray<FSRStellarFuelInboundProjection>& InboundFuel,
		int32 CurrentCycleIndex,
		float SecondsUntilNextCycle,
		bool bSimulationPaused);

	static bool TryBuildWorldSnapshot(const UWorld* World, FSRStellarSurvivalSnapshot& OutSnapshot);

	static FSRStellarSurvivalPresentation BuildPresentation(
		const FSRStellarSurvivalSnapshot& Snapshot);
};
