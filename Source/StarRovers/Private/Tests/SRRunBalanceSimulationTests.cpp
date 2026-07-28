#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include "Simulation/SRRunBalanceSimulation.h"
#include "Simulation/SRStellarDemandModel.h"

#include <limits>

namespace
{
	FSRRunBalanceScenario MakeFlatVictoryScenario()
	{
		FSRRunBalanceScenario Scenario;
		Scenario.ScenarioId = TEXT("FlatVictory");
		Scenario.DurationSeconds = 120.0;
		Scenario.OutputSampleIntervalSeconds = 5.0;
		Scenario.InitialStageFuel = 1000.0;
		Scenario.StartingStoredFuel = 1000.0;
		Scenario.InitialDemandPerSecond = 1.0;
		Scenario.SecondsPerCycle = 60.0;
		Scenario.DemandCurve = ESRRunBalanceDemandCurve::Flat;
		Scenario.Contract.EmergencyDeliveryTarget = 100.0;
		Scenario.Contract.SustainedSupplyDeliveryTarget = 300.0;
		Scenario.Contract.VictoryDeliveryTarget = 600.0;
		Scenario.Contract.VictoryRequiredIncomePerSecond = 20.0;
		Scenario.Contract.VictoryRequiredSustainSeconds = 10.0;

		FSRRunBalanceSupplyStage& Supply = Scenario.SupplyStages.AddDefaulted_GetRef();
		Supply.FuelPerSecond = 50.0;
		Supply.DeliveryIntervalSeconds = 1.0;
		return Scenario;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRRunBalanceDemandParityTest,
	"StarRovers.UI.RunCommand.BalanceHarness.LegacyDemandParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRRunBalanceDemandParityTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("The first live legacy cycle uses the shared 201 percent multiplier"),
		FMath::IsNearlyEqual(
			FSRStellarDemandModel::CalculateLegacyNextCycleDemand(50.0, 1),
			100.5));
	TestTrue(TEXT("Invalid demand never contaminates a deterministic run"),
		FMath::IsNearlyEqual(
			FSRStellarDemandModel::CalculateLegacyNextCycleDemand(-10.0, 3),
			0.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRRunBalanceDeterminismTest,
	"StarRovers.UI.RunCommand.BalanceHarness.DeterminismAndVictory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRRunBalanceDeterminismTest::RunTest(const FString& Parameters)
{
	const FSRRunBalanceScenario Scenario = MakeFlatVictoryScenario();
	const FSRRunBalanceResult First = FSRRunBalanceSimulator::Simulate(Scenario);
	const FSRRunBalanceResult Second = FSRRunBalanceSimulator::Simulate(Scenario);

	TestEqual(TEXT("A sufficient flat supply completes the finite Run"),
		First.Outcome,
		ESRStellarRunOutcome::Victory);
	TestTrue(TEXT("The rolling income and sustain rules complete at the expected second"),
		FMath::IsNearlyEqual(First.CompletionSeconds, 21.0));
	TestTrue(TEXT("Emergency and sustained delivery boundaries are observable"),
		FMath::IsNearlyEqual(First.EmergencyIgnitionCompletedSeconds, 2.0)
			&& FMath::IsNearlyEqual(First.SustainedSupplyCompletedSeconds, 6.0));
	TestEqual(TEXT("Repeated runs emit the same number of samples"),
		First.Timeline.Num(),
		Second.Timeline.Num());
	TestTrue(TEXT("Repeated runs emit identical terminal state"),
		First.Outcome == Second.Outcome
			&& FMath::IsNearlyEqual(First.SimulatedUntilSeconds, Second.SimulatedUntilSeconds)
			&& FMath::IsNearlyEqual(First.TotalDeliveredFuel, Second.TotalDeliveredFuel)
			&& FMath::IsNearlyEqual(First.PeakDemandPerSecond, Second.PeakDemandPerSecond));
	for (int32 Index = 0; Index < First.Timeline.Num() && Index < Second.Timeline.Num(); ++Index)
	{
		TestTrue(
			*FString::Printf(TEXT("Timeline sample %d is deterministic"), Index),
			FMath::IsNearlyEqual(
				First.Timeline[Index].SimulationSeconds,
				Second.Timeline[Index].SimulationSeconds)
				&& FMath::IsNearlyEqual(
					First.Timeline[Index].StoredFuel,
					Second.Timeline[Index].StoredFuel)
				&& FMath::IsNearlyEqual(
					First.Timeline[Index].TotalDeliveredFuel,
					Second.Timeline[Index].TotalDeliveredFuel)
				&& First.Timeline[Index].Outcome == Second.Timeline[Index].Outcome);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRRunBalanceCollapseTest,
	"StarRovers.UI.RunCommand.BalanceHarness.NoSupplyCollapse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRRunBalanceCollapseTest::RunTest(const FString& Parameters)
{
	FSRRunBalanceScenario Scenario;
	Scenario.ScenarioId = TEXT("NoSupply");
	Scenario.DurationSeconds = 30.0;
	Scenario.InitialStageFuel = 10.0;
	Scenario.StartingStoredFuel = 10.0;
	Scenario.InitialDemandPerSecond = 6.0;
	Scenario.DemandCurve = ESRRunBalanceDemandCurve::Flat;
	Scenario.Contract.bFiniteVictoryEnabled = false;

	const FSRRunBalanceResult Result = FSRRunBalanceSimulator::Simulate(Scenario);
	TestEqual(TEXT("Two depleted stellar stages end in defeat"),
		Result.Outcome,
		ESRStellarRunOutcome::Defeat);
	TestTrue(TEXT("The deterministic pressure order collapses on second four"),
		FMath::IsNearlyEqual(Result.CompletionSeconds, 4.0));
	TestEqual(TEXT("Main sequence to red giant and red giant to supernova are both counted"),
		Result.StellarStageTransitionCount,
		2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRRunBalanceSupplyTimingTest,
	"StarRovers.UI.RunCommand.BalanceHarness.TransitCapacityAndBatching",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRRunBalanceSupplyTimingTest::RunTest(const FString& Parameters)
{
	FSRRunBalanceScenario Scenario;
	Scenario.ScenarioId = TEXT("TimedSupply");
	Scenario.DurationSeconds = 20.0;
	Scenario.InitialStageFuel = 1000.0;
	Scenario.StartingStoredFuel = 1000.0;
	Scenario.InitialDemandPerSecond = 0.0;
	Scenario.DemandCurve = ESRRunBalanceDemandCurve::Flat;
	Scenario.Contract.bFiniteVictoryEnabled = false;
	FSRRunBalanceSupplyStage& Stage = Scenario.SupplyStages.AddDefaulted_GetRef();
	Stage.FuelPerSecond = 10.0;
	Stage.DeliveryIntervalSeconds = 5.0;
	Stage.TransitDelaySeconds = 10.0;
	Stage.OperationalSpeedFactor = 0.5;

	const FSRRunBalanceResult Result = FSRRunBalanceSimulator::Simulate(Scenario);
	TestTrue(TEXT("Transit delay, five-second batches, and fifty-percent Capacity all apply"),
		FMath::IsNearlyEqual(Result.TotalDeliveredFuel, 75.0));
	TestTrue(TEXT("The delayed scenario remains active when finite victory is disabled"),
		Result.Outcome == ESRStellarRunOutcome::InProgress
			&& FMath::IsNearlyEqual(Result.SimulatedUntilSeconds, 20.0));

	Scenario.SupplyStages.Reset();
	Scenario.DurationSeconds = 10.0;
	Scenario.InitialObservedIncomePerSecond = 10.0;
	FSRRunBalanceSupplyStage& FlatContinuation = Scenario.SupplyStages.AddDefaulted_GetRef();
	FlatContinuation.FuelPerSecond = 10.0;
	FlatContinuation.EndTimeSeconds = 11.0;
	const FSRRunBalanceResult Projection = FSRRunBalanceSimulator::Simulate(Scenario);
	TestTrue(TEXT("A live rolling-window seed does not double-count continuing flat supply"),
		!Projection.Timeline.IsEmpty()
			&& FMath::IsNearlyEqual(
				Projection.Timeline.Last().RecentIncomePerSecond,
				10.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRRunBalanceConsoleContractTest,
	"StarRovers.UI.RunCommand.BalanceHarness.ConsoleCommands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRRunBalanceConsoleContractTest::RunTest(const FString& Parameters)
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	TestNotNull(TEXT("PIE telemetry report command is registered"),
		ConsoleManager.FindConsoleObject(TEXT("sr.Balance.Telemetry.Report")));
	TestNotNull(TEXT("PIE telemetry reset command is registered"),
		ConsoleManager.FindConsoleObject(TEXT("sr.Balance.Telemetry.Reset")));
	TestNotNull(TEXT("Current flat-supply projection command is registered"),
		ConsoleManager.FindConsoleObject(TEXT("sr.Balance.ProjectCurrent")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRRunBalanceSanitizationTest,
	"StarRovers.UI.RunCommand.BalanceHarness.ScenarioSanitization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRRunBalanceSanitizationTest::RunTest(const FString& Parameters)
{
	FSRRunBalanceScenario Scenario;
	Scenario.DemandCurve = ESRRunBalanceDemandCurve::Flat;
	Scenario.DurationSeconds = TNumericLimits<double>::Max();
	Scenario.OutputSampleIntervalSeconds = -10.0;
	Scenario.StartingStoredFuel = -1.0;
	Scenario.InitialDemandPerSecond = std::numeric_limits<double>::quiet_NaN();
	Scenario.IncomeWindowSeconds = 0.0;
	FSRRunBalanceSupplyStage& Stage = Scenario.SupplyStages.AddDefaulted_GetRef();
	Stage.FuelPerSecond = std::numeric_limits<double>::infinity();
	Stage.DeliveryIntervalSeconds = 0.0;
	Stage.TransitDelaySeconds = -1.0;
	Stage.OperationalSpeedFactor = 5.0;
	Stage.MaximumDeliveryCount = -4;

	const FSRRunBalanceScenario Safe = FSRRunBalanceSimulator::SanitizeScenario(Scenario);
	TestTrue(TEXT("Scenario duration is bounded to one day"),
		FMath::IsNearlyEqual(Safe.DurationSeconds, 24.0 * 60.0 * 60.0));
	TestTrue(TEXT("Invalid scalar inputs resolve to finite simulation-safe values"),
		Safe.OutputSampleIntervalSeconds >= 1.0
			&& FMath::IsNearlyZero(Safe.StartingStoredFuel)
			&& FMath::IsNearlyZero(Safe.InitialDemandPerSecond)
			&& Safe.IncomeWindowSeconds >= 1.0);
	TestTrue(TEXT("Supply controls remain finite and within their supported ranges"),
		FMath::IsNearlyZero(Safe.SupplyStages[0].FuelPerSecond)
			&& Safe.SupplyStages[0].DeliveryIntervalSeconds >= 1.0
			&& FMath::IsNearlyZero(Safe.SupplyStages[0].TransitDelaySeconds)
			&& FMath::IsNearlyEqual(Safe.SupplyStages[0].OperationalSpeedFactor, 1.0)
			&& Safe.SupplyStages[0].MaximumDeliveryCount == 0);
	return true;
}

#endif
