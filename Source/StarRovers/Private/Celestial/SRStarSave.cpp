#include "Celestial/SRStar.h"

#include "Simulation/SRStellarDemandModel.h"

namespace
{
	bool IsFiniteNonNegative(double Value)
	{
		return FMath::IsFinite(Value) && Value >= 0.0;
	}

	bool ValidateRunProgress(
		const FSRStellarRunProgress& Progress,
		FString& OutFailureReason)
	{
		const UEnum* PhaseEnum = StaticEnum<ESRStellarRunPhase>();
		const UEnum* OutcomeEnum = StaticEnum<ESRStellarRunOutcome>();
		if (!PhaseEnum
			|| !OutcomeEnum
			|| !PhaseEnum->IsValidEnumValue(static_cast<int64>(Progress.Phase))
			|| !OutcomeEnum->IsValidEnumValue(static_cast<int64>(Progress.Outcome))
			|| !IsFiniteNonNegative(Progress.TotalDeliveredFuel)
			|| !IsFiniteNonNegative(Progress.RecentIncomePerSecond)
			|| !IsFiniteNonNegative(Progress.SustainedIncomeProgressSeconds)
			|| !IsFiniteNonNegative(Progress.ElapsedSimulationSeconds)
			|| !FMath::IsFinite(Progress.CompletionSimulationSeconds))
		{
			OutFailureReason = TEXT("Star save contains invalid Run progress.");
			return false;
		}
		return true;
	}
}

void ASRStar::ExportRuntimeSaveData(FSRStellarRuntimeSaveData& OutSaveData) const
{
	OutSaveData = FSRStellarRuntimeSaveData();
	OutSaveData.bUsesStellarPressureCurveV2 = bUsesStellarPressureCurveV2;
	OutSaveData.EvolutionStage = StellarEvolutionStage;
	OutSaveData.bSupernovaGameOver = bSupernovaGameOver;
	OutSaveData.StoredFuel = StoredStellarFuel;
	OutSaveData.InitialStageFuel = InitialStageStellarFuel;
	OutSaveData.InitialFuelDecreasePerSecond = InitialStellarFuelDecreasePerSecond;
	OutSaveData.RequiredFuelPerCycle = RequiredStellarFuelPerCycle;
	OutSaveData.RequirementGrowthPerCycle = StellarFuelRequirementGrowthPerCycle;
	OutSaveData.LastFuelDecreaseRateCycleIndex = LastFuelDecreaseRateCycleIndex;
	OutSaveData.RedGiantPressure = RedGiantPressure;
	OutSaveData.RedGiantPressurePerMissingFuel = RedGiantPressurePerMissingFuel;
	OutSaveData.LastSettledSecondIndex = LastSettledSecondIndex;
	OutSaveData.LastSecondFuelConsumed = LastSecondFuelConsumed;
	OutSaveData.LastSecondFuelDecrease = LastSecondFuelDecrease;
	OutSaveData.LastSecondFuelDeficit = LastSecondFuelDeficit;
	OutSaveData.bLastSecondSurvived = bLastSecondSurvived;
	OutSaveData.FuelSecondAccumulator = StellarFuelSecondAccumulator;
	OutSaveData.ElapsedSimulationSeconds = StellarFuelElapsedSimulationSeconds;
	OutSaveData.LastFuelDeliverySimulationSeconds =
		LastFuelDeliverySimulationSeconds;
	OutSaveData.LastFuelDeliveryAmount = LastFuelDeliveryAmount;
	OutSaveData.LastFuelReserveGain = LastFuelReserveGain;
	OutSaveData.LastFuelReserveOverflow = LastFuelReserveOverflow;
	OutSaveData.TotalDeliveredFuel = TotalDeliveredFuel;
	OutSaveData.RecentFuelDeliverySamples = StellarFuelDeliverySamples;
	OutSaveData.RunProgress = StellarRunProgress;
}

