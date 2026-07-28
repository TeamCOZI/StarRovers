#include "Simulation/SRStellarRunContract.h"

namespace
{
	double SanitizeRunContractNonNegative(double Value)
	{
		return FMath::IsFinite(Value) ? FMath::Max(0.0, Value) : 0.0;
	}

	float ResolveRangeProgress(double Value, double Minimum, double Maximum)
	{
		if (Maximum <= Minimum + UE_DOUBLE_SMALL_NUMBER)
		{
			return Value >= Maximum ? 1.0f : 0.0f;
		}
		return FMath::Clamp(static_cast<float>((Value - Minimum) / (Maximum - Minimum)), 0.0f, 1.0f);
	}

	ESRStellarRunPhase ResolvePhase(
		const FSRStellarRunContract& Contract,
		double TotalDeliveredFuel)
	{
		if (TotalDeliveredFuel < Contract.EmergencyDeliveryTarget)
		{
			return ESRStellarRunPhase::EmergencyIgnition;
		}
		if (TotalDeliveredFuel < Contract.SustainedSupplyDeliveryTarget)
		{
			return ESRStellarRunPhase::SustainedSupply;
		}
		return ESRStellarRunPhase::FinalStabilization;
	}
}

FSRStellarRunContract FSRStellarRunContractModel::Sanitize(
	const FSRStellarRunContract& Contract)
{
	FSRStellarRunContract Result = Contract;
	Result.EmergencyDeliveryTarget = SanitizeRunContractNonNegative(Contract.EmergencyDeliveryTarget);
	Result.SustainedSupplyDeliveryTarget = FMath::Max(
		Result.EmergencyDeliveryTarget,
		SanitizeRunContractNonNegative(Contract.SustainedSupplyDeliveryTarget));
	Result.VictoryDeliveryTarget = FMath::Max(
		Result.SustainedSupplyDeliveryTarget,
		SanitizeRunContractNonNegative(Contract.VictoryDeliveryTarget));
	Result.VictoryRequiredIncomePerSecond = SanitizeRunContractNonNegative(
		Contract.VictoryRequiredIncomePerSecond);
	Result.VictoryRequiredSustainSeconds = SanitizeRunContractNonNegative(
		Contract.VictoryRequiredSustainSeconds);
	Result.TargetRunDurationSeconds = SanitizeRunContractNonNegative(Contract.TargetRunDurationSeconds);
	return Result;
}

FSRStellarRunProgress FSRStellarRunContractModel::MakeInitialProgress(
	const FSRStellarRunContract& Contract)
{
	return Advance(
		Contract,
		FSRStellarRunProgress(),
		0.0,
		0.0,
		0.0,
		0.0,
		false);
}

