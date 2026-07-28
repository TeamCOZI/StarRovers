#include "Simulation/SRRunBalanceSimulation.h"

#include "Simulation/SRStellarDemandModel.h"

namespace
{
	constexpr double MaximumBalanceSimulationSeconds = 24.0 * 60.0 * 60.0;
	constexpr double MaximumBalanceFuelValue = 1.0e15;

	struct FFuelDeliverySample
	{
		double SimulationSeconds = 0.0;
		double FuelAmount = 0.0;
	};

	double SanitizeNonNegative(double Value)
	{
		return FMath::IsFinite(Value) ? FMath::Max(0.0, Value) : 0.0;
	}

	bool IsInsideTargetWindow(
		double CompletionSeconds,
		const FSRStellarRunContract& Contract)
	{
		if (CompletionSeconds < 0.0 || Contract.TargetRunDurationSeconds <= 0.0)
		{
			return false;
		}
		return CompletionSeconds >= FMath::Max(0.0, Contract.TargetRunDurationSeconds - 300.0)
			&& CompletionSeconds <= Contract.TargetRunDurationSeconds + 300.0;
	}

	double CalculateDeliveryForSecond(
		const FSRRunBalanceSupplyStage& Stage,
		int32 LocalSecond,
		double ScenarioDuration)
	{
		const int32 FirstDeliverySecond = FMath::Max(
			1,
			FMath::CeilToInt(Stage.StartTimeSeconds + Stage.TransitDelaySeconds));
		const double EffectiveEnd = Stage.EndTimeSeconds > Stage.StartTimeSeconds
			? Stage.EndTimeSeconds + Stage.TransitDelaySeconds
			: ScenarioDuration + 1.0;
		if (LocalSecond < FirstDeliverySecond
			|| static_cast<double>(LocalSecond) >= EffectiveEnd)
		{
			return 0.0;
		}

		const int32 IntervalSeconds = FMath::Max(
			1,
			FMath::RoundToInt(Stage.DeliveryIntervalSeconds));
		if ((LocalSecond - FirstDeliverySecond) % IntervalSeconds != 0)
		{
			return 0.0;
		}
		const double DeliveredFuel = Stage.FuelPerSecond
			* static_cast<double>(IntervalSeconds)
			* Stage.OperationalSpeedFactor;
		return FMath::IsFinite(DeliveredFuel)
			? FMath::Clamp(DeliveredFuel, 0.0, MaximumBalanceFuelValue)
			: MaximumBalanceFuelValue;
	}

	double CalculateRecentIncome(
		const TArray<FFuelDeliverySample>& Samples,
		double CurrentSeconds,
		double WindowSeconds)
	{
		const double OldestRelevantSeconds = CurrentSeconds - WindowSeconds;
		double FuelInWindow = 0.0;
		for (const FFuelDeliverySample& Sample : Samples)
		{
			if (Sample.SimulationSeconds > OldestRelevantSeconds)
			{
				FuelInWindow += Sample.FuelAmount;
			}
		}
		return FuelInWindow / WindowSeconds;
	}

	void AddTimelineSample(
		FSRRunBalanceResult& Result,
		double SimulationSeconds,
		double StoredFuel,
		double DemandPerSecond,
		double RecentIncomePerSecond,
		double TotalDeliveredFuel,
		ESRStellarEvolutionStage EvolutionStage,
		const FSRStellarRunProgress& Progress)
	{
		if (!Result.Timeline.IsEmpty()
			&& FMath::IsNearlyEqual(Result.Timeline.Last().SimulationSeconds, SimulationSeconds))
		{
			Result.Timeline.Pop(EAllowShrinking::No);
		}
		FSRRunBalanceTimelineSample& Sample = Result.Timeline.AddDefaulted_GetRef();
		Sample.SimulationSeconds = SimulationSeconds;
		Sample.StoredFuel = StoredFuel;
		Sample.DemandPerSecond = DemandPerSecond;
		Sample.RecentIncomePerSecond = RecentIncomePerSecond;
		Sample.TotalDeliveredFuel = TotalDeliveredFuel;
		Sample.EvolutionStage = EvolutionStage;
		Sample.RunPhase = Progress.Phase;
		Sample.Outcome = Progress.Outcome;
	}
}