bool ASRStar::ImportRuntimeSaveData(
	const FSRStellarRuntimeSaveData& SaveData,
	FString& OutFailureReason)
{
	OutFailureReason.Reset();
	if (!SaveData.IsSupportedVersion())
	{
		OutFailureReason = FString::Printf(
			TEXT("Unsupported Star save version %d."),
			SaveData.Version);
		return false;
	}
	const UEnum* StageEnum = StaticEnum<ESRStellarEvolutionStage>();
	if (!StageEnum
		|| !StageEnum->IsValidEnumValue(static_cast<int64>(SaveData.EvolutionStage))
		|| SaveData.bSupernovaGameOver
			!= (SaveData.EvolutionStage == ESRStellarEvolutionStage::Supernova)
		|| !IsFiniteNonNegative(SaveData.StoredFuel)
		|| !IsFiniteNonNegative(SaveData.InitialStageFuel)
		|| !IsFiniteNonNegative(SaveData.InitialFuelDecreasePerSecond)
		|| !IsFiniteNonNegative(SaveData.RequiredFuelPerCycle)
		|| !IsFiniteNonNegative(SaveData.RequirementGrowthPerCycle)
		|| SaveData.LastFuelDecreaseRateCycleIndex < 0
		|| !IsFiniteNonNegative(SaveData.RedGiantPressure)
		|| !IsFiniteNonNegative(SaveData.RedGiantPressurePerMissingFuel)
		|| SaveData.LastSettledSecondIndex < 0
		|| !IsFiniteNonNegative(SaveData.LastSecondFuelConsumed)
		|| !IsFiniteNonNegative(SaveData.LastSecondFuelDecrease)
		|| !IsFiniteNonNegative(SaveData.LastSecondFuelDeficit)
		|| !FMath::IsFinite(SaveData.FuelSecondAccumulator)
		|| SaveData.FuelSecondAccumulator < 0.0f
		|| SaveData.FuelSecondAccumulator >= 1.0f
		|| !IsFiniteNonNegative(SaveData.ElapsedSimulationSeconds)
		|| !FMath::IsFinite(SaveData.LastFuelDeliverySimulationSeconds)
		|| SaveData.LastFuelDeliverySimulationSeconds < -1.0
		|| SaveData.LastFuelDeliverySimulationSeconds
			> SaveData.ElapsedSimulationSeconds
		|| !IsFiniteNonNegative(SaveData.LastFuelDeliveryAmount)
		|| !IsFiniteNonNegative(SaveData.LastFuelReserveGain)
		|| !IsFiniteNonNegative(SaveData.LastFuelReserveOverflow)
		|| !IsFiniteNonNegative(SaveData.TotalDeliveredFuel)
		|| !ValidateRunProgress(SaveData.RunProgress, OutFailureReason))
	{
		if (OutFailureReason.IsEmpty())
		{
			OutFailureReason = TEXT("Star save contains an invalid pressure value.");
		}
		return false;
	}
	if (SaveData.Version >= FSRStellarRuntimeSaveData::StellarPressureV2Version
		&& (!FMath::IsNearlyEqual(
			SaveData.RunProgress.TotalDeliveredFuel,
			SaveData.TotalDeliveredFuel)
			|| !FMath::IsNearlyEqual(
				SaveData.RunProgress.ElapsedSimulationSeconds,
				SaveData.ElapsedSimulationSeconds)))
	{
		OutFailureReason = TEXT("Star save contains inconsistent Run progress totals.");
		return false;
	}

	double PreviousSampleTime = -1.0;
	double RecentSampleTotal = 0.0;
	for (const FSRStellarFuelDeliverySample& Sample :
		SaveData.RecentFuelDeliverySamples)
	{
		if (!FMath::IsFinite(Sample.SimulationTimeSeconds)
			|| Sample.SimulationTimeSeconds < 0.0
			|| Sample.SimulationTimeSeconds < PreviousSampleTime
			|| Sample.SimulationTimeSeconds > SaveData.ElapsedSimulationSeconds
			|| !IsFiniteNonNegative(Sample.FuelAmount))
		{
			OutFailureReason = TEXT("Star save contains an invalid fuel-delivery sample.");
			return false;
		}
		PreviousSampleTime = Sample.SimulationTimeSeconds;
		RecentSampleTotal += Sample.FuelAmount;
	}
	if (!FMath::IsFinite(RecentSampleTotal)
		|| RecentSampleTotal > SaveData.TotalDeliveredFuel + UE_DOUBLE_SMALL_NUMBER)
	{
		OutFailureReason = TEXT("Star save delivery samples exceed cumulative delivered fuel.");
		return false;
	}

	StellarEvolutionStage = SaveData.EvolutionStage;
	bSupernovaGameOver = SaveData.bSupernovaGameOver;
	LastFuelDecreaseRateCycleIndex = SaveData.LastFuelDecreaseRateCycleIndex;
	if (bUsesStellarPressureCurveV2)
	{
		InitialStageStellarFuel = StellarPressureRulesV2.FuelReserveCapacity;
		InitialStellarFuelDecreasePerSecond = StellarDemandCurveV2.InitialDemandPerSecond;
		RequiredStellarFuelPerCycle = FSRStellarDemandModel::CalculateDemandForCycleV2(
			StellarDemandCurveV2,
			LastFuelDecreaseRateCycleIndex);
		StellarFuelRequirementGrowthPerCycle =
			FSRStellarDemandModel::CalculateNextCycleMultiplierV2(
				StellarDemandCurveV2,
				LastFuelDecreaseRateCycleIndex);
		StoredStellarFuel = FSRStellarDemandModel::ClampFuelReserveV2(
			SaveData.StoredFuel,
			StellarPressureRulesV2);
	}
	else
	{
		InitialStageStellarFuel = SaveData.InitialStageFuel;
		InitialStellarFuelDecreasePerSecond =
			SaveData.InitialFuelDecreasePerSecond;
		RequiredStellarFuelPerCycle = SaveData.RequiredFuelPerCycle;
		StellarFuelRequirementGrowthPerCycle =
			SaveData.RequirementGrowthPerCycle;
		StoredStellarFuel = SaveData.StoredFuel;
		RedGiantPressure = SaveData.RedGiantPressure;
		RedGiantPressurePerMissingFuel =
			SaveData.RedGiantPressurePerMissingFuel;
	}
	if (bSupernovaGameOver)
	{
		StoredStellarFuel = 0.0;
	}

	LastSettledSecondIndex = SaveData.LastSettledSecondIndex;
	LastSecondFuelConsumed = SaveData.LastSecondFuelConsumed;
	LastSecondFuelDecrease = SaveData.LastSecondFuelDecrease;
	LastSecondFuelDeficit = SaveData.LastSecondFuelDeficit;
	bLastSecondSurvived = SaveData.bLastSecondSurvived;
	StellarFuelSecondAccumulator = SaveData.FuelSecondAccumulator;
	StellarFuelElapsedSimulationSeconds = SaveData.ElapsedSimulationSeconds;
	LastFuelDeliverySimulationSeconds =
		SaveData.LastFuelDeliverySimulationSeconds;
	LastFuelDeliveryAmount = SaveData.LastFuelDeliveryAmount;
	LastFuelReserveGain = SaveData.LastFuelReserveGain;
	LastFuelReserveOverflow = SaveData.LastFuelReserveOverflow;
	TotalDeliveredFuel = SaveData.TotalDeliveredFuel;
	StellarFuelDeliverySamples = SaveData.RecentFuelDeliverySamples;
	PruneStellarFuelDeliverySamples();

	StellarRunProgress = SaveData.RunProgress;
	if (bSupernovaGameOver)
	{
		StellarRunProgress = FSRStellarRunContractModel::Advance(
			StellarRunContract,
			FSRStellarRunProgress(),
			TotalDeliveredFuel,
			CalculateRecentFuelIncomePerSecond(),
			0.0,
			StellarFuelElapsedSimulationSeconds,
			true);
	}
	else if (!StellarRunProgress.HasEnded())
	{
		StellarRunProgress = FSRStellarRunContractModel::Advance(
			StellarRunContract,
			StellarRunProgress,
			TotalDeliveredFuel,
			CalculateRecentFuelIncomePerSecond(),
			0.0,
			StellarFuelElapsedSimulationSeconds,
			false);
	}
	RefreshStellarPressureState();
	ApplyStarAppearance();
	return true;
}
