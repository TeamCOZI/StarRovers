#include "Simulation/SRRunTelemetrySubsystem.h"

#include "Automation/SRFacilityNetworkComponent.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Celestial/SRStar.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Logistics/SRSpaceLogisticsSubsystem.h"
#include "Logistics/SRSpaceLogisticsStarFuelMissileProcessor.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "Simulation/SRRunBalanceSimulation.h"
#include "Simulation/SRSimulationSettings.h"
#include "Simulation/SRTimeControlSubsystem.h"
#include "Structure/SRStructureInstanceManagerComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogSRRunBalance, Log, All);

namespace
{
	double SanitizeNonNegative(double Value)
	{
		return FMath::IsFinite(Value) ? FMath::Max(0.0, Value) : 0.0;
	}

	float ResolveOperationalUtilization(const FSRRunTelemetrySnapshot& Snapshot)
	{
		if (Snapshot.OperationalCapacity <= 0)
		{
			return Snapshot.OperationalDemand > 0 ? 2.0f : 0.0f;
		}
		return static_cast<float>(Snapshot.OperationalDemand)
			/ static_cast<float>(Snapshot.OperationalCapacity);
	}

	const TCHAR* ResolveOutcomeLabel(ESRStellarRunOutcome Outcome)
	{
		switch (Outcome)
		{
		case ESRStellarRunOutcome::Victory:
			return TEXT("Victory");
		case ESRStellarRunOutcome::Defeat:
			return TEXT("Defeat");
		case ESRStellarRunOutcome::InProgress:
		default:
			return TEXT("InProgress");
		}
	}

	UWorld* FindRuntimeWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		UWorld* GameWorld = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* Candidate = Context.World();
			if (!IsValid(Candidate))
			{
				continue;
			}
			if (Context.WorldType == EWorldType::PIE)
			{
				return Candidate;
			}
			if (Context.WorldType == EWorldType::Game)
			{
				GameWorld = Candidate;
			}
		}
		return GameWorld;
	}

	USRRunTelemetrySubsystem* FindRuntimeTelemetry()
	{
		UWorld* World = FindRuntimeWorld();
		return IsValid(World) ? World->GetSubsystem<USRRunTelemetrySubsystem>() : nullptr;
	}

	void LogCurrentTelemetry()
	{
		if (USRRunTelemetrySubsystem* Telemetry = FindRuntimeTelemetry())
		{
			Telemetry->CaptureSnapshotNow();
			Telemetry->LogSummary();
			return;
		}
		UE_LOG(LogSRRunBalance, Warning, TEXT("[RunTelemetry] No active PIE or game World."));
	}

	void ResetCurrentTelemetry()
	{
		if (USRRunTelemetrySubsystem* Telemetry = FindRuntimeTelemetry())
		{
			Telemetry->ResetTelemetry();
			Telemetry->CaptureSnapshotNow();
			UE_LOG(LogSRRunBalance, Display, TEXT("[RunTelemetry] Session reset."));
		}
	}

	void ProjectCurrentFlatSupply()
	{
		USRRunTelemetrySubsystem* Telemetry = FindRuntimeTelemetry();
		FSRRunBalanceResult Result;
		if (!IsValid(Telemetry)
			|| !Telemetry->CaptureSnapshotNow()
			|| !Telemetry->BuildCurrentFlatSupplyProjection(Result))
		{
			UE_LOG(LogSRRunBalance, Warning,
				TEXT("[RunBalance] Current World does not have a projection-ready Star snapshot."));
			return;
		}

		UE_LOG(LogSRRunBalance, Display,
			TEXT("[RunBalance] Current flat-supply projection | Outcome=%s | Until=%.0fs | Completion=%.0fs | Delivered=%.0f | AvgIncome=%.1f/s | PeakDemand=%.1f/s | StageTransitions=%d"),
			ResolveOutcomeLabel(Result.Outcome),
			Result.SimulatedUntilSeconds,
			Result.CompletionSeconds,
			Result.TotalDeliveredFuel,
			Result.AverageDeliveredFuelPerSecond,
			Result.PeakDemandPerSecond,
			Result.StellarStageTransitionCount);
	}

	FAutoConsoleCommand LogTelemetryCommand(
		TEXT("sr.Balance.Telemetry.Report"),
		TEXT("Logs a bounded Run telemetry summary for the active PIE/game World."),
		FConsoleCommandDelegate::CreateStatic(&LogCurrentTelemetry));

	FAutoConsoleCommand ResetTelemetryCommand(
		TEXT("sr.Balance.Telemetry.Reset"),
		TEXT("Resets the active Run telemetry session without changing gameplay state."),
		FConsoleCommandDelegate::CreateStatic(&ResetCurrentTelemetry));

	FAutoConsoleCommand ProjectCurrentCommand(
		TEXT("sr.Balance.ProjectCurrent"),
		TEXT("Projects the current Star forward with its observed fuel income held flat."),
		FConsoleCommandDelegate::CreateStatic(&ProjectCurrentFlatSupply));
}