FSRRunBalanceScenario FSRRunBalanceSimulator::SanitizeScenario(
	const FSRRunBalanceScenario& Scenario)
{
	FSRRunBalanceScenario Result = Scenario;
	Result.DurationSeconds = FMath::Clamp(
		SanitizeNonNegative(Scenario.DurationSeconds),
		1.0,
		MaximumBalanceSimulationSeconds);
	Result.OutputSampleIntervalSeconds = FMath::Clamp(
		SanitizeNonNegative(Scenario.OutputSampleIntervalSeconds),
		1.0,
		Result.DurationSeconds);
	Result.InitialStageFuel = FMath::Min(
		SanitizeNonNegative(Scenario.InitialStageFuel),
		MaximumBalanceFuelValue);
	Result.StartingStoredFuel = FMath::Min(
		SanitizeNonNegative(Scenario.StartingStoredFuel),
		MaximumBalanceFuelValue);
	Result.InitialDemandPerSecond = FMath::Min(
		SanitizeNonNegative(Scenario.InitialDemandPerSecond),
		MaximumBalanceFuelValue);
	Result.SecondsPerCycle = FMath::Clamp(
		SanitizeNonNegative(Scenario.SecondsPerCycle),
		1.0,
		Result.DurationSeconds);
	Result.StartingCycleIndex = FMath::Max(0, Scenario.StartingCycleIndex);
	Result.FirstCycleDurationSeconds = SanitizeNonNegative(
		Scenario.FirstCycleDurationSeconds);
	if (Result.FirstCycleDurationSeconds <= UE_DOUBLE_SMALL_NUMBER
		|| Result.FirstCycleDurationSeconds > Result.SecondsPerCycle)
	{
		Result.FirstCycleDurationSeconds = Result.SecondsPerCycle;
	}
	Result.IncomeWindowSeconds = FMath::Clamp(
		SanitizeNonNegative(Scenario.IncomeWindowSeconds),
		1.0,
		MaximumBalanceSimulationSeconds);
	Result.InitialObservedIncomePerSecond = FMath::Min(
		SanitizeNonNegative(Scenario.InitialObservedIncomePerSecond),
		MaximumBalanceFuelValue);
	Result.Contract = FSRStellarRunContractModel::Sanitize(Scenario.Contract);
	Result.DemandCurveV2 = FSRStellarDemandModel::SanitizeCurveV2(
		Scenario.DemandCurveV2);
	Result.PressureRulesV2 = FSRStellarDemandModel::SanitizePressureRulesV2(
		Scenario.PressureRulesV2);
	if (Result.DemandCurve == ESRRunBalanceDemandCurve::StellarPressureV2)
	{
		Result.InitialStageFuel = Result.PressureRulesV2.FuelReserveCapacity;
		Result.StartingStoredFuel = FSRStellarDemandModel::ClampFuelReserveV2(
			Scenario.StartingStoredFuel,
			Result.PressureRulesV2);
		Result.InitialDemandPerSecond =
			FSRStellarDemandModel::CalculateDemandForCycleV2(
				Result.DemandCurveV2,
				Result.StartingCycleIndex);
	}
	for (FSRRunBalanceSupplyStage& Stage : Result.SupplyStages)
	{
		Stage.StartTimeSeconds = FMath::Min(
			SanitizeNonNegative(Stage.StartTimeSeconds),
			MaximumBalanceSimulationSeconds);
		Stage.EndTimeSeconds = FMath::Min(
			SanitizeNonNegative(Stage.EndTimeSeconds),
			MaximumBalanceSimulationSeconds);
		Stage.FuelPerSecond = FMath::Min(
			SanitizeNonNegative(Stage.FuelPerSecond),
			MaximumBalanceFuelValue);
		Stage.DeliveryIntervalSeconds = FMath::Clamp(
			SanitizeNonNegative(Stage.DeliveryIntervalSeconds),
			1.0,
			MaximumBalanceSimulationSeconds);
		Stage.TransitDelaySeconds = FMath::Min(
			SanitizeNonNegative(Stage.TransitDelaySeconds),
			MaximumBalanceSimulationSeconds);
		Stage.OperationalSpeedFactor = FMath::Clamp(
			SanitizeNonNegative(Stage.OperationalSpeedFactor),
			0.0,
			1.0);
		Stage.MaximumDeliveryCount = FMath::Max(0, Stage.MaximumDeliveryCount);
	}
	return Result;
}

