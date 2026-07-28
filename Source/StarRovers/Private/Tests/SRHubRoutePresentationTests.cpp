#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/SRHubRoutePresentation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRHubNetworkPresentationTest,
	"StarRovers.UI.HubRoutes.NetworkSummary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRHubNetworkPresentationTest::RunTest(const FString& Parameters)
{
	FSRHubNetworkPresentationInput Input;
	Input.bLogisticsAvailable = true;
	Input.DestinationCount = 4;
	Input.ConnectedRouteCount = 3;
	Input.ActiveMissileCount = 1;
	Input.AutoLaunchSlotCount = 2;
	Input.FleetCapacity.bRulesActive = true;
	Input.FleetCapacity.TotalCapacity = 16;
	Input.FleetCapacity.ReservedLoad = 6;
	Input.FleetCapacity.AvailableCapacity = 10;
	Input.FleetCapacity.ActiveFleetBerthCount = 1;

	FSRHubNetworkPresentation Presentation =
		FSRHubRoutePresentationBuilder::BuildNetwork(Input);
	TestEqual(TEXT("A connected Hub with spare capacity is ready"),
		Presentation.Condition,
		ESRHubNetworkCondition::Ready);
	TestEqual(TEXT("Ready logistics use the positive semantic state"),
		Presentation.VisualState,
		ESRUIVisualState::Positive);
	TestEqual(TEXT("Fleet Load is displayed as Reserved / Total"),
		Presentation.FleetValue.ToString(),
		FString(TEXT("6 / 16")));
	TestTrue(TEXT("Supplied Fleet Berths remain visible"),
		Presentation.FleetDetail.ToString().Contains(TEXT("1 supplied Berth")));

	Input.FleetCapacity.QueuedDepartureCount = 2;
	Presentation = FSRHubRoutePresentationBuilder::BuildNetwork(Input);
	TestEqual(TEXT("A departure queue takes priority over ready state"),
		Presentation.Condition,
		ESRHubNetworkCondition::Queued);
	TestEqual(TEXT("Queued logistics are a warning rather than destructive failure"),
		Presentation.VisualState,
		ESRUIVisualState::Warning);

	Input.FleetCapacity.QueuedDepartureCount = 0;
	Input.DestinationCount = 0;
	Presentation = FSRHubRoutePresentationBuilder::BuildNetwork(Input);
	TestEqual(TEXT("A Hub with no other endpoint is explicitly isolated"),
		Presentation.Condition,
		ESRHubNetworkCondition::Isolated);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRHubRouteCardPresentationTest,
	"StarRovers.UI.HubRoutes.RouteCards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRHubRouteCardPresentationTest::RunTest(const FString& Parameters)
{
	FSRHubRouteCardPresentationInput Input;
	Input.SourceName = TEXT("Concord");
	Input.DestinationName = TEXT("Helios");
	Input.bSelected = true;

	FSRHubRouteCardPresentation Presentation =
		FSRHubRoutePresentationBuilder::BuildRoute(Input);
	TestEqual(TEXT("An unconnected selected endpoint is ready to create"),
		Presentation.Activity,
		ESRHubRouteCardActivity::NewDestination);
	TestEqual(TEXT("A selected destination uses the selected semantic state"),
		Presentation.VisualState,
		ESRUIVisualState::Selected);
	TestTrue(TEXT("The lane always presents explicit direction"),
		Presentation.LaneTitle.ToString().Contains(TEXT("Concord  ->  Helios")));

	Input.bHasRoute = true;
	Input.RouteId = TEXT("Route_1");
	Input.Phase = ESRSpaceLogisticsHubRoutePhase::WaitingForFleetCapacity;
	Input.FleetQueuePosition = 3;
	Input.Cargo.ResourceId = TEXT("HeliosIron");
	Input.Cargo.ResourceClass = ESRResourceClass::Card;
	Input.Cargo.Family = ESRResourceFamily::Metal;
	Input.Cargo.CurrentEnergy = 55.0;
	Input.Cargo.Spectrum = ESRResourceSpectrum::Red;
	Input.Cargo.Grade = 2;
	Input.Cargo.StackCount = 4;
	Input.CargoResourceId = TEXT("HeliosIron");
	Input.MaxCargoStackCount = 8;
	Presentation = FSRHubRoutePresentationBuilder::BuildRoute(Input);
	TestEqual(TEXT("Fleet Capacity waiting is distinct from cargo waiting"),
		Presentation.Activity,
		ESRHubRouteCardActivity::WaitingForFleet);
	TestTrue(TEXT("Queue order is visible on the Route card"),
		Presentation.PhaseDetail.ToString().Contains(TEXT("#3")));
	TestTrue(TEXT("Queued cargo is described as protected"),
		Presentation.PhaseDetail.ToString().Contains(TEXT("protected")));
	TestTrue(TEXT("Loaded cargo is elevated into the shared Resource Glyph"),
		Presentation.bShowCargoGlyph
			&& Presentation.CargoGlyph.DisplayName.ToString().Contains(TEXT("HeliosIron"))
			&& Presentation.CargoGlyph.SpectrumGradeToken.ToString().Contains(TEXT("R2"))
			&& Presentation.CargoGlyph.StackToken.ToString() == TEXT("x4"));
	TestTrue(TEXT("Cargo policy stays visible without duplicating the manifest"),
		Presentation.CargoDetail.ToString().Contains(TEXT("Filter HeliosIron"))
			&& Presentation.CargoDetail.ToString().Contains(TEXT("Limit x8")));

	Input.Phase = ESRSpaceLogisticsHubRoutePhase::TravelingToDestination;
	Input.TravelProgressSeconds = 3.0f;
	Input.TravelDurationSeconds = 8.0f;
	Input.TravelProgressRatio = 0.375f;
	Presentation = FSRHubRoutePresentationBuilder::BuildRoute(Input);
	TestEqual(TEXT("Travel has its own active state"),
		Presentation.Activity,
		ESRHubRouteCardActivity::Traveling);
	TestTrue(TEXT("Travel exposes a progress bar contract"), Presentation.bShowProgress);
	TestTrue(TEXT("Travel ratio remains exact for the progress bar"),
		FMath::IsNearlyEqual(Presentation.ProgressRatio, 0.375f));

	Input.Phase = ESRSpaceLogisticsHubRoutePhase::ConditioningAtDestination;
	Input.ConditioningProgressSeconds = 4.0f;
	Input.ConditioningDurationSeconds = 10.0f;
	Input.RouteProfile = ESRSpaceLogisticsRouteProfileV2::ConditionedHold;
	Input.ConditionedTransitModule = ESRConditionedTransitModuleV2::CryogenicHold;
	Presentation = FSRHubRoutePresentationBuilder::BuildRoute(Input);
	TestEqual(TEXT("Conditioned Hold dwell is a visible Line step"),
		Presentation.Activity,
		ESRHubRouteCardActivity::Conditioning);
	TestTrue(TEXT("Conditioning warns that Fleet Load stays reserved"),
		Presentation.PhaseDetail.ToString().Contains(TEXT("remains reserved")));
	TestTrue(TEXT("Conditioned Hold contract exposes Cargo 4 and Fleet Load 3"),
		Presentation.ProfileDetail.ToString().Contains(TEXT("Cargo 4"))
		&& Presentation.ProfileDetail.ToString().Contains(TEXT("Fleet Load 3")));
	TestTrue(TEXT("The Route card exposes eligibility and normalized hull efficiency"),
		Presentation.ProfileDetail.ToString().Contains(TEXT("matching the fitted Hold module"))
			&& Presentation.ProfileDetail.ToString().Contains(TEXT("1.3 cargo/load")));
	TestTrue(TEXT("The concrete Hold module is named"),
		Presentation.ModuleDetail.ToString().Contains(TEXT("Cryogenic Hold")));

	Input.Phase = ESRSpaceLogisticsHubRoutePhase::Blocked;
	Presentation = FSRHubRoutePresentationBuilder::BuildRoute(Input);
	TestEqual(TEXT("Blocked routes use the danger semantic state"),
		Presentation.VisualState,
		ESRUIVisualState::Danger);
	return true;
}

#endif
