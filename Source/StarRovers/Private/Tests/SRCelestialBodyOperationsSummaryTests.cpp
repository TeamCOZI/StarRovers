#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/SRCelestialBodyOperationsSummary.h"

namespace StarRovers::CelestialBodyOperationsSummaryTests
{
	FSRCelestialBodyOperationsSummary MakeSummary(int32 Demand, int32 Capacity)
	{
		FSRCelestialBodyOperationsSummary Summary;
		Summary.bIsValid = true;
		Summary.OperationalCapacity.bRulesActive = true;
		Summary.OperationalCapacity.BaseCapacity = Capacity;
		Summary.OperationalCapacity.TotalCapacity = Capacity;
		Summary.OperationalCapacity.TotalDemand = Demand;
		Summary.OperationalCapacity.RemainingCapacity =
			static_cast<float>(FMath::Max(0, Capacity - Demand));
		return Summary;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRCelestialBodyOperationsPressureAndStatusTest,
	"StarRovers.ResourceSystem.UI.BodyOperations.PressureAndStatus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRCelestialBodyOperationsPressureAndStatusTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::CelestialBodyOperationsSummaryTests;

	const FSRCelestialBodyOperationsSummary Idle = MakeSummary(0, 30);
	const FSRCelestialBodyOperationsSummary Nominal = MakeSummary(18, 30);
	const FSRCelestialBodyOperationsSummary Near = MakeSummary(24, 30);
	const FSRCelestialBodyOperationsSummary Full = MakeSummary(30, 30);
	const FSRCelestialBodyOperationsSummary Over = MakeSummary(37, 30);

	TestEqual(TEXT("Zero active Load is idle"),
		FSRCelestialBodyOperationsSummaryBuilder::ResolveOperationalPressure(Idle),
		ESRCelestialBodyOperationsPressure::Idle);
	TestEqual(TEXT("Load below eighty percent is nominal"),
		FSRCelestialBodyOperationsSummaryBuilder::ResolveOperationalPressure(Nominal),
		ESRCelestialBodyOperationsPressure::Nominal);
	TestEqual(TEXT("Eighty percent begins the near-capacity warning band"),
		FSRCelestialBodyOperationsSummaryBuilder::ResolveOperationalPressure(Near),
		ESRCelestialBodyOperationsPressure::NearCapacity);
	TestEqual(TEXT("Exact saturation is distinct from overload"),
		FSRCelestialBodyOperationsSummaryBuilder::ResolveOperationalPressure(Full),
		ESRCelestialBodyOperationsPressure::AtCapacity);
	TestEqual(TEXT("Demand above Capacity is overload"),
		FSRCelestialBodyOperationsSummaryBuilder::ResolveOperationalPressure(Over),
		ESRCelestialBodyOperationsPressure::OverCapacity);
	TestTrue(TEXT("Overload status exposes the unsupported Load"),
		FSRCelestialBodyOperationsSummaryBuilder::BuildOperationalStatusText(Over)
			.Contains(TEXT("7 load unsupported")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRCelestialBodyOperationsFormattingContractTest,
	"StarRovers.ResourceSystem.UI.BodyOperations.FormattingContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRCelestialBodyOperationsFormattingContractTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::CelestialBodyOperationsSummaryTests;

	FSRCelestialBodyOperationsSummary Summary = MakeSummary(18, 48);
	Summary.OperationalCapacity.BaseCapacity = 30;
	Summary.OperationalCapacity.ActiveServiceCoreCount = 1;
	Summary.OperationalCapacity.ServiceCoreCapacity = 18;
	Summary.FacilityCount = 12;
	Summary.EnabledFacilityCount = 10;
	Summary.ProcessingFacilityCount = 5;
	Summary.ThrottledFacilityCount = 0;

	TestEqual(TEXT("Overview badge remains compact"),
		FSRCelestialBodyOperationsSummaryBuilder::BuildOperationalBadgeText(Summary),
		FString(TEXT("L 18/48")));
	TestTrue(TEXT("Tooltip explains that Load is dynamic"),
		FSRCelestialBodyOperationsSummaryBuilder::BuildOperationalToolTipText(Summary)
			.Contains(TEXT("processing right now")));
	TestTrue(TEXT("Tooltip exposes the Service Core capacity source"),
		FSRCelestialBodyOperationsSummaryBuilder::BuildOperationalToolTipText(Summary)
			.Contains(TEXT("Service Cores 1 (+18)")));
	TestTrue(TEXT("Tooltip exposes installed and processing facility counts"),
		FSRCelestialBodyOperationsSummaryBuilder::BuildOperationalToolTipText(Summary)
			.Contains(TEXT("Facilities 12, enabled 10, processing 5")));

	FSRCelestialBodyOperationsSummary Invalid;
	TestTrue(TEXT("Bodies without a Facility Network have no misleading badge"),
		FSRCelestialBodyOperationsSummaryBuilder::BuildOperationalBadgeText(Invalid).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRCelestialBodyResourceReserveFormattingTest,
	"StarRovers.ResourceSystem.Phase20.UI.BodyOperations.ResourceReserve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRCelestialBodyResourceReserveFormattingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace StarRovers::CelestialBodyOperationsSummaryTests;
	FSRCelestialBodyOperationsSummary Summary = MakeSummary(18, 30);
	Summary.ResourceReserve.bHasDeposits = true;
	Summary.ResourceReserve.DepositCount = 7;
	Summary.ResourceReserve.ActiveDepositCount = 5;
	Summary.ResourceReserve.DepletedDepositCount = 2;
	Summary.ResourceReserve.TotalFiniteAmount = 1000;
	Summary.ResourceReserve.RemainingFiniteAmount = 240;
	Summary.ResourceReserve.RemainingCardAmount = 180;
	Summary.ResourceReserve.RemainingUtilityAmount = 60;
	Summary.ResourceReserve.RemainingRatio = 0.24f;
	Summary.ResourceReserve.Pressure = ESRResourceReservePressure::Low;

	TestEqual(TEXT("The overview badge adds one glanceable reserve number"),
		FSRCelestialBodyOperationsSummaryBuilder::BuildOperationalBadgeText(Summary),
		FString(TEXT("L 18/30 | R 24%")));
	TestTrue(TEXT("The focus row separates Card and raw utility stock"),
		FSRCelestialBodyOperationsSummaryBuilder::BuildResourceReserveText(Summary)
			.Contains(TEXT("CARD 180 | RAW 60")));
	TestTrue(TEXT("Low reserve status recommends the next Miner before depletion"),
		FSRCelestialBodyOperationsSummaryBuilder::BuildResourceReserveStatusText(Summary)
			.Contains(TEXT("prepare the next Miner")));
	TestTrue(TEXT("The tooltip exposes depleted vein count"),
		FSRCelestialBodyOperationsSummaryBuilder::BuildOperationalToolTipText(Summary)
			.Contains(TEXT("depleted 2")));
	return !HasAnyErrors();
}

#endif