FSRStellarRunProgress FSRStellarRunContractModel::Advance(
	const FSRStellarRunContract& Contract,
	const FSRStellarRunProgress& PreviousProgress,
	double TotalDeliveredFuel,
	double RecentIncomePerSecond,
	double DeltaSimulationSeconds,
	double ElapsedSimulationSeconds,
	bool bDefeatTriggered)
{
	const FSRStellarRunContract SafeContract = Sanitize(Contract);
	if (PreviousProgress.HasEnded())
	{
		return PreviousProgress;
	}

	FSRStellarRunProgress Result = PreviousProgress;
	Result.bFiniteVictoryEnabled = SafeContract.bFiniteVictoryEnabled;
	Result.TotalDeliveredFuel = SanitizeRunContractNonNegative(TotalDeliveredFuel);
	Result.RecentIncomePerSecond = SanitizeRunContractNonNegative(RecentIncomePerSecond);
	Result.RequiredIncomePerSecond = SafeContract.VictoryRequiredIncomePerSecond;
	Result.RequiredSustainSeconds = SafeContract.VictoryRequiredSustainSeconds;
	Result.VictoryDeliveryTarget = SafeContract.VictoryDeliveryTarget;
	Result.ElapsedSimulationSeconds = SanitizeRunContractNonNegative(ElapsedSimulationSeconds);
	Result.TargetRunDurationSeconds = SafeContract.TargetRunDurationSeconds;

	if (bDefeatTriggered)
	{
		Result.Outcome = ESRStellarRunOutcome::Defeat;
		Result.CompletionSimulationSeconds = Result.ElapsedSimulationSeconds;
		return Result;
	}

	Result.Phase = ResolvePhase(SafeContract, Result.TotalDeliveredFuel);
	Result.CurrentDeliveryTarget = ResolveDeliveryTarget(SafeContract, Result.Phase);
	Result.bDeliveryTargetMet =
		Result.TotalDeliveredFuel + UE_DOUBLE_SMALL_NUMBER >= SafeContract.VictoryDeliveryTarget;
	Result.bIncomeRequirementMet =
		Result.RecentIncomePerSecond + UE_DOUBLE_SMALL_NUMBER
			>= SafeContract.VictoryRequiredIncomePerSecond;

	if (Result.Phase == ESRStellarRunPhase::FinalStabilization
		&& Result.bDeliveryTargetMet
		&& Result.bIncomeRequirementMet)
	{
		Result.SustainedIncomeProgressSeconds = FMath::Min(
			SafeContract.VictoryRequiredSustainSeconds,
			SanitizeRunContractNonNegative(PreviousProgress.SustainedIncomeProgressSeconds)
				+ SanitizeRunContractNonNegative(DeltaSimulationSeconds));
	}
	else
	{
		Result.SustainedIncomeProgressSeconds = 0.0;
	}

	const bool bSustainTargetMet =
		Result.SustainedIncomeProgressSeconds + UE_DOUBLE_SMALL_NUMBER
			>= SafeContract.VictoryRequiredSustainSeconds;
	if (SafeContract.bFiniteVictoryEnabled
		&& Result.bDeliveryTargetMet
		&& Result.bIncomeRequirementMet
		&& bSustainTargetMet)
	{
		Result.Outcome = ESRStellarRunOutcome::Victory;
		Result.Phase = ESRStellarRunPhase::Complete;
		Result.CurrentDeliveryTarget = SafeContract.VictoryDeliveryTarget;
		Result.CompletionSimulationSeconds = Result.ElapsedSimulationSeconds;
	}

	switch (Result.Phase)
	{
	case ESRStellarRunPhase::EmergencyIgnition:
		Result.PhaseProgressRatio = ResolveRangeProgress(
			Result.TotalDeliveredFuel,
			0.0,
			SafeContract.EmergencyDeliveryTarget);
		break;
	case ESRStellarRunPhase::SustainedSupply:
		Result.PhaseProgressRatio = ResolveRangeProgress(
			Result.TotalDeliveredFuel,
			SafeContract.EmergencyDeliveryTarget,
			SafeContract.SustainedSupplyDeliveryTarget);
		break;
	case ESRStellarRunPhase::FinalStabilization:
		Result.PhaseProgressRatio = Result.bDeliveryTargetMet
			? ResolveRangeProgress(
				Result.SustainedIncomeProgressSeconds,
				0.0,
				SafeContract.VictoryRequiredSustainSeconds)
			: ResolveRangeProgress(
				Result.TotalDeliveredFuel,
				SafeContract.SustainedSupplyDeliveryTarget,
				SafeContract.VictoryDeliveryTarget);
		break;
	case ESRStellarRunPhase::Complete:
		Result.PhaseProgressRatio = 1.0f;
		break;
	default:
		Result.PhaseProgressRatio = 0.0f;
		break;
	}

	Result.OverallDeliveryProgressRatio = ResolveRangeProgress(
		Result.TotalDeliveredFuel,
		0.0,
		SafeContract.VictoryDeliveryTarget);
	Result.SustainProgressRatio = ResolveRangeProgress(
		Result.SustainedIncomeProgressSeconds,
		0.0,
		SafeContract.VictoryRequiredSustainSeconds);
	return Result;
}

double FSRStellarRunContractModel::ResolveDeliveryTarget(
	const FSRStellarRunContract& Contract,
	ESRStellarRunPhase Phase)
{
	const FSRStellarRunContract SafeContract = Sanitize(Contract);
	switch (Phase)
	{
	case ESRStellarRunPhase::EmergencyIgnition:
		return SafeContract.EmergencyDeliveryTarget;
	case ESRStellarRunPhase::SustainedSupply:
		return SafeContract.SustainedSupplyDeliveryTarget;
	case ESRStellarRunPhase::FinalStabilization:
	case ESRStellarRunPhase::Complete:
		return SafeContract.VictoryDeliveryTarget;
	default:
		return SafeContract.VictoryDeliveryTarget;
	}
}