FString FSRRunTelemetrySummaryModel::ResolveBottleneckLabel(
	ESRRunTelemetryBottleneck Bottleneck)
{
	switch (Bottleneck)
	{
	case ESRRunTelemetryBottleneck::MissingPrimaryStar:
		return TEXT("Primary Star 없음");
	case ESRRunTelemetryBottleneck::StellarCollapse:
		return TEXT("항성 붕괴");
	case ESRRunTelemetryBottleneck::OperationalCapacity:
		return TEXT("Operational Capacity 포화");
	case ESRRunTelemetryBottleneck::StellarFuelProduction:
		return TEXT("항성 연료 생산 부재");
	case ESRRunTelemetryBottleneck::DeliveryPath:
		return TEXT("항성 연료 전달 경로 부재");
	case ESRRunTelemetryBottleneck::StellarFuelDeficit:
		return TEXT("항성 연료 수지 적자");
	case ESRRunTelemetryBottleneck::FinalThroughput:
		return TEXT("최종 안정화 유입 부족");
	case ESRRunTelemetryBottleneck::ResourceDepletion:
		return TEXT("핵심 자원 고갈");
	case ESRRunTelemetryBottleneck::None:
	default:
		return TEXT("명확한 병목 없음");
	}
}

FSRRunTelemetrySummary FSRRunTelemetrySummaryModel::BuildSummary(
	const TArray<FSRRunTelemetrySnapshot>& Samples)
{
	FSRRunTelemetrySummary Summary;
	Summary.SampleCount = Samples.Num();
	if (Samples.IsEmpty())
	{
		Summary.SummaryText = TEXT("Run telemetry sample 없음");
		return Summary;
	}

	const FSRRunTelemetrySnapshot& First = Samples[0];
	const FSRRunTelemetrySnapshot& Last = Samples.Last();
	Summary.bIsValid = Samples.ContainsByPredicate(
		[](const FSRRunTelemetrySnapshot& Sample)
		{
			return Sample.bHasPrimaryStar;
		});
	Summary.RecordedDurationSeconds = FMath::Max(
		0.0,
		Last.SimulationSeconds - First.SimulationSeconds);
	Summary.Outcome = Last.RunProgress.Outcome;
	Summary.MinimumStoredStellarFuel = TNumericLimits<double>::Max();
	double WeightedIncome = 0.0;
	double WeightedIncomeDuration = 0.0;
	int32 StarSampleCount = 0;
	bool bSawResourceReserve = false;
	Summary.MinimumResourceReserveRatio = 1.0f;
	const FSRRunTelemetrySnapshot* PreviousStarSample = nullptr;
	for (const FSRRunTelemetrySnapshot& Sample : Samples)
	{
		if (Sample.ProducedCardItemCount > 0
			&& Summary.Milestones.FirstCardProducedSeconds < 0.0)
		{
			Summary.Milestones.FirstCardProducedSeconds = Sample.SimulationSeconds;
		}
		if (Sample.ProducedStellarFuelItemCount > 0
			&& Summary.Milestones.FirstStellarFuelProducedSeconds < 0.0)
		{
			Summary.Milestones.FirstStellarFuelProducedSeconds = Sample.SimulationSeconds;
		}
		if (Sample.RunProgress.TotalDeliveredFuel > UE_DOUBLE_SMALL_NUMBER
			&& Summary.Milestones.FirstStellarFuelDeliveredSeconds < 0.0)
		{
			Summary.Milestones.FirstStellarFuelDeliveredSeconds = Sample.SimulationSeconds;
		}
		if (Sample.RunProgress.Phase >= ESRStellarRunPhase::SustainedSupply
			&& Summary.Milestones.EmergencyIgnitionCompletedSeconds < 0.0)
		{
			Summary.Milestones.EmergencyIgnitionCompletedSeconds = Sample.SimulationSeconds;
		}
		if (Sample.RunProgress.Phase >= ESRStellarRunPhase::FinalStabilization
			&& Summary.Milestones.SustainedSupplyCompletedSeconds < 0.0)
		{
			Summary.Milestones.SustainedSupplyCompletedSeconds = Sample.SimulationSeconds;
		}
		if (Sample.RunProgress.HasEnded()
			&& Summary.Milestones.RunCompletedSeconds < 0.0)
		{
			Summary.Milestones.RunCompletedSeconds =
				Sample.RunProgress.CompletionSimulationSeconds;
		}

		if (Sample.bHasPrimaryStar)
		{
			Summary.MinimumStoredStellarFuel = FMath::Min(
				Summary.MinimumStoredStellarFuel,
				Sample.StoredStellarFuel);
			Summary.PeakStellarConsumptionPerSecond = FMath::Max(
				Summary.PeakStellarConsumptionPerSecond,
				Sample.StellarConsumptionPerSecond);
			Summary.PeakStellarFuelIncomePerSecond = FMath::Max(
				Summary.PeakStellarFuelIncomePerSecond,
				Sample.RecentStellarFuelIncomePerSecond);
			Summary.PeakStellarFuelPressureRatio = FMath::Max(
				Summary.PeakStellarFuelPressureRatio,
				Sample.StellarFuelPressureRatio);
			if (PreviousStarSample)
			{
				const double IntervalSeconds = FMath::Max(
					0.0,
					Sample.SimulationSeconds - PreviousStarSample->SimulationSeconds);
				WeightedIncome += 0.5
					* (PreviousStarSample->RecentStellarFuelIncomePerSecond
						+ Sample.RecentStellarFuelIncomePerSecond)
					* IntervalSeconds;
				WeightedIncomeDuration += IntervalSeconds;
			}
			PreviousStarSample = &Sample;
			++StarSampleCount;
		}
		Summary.PeakOperationalUtilization = FMath::Max(
			Summary.PeakOperationalUtilization,
			ResolveOperationalUtilization(Sample));
		Summary.PeakThrottledFacilityCount = FMath::Max(
			Summary.PeakThrottledFacilityCount,
			Sample.ThrottledFacilityCount);
		Summary.PeakBlockedOrQueuedRouteCount = FMath::Max(
			Summary.PeakBlockedOrQueuedRouteCount,
			Sample.BlockedRouteCount + Sample.FleetQueuedRouteCount);
		if (Sample.ResourceReserve.bHasDeposits)
		{
			bSawResourceReserve = true;
			Summary.MinimumResourceReserveRatio = FMath::Min(
				Summary.MinimumResourceReserveRatio,
				Sample.ResourceReserve.RemainingRatio);
		}
	}
	if (StarSampleCount <= 0)
	{
		Summary.MinimumStoredStellarFuel = 0.0;
	}
	else
	{
		Summary.AverageStellarFuelIncomePerSecond = WeightedIncomeDuration > UE_DOUBLE_SMALL_NUMBER
			? WeightedIncome / WeightedIncomeDuration
			: PreviousStarSample->RecentStellarFuelIncomePerSecond;
	}
	Summary.FinalDeliveredFuel = Last.RunProgress.TotalDeliveredFuel;
	Summary.ProducedCardItemCount = Last.ProducedCardItemCount;
	Summary.ProducedStellarFuelItemCount = Last.ProducedStellarFuelItemCount;
	if (!bSawResourceReserve)
	{
		Summary.MinimumResourceReserveRatio = 0.0f;
	}
	Summary.FinalPotentialFuelBatchCount = Last.ResourceReserve.bPotentialFuelBatchesInfinite
		? -1
		: Last.ResourceReserve.PotentialFuelBatchCount;
	Summary.FinalLimitingReferenceCardId = Last.ResourceReserve.LimitingReferenceCardId;

	if (!Last.bHasPrimaryStar)
	{
		Summary.PrimaryBottleneck = ESRRunTelemetryBottleneck::MissingPrimaryStar;
	}
	else if (Last.RunProgress.Outcome == ESRStellarRunOutcome::Defeat)
	{
		Summary.PrimaryBottleneck = ESRRunTelemetryBottleneck::StellarCollapse;
	}
	else if (Last.RunProgress.Phase == ESRStellarRunPhase::FinalStabilization
		&& Last.RunProgress.bDeliveryTargetMet
		&& !Last.RunProgress.bIncomeRequirementMet)
	{
		Summary.PrimaryBottleneck = ESRRunTelemetryBottleneck::FinalThroughput;
	}
	else if (Last.ResourceReserve.bHasDeposits
		&& ((!Last.ResourceReserve.bPotentialFuelBatchesInfinite
				&& Last.ResourceReserve.PotentialFuelBatchCount <= 0)
			|| Last.ResourceReserve.Pressure == ESRResourceReservePressure::Critical
			|| Last.ResourceReserve.Pressure == ESRResourceReservePressure::Depleted))
	{
		Summary.PrimaryBottleneck = ESRRunTelemetryBottleneck::ResourceDepletion;
	}
	else if (Last.ThrottledFacilityCount > 0
		|| Last.OperationalDemand > Last.OperationalCapacity)
	{
		Summary.PrimaryBottleneck = ESRRunTelemetryBottleneck::OperationalCapacity;
	}
	else if (Last.ProducedStellarFuelItemCount <= 0)
	{
		Summary.PrimaryBottleneck = ESRRunTelemetryBottleneck::StellarFuelProduction;
	}
	else if (Last.RunProgress.TotalDeliveredFuel <= UE_DOUBLE_SMALL_NUMBER)
	{
		Summary.PrimaryBottleneck = ESRRunTelemetryBottleneck::DeliveryPath;
	}
	else if (Last.StellarFuelNetPerSecond < -UE_DOUBLE_SMALL_NUMBER)
	{
		Summary.PrimaryBottleneck = ESRRunTelemetryBottleneck::StellarFuelDeficit;
	}

	Summary.SummaryText = FString::Printf(
		TEXT("Samples=%d Duration=%.0fs Outcome=%s Delivered=%.0f Income(avg/peak)=%.1f/%.1f/s DemandPeak=%.1f/s PressurePeak=%.0f%% CapacityPeak=%.0f%% ReserveMin=%.0f%% FuelBatches=%lld Limiting=%s Bottleneck=%s"),
		Summary.SampleCount,
		Summary.RecordedDurationSeconds,
		ResolveOutcomeLabel(Summary.Outcome),
		Summary.FinalDeliveredFuel,
		Summary.AverageStellarFuelIncomePerSecond,
		Summary.PeakStellarFuelIncomePerSecond,
		Summary.PeakStellarConsumptionPerSecond,
		Summary.PeakStellarFuelPressureRatio * 100.0f,
		Summary.PeakOperationalUtilization * 100.0f,
		Summary.MinimumResourceReserveRatio * 100.0f,
		Summary.FinalPotentialFuelBatchCount,
		Summary.FinalLimitingReferenceCardId.IsNone()
			? TEXT("None")
			: *Summary.FinalLimitingReferenceCardId.ToString(),
		*ResolveBottleneckLabel(Summary.PrimaryBottleneck));
	return Summary;
}

void USRRunTelemetrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const USRSimulationSettings* Settings = GetDefault<USRSimulationSettings>();
	bTelemetryEnabled = !IsValid(Settings) || Settings->bEnableRunTelemetryV2;
	SampleIntervalSeconds = IsValid(Settings)
		? FMath::Max(1.0, Settings->RunTelemetrySampleIntervalSecondsV2)
		: 5.0;
	MaxSampleCount = IsValid(Settings)
		? FMath::Max(16, Settings->RunTelemetryMaxSamplesV2)
		: 720;
	bLogOnRunCompletion = !IsValid(Settings) || Settings->bLogRunTelemetryOnCompletionV2;
	ResetTelemetry();
}

void USRRunTelemetrySubsystem::Deinitialize()
{
	BindPrimaryStar(nullptr);
	for (const TWeakObjectPtr<USRFacilityNetworkComponent>& Network : BoundFacilityNetworks)
	{
		if (Network.IsValid())
		{
			Network->OnResourceProduced().RemoveAll(this);
		}
	}
	BoundFacilityNetworks.Reset();
	Super::Deinitialize();
}

void USRRunTelemetrySubsystem::Tick(float DeltaTime)
{
	if (!bTelemetryEnabled || !IsValid(GetWorld()) || !GetWorld()->IsGameWorld())
	{
		return;
	}

	RefreshSourceBindings();
	if (!bCapturedInitialStarSnapshot && BoundPrimaryStar.IsValid())
	{
		bCapturedInitialStarSnapshot = CaptureSnapshotNow();
	}

	const USRTimeControlSubsystem* TimeControl =
		GetWorld()->GetSubsystem<USRTimeControlSubsystem>();
	const double EffectiveDeltaSeconds = IsValid(TimeControl)
		? static_cast<double>(FMath::Max(0.0f, DeltaTime)
			* FMath::Max(0.0f, TimeControl->GetEffectiveTimeScale()))
		: static_cast<double>(FMath::Max(0.0f, DeltaTime));
	if (EffectiveDeltaSeconds <= UE_DOUBLE_SMALL_NUMBER)
	{
		return;
	}

	SampleAccumulatorSeconds += EffectiveDeltaSeconds;
	if (SampleAccumulatorSeconds + UE_DOUBLE_SMALL_NUMBER < SampleIntervalSeconds)
	{
		return;
	}
	SampleAccumulatorSeconds = FMath::Fmod(SampleAccumulatorSeconds, SampleIntervalSeconds);
	CaptureSnapshotNow();
}

