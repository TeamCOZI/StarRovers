#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Simulation/SRSimulationSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRRunStartPolicySettingsTest,
	"StarRovers.UI.RunCommand.StartPolicySettings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRRunStartPolicySettingsTest::RunTest(const FString& Parameters)
{
	const USRSimulationSettings* Settings = GetDefault<USRSimulationSettings>();
	TestNotNull(TEXT("The project exposes Run start policy through Simulation Settings"), Settings);
	if (!IsValid(Settings))
	{
		return false;
	}

	TestTrue(
		TEXT("The shipped Run starts paused so the opening system scan cannot consume stellar fuel"),
		Settings->bPauseSimulationOnRunStart);
	TestFalse(
		TEXT("The shipped Run does not flood the Build Dock with debug-only Facility unlocks"),
		Settings->bDebugUnlockAllFacilitiesWithoutAugments);
	TestFalse(
		TEXT("The shipped Run preserves Augment package progression"),
		Settings->bDebugUnlockAllAugmentPackagesV2);

	const FSRStellarRunContract Contract = Settings->BuildStellarRunContractV2();
	TestTrue(TEXT("The shipped Run has a finite victory condition"),
		Contract.bFiniteVictoryEnabled);
	TestTrue(TEXT("Stellar delivery milestones are monotonic"),
		Contract.EmergencyDeliveryTarget > 0.0
			&& Contract.EmergencyDeliveryTarget < Contract.SustainedSupplyDeliveryTarget
			&& Contract.SustainedSupplyDeliveryTarget < Contract.VictoryDeliveryTarget);
	TestTrue(TEXT("Victory requires both throughput and a continuous sustain window"),
		Contract.VictoryRequiredIncomePerSecond > 0.0
			&& Contract.VictoryRequiredSustainSeconds > 0.0);
	TestTrue(TEXT("The balancing target represents a 25-35 minute Run"),
		Contract.TargetRunDurationSeconds >= 25.0 * 60.0
			&& Contract.TargetRunDurationSeconds <= 35.0 * 60.0);
	TestTrue(TEXT("The shipped Run keeps bounded in-memory balance telemetry enabled"),
		Settings->bEnableRunTelemetryV2
			&& Settings->RunTelemetrySampleIntervalSecondsV2 >= 1.0
			&& Settings->RunTelemetryMaxSamplesV2 >= 16);

	const FSRStellarDemandCurveV2 DemandCurve = Settings->BuildStellarDemandCurveV2();
	const FSRStellarPressureRulesV2 PressureRules = Settings->BuildStellarPressureRulesV2();
	TestTrue(TEXT("The shipped Run replaces legacy exponential demand with the readable V2 curve"),
		Settings->bUseStellarPressureCurveV2
			&& DemandCurve.InitialDemandPerSecond > 0.0
			&& DemandCurve.GraceCycleCount >= 1
			&& DemandCurve.DemandIncreasePerCycle > 0.0
			&& DemandCurve.MaximumDemandPerSecond
				> DemandCurve.InitialDemandPerSecond);
	TestTrue(TEXT("The final pressure plateau matches the visible stabilization throughput"),
		FMath::IsNearlyEqual(
			DemandCurve.MaximumDemandPerSecond,
			Contract.VictoryRequiredIncomePerSecond));
	TestTrue(TEXT("The bounded survival reserve cannot absorb an entire victory target"),
		PressureRules.FuelReserveCapacity > 0.0
			&& PressureRules.FuelReserveCapacity < Contract.VictoryDeliveryTarget
			&& PressureRules.RedGiantEmergencyReserveRatio > 0.0
			&& PressureRules.RedGiantEmergencyReserveRatio <= 1.0);
	return true;
}

#endif
