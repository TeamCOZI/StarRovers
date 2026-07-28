#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Simulation/SRStellarRunContract.h"

namespace
{
	FSRStellarRunContract MakeTestContract()
	{
		FSRStellarRunContract Contract;
		Contract.bFiniteVictoryEnabled = true;
		Contract.EmergencyDeliveryTarget = 100.0;
		Contract.SustainedSupplyDeliveryTarget = 300.0;
		Contract.VictoryDeliveryTarget = 600.0;
		Contract.VictoryRequiredIncomePerSecond = 20.0;
		Contract.VictoryRequiredSustainSeconds = 10.0;
		Contract.TargetRunDurationSeconds = 1800.0;
		return Contract;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStellarRunContractPhaseTest,
	"StarRovers.UI.RunCommand.StellarContract.PhaseProgression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStellarRunContractPhaseTest::RunTest(const FString& Parameters)
{
	const FSRStellarRunContract Contract = MakeTestContract();
	FSRStellarRunProgress Progress = FSRStellarRunContractModel::MakeInitialProgress(Contract);
	TestEqual(TEXT("A new Run begins in emergency ignition"),
		Progress.Phase,
		ESRStellarRunPhase::EmergencyIgnition);

	Progress = FSRStellarRunContractModel::Advance(
		Contract, Progress, 100.0, 0.0, 0.0, 10.0, false);
	TestEqual(TEXT("The emergency delivery target advances to sustained supply"),
		Progress.Phase,
		ESRStellarRunPhase::SustainedSupply);

	Progress = FSRStellarRunContractModel::Advance(
		Contract, Progress, 300.0, 0.0, 0.0, 20.0, false);
	TestEqual(TEXT("The second delivery target advances to final stabilization"),
		Progress.Phase,
		ESRStellarRunPhase::FinalStabilization);
	TestFalse(TEXT("Reaching the final phase is not itself a victory"), Progress.HasEnded());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStellarRunContractSustainTest,
	"StarRovers.UI.RunCommand.StellarContract.SustainedVictory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStellarRunContractSustainTest::RunTest(const FString& Parameters)
{
	const FSRStellarRunContract Contract = MakeTestContract();
	FSRStellarRunProgress Progress = FSRStellarRunContractModel::MakeInitialProgress(Contract);
	Progress = FSRStellarRunContractModel::Advance(
		Contract, Progress, 600.0, 20.0, 4.0, 100.0, false);
	TestEqual(TEXT("Meeting both final requirements starts the sustain timer"),
		Progress.SustainedIncomeProgressSeconds,
		4.0);
	TestEqual(TEXT("The Run remains active until the full sustain duration"),
		Progress.Outcome,
		ESRStellarRunOutcome::InProgress);

	Progress = FSRStellarRunContractModel::Advance(
		Contract, Progress, 600.0, 19.0, 1.0, 101.0, false);
	TestEqual(TEXT("Dropping below the required income resets continuous sustain"),
		Progress.SustainedIncomeProgressSeconds,
		0.0);

	Progress = FSRStellarRunContractModel::Advance(
		Contract, Progress, 600.0, 25.0, 10.0, 111.0, false);
	TestEqual(TEXT("A full continuous sustain window completes the Run"),
		Progress.Outcome,
		ESRStellarRunOutcome::Victory);
	TestEqual(TEXT("Victory uses the terminal complete phase"),
		Progress.Phase,
		ESRStellarRunPhase::Complete);
	TestTrue(TEXT("Victory records authoritative simulation completion time"),
		FMath::IsNearlyEqual(Progress.CompletionSimulationSeconds, 111.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStellarRunContractTerminalTest,
	"StarRovers.UI.RunCommand.StellarContract.TerminalOutcomes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStellarRunContractTerminalTest::RunTest(const FString& Parameters)
{
	const FSRStellarRunContract Contract = MakeTestContract();
	FSRStellarRunProgress Progress = FSRStellarRunContractModel::MakeInitialProgress(Contract);
	Progress = FSRStellarRunContractModel::Advance(
		Contract, Progress, 50.0, 0.0, 1.0, 30.0, true);
	TestEqual(TEXT("A stellar collapse produces a defeat outcome"),
		Progress.Outcome,
		ESRStellarRunOutcome::Defeat);

	const FSRStellarRunProgress TerminalProgress = FSRStellarRunContractModel::Advance(
		Contract, Progress, 1000.0, 1000.0, 1000.0, 1030.0, false);
	TestEqual(TEXT("A terminal defeat cannot later become a victory"),
		TerminalProgress.Outcome,
		ESRStellarRunOutcome::Defeat);
	TestTrue(TEXT("Terminal completion time is immutable"),
		FMath::IsNearlyEqual(TerminalProgress.CompletionSimulationSeconds, 30.0));
	TestTrue(TEXT("Terminal objective progress is immutable"),
		FMath::IsNearlyEqual(TerminalProgress.TotalDeliveredFuel, 50.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStellarRunContractSanitizationTest,
	"StarRovers.UI.RunCommand.StellarContract.Sanitization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStellarRunContractSanitizationTest::RunTest(const FString& Parameters)
{
	FSRStellarRunContract Invalid;
	Invalid.EmergencyDeliveryTarget = 300.0;
	Invalid.SustainedSupplyDeliveryTarget = 100.0;
	Invalid.VictoryDeliveryTarget = -1.0;
	Invalid.VictoryRequiredIncomePerSecond = -10.0;
	Invalid.VictoryRequiredSustainSeconds = -5.0;
	const FSRStellarRunContract Sanitized = FSRStellarRunContractModel::Sanitize(Invalid);

	TestTrue(TEXT("Delivery thresholds become monotonic"),
		Sanitized.EmergencyDeliveryTarget <= Sanitized.SustainedSupplyDeliveryTarget
			&& Sanitized.SustainedSupplyDeliveryTarget <= Sanitized.VictoryDeliveryTarget);
	TestEqual(TEXT("Invalid income requirements clamp to zero"),
		Sanitized.VictoryRequiredIncomePerSecond,
		0.0);
	TestEqual(TEXT("Invalid sustain requirements clamp to zero"),
		Sanitized.VictoryRequiredSustainSeconds,
		0.0);
	return true;
}

#endif
