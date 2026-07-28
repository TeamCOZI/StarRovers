#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Simulation/SRRunTelemetrySubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRRunTelemetrySummaryTest,
	"StarRovers.UI.RunCommand.BalanceTelemetry.SummaryAndBottleneck",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRRunTelemetrySummaryTest::RunTest(const FString& Parameters)
{
	FSRRunTelemetrySnapshot Start;
	Start.bHasPrimaryStar = true;
	Start.StoredStellarFuel = 1000.0;
	Start.StellarConsumptionPerSecond = 10.0;
	Start.OperationalCapacity = 30;
	Start.OperationalDemand = 10;
	Start.RunProgress.bFiniteVictoryEnabled = true;
	Start.RunProgress.VictoryDeliveryTarget = 600.0;

	FSRRunTelemetrySnapshot Saturated = Start;
	Saturated.SimulationSeconds = 100.0;
	Saturated.StoredStellarFuel = 500.0;
	Saturated.StellarConsumptionPerSecond = 20.0;
	Saturated.RecentStellarFuelIncomePerSecond = 5.0;
	Saturated.StellarFuelNetPerSecond = -15.0;
	Saturated.OperationalDemand = 40;
	Saturated.ThrottledFacilityCount = 2;
	Saturated.ProducedCardItemCount = 10;
	Saturated.ProducedStellarFuelItemCount = 2;

	const FSRRunTelemetrySummary Summary =
		FSRRunTelemetrySummaryModel::BuildSummary({ Start, Saturated });
	TestTrue(TEXT("Two authoritative samples form a valid summary"),
		Summary.bIsValid && Summary.SampleCount == 2);
	TestTrue(TEXT("Summary duration follows simulation rather than wall time"),
		FMath::IsNearlyEqual(Summary.RecordedDurationSeconds, 100.0));
	TestTrue(TEXT("Capacity saturation outranks a downstream fuel deficit"),
		Summary.PrimaryBottleneck == ESRRunTelemetryBottleneck::OperationalCapacity);
	TestTrue(TEXT("Peak utilization preserves overload magnitude"),
		FMath::IsNearlyEqual(Summary.PeakOperationalUtilization, 4.0f / 3.0f));
	TestTrue(TEXT("Average income is weighted by Simulation duration"),
		FMath::IsNearlyEqual(Summary.AverageStellarFuelIncomePerSecond, 2.5));
	TestTrue(TEXT("The first observed Fuel output becomes a milestone"),
		FMath::IsNearlyEqual(Summary.Milestones.FirstStellarFuelProducedSeconds, 100.0));

	Saturated.RunProgress.Phase = ESRStellarRunPhase::FinalStabilization;
	Saturated.RunProgress.bDeliveryTargetMet = true;
	Saturated.RunProgress.bIncomeRequirementMet = false;
	const FSRRunTelemetrySummary FinalSummary =
		FSRRunTelemetrySummaryModel::BuildSummary({ Start, Saturated });
	TestEqual(TEXT("An unmet final throughput contract becomes the primary diagnosis"),
		FinalSummary.PrimaryBottleneck,
		ESRRunTelemetryBottleneck::FinalThroughput);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRRunTelemetryTerminalMilestoneTest,
	"StarRovers.UI.RunCommand.BalanceTelemetry.TerminalMilestones",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRRunTelemetryTerminalMilestoneTest::RunTest(const FString& Parameters)
{
	FSRRunTelemetrySnapshot Emergency;
	Emergency.bHasPrimaryStar = true;
	Emergency.RunProgress.Phase = ESRStellarRunPhase::EmergencyIgnition;
	Emergency.StoredStellarFuel = 1000.0;

	FSRRunTelemetrySnapshot Sustained = Emergency;
	Sustained.SimulationSeconds = 300.0;
	Sustained.RunProgress.Phase = ESRStellarRunPhase::SustainedSupply;
	Sustained.RunProgress.TotalDeliveredFuel = 5000.0;

	FSRRunTelemetrySnapshot Final = Sustained;
	Final.SimulationSeconds = 900.0;
	Final.RunProgress.Phase = ESRStellarRunPhase::FinalStabilization;
	Final.RunProgress.TotalDeliveredFuel = 25000.0;

	FSRRunTelemetrySnapshot Victory = Final;
	Victory.SimulationSeconds = 1800.0;
	Victory.RunProgress.Phase = ESRStellarRunPhase::Complete;
	Victory.RunProgress.Outcome = ESRStellarRunOutcome::Victory;
	Victory.RunProgress.CompletionSimulationSeconds = 1800.0;
	Victory.RunProgress.TotalDeliveredFuel = 100000.0;

	const FSRRunTelemetrySummary Summary =
		FSRRunTelemetrySummaryModel::BuildSummary({ Emergency, Sustained, Final, Victory });
	TestTrue(TEXT("Run phase completion times remain separately measurable"),
		FMath::IsNearlyEqual(Summary.Milestones.EmergencyIgnitionCompletedSeconds, 300.0)
			&& FMath::IsNearlyEqual(Summary.Milestones.SustainedSupplyCompletedSeconds, 900.0)
			&& FMath::IsNearlyEqual(Summary.Milestones.RunCompletedSeconds, 1800.0));
	TestEqual(TEXT("The terminal outcome is preserved in the summary"),
		Summary.Outcome,
		ESRStellarRunOutcome::Victory);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRRunTelemetryResourceDepletionDiagnosisTest,
	"StarRovers.ResourceSystem.Phase20.Telemetry.ResourceDepletionDiagnosis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRRunTelemetryResourceDepletionDiagnosisTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FSRRunTelemetrySnapshot Snapshot;
	Snapshot.bHasPrimaryStar = true;
	Snapshot.StoredStellarFuel = 12000.0;
	Snapshot.StellarConsumptionPerSecond = 80.0;
	Snapshot.RecentStellarFuelIncomePerSecond = 82.4;
	Snapshot.StellarFuelNetPerSecond = 2.4;
	Snapshot.OperationalCapacity = 30;
	Snapshot.OperationalDemand = 18;
	Snapshot.ProducedCardItemCount = 600;
	Snapshot.ProducedStellarFuelItemCount = 100;
	Snapshot.RunProgress.TotalDeliveredFuel = 50000.0;
	Snapshot.ResourceReserve.bHasDeposits = true;
	Snapshot.ResourceReserve.DepositCount = 24;
	Snapshot.ResourceReserve.ActiveDepositCount = 18;
	Snapshot.ResourceReserve.DepletedDepositCount = 6;
	Snapshot.ResourceReserve.PotentialFuelBatchCount = 0;
	Snapshot.ResourceReserve.LimitingReferenceCardId = FName(TEXT("NullPearl"));
	Snapshot.ResourceReserve.RemainingRatio = 0.08f;
	Snapshot.ResourceReserve.Pressure = ESRResourceReservePressure::Critical;

	const FSRRunTelemetrySummary Summary =
		FSRRunTelemetrySummaryModel::BuildSummary({ Snapshot });
	TestEqual(TEXT("No complete five-Card batch remaining becomes the primary diagnosis"),
		Summary.PrimaryBottleneck,
		ESRRunTelemetryBottleneck::ResourceDepletion);
	TestEqual(TEXT("The limiting Card remains in the telemetry report"),
		Summary.FinalLimitingReferenceCardId,
		FName(TEXT("NullPearl")));
	TestTrue(TEXT("The report exposes remaining batch count and limiting resource"),
		Summary.SummaryText.Contains(TEXT("FuelBatches=0"))
			&& Summary.SummaryText.Contains(TEXT("Limiting=NullPearl")));
	return !HasAnyErrors();
}

#endif