TStatId USRRunTelemetrySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USRRunTelemetrySubsystem, STATGROUP_Tickables);
}

bool USRRunTelemetrySubsystem::CaptureSnapshotNow()
{
	if (!bTelemetryEnabled)
	{
		return false;
	}
	RefreshSourceBindings();
	FSRRunTelemetrySnapshot Snapshot;
	if (!BuildWorldSnapshot(Snapshot))
	{
		return false;
	}
	StoreSnapshot(Snapshot);
	return true;
}

void USRRunTelemetrySubsystem::ResetTelemetry()
{
	Samples.Reset();
	ProducedCardItemCount = 0;
	ProducedStellarFuelItemCount = 0;
	ProducedResourceEnergy = 0.0;
	SampleAccumulatorSeconds = 0.0;
	bCapturedInitialStarSnapshot = false;
}

bool USRRunTelemetrySubsystem::GetLatestSnapshot(FSRRunTelemetrySnapshot& OutSnapshot) const
{
	if (Samples.IsEmpty())
	{
		OutSnapshot = FSRRunTelemetrySnapshot();
		return false;
	}
	OutSnapshot = Samples.Last();
	return true;
}

void USRRunTelemetrySubsystem::GetTelemetrySamples(
	TArray<FSRRunTelemetrySnapshot>& OutSamples) const
{
	OutSamples = Samples;
}

