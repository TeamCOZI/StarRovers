#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/SRStrategicOverlayPresentation.h"

namespace StarRovers::StrategicOverlayTests
{
	FSRStrategicBodySnapshot MakeBody(
		FName BodyKey,
		const TCHAR* BodyName,
		int32 Demand,
		int32 Capacity,
		int32 ThrottledFacilities = 0)
	{
		FSRStrategicBodySnapshot Body;
		Body.BodyKey = BodyKey;
		Body.BodyName = FText::FromString(BodyName);
		Body.Operations.bIsValid = true;
		Body.Operations.OperationalCapacity.bRulesActive = true;
		Body.Operations.OperationalCapacity.BaseCapacity = Capacity;
		Body.Operations.OperationalCapacity.TotalCapacity = Capacity;
		Body.Operations.OperationalCapacity.TotalDemand = Demand;
		Body.Operations.OperationalCapacity.RemainingCapacity =
			static_cast<float>(FMath::Max(0, Capacity - Demand));
		Body.Operations.ThrottledFacilityCount = ThrottledFacilities;
		return Body;
	}

	FSRStrategicRouteSnapshot MakeRoute(
		FName RouteId,
		FName SourceBodyKey,
		FName DestinationBodyKey,
		ESRSpaceLogisticsHubRoutePhase Phase)
	{
		FSRStrategicRouteSnapshot Route;
		Route.RouteId = RouteId;
		Route.SourceBodyKey = SourceBodyKey;
		Route.DestinationBodyKey = DestinationBodyKey;
		Route.SourceBodyName = FText::FromName(SourceBodyKey);
		Route.DestinationBodyName = FText::FromName(DestinationBodyKey);
		Route.Phase = Phase;
		return Route;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStrategicOverlayPriorityTest,
	"StarRovers.UI.StrategicOverlay.Priority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStrategicOverlayPriorityTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::StrategicOverlayTests;
	FSRStrategicOverlayInput Input;
	Input.Bodies = {
		MakeBody(TEXT("Forge"), TEXT("Forge"), 37, 30, 2),
		MakeBody(TEXT("Relay"), TEXT("Relay"), 12, 30),
	};
	FSRStrategicRouteSnapshot QueueRoute = MakeRoute(
		TEXT("RelayQueue"),
		TEXT("Forge"),
		TEXT("Relay"),
		ESRSpaceLogisticsHubRoutePhase::WaitingForFleetCapacity);
	QueueRoute.CurrentDockSide = ESRSpaceLogisticsHubRouteDockSide::Destination;
	QueueRoute.FleetQueuePosition = 2;
	Input.Routes.Add(QueueRoute);

	const FSRStrategicOverlayPresentation Result =
		FSRStrategicOverlayPresentationBuilder::Build(Input);
	const FSRStrategicBodyPresentation* Forge = Result.FindBody(TEXT("Forge"));
	const FSRStrategicBodyPresentation* Relay = Result.FindBody(TEXT("Relay"));
	TestTrue(TEXT("An active bottleneck produces one focus recommendation"),
		Result.bHasRecommendation);
	TestEqual(TEXT("Operational overload outranks a fleet queue"),
		Result.RecommendedBodyKey,
		FName(TEXT("Forge")));
	TestNotNull(TEXT("Forge has a presentation"), Forge);
	TestNotNull(TEXT("Relay has a presentation"), Relay);
	if (Forge)
	{
		TestEqual(TEXT("Overload uses the hard-stop semantic kind"),
			Forge->BottleneckKind,
			ESRStrategicBottleneckKind::OperationalOverload);
		TestEqual(TEXT("Overload exposes the unsupported amount at a glance"),
			Forge->ShortBadgeText.ToString(),
			FString(TEXT("! LOAD +7")));
		TestEqual(TEXT("Overload uses the danger palette"),
			Forge->VisualState,
			ESRUIVisualState::Danger);
	}
	if (Relay)
	{
		TestEqual(TEXT("The queued dock body owns the fleet queue warning"),
			Relay->BottleneckKind,
			ESRStrategicBottleneckKind::FleetQueue);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStrategicOverlayBlockedRouteTest,
	"StarRovers.UI.StrategicOverlay.BlockedRouteHardStop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStrategicOverlayBlockedRouteTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::StrategicOverlayTests;
	FSRStrategicOverlayInput Input;
	Input.Bodies = {
		MakeBody(TEXT("Mine"), TEXT("Mine"), 14, 30),
		MakeBody(TEXT("Factory"), TEXT("Factory"), 45, 30, 1),
	};
	Input.Routes.Add(MakeRoute(
		TEXT("OreLane"),
		TEXT("Mine"),
		TEXT("Factory"),
		ESRSpaceLogisticsHubRoutePhase::Blocked));

	const FSRStrategicOverlayPresentation Result =
		FSRStrategicOverlayPresentationBuilder::Build(Input);
	const FSRStrategicRoutePresentation* Route = Result.FindRoute(TEXT("OreLane"));
	const FSRStrategicBodyPresentation* Source = Result.FindBody(TEXT("Mine"));
	const FSRStrategicBodyPresentation* Destination = Result.FindBody(TEXT("Factory"));
	TestEqual(TEXT("The overlay counts each blocked Route once"),
		Result.BlockedRouteCount,
		1);
	TestEqual(TEXT("A blocked Route is the highest-priority hard stop"),
		Result.RecommendedBodyKey,
		FName(TEXT("Mine")));
	TestNotNull(TEXT("The blocked Route has a visual presentation"), Route);
	if (Route)
	{
		TestEqual(TEXT("The blocked lane is danger colored"),
			Route->VisualState,
			ESRUIVisualState::Danger);
		TestEqual(TEXT("The Route surface names its exact state"),
			Route->StatusLabel.ToString(),
			FString(TEXT("BLOCKED")));
	}
	TestTrue(TEXT("The source body exposes the connected hard stop"),
		Source && Source->BottleneckKind == ESRStrategicBottleneckKind::RouteBlocked);
	TestTrue(TEXT("The destination body exposes the same connected hard stop"),
		Destination && Destination->BottleneckKind == ESRStrategicBottleneckKind::RouteBlocked);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStrategicOverlayNominalTest,
	"StarRovers.UI.StrategicOverlay.Nominal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStrategicOverlayNominalTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::StrategicOverlayTests;
	FSRStrategicOverlayInput Input;
	Input.Bodies.Add(MakeBody(TEXT("Home"), TEXT("Home"), 12, 30));

	const FSRStrategicOverlayPresentation Result =
		FSRStrategicOverlayPresentationBuilder::Build(Input);
	TestFalse(TEXT("A nominal network does not fabricate a recommendation"),
		Result.bHasRecommendation);
	TestEqual(TEXT("Nominal state uses the positive semantic palette"),
		Result.SummaryState,
		ESRUIVisualState::Positive);
	TestEqual(TEXT("The header remains glanceable when no action is required"),
		Result.SummaryLabel.ToString(),
		FString(TEXT("NETWORK NOMINAL")));
	return true;
}

#endif
