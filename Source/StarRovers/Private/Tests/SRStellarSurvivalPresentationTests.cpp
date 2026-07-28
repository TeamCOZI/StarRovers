#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/SRStellarSurvivalPresentation.h"

namespace
{
	FSRStellarFuelState MakeFuelState(
		double StoredFuel,
		double ConsumptionPerSecond,
		double RecentIncomePerSecond)
	{
		FSRStellarFuelState FuelState;
		FuelState.StoredFuel = StoredFuel;
		FuelState.InitialStageFuel = 1000.0;
		FuelState.RequiredFuelPerCycle = ConsumptionPerSecond;
		FuelState.bUsesStellarPressureCurveV2 = true;
		FuelState.DemandCurveV2.InitialDemandPerSecond = ConsumptionPerSecond;
		FuelState.DemandCurveV2.MaximumDemandPerSecond = FMath::Max(
			ConsumptionPerSecond,
			100.0);
		FuelState.RecentFuelIncomePerSecond = RecentIncomePerSecond;
		FuelState.FuelIncomeWindowSeconds = 30.0;
		return FuelState;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStellarSurvivalDeficitTest,
	"StarRovers.UI.RunCommand.SurvivalRail.DeficitAndCycleForecast",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStellarSurvivalDeficitTest::RunTest(const FString& Parameters)
{
	const FSRStellarSurvivalSnapshot Snapshot =
		FSRStellarSurvivalPresentationBuilder::BuildSnapshot(
			MakeFuelState(400.0, 50.0, 10.0),
			{},
			2,
			30.0f,
			false);

	TestTrue(TEXT("A negative fuel balance has a finite runway"), Snapshot.bHasFiniteRunway);
	TestTrue(TEXT("Current fuel runway uses the observed net deficit"),
		FMath::IsNearlyEqual(Snapshot.CurrentFuelRunwaySeconds, 10.0));
	TestTrue(TEXT("No inbound cargo leaves secured runway unchanged"),
		FMath::IsNearlyEqual(Snapshot.SecuredFuelRunwaySeconds, 10.0));
	TestTrue(TEXT("The next cycle demand follows the Star pressure rule"),
		FMath::IsNearlyEqual(Snapshot.NextCycleConsumptionPerSecond, 55.0));
	TestTrue(TEXT("The next cycle remains a deficit"), Snapshot.bNextCycleCreatesDeficit);

	const FSRStellarSurvivalPresentation Presentation =
		FSRStellarSurvivalPresentationBuilder::BuildPresentation(Snapshot);
	TestEqual(TEXT("A ten-second runway is a danger state"),
		Presentation.SurvivalVisualState,
		ESRUIVisualState::Danger);
	TestTrue(TEXT("The rail exposes survival time without opening a detail panel"),
		Presentation.SurvivalText.ToString().Contains(TEXT("00:10")));
	TestTrue(TEXT("The rail exposes the signed balance"),
		Presentation.NetText.ToString().Contains(TEXT("-40")));
	TestTrue(TEXT("The cycle caption previews the next demand instead of only naming the cycle"),
		Presentation.CycleText.ToString().Contains(TEXT("소비"))
			&& Presentation.CycleText.ToString().Contains(TEXT("/s")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStellarSurvivalStableTest,
	"StarRovers.UI.RunCommand.SurvivalRail.StableObservedIncome",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStellarSurvivalStableTest::RunTest(const FString& Parameters)
{
	const FSRStellarSurvivalSnapshot Snapshot =
		FSRStellarSurvivalPresentationBuilder::BuildSnapshot(
			MakeFuelState(400.0, 50.0, 60.0),
			{},
			0,
			60.0f,
			true);
	const FSRStellarSurvivalPresentation Presentation =
		FSRStellarSurvivalPresentationBuilder::BuildPresentation(Snapshot);

	TestFalse(TEXT("Observed income above demand removes the finite runway"), Snapshot.bHasFiniteRunway);
	TestEqual(TEXT("Stable income is positive"),
		Presentation.SurvivalVisualState,
		ESRUIVisualState::Positive);
	TestTrue(TEXT("Paused planning mode remains visible in the survival chip"),
		Presentation.SurvivalText.ToString().Contains(TEXT("정지")));
	TestTrue(TEXT("A stable balance is explicitly signed positive"),
		Presentation.NetText.ToString().Contains(TEXT("+10")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStellarSurvivalInboundProjectionTest,
	"StarRovers.UI.RunCommand.SurvivalRail.InboundProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStellarSurvivalInboundProjectionTest::RunTest(const FString& Parameters)
{
	FSRStellarFuelInboundProjection FirstArrival;
	FirstArrival.FuelAmount = 500.0;
	FirstArrival.SecondsUntilArrival = 1.0f;
	FSRStellarFuelInboundProjection LateArrival;
	LateArrival.FuelAmount = 200.0;
	LateArrival.SecondsUntilArrival = 20.0f;

	const FSRStellarSurvivalSnapshot SecuredSnapshot =
		FSRStellarSurvivalPresentationBuilder::BuildSnapshot(
			MakeFuelState(100.0, 50.0, 0.0),
			{ LateArrival, FirstArrival },
			0,
			60.0f,
			false);
	TestEqual(TEXT("All valid in-flight missiles are counted"), SecuredSnapshot.InboundMissileCount, 2);
	TestTrue(TEXT("Inbound fuel is sorted by ETA"),
		FMath::IsNearlyEqual(SecuredSnapshot.NextInboundFuel, 500.0));
	TestTrue(TEXT("The first missile arrives before current storage depletes"),
		SecuredSnapshot.bNextInboundArrivesBeforeDepletion);
	TestTrue(TEXT("Only arrivals reached before depletion extend secured runway"),
		FMath::IsNearlyEqual(SecuredSnapshot.SecuredFuelRunwaySeconds, 12.0));

	FirstArrival.SecondsUntilArrival = 3.0f;
	const FSRStellarSurvivalSnapshot MissedSnapshot =
		FSRStellarSurvivalPresentationBuilder::BuildSnapshot(
			MakeFuelState(100.0, 50.0, 0.0),
			{ FirstArrival },
			0,
			60.0f,
			false);
	const FSRStellarSurvivalPresentation MissedPresentation =
		FSRStellarSurvivalPresentationBuilder::BuildPresentation(MissedSnapshot);
	TestFalse(TEXT("A missile arriving after depletion is not treated as secured fuel"),
		MissedSnapshot.bNextInboundArrivesBeforeDepletion);
	TestTrue(TEXT("A late missile does not extend survival projection"),
		FMath::IsNearlyEqual(MissedSnapshot.SecuredFuelRunwaySeconds, 2.0));
	TestEqual(TEXT("A late arrival uses danger semantics"),
		MissedPresentation.InboundVisualState,
		ESRUIVisualState::Danger);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStellarSurvivalObjectiveTest,
	"StarRovers.UI.RunCommand.SurvivalRail.StellarObjective",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStellarSurvivalObjectiveTest::RunTest(const FString& Parameters)
{
	FSRStellarRunContract Contract;
	Contract.EmergencyDeliveryTarget = 100.0;
	Contract.SustainedSupplyDeliveryTarget = 300.0;
	Contract.VictoryDeliveryTarget = 600.0;
	Contract.VictoryRequiredIncomePerSecond = 20.0;
	Contract.VictoryRequiredSustainSeconds = 10.0;

	FSRStellarFuelState FuelState = MakeFuelState(400.0, 10.0, 15.0);
	FuelState.RunProgress = FSRStellarRunContractModel::Advance(
		Contract,
		FSRStellarRunContractModel::MakeInitialProgress(Contract),
		600.0,
		15.0,
		1.0,
		100.0,
		false);
	FSRStellarSurvivalPresentation Presentation =
		FSRStellarSurvivalPresentationBuilder::BuildPresentation(
			FSRStellarSurvivalPresentationBuilder::BuildSnapshot(
				FuelState, {}, 0, 60.0f, false));
	TestTrue(TEXT("Final stabilization exposes the missing throughput at a glance"),
		Presentation.ObjectiveText.ToString().Contains(TEXT("유입"))
			&& Presentation.ObjectiveText.ToString().Contains(TEXT("15/20")));
	TestEqual(TEXT("Missing final throughput is a warning"),
		Presentation.ObjectiveVisualState,
		ESRUIVisualState::Warning);

	FuelState.RunProgress = FSRStellarRunContractModel::Advance(
		Contract,
		FuelState.RunProgress,
		600.0,
		25.0,
		4.0,
		104.0,
		false);
	Presentation = FSRStellarSurvivalPresentationBuilder::BuildPresentation(
		FSRStellarSurvivalPresentationBuilder::BuildSnapshot(
			FuelState, {}, 0, 60.0f, false));
	TestTrue(TEXT("Meeting throughput switches the compact objective to sustain time"),
		Presentation.ObjectiveText.ToString().Contains(TEXT("유지"))
			&& Presentation.ObjectiveText.ToString().Contains(TEXT("00:04/00:10")));

	FuelState.RunProgress = FSRStellarRunContractModel::Advance(
		Contract,
		FuelState.RunProgress,
		600.0,
		25.0,
		6.0,
		110.0,
		false);
	Presentation = FSRStellarSurvivalPresentationBuilder::BuildPresentation(
		FSRStellarSurvivalPresentationBuilder::BuildSnapshot(
			FuelState, {}, 0, 60.0f, true));
	TestEqual(TEXT("Victory replaces pressure text with an explicit completion label"),
		Presentation.ObjectiveText.ToString(),
		FString(TEXT("목표 완료")));
	TestEqual(TEXT("A completed objective uses positive semantics"),
		Presentation.ObjectiveVisualState,
		ESRUIVisualState::Positive);
	return true;
}

#endif