FSRRunTelemetrySummary USRRunTelemetrySubsystem::GetSummary() const
{
	return FSRRunTelemetrySummaryModel::BuildSummary(Samples);
}

bool USRRunTelemetrySubsystem::BuildCurrentFlatSupplyProjection(
	FSRRunBalanceResult& OutResult) const
{
	OutResult = FSRRunBalanceResult();
	FSRRunTelemetrySnapshot Snapshot;
	if (!GetLatestSnapshot(Snapshot) || !Snapshot.bHasPrimaryStar)
	{
		return false;
	}

	const USRSimulationSettings* Settings = GetDefault<USRSimulationSettings>();
	FSRRunBalanceScenario Scenario;
	Scenario.ScenarioId = TEXT("CurrentFlatSupply");
	Scenario.DurationSeconds = FMath::Max(
		600.0,
		Snapshot.RunProgress.TargetRunDurationSeconds
			- Snapshot.RunProgress.ElapsedSimulationSeconds + 300.0);
	Scenario.InitialStageFuel = Snapshot.InitialStageFuel;
	Scenario.StartingStoredFuel = Snapshot.StoredStellarFuel;
	Scenario.InitialDemandPerSecond = Snapshot.StellarConsumptionPerSecond;
	Scenario.SecondsPerCycle = IsValid(Settings)
		? FMath::Max(1.0f, Settings->SecondsPerPeriod)
		: 60.0;
	Scenario.StartingCycleIndex = Snapshot.CycleIndex;
	Scenario.FirstCycleDurationSeconds = Snapshot.SecondsUntilNextCycle;
	Scenario.DemandCurve = IsValid(Settings) && Settings->bUseStellarPressureCurveV2
		? ESRRunBalanceDemandCurve::StellarPressureV2
		: ESRRunBalanceDemandCurve::LegacyExponential;
	if (IsValid(Settings))
	{
		Scenario.DemandCurveV2 = Settings->BuildStellarDemandCurveV2();
		Scenario.PressureRulesV2 = Settings->BuildStellarPressureRulesV2();
	}
	Scenario.Contract = IsValid(Settings)
		? Settings->BuildStellarRunContractV2()
		: FSRStellarRunContract();
	Scenario.StartingEvolutionStage = Snapshot.EvolutionStage;
	Scenario.bResumeRunProgress = true;
	Scenario.StartingRunProgress = Snapshot.RunProgress;
	Scenario.InitialObservedIncomePerSecond =
		Snapshot.RecentStellarFuelIncomePerSecond;
	FSRRunBalanceSupplyStage& FlatSupply = Scenario.SupplyStages.AddDefaulted_GetRef();
	FlatSupply.FuelPerSecond = Snapshot.RecentStellarFuelIncomePerSecond;
	FlatSupply.EndTimeSeconds = Scenario.DurationSeconds + 1.0;
	OutResult = FSRRunBalanceSimulator::Simulate(Scenario);
	return true;
}

