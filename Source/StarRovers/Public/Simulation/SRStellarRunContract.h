#pragma once

#include "CoreMinimal.h"
#include "SRStellarRunContract.generated.h"

UENUM(BlueprintType)
enum class ESRStellarRunPhase : uint8
{
	EmergencyIgnition UMETA(DisplayName = "Emergency Ignition"),
	SustainedSupply UMETA(DisplayName = "Sustained Supply"),
	FinalStabilization UMETA(DisplayName = "Final Stabilization"),
	Complete UMETA(DisplayName = "Complete"),
};

UENUM(BlueprintType)
enum class ESRStellarRunOutcome : uint8
{
	InProgress UMETA(DisplayName = "In Progress"),
	Victory UMETA(DisplayName = "Victory"),
	Defeat UMETA(DisplayName = "Defeat"),
};

/**
 * Authoritative finite-Run objective. Fuel consumption remains a separate
 * pressure system; this contract only decides objective progress and outcome.
 */
USTRUCT(BlueprintType)
struct STARROVERS_API FSRStellarRunContract
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Contract")
	bool bFiniteVictoryEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Contract", meta = (ClampMin = "0.0"))
	double EmergencyDeliveryTarget = 5000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Contract", meta = (ClampMin = "0.0"))
	double SustainedSupplyDeliveryTarget = 25000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Contract", meta = (ClampMin = "0.0"))
	double VictoryDeliveryTarget = 100000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Contract", meta = (ClampMin = "0.0"))
	double VictoryRequiredIncomePerSecond = 100.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Contract", meta = (ClampMin = "0.0"))
	double VictoryRequiredSustainSeconds = 30.0;

	/** Informational balancing target used by UI, telemetry, and automated simulations. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Contract", meta = (ClampMin = "0.0"))
	double TargetRunDurationSeconds = 1800.0;
};

/** Runtime progress derived from the contract and authoritative Star samples. */
USTRUCT(BlueprintType)
struct STARROVERS_API FSRStellarRunProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Contract")
	ESRStellarRunPhase Phase = ESRStellarRunPhase::EmergencyIgnition;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Contract")
	ESRStellarRunOutcome Outcome = ESRStellarRunOutcome::InProgress;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Contract")
	bool bFiniteVictoryEnabled = true;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Contract")
	double TotalDeliveredFuel = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Contract")
	double CurrentDeliveryTarget = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Contract")
	double VictoryDeliveryTarget = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Contract")
	double RecentIncomePerSecond = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Contract")
	double RequiredIncomePerSecond = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Contract")
	double SustainedIncomeProgressSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Contract")
	double RequiredSustainSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Contract")
	double ElapsedSimulationSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Contract")
	double CompletionSimulationSeconds = -1.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Contract")
	double TargetRunDurationSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Contract")
	float PhaseProgressRatio = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Contract")
	float OverallDeliveryProgressRatio = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Contract")
	float SustainProgressRatio = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Contract")
	bool bDeliveryTargetMet = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Contract")
	bool bIncomeRequirementMet = false;

	bool HasEnded() const
	{
		return Outcome != ESRStellarRunOutcome::InProgress;
	}
};

/** Pure deterministic evaluator shared by runtime, UI fixtures, and tests. */
class STARROVERS_API FSRStellarRunContractModel final
{
public:
	static FSRStellarRunContract Sanitize(const FSRStellarRunContract& Contract);
	static FSRStellarRunProgress MakeInitialProgress(const FSRStellarRunContract& Contract);
	static FSRStellarRunProgress Advance(
		const FSRStellarRunContract& Contract,
		const FSRStellarRunProgress& PreviousProgress,
		double TotalDeliveredFuel,
		double RecentIncomePerSecond,
		double DeltaSimulationSeconds,
		double ElapsedSimulationSeconds,
		bool bDefeatTriggered);

	static double ResolveDeliveryTarget(
		const FSRStellarRunContract& Contract,
		ESRStellarRunPhase Phase);
};
