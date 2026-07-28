#pragma once

#include "CoreMinimal.h"
#include "Celestial/SRStellarEvolutionTypes.h"
#include "Simulation/SRStellarDemandModel.h"
#include "Simulation/SRStellarRunContract.h"
#include "SRRunBalanceSimulation.generated.h"

UENUM(BlueprintType)
enum class ESRRunBalanceDemandCurve : uint8
{
	LegacyExponential UMETA(DisplayName = "Legacy Exponential"),
	Flat UMETA(DisplayName = "Flat"),
	StellarPressureV2 UMETA(DisplayName = "Stellar Pressure V2"),
};

/**
 * A deterministic approximation of fuel reaching the Star. Start and end are
 * production times; transit delay shifts deliveries without changing output.
 */
USTRUCT(BlueprintType)
struct STARROVERS_API FSRRunBalanceSupplyStage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Balance")
	double StartTimeSeconds = 0.0;

	/** Zero or a value below Start means the stage remains active to scenario end. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Balance")
	double EndTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Balance")
	double FuelPerSecond = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Balance")
	double DeliveryIntervalSeconds = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Balance")
	double TransitDelaySeconds = 0.0;

	/** Represents Capacity throttling or intentional uptime without changing recipes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Balance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double OperationalSpeedFactor = 1.0;

	/** Zero is unlimited; positive values stop this source after exactly N batches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Balance", meta = (ClampMin = "0"))
	int32 MaximumDeliveryCount = 0;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRRunBalanceScenario
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Balance")
	FName ScenarioId = TEXT("Reference");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Balance", meta = (ClampMin = "1.0"))
	double DurationSeconds = 2100.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Balance", meta = (ClampMin = "1.0"))
	double OutputSampleIntervalSeconds = 30.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Balance", meta = (ClampMin = "0.0"))
	double InitialStageFuel = 1000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Balance", meta = (ClampMin = "0.0"))
	double StartingStoredFuel = 1000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Balance", meta = (ClampMin = "0.0"))
	double InitialDemandPerSecond = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Balance", meta = (ClampMin = "1.0"))
	double SecondsPerCycle = 60.0;

	/** Absolute cycle used when projecting forward from a live Run. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Balance", meta = (ClampMin = "0"))
	int32 StartingCycleIndex = 0;

	/** Zero uses a complete cycle; live projections pass the remaining partial cycle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Balance", meta = (ClampMin = "0.0"))
	double FirstCycleDurationSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Balance", meta = (ClampMin = "1.0"))
	double IncomeWindowSeconds = 30.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Balance")
	ESRRunBalanceDemandCurve DemandCurve = ESRRunBalanceDemandCurve::StellarPressureV2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Balance")
	FSRStellarDemandCurveV2 DemandCurveV2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Balance")
	FSRStellarPressureRulesV2 PressureRulesV2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Balance")
	ESRStellarEvolutionStage StartingEvolutionStage = ESRStellarEvolutionStage::MainSequence;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Balance")
	FSRStellarRunContract Contract;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Balance")
	bool bResumeRunProgress = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Balance", meta = (EditCondition = "bResumeRunProgress"))
	FSRStellarRunProgress StartingRunProgress;

	/** Seeds the rolling window when projecting forward from a live snapshot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Balance", meta = (ClampMin = "0.0"))
	double InitialObservedIncomePerSecond = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Balance")
	TArray<FSRRunBalanceSupplyStage> SupplyStages;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRRunBalanceTimelineSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Balance")
	double SimulationSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Balance")
	double StoredFuel = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Balance")
	double DemandPerSecond = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Balance")
	double RecentIncomePerSecond = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Balance")
	double TotalDeliveredFuel = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Balance")
	ESRStellarEvolutionStage EvolutionStage = ESRStellarEvolutionStage::MainSequence;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Balance")
	ESRStellarRunPhase RunPhase = ESRStellarRunPhase::EmergencyIgnition;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Balance")
	ESRStellarRunOutcome Outcome = ESRStellarRunOutcome::InProgress;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRRunBalanceResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Balance")
	FName ScenarioId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Balance")
	ESRStellarRunOutcome Outcome = ESRStellarRunOutcome::InProgress;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Balance")
	double SimulatedUntilSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Balance")
	double EmergencyIgnitionCompletedSeconds = -1.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Balance")
	double SustainedSupplyCompletedSeconds = -1.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Balance")
	double CompletionSeconds = -1.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Balance")
	double TotalDeliveredFuel = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Balance")
	double AverageDeliveredFuelPerSecond = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Balance")
	double MinimumStoredFuel = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Balance")
	double PeakDemandPerSecond = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Balance")
	int32 StellarStageTransitionCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Balance")
	int32 SupplyDeliveryCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Balance")
	int32 ExhaustedSupplyStageCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Balance")
	double FirstSupplyExhaustionSeconds = -1.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Balance")
	bool bCompletedInsideTargetWindow = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Balance")
	TArray<FSRRunBalanceTimelineSample> Timeline;
};

/** Fixed one-second evaluator. It never reads World state or random numbers. */
class STARROVERS_API FSRRunBalanceSimulator final
{
public:
	static FSRRunBalanceScenario SanitizeScenario(const FSRRunBalanceScenario& Scenario);
	static FSRRunBalanceResult Simulate(const FSRRunBalanceScenario& Scenario);
};