void USRRunTelemetrySubsystem::LogSummary() const
{
	const FSRRunTelemetrySummary Summary = GetSummary();
	UE_LOG(LogSRRunBalance, Display, TEXT("[RunTelemetry] %s"), *Summary.SummaryText);
	UE_LOG(LogSRRunBalance, Display,
		TEXT("[RunTelemetry] Milestones Card=%.0f Fuel=%.0f Delivery=%.0f Emergency=%.0f Sustained=%.0f Complete=%.0f | Produced Cards=%lld Fuel=%lld | ThrottledPeak=%d RouteQueuePeak=%d"),
		Summary.Milestones.FirstCardProducedSeconds,
		Summary.Milestones.FirstStellarFuelProducedSeconds,
		Summary.Milestones.FirstStellarFuelDeliveredSeconds,
		Summary.Milestones.EmergencyIgnitionCompletedSeconds,
		Summary.Milestones.SustainedSupplyCompletedSeconds,
		Summary.Milestones.RunCompletedSeconds,
		Summary.ProducedCardItemCount,
		Summary.ProducedStellarFuelItemCount,
		Summary.PeakThrottledFacilityCount,
		Summary.PeakBlockedOrQueuedRouteCount);
}

void USRRunTelemetrySubsystem::RefreshSourceBindings()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}
	for (auto It = BoundFacilityNetworks.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}

	USRCelestialBodyRegistrySubsystem* Registry =
		World->GetSubsystem<USRCelestialBodyRegistrySubsystem>();
	ASRStar* PrimaryStar = IsValid(Registry)
		? Cast<ASRStar>(Registry->GetPrimaryStarActor())
		: nullptr;
	BindPrimaryStar(PrimaryStar);
	if (!IsValid(Registry))
	{
		return;
	}
	TArray<AActor*> Bodies;
	Registry->GetCelestialBodies(Bodies);
	for (AActor* Body : Bodies)
	{
		if (IsValid(Body))
		{
			BindFacilityNetwork(Body->FindComponentByClass<USRFacilityNetworkComponent>());
		}
	}
}

void USRRunTelemetrySubsystem::BindPrimaryStar(ASRStar* PrimaryStar)
{
	if (BoundPrimaryStar == PrimaryStar)
	{
		return;
	}
	if (BoundPrimaryStar.IsValid())
	{
		BoundPrimaryStar->OnStellarRunCompleted.RemoveDynamic(
			this,
			&USRRunTelemetrySubsystem::HandleStellarRunCompleted);
	}
	BoundPrimaryStar = PrimaryStar;
	if (BoundPrimaryStar.IsValid())
	{
		BoundPrimaryStar->OnStellarRunCompleted.RemoveDynamic(
			this,
			&USRRunTelemetrySubsystem::HandleStellarRunCompleted);
		BoundPrimaryStar->OnStellarRunCompleted.AddDynamic(
			this,
			&USRRunTelemetrySubsystem::HandleStellarRunCompleted);
	}
}

void USRRunTelemetrySubsystem::BindFacilityNetwork(
	USRFacilityNetworkComponent* FacilityNetwork)
{
	if (!IsValid(FacilityNetwork))
	{
		return;
	}
	const TWeakObjectPtr<USRFacilityNetworkComponent> Key(FacilityNetwork);
	if (BoundFacilityNetworks.Contains(Key))
	{
		return;
	}
	FacilityNetwork->OnResourceProduced().AddUObject(
		this,
		&USRRunTelemetrySubsystem::HandleResourceProduced);
	BoundFacilityNetworks.Add(Key);
}

