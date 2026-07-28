#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Simulation/SRRunBalanceSimulation.h"
#include "Simulation/SRStellarDemandModel.h"

namespace
{
	FSRRunBalanceScenario MakePressureReferenceScenario(double DurationSeconds = 2100.0)
	{
		FSRRunBalanceScenario Scenario;
		Scenario.ScenarioId = TEXT("PressureReference");
		Scenario.DurationSeconds = DurationSeconds;
		Scenario.OutputSampleIntervalSeconds = 1.0;
		Scenario.SecondsPerCycle = 60.0;
		Scenario.StartingStoredFuel = 20000.0;
		Scenario.DemandCurve = ESRRunBalanceDemandCurve::StellarPressureV2;
		Scenario.DemandCurveV2.InitialDemandPerSecond = 50.0;
		Scenario.DemandCurveV2.GraceCycleCount = 2;
		Scenario.DemandCurveV2.DemandIncreasePerCycle = 5.0;
		Scenario.DemandCurveV2.MaximumDemandPerSecond = 100.0;
		Scenario.PressureRulesV2.FuelReserveCapacity = 20000.0;
		Scenario.PressureRulesV2.RedGiantEmergencyReserveRatio = 1.0;
		return Scenario;
	}

	FSRRunBalanceSupplyStage MakeStarterSupplyStage()
	{
		FSRRunBalanceSupplyStage Stage;
		Stage.StartTimeSeconds = 300.0;
		Stage.TransitDelaySeconds = 30.0;
		Stage.FuelPerSecond = 82.4;
		Stage.DeliveryIntervalSeconds = 10.0;
		return Stage;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStellarPressureDemandCurveTest,
	"StarRovers.UI.RunCommand.StellarPressure.DemandCurve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStellarPressureDemandCurveTest::RunTest(const FString& Parameters)
{
	FSRStellarDemandCurveV2 Curve;
	TestTrue(TEXT("Cycles zero through two retain the fifty-per-second grace demand"),
		FMath::IsNearlyEqual(FSRStellarDemandModel::CalculateDemandForCycleV2(Curve, 0), 50.0)
			&& FMath::IsNearlyEqual(FSRStellarDemandModel::CalculateDemandForCycleV2(Curve, 2), 50.0));
	TestTrue(TEXT("The expansion ramp adds five demand per completed cycle"),
		FMath::IsNearlyEqual(FSRStellarDemandModel::CalculateDemandForCycleV2(Curve, 3), 55.0)
			&& FMath::IsNearlyEqual(FSRStellarDemandModel::CalculateDemandForCycleV2(Curve, 8), 80.0));
	TestTrue(TEXT("Demand reaches one hundred and never grows beyond the plateau"),
		FMath::IsNearlyEqual(FSRStellarDemandModel::CalculateDemandForCycleV2(Curve, 12), 100.0)
			&& FMath::IsNearlyEqual(FSRStellarDemandModel::CalculateDemandForCycleV2(Curve, 1000), 100.0));
	TestEqual(TEXT("Cycle two is still grace"),
		FSRStellarDemandModel::ResolveDemandPhaseV2(Curve, 2),
		ESRStellarDemandPhaseV2::Grace);
	TestEqual(TEXT("Cycle three begins expansion"),
		FSRStellarDemandModel::ResolveDemandPhaseV2(Curve, 3),
		ESRStellarDemandPhaseV2::Expansion);
	TestEqual(TEXT("Cycle twelve is the final plateau"),
		FSRStellarDemandModel::ResolveDemandPhaseV2(Curve, 12),
		ESRStellarDemandPhaseV2::Plateau);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStellarPressureReserveTest,
	"StarRovers.UI.RunCommand.StellarPressure.ReserveAndRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStellarPressureReserveTest::RunTest(const FString& Parameters)
{
	FSRStellarPressureRulesV2 Rules;
	TestTrue(TEXT("Early delivery cannot stockpile beyond the twenty-thousand reserve"),
		FMath::IsNearlyEqual(
			FSRStellarDemandModel::ClampFuelReserveV2(25000.0, Rules),
			20000.0));
	TestTrue(TEXT("Half reserve maps to half pressure and surplus fuel lowers it"),
		FMath::IsNearlyEqual(
			FSRStellarDemandModel::CalculateFuelPressureRatioV2(10000.0, Rules),
			0.5f)
			&& FSRStellarDemandModel::CalculateFuelPressureRatioV2(15000.0, Rules) < 0.5f);
	TestTrue(TEXT("Red Giant receives exactly one configured emergency reserve"),
		FMath::IsNearlyEqual(
			FSRStellarDemandModel::ResolveRedGiantEmergencyReserveV2(Rules),
			20000.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStellarPressurePartialCycleTest,
	"StarRovers.UI.RunCommand.StellarPressure.PartialCycleProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStellarPressurePartialCycleTest::RunTest(const FString& Parameters)
{
	FSRRunBalanceScenario Scenario = MakePressureReferenceScenario(21.0);
	Scenario.StartingCycleIndex = 5;
	Scenario.FirstCycleDurationSeconds = 20.0;
	Scenario.Contract.bFiniteVictoryEnabled = false;
	const FSRRunBalanceResult Result = FSRRunBalanceSimulator::Simulate(Scenario);
	TestTrue(TEXT("A resumed projection starts from the absolute cycle-five demand"),
		!Result.Timeline.IsEmpty()
			&& FMath::IsNearlyEqual(Result.Timeline[0].DemandPerSecond, 65.0));
	TestTrue(TEXT("The partial cycle changes to cycle six only after its twenty remaining seconds"),
		Result.Timeline.Num() >= 22
			&& FMath::IsNearlyEqual(Result.Timeline[20].DemandPerSecond, 65.0)
			&& FMath::IsNearlyEqual(Result.Timeline[21].DemandPerSecond, 70.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStellarPressureReferenceLinesTest,
	"StarRovers.UI.RunCommand.StellarPressure.ReferenceLines",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStellarPressureReferenceLinesTest::RunTest(const FString& Parameters)
{
	const FSRRunBalanceResult NoSupply = FSRRunBalanceSimulator::Simulate(
		MakePressureReferenceScenario());
	TestTrue(TEXT("No supply consumes both stellar reserves in a readable ten-minute window"),
		NoSupply.Outcome == ESRStellarRunOutcome::Defeat
			&& NoSupply.CompletionSeconds >= 600.0
			&& NoSupply.CompletionSeconds <= 660.0
			&& NoSupply.StellarStageTransitionCount == 2);
	TestTrue(TEXT("The no-supply failure remains below the one-hundred plateau"),
		NoSupply.PeakDemandPerSecond <= 100.0);

	FSRRunBalanceScenario StarterScenario = MakePressureReferenceScenario(1800.0);
	StarterScenario.ScenarioId = TEXT("StarterLine");
	StarterScenario.SupplyStages.Add(MakeStarterSupplyStage());
	const FSRRunBalanceResult Starter = FSRRunBalanceSimulator::Simulate(StarterScenario);
	TestTrue(TEXT("One basic 82.4-per-second line reaches thirty minutes without falsely winning"),
		Starter.Outcome == ESRStellarRunOutcome::InProgress
			&& FMath::IsNearlyEqual(Starter.SimulatedUntilSeconds, 1800.0)
			&& Starter.StellarStageTransitionCount == 1
			&& !Starter.Timeline.IsEmpty()
			&& Starter.Timeline.Last().EvolutionStage == ESRStellarEvolutionStage::RedGiant);
	TestTrue(TEXT("The basic line reaches the final delivery total but misses visible throughput"),
		Starter.TotalDeliveredFuel >= StarterScenario.Contract.VictoryDeliveryTarget
			&& Starter.Timeline.Last().RecentIncomePerSecond
				< StarterScenario.Contract.VictoryRequiredIncomePerSecond);

	FSRRunBalanceScenario ScaledScenario = MakePressureReferenceScenario();
	ScaledScenario.ScenarioId = TEXT("ScaledLine");
	ScaledScenario.SupplyStages.Add(MakeStarterSupplyStage());
	FSRRunBalanceSupplyStage& Upgrade = ScaledScenario.SupplyStages.AddDefaulted_GetRef();
	Upgrade.StartTimeSeconds = 1500.0;
	Upgrade.FuelPerSecond = 35.6;
	Upgrade.DeliveryIntervalSeconds = 10.0;
	const FSRRunBalanceResult Scaled = FSRRunBalanceSimulator::Simulate(ScaledScenario);
	AddInfo(FString::Printf(
		TEXT("Pressure references | no supply %.0fs peak %.1f/s | starter %.0fs outcome %d delivered %.0f | scaled victory %.0fs delivered %.0f"),
		NoSupply.CompletionSeconds,
		NoSupply.PeakDemandPerSecond,
		Starter.SimulatedUntilSeconds,
		static_cast<int32>(Starter.Outcome),
		Starter.TotalDeliveredFuel,
		Scaled.CompletionSeconds,
		Scaled.TotalDeliveredFuel));
	TestTrue(TEXT("Scaling aggregate throughput to 118 per second wins inside the target window"),
		Scaled.Outcome == ESRStellarRunOutcome::Victory
			&& Scaled.bCompletedInsideTargetWindow
			&& Scaled.CompletionSeconds >= 1500.0
			&& Scaled.CompletionSeconds <= 1600.0);
	TestTrue(TEXT("Recovery fills but never exceeds the bounded survival reserve"),
		!Scaled.Timeline.IsEmpty()
			&& Scaled.Timeline.Last().StoredFuel <= 20000.0 + UE_DOUBLE_SMALL_NUMBER);
	return true;
}

#endif