FSRRunBalanceResult FSRRunBalanceSimulator::Simulate(
	const FSRRunBalanceScenario& Scenario)
{
	const FSRRunBalanceScenario SafeScenario = SanitizeScenario(Scenario);
	FSRRunBalanceResult Result;
	Result.ScenarioId = SafeScenario.ScenarioId;

	double StoredFuel = SafeScenario.StartingStoredFuel;
	double DemandPerSecond = SafeScenario.InitialDemandPerSecond;
	double TotalDeliveredFuel = SafeScenario.bResumeRunProgress
		? SanitizeNonNegative(SafeScenario.StartingRunProgress.TotalDeliveredFuel)
		: 0.0;
	double StartingSimulationSeconds = SafeScenario.bResumeRunProgress
		? FMath::Min(
			SanitizeNonNegative(SafeScenario.StartingRunProgress.ElapsedSimulationSeconds),
			MaximumBalanceSimulationSeconds)
		: 0.0;
	ESRStellarEvolutionStage EvolutionStage = SafeScenario.StartingEvolutionStage;
	FSRStellarRunProgress Progress = SafeScenario.bResumeRunProgress
		? SafeScenario.StartingRunProgress
		: FSRStellarRunContractModel::MakeInitialProgress(SafeScenario.Contract);
	TArray<FFuelDeliverySample> DeliverySamples;
	if (SafeScenario.InitialObservedIncomePerSecond > UE_DOUBLE_SMALL_NUMBER)
	{
		const int32 WholeWindowSeconds = FMath::Max(
			1,
			FMath::CeilToInt(SafeScenario.IncomeWindowSeconds));
		const double FuelPerSeedSample = SafeScenario.InitialObservedIncomePerSecond
			* SafeScenario.IncomeWindowSeconds
			/ static_cast<double>(WholeWindowSeconds);
		DeliverySamples.Reserve(WholeWindowSeconds);
		for (int32 Offset = WholeWindowSeconds - 1; Offset >= 0; --Offset)
		{
			FFuelDeliverySample& SeedSample = DeliverySamples.AddDefaulted_GetRef();
			SeedSample.SimulationSeconds = StartingSimulationSeconds
				- static_cast<double>(Offset);
			SeedSample.FuelAmount = FuelPerSeedSample;
		}
	}

	Result.MinimumStoredFuel = StoredFuel;
	Result.PeakDemandPerSecond = DemandPerSecond;
	AddTimelineSample(
		Result,
		StartingSimulationSeconds,
		StoredFuel,
		DemandPerSecond,
		SafeScenario.InitialObservedIncomePerSecond,
		TotalDeliveredFuel,
		EvolutionStage,
		Progress);
	if (Progress.HasEnded())
	{
		Result.Outcome = Progress.Outcome;
		Result.SimulatedUntilSeconds = StartingSimulationSeconds;
		Result.TotalDeliveredFuel = TotalDeliveredFuel;
		Result.CompletionSeconds = Progress.CompletionSimulationSeconds;
		Result.bCompletedInsideTargetWindow = Result.Outcome == ESRStellarRunOutcome::Victory
			&& IsInsideTargetWindow(Result.CompletionSeconds, SafeScenario.Contract);
		return Result;
	}

	const int32 DurationSeconds = FMath::Max(1, FMath::CeilToInt(SafeScenario.DurationSeconds));
	int32 CurrentCycleIndex = SafeScenario.StartingCycleIndex;
	double NextCycleBoundarySeconds = SafeScenario.FirstCycleDurationSeconds;
	const int32 OutputSampleSeconds = FMath::Max(
		1,
		FMath::RoundToInt(SafeScenario.OutputSampleIntervalSeconds));
	double DeliveredDuringSimulation = 0.0;
	TArray<int32> StageDeliveryCounts;
	StageDeliveryCounts.Init(0, SafeScenario.SupplyStages.Num());
	for (int32 LocalSecond = 1; LocalSecond <= DurationSeconds; ++LocalSecond)
	{
		const double SimulationSeconds = StartingSimulationSeconds
			+ static_cast<double>(LocalSecond);
		double DeliveredThisSecond = 0.0;
		for (int32 StageIndex = 0; StageIndex < SafeScenario.SupplyStages.Num(); ++StageIndex)
		{
			const FSRRunBalanceSupplyStage& Stage = SafeScenario.SupplyStages[StageIndex];
			if (Stage.MaximumDeliveryCount > 0
				&& StageDeliveryCounts[StageIndex] >= Stage.MaximumDeliveryCount)
			{
				continue;
			}
			const double StageDelivery = CalculateDeliveryForSecond(
				Stage,
				LocalSecond,
				SafeScenario.DurationSeconds);
			if (StageDelivery <= UE_DOUBLE_SMALL_NUMBER)
			{
				continue;
			}

			DeliveredThisSecond = FMath::Min(
				MaximumBalanceFuelValue,
				DeliveredThisSecond + StageDelivery);
			++StageDeliveryCounts[StageIndex];
			++Result.SupplyDeliveryCount;
			if (Stage.MaximumDeliveryCount > 0
				&& StageDeliveryCounts[StageIndex] == Stage.MaximumDeliveryCount)
			{
				++Result.ExhaustedSupplyStageCount;
				if (Result.FirstSupplyExhaustionSeconds < 0.0)
				{
					Result.FirstSupplyExhaustionSeconds = SimulationSeconds;
				}
			}
		}
		if (DeliveredThisSecond > UE_DOUBLE_SMALL_NUMBER)
		{
			StoredFuel = SafeScenario.DemandCurve == ESRRunBalanceDemandCurve::StellarPressureV2
				? FSRStellarDemandModel::ClampFuelReserveV2(
					StoredFuel + DeliveredThisSecond,
					SafeScenario.PressureRulesV2)
				: FMath::Min(
					MaximumBalanceFuelValue,
					StoredFuel + DeliveredThisSecond);
			TotalDeliveredFuel = FMath::Min(
				MaximumBalanceFuelValue,
				TotalDeliveredFuel + DeliveredThisSecond);
			DeliveredDuringSimulation = FMath::Min(
				MaximumBalanceFuelValue,
				DeliveredDuringSimulation + DeliveredThisSecond);
			FFuelDeliverySample& Sample = DeliverySamples.AddDefaulted_GetRef();
			Sample.SimulationSeconds = SimulationSeconds;
			Sample.FuelAmount = DeliveredThisSecond;
		}

		const double OldestRelevantSeconds =
			SimulationSeconds - SafeScenario.IncomeWindowSeconds;
		DeliverySamples.RemoveAll(
			[OldestRelevantSeconds](const FFuelDeliverySample& Sample)
			{
				return Sample.SimulationSeconds <= OldestRelevantSeconds;
			});

		StoredFuel = FMath::Max(0.0, StoredFuel - DemandPerSecond);
		Result.MinimumStoredFuel = FMath::Min(Result.MinimumStoredFuel, StoredFuel);
		bool bDefeatTriggered = false;
		if (StoredFuel <= UE_DOUBLE_SMALL_NUMBER)
		{
			if (EvolutionStage == ESRStellarEvolutionStage::MainSequence)
			{
				EvolutionStage = ESRStellarEvolutionStage::RedGiant;
				StoredFuel = SafeScenario.DemandCurve
					== ESRRunBalanceDemandCurve::StellarPressureV2
					? FSRStellarDemandModel::ResolveRedGiantEmergencyReserveV2(
						SafeScenario.PressureRulesV2)
					: SafeScenario.InitialStageFuel;
				++Result.StellarStageTransitionCount;
			}
			else
			{
				EvolutionStage = ESRStellarEvolutionStage::Supernova;
				bDefeatTriggered = true;
				++Result.StellarStageTransitionCount;
			}
		}

		const double RecentIncomePerSecond = CalculateRecentIncome(
			DeliverySamples,
			SimulationSeconds,
			SafeScenario.IncomeWindowSeconds);
		const ESRStellarRunPhase PreviousPhase = Progress.Phase;
		Progress = FSRStellarRunContractModel::Advance(
			SafeScenario.Contract,
			Progress,
			TotalDeliveredFuel,
			RecentIncomePerSecond,
			1.0,
			SimulationSeconds,
			bDefeatTriggered);
		if (PreviousPhase != Progress.Phase)
		{
			if (Progress.Phase == ESRStellarRunPhase::SustainedSupply
				&& Result.EmergencyIgnitionCompletedSeconds < 0.0)
			{
				Result.EmergencyIgnitionCompletedSeconds = SimulationSeconds;
			}
			else if (Progress.Phase == ESRStellarRunPhase::FinalStabilization
				&& Result.SustainedSupplyCompletedSeconds < 0.0)
			{
				Result.SustainedSupplyCompletedSeconds = SimulationSeconds;
			}
		}

		Result.MinimumStoredFuel = FMath::Min(Result.MinimumStoredFuel, StoredFuel);
		Result.PeakDemandPerSecond = FMath::Max(Result.PeakDemandPerSecond, DemandPerSecond);
		Result.SimulatedUntilSeconds = SimulationSeconds;
		const bool bShouldSample = LocalSecond % OutputSampleSeconds == 0
			|| Progress.HasEnded()
			|| LocalSecond == DurationSeconds;
		if (bShouldSample)
		{
			AddTimelineSample(
				Result,
				SimulationSeconds,
				StoredFuel,
				DemandPerSecond,
				RecentIncomePerSecond,
				TotalDeliveredFuel,
				EvolutionStage,
				Progress);
		}
		if (Progress.HasEnded())
		{
			break;
		}

		if (static_cast<double>(LocalSecond) + UE_DOUBLE_SMALL_NUMBER
			>= NextCycleBoundarySeconds)
		{
			do
			{
				++CurrentCycleIndex;
				NextCycleBoundarySeconds += SafeScenario.SecondsPerCycle;
			}
			while (static_cast<double>(LocalSecond) + UE_DOUBLE_SMALL_NUMBER
				>= NextCycleBoundarySeconds);

			if (SafeScenario.DemandCurve == ESRRunBalanceDemandCurve::LegacyExponential)
			{
				DemandPerSecond = FSRStellarDemandModel::CalculateLegacyNextCycleDemand(
					DemandPerSecond,
					CurrentCycleIndex);
			}
			else if (SafeScenario.DemandCurve
				== ESRRunBalanceDemandCurve::StellarPressureV2)
			{
				DemandPerSecond = FSRStellarDemandModel::CalculateDemandForCycleV2(
					SafeScenario.DemandCurveV2,
					CurrentCycleIndex);
			}
		}
	}

	Result.Outcome = Progress.Outcome;
	Result.TotalDeliveredFuel = TotalDeliveredFuel;
	const double SimulatedDuration = FMath::Max(
		1.0,
		Result.SimulatedUntilSeconds - StartingSimulationSeconds);
	Result.AverageDeliveredFuelPerSecond = DeliveredDuringSimulation / SimulatedDuration;
	if (Progress.HasEnded())
	{
		Result.CompletionSeconds = Progress.CompletionSimulationSeconds;
	}
	Result.bCompletedInsideTargetWindow = Result.Outcome == ESRStellarRunOutcome::Victory
		&& IsInsideTargetWindow(Result.CompletionSeconds, SafeScenario.Contract);
	return Result;
}