bool USRRunTelemetrySubsystem::BuildWorldSnapshot(
	FSRRunTelemetrySnapshot& OutSnapshot) const
{
	OutSnapshot = FSRRunTelemetrySnapshot();
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	const USRTimeControlSubsystem* TimeControl =
		World->GetSubsystem<USRTimeControlSubsystem>();
	OutSnapshot.CycleIndex = IsValid(TimeControl)
		? TimeControl->GetCurrentCycleIndex()
		: 0;
	OutSnapshot.CycleProgressSeconds = IsValid(TimeControl)
		? FMath::Max(0.0f, TimeControl->GetCycleProgressSeconds())
		: 0.0;
	OutSnapshot.SecondsUntilNextCycle = IsValid(TimeControl)
		? FMath::Max(
			0.0f,
			TimeControl->GetSecondsPerPeriod()
				- TimeControl->GetCycleProgressSeconds())
		: 0.0;
	OutSnapshot.bSimulationPaused = IsValid(TimeControl)
		&& TimeControl->IsSimulationPaused();

	const USRCelestialBodyRegistrySubsystem* Registry =
		World->GetSubsystem<USRCelestialBodyRegistrySubsystem>();
	TArray<AActor*> Bodies;
	if (IsValid(Registry))
	{
		Registry->GetCelestialBodies(Bodies);
	}
	OutSnapshot.CelestialBodyCount = Bodies.Num();
	TArray<FSRResourceDepositInstance> SystemDeposits;
	for (AActor* Body : Bodies)
	{
		if (!IsValid(Body))
		{
			continue;
		}
		if (USRCelestialBodyRuntimeLibrary::GetCelestialCanConstruct(Body))
		{
			++OutSnapshot.ConstructibleBodyCount;
		}
		if (const USRStructureInstanceManagerComponent* StructureManager =
			Body->FindComponentByClass<USRStructureInstanceManagerComponent>())
		{
			TArray<FSRResourceDepositInstance> BodyDeposits;
			StructureManager->GetResourceDepositInstances(BodyDeposits);
			SystemDeposits.Append(MoveTemp(BodyDeposits));
		}
		const USRFacilityNetworkComponent* Network =
			Body->FindComponentByClass<USRFacilityNetworkComponent>();
		if (!IsValid(Network))
		{
			continue;
		}
		++OutSnapshot.FacilityNetworkCount;
		const FSROperationalCapacityReportV2 Capacity =
			Network->GetOperationalCapacityReport();
		const FSROperationalFacilityStatusCountsV2 Status =
			Network->GetOperationalFacilityStatusCounts();
		OutSnapshot.RegisteredFacilityCount += Status.RegisteredFacilityCount;
		OutSnapshot.EnabledFacilityCount += Status.EnabledFacilityCount;
		OutSnapshot.ProcessingFacilityCount += Status.ProcessingFacilityCount;
		OutSnapshot.ThrottledFacilityCount += Status.ThrottledFacilityCount;
		OutSnapshot.OperationalDemand += FMath::Max(0, Capacity.TotalDemand);
		OutSnapshot.OperationalCapacity += FMath::Max(0, Capacity.TotalCapacity);
		OutSnapshot.ActiveServiceCoreCount += FMath::Max(0, Capacity.ActiveServiceCoreCount);
	}
	OutSnapshot.ResourceReserve = FSRResourceReserveModel::BuildSnapshot(SystemDeposits);

	const ASRStar* PrimaryStar = IsValid(Registry)
		? Cast<ASRStar>(Registry->GetPrimaryStarActor())
		: nullptr;
	if (IsValid(PrimaryStar))
	{
		const FSRStellarFuelState FuelState = PrimaryStar->GetStellarFuelState();
		OutSnapshot.bHasPrimaryStar = true;
		OutSnapshot.EvolutionStage = FuelState.EvolutionStage;
		OutSnapshot.InitialStageFuel = FuelState.InitialStageFuel;
		OutSnapshot.StoredStellarFuel = FuelState.StoredFuel;
		OutSnapshot.StellarConsumptionPerSecond = FuelState.RequiredFuelPerCycle;
		OutSnapshot.StellarDemandPhase = FuelState.DemandPhase;
		OutSnapshot.StellarFuelPressureRatio = FuelState.FuelPressureRatio;
		OutSnapshot.RecentStellarFuelIncomePerSecond = FuelState.RecentFuelIncomePerSecond;
		OutSnapshot.StellarFuelNetPerSecond =
			FuelState.RecentFuelIncomePerSecond - FuelState.RequiredFuelPerCycle;
		OutSnapshot.RunProgress = FuelState.RunProgress;
		OutSnapshot.LastStellarFuelReserveOverflow = FuelState.LastFuelReserveOverflow;
		OutSnapshot.SimulationSeconds = FuelState.RunProgress.ElapsedSimulationSeconds;
	}

	const USRSpaceLogisticsSubsystem* Logistics =
		World->GetSubsystem<USRSpaceLogisticsSubsystem>();
	if (IsValid(Logistics))
	{
		TArray<FSRSpaceLogisticsHubRoute> Routes;
		Logistics->GetHubRoutes(Routes);
		for (const FSRSpaceLogisticsHubRoute& Route : Routes)
		{
			if (!Route.bEnabled)
			{
				continue;
			}
			++OutSnapshot.ActiveRouteCount;
			OutSnapshot.BlockedRouteCount +=
				Route.Phase == ESRSpaceLogisticsHubRoutePhase::Blocked ? 1 : 0;
			OutSnapshot.FleetQueuedRouteCount +=
				Route.Phase == ESRSpaceLogisticsHubRoutePhase::WaitingForFleetCapacity ? 1 : 0;
		}

		TArray<FSRSpaceLogisticsStarFuelMissile> Missiles;
		Logistics->GetStarFuelMissiles(Missiles);
		for (const FSRSpaceLogisticsStarFuelMissile& Missile : Missiles)
		{
			if (!Missile.bEnabled)
			{
				continue;
			}
			++OutSnapshot.InFlightStellarFuelMissileCount;
			OutSnapshot.InFlightStellarFuel +=
				StarRovers::SpaceLogistics::StarFuelMissiles::CalculateMissileFuelValue(
					Missile.Cargo);
		}
	}

	OutSnapshot.ProducedCardItemCount = ProducedCardItemCount;
	OutSnapshot.ProducedStellarFuelItemCount = ProducedStellarFuelItemCount;
	OutSnapshot.ProducedResourceEnergy = ProducedResourceEnergy;
	return true;
}

void USRRunTelemetrySubsystem::StoreSnapshot(
	const FSRRunTelemetrySnapshot& Snapshot)
{
	if (!Samples.IsEmpty()
		&& FMath::IsNearlyEqual(Samples.Last().SimulationSeconds, Snapshot.SimulationSeconds))
	{
		Samples.Last() = Snapshot;
	}
	else
	{
		Samples.Add(Snapshot);
	}
	const int32 ExcessSampleCount = Samples.Num() - MaxSampleCount;
	if (ExcessSampleCount > 0)
	{
		Samples.RemoveAt(0, ExcessSampleCount, EAllowShrinking::No);
	}
}

void USRRunTelemetrySubsystem::HandleResourceProduced(
	USRFacilityNetworkComponent* FacilityNetwork,
	FName OccupantId,
	const FSRResourceInstance& ResourceInstance)
{
	const int32 ItemCount = FMath::Max(0, ResourceInstance.StackCount);
	if (ResourceInstance.ResourceClass == ESRResourceClass::Card)
	{
		ProducedCardItemCount += static_cast<int64>(ItemCount);
	}
	if (ResourceInstance.ResourceClass == ESRResourceClass::StellarFuel
		|| ResourceInstance.ResourceId == FName(TEXT("StellarFuel")))
	{
		ProducedStellarFuelItemCount += static_cast<int64>(ItemCount);
	}
	const double UnitEnergy = ResourceInstance.ResourceClass != ESRResourceClass::Unknown
		? SanitizeNonNegative(ResourceInstance.CurrentEnergy)
		: SanitizeNonNegative(ResourceInstance.EnergyValue);
	ProducedResourceEnergy += UnitEnergy * static_cast<double>(ItemCount);
}

void USRRunTelemetrySubsystem::HandleStellarRunCompleted(ASRStar* Star)
{
	CaptureSnapshotNow();
	if (bLogOnRunCompletion)
	{
		LogSummary();
	}
}
