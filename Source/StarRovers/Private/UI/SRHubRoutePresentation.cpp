#include "UI/SRHubRoutePresentation.h"

#include "Logistics/SRConditionedTransitV2.h"
#include "Logistics/SRFleetCapacityV2.h"

namespace
{
	FString SafeLocationName(const FString& Name)
	{
		return Name.IsEmpty() ? TEXT("Unknown Body") : Name;
	}

	FString DockSideLabel(ESRSpaceLogisticsHubRouteDockSide DockSide)
	{
		return DockSide == ESRSpaceLogisticsHubRouteDockSide::Destination
			? TEXT("destination")
			: TEXT("source");
	}
}

FSRHubNetworkPresentation FSRHubRoutePresentationBuilder::BuildNetwork(
	const FSRHubNetworkPresentationInput& Input)
{
	FSRHubNetworkPresentation Result;
	if (!Input.bLogisticsAvailable)
	{
		Result.Condition = ESRHubNetworkCondition::Unavailable;
		Result.VisualState = ESRUIVisualState::Danger;
		Result.StatusLabel = NSLOCTEXT("StarRoversHubUI", "NetworkUnavailable", "LOGISTICS OFFLINE");
	}
	else if (Input.DestinationCount <= 0)
	{
		Result.Condition = ESRHubNetworkCondition::Isolated;
		Result.VisualState = ESRUIVisualState::Warning;
		Result.StatusLabel = NSLOCTEXT("StarRoversHubUI", "NetworkIsolated", "ISOLATED HUB");
	}
	else if (Input.FleetCapacity.bRulesActive && Input.FleetCapacity.QueuedDepartureCount > 0)
	{
		Result.Condition = ESRHubNetworkCondition::Queued;
		Result.VisualState = ESRUIVisualState::Warning;
		Result.StatusLabel = NSLOCTEXT("StarRoversHubUI", "NetworkQueued", "FLEET QUEUE");
	}
	else if (Input.FleetCapacity.bRulesActive
		&& Input.FleetCapacity.TotalCapacity > 0
		&& Input.FleetCapacity.ReservedLoad >= Input.FleetCapacity.TotalCapacity)
	{
		Result.Condition = ESRHubNetworkCondition::AtCapacity;
		Result.VisualState = ESRUIVisualState::Warning;
		Result.StatusLabel = NSLOCTEXT("StarRoversHubUI", "NetworkCapacity", "AT CAPACITY");
	}
	else
	{
		Result.Condition = ESRHubNetworkCondition::Ready;
		Result.VisualState = ESRUIVisualState::Positive;
		Result.StatusLabel = NSLOCTEXT("StarRoversHubUI", "NetworkReady", "NETWORK READY");
	}

	if (Input.FleetCapacity.bRulesActive)
	{
		Result.FleetValue = FText::FromString(FString::Printf(
			TEXT("%d / %d"),
			Input.FleetCapacity.ReservedLoad,
			Input.FleetCapacity.TotalCapacity));
		Result.FleetDetail = FText::FromString(FString::Printf(
			TEXT("%d available  |  %d supplied Berth%s"),
			Input.FleetCapacity.AvailableCapacity,
			Input.FleetCapacity.ActiveFleetBerthCount,
			Input.FleetCapacity.ActiveFleetBerthCount == 1 ? TEXT("") : TEXT("s")));
	}
	else
	{
		Result.FleetValue = NSLOCTEXT("StarRoversHubUI", "FleetLegacyValue", "UNLIMITED");
		Result.FleetDetail = NSLOCTEXT("StarRoversHubUI", "FleetLegacyDetail", "Fleet Capacity rules inactive");
	}

	Result.QueueValue = FText::AsNumber(Input.FleetCapacity.QueuedDepartureCount);
	Result.QueueDetail = FText::FromString(FString::Printf(
		TEXT("%d connected Route%s  |  %d destination%s"),
		Input.ConnectedRouteCount,
		Input.ConnectedRouteCount == 1 ? TEXT("") : TEXT("s"),
		Input.DestinationCount,
		Input.DestinationCount == 1 ? TEXT("") : TEXT("s")));
	Result.MissileValue = FText::AsNumber(Input.ActiveMissileCount);
	Result.MissileDetail = FText::FromString(FString::Printf(
		TEXT("%d auto-launch slot%s"),
		Input.AutoLaunchSlotCount,
		Input.AutoLaunchSlotCount == 1 ? TEXT("") : TEXT("s")));
	return Result;
}

FSRHubRouteCardPresentation FSRHubRoutePresentationBuilder::BuildRoute(
	const FSRHubRouteCardPresentationInput& Input)
{
	FSRHubRouteCardPresentation Result;
	Result.DirectionLabel = Input.bSelectedHubIsSource
		? NSLOCTEXT("StarRoversHubUI", "RouteOutbound", "OUTBOUND")
		: NSLOCTEXT("StarRoversHubUI", "RouteInbound", "INBOUND");
	Result.LaneTitle = FText::FromString(FString::Printf(
		TEXT("%s  ->  %s"),
		*SafeLocationName(Input.SourceName),
		*SafeLocationName(Input.DestinationName)));

	if (!Input.bHasRoute)
	{
		Result.Activity = ESRHubRouteCardActivity::NewDestination;
		Result.VisualState = Input.bSelected ? ESRUIVisualState::Selected : ESRUIVisualState::Neutral;
		Result.StatusLabel = Input.bSelected
			? NSLOCTEXT("StarRoversHubUI", "RouteReadyToLaunch", "READY TO CREATE")
			: NSLOCTEXT("StarRoversHubUI", "RouteNewDestination", "NEW DESTINATION");
		Result.PhaseDetail = Input.bSelected
			? NSLOCTEXT("StarRoversHubUI", "RoutePressLaunch", "Destination selected; press Create Route.")
			: NSLOCTEXT("StarRoversHubUI", "RouteSelectDestination", "Select this Hub to configure a new lane.");
		Result.CargoDetail = NSLOCTEXT("StarRoversHubUI", "RouteNoCargoYet", "Cargo policy is configured after creation.");
		Result.ProfileDetail = NSLOCTEXT("StarRoversHubUI", "RouteDefaultProfile", "Default: Neutral Shuttle");
		Result.ModuleDetail = NSLOCTEXT("StarRoversHubUI", "RouteNoModule", "Hold: State-neutral");
		return Result;
	}

	if (!Input.bEnabled)
	{
		Result.Activity = ESRHubRouteCardActivity::Disabled;
		Result.VisualState = ESRUIVisualState::Disabled;
		Result.StatusLabel = NSLOCTEXT("StarRoversHubUI", "RouteDisabled", "DISABLED");
		Result.PhaseDetail = NSLOCTEXT("StarRoversHubUI", "RouteDisabledDetail", "This Route is not participating in logistics.");
	}
	else
	{
		switch (Input.Phase)
		{
		case ESRSpaceLogisticsHubRoutePhase::WaitingForCargo:
			Result.Activity = ESRHubRouteCardActivity::WaitingForCargo;
			Result.VisualState = ESRUIVisualState::Neutral;
			Result.StatusLabel = NSLOCTEXT("StarRoversHubUI", "RouteWaitingCargo", "WAITING CARGO");
			Result.PhaseDetail = FText::FromString(FString::Printf(
				TEXT("Docked at %s; waiting for eligible cargo."),
				*DockSideLabel(Input.CurrentDockSide)));
			break;
		case ESRSpaceLogisticsHubRoutePhase::WaitingForFleetCapacity:
			Result.Activity = ESRHubRouteCardActivity::WaitingForFleet;
			Result.VisualState = ESRUIVisualState::Warning;
			Result.StatusLabel = NSLOCTEXT("StarRoversHubUI", "RouteWaitingFleet", "FLEET QUEUE");
			Result.PhaseDetail = FText::FromString(FString::Printf(
				TEXT("Departure queue #%d; cargo remains protected in the Export Buffer."),
				FMath::Max(1, Input.FleetQueuePosition)));
			break;
		case ESRSpaceLogisticsHubRoutePhase::TravelingToDestination:
		case ESRSpaceLogisticsHubRoutePhase::TravelingToSource:
			Result.Activity = ESRHubRouteCardActivity::Traveling;
			Result.VisualState = ESRUIVisualState::Positive;
			Result.StatusLabel = Input.Phase == ESRSpaceLogisticsHubRoutePhase::TravelingToDestination
				? NSLOCTEXT("StarRoversHubUI", "RouteOutboundTravel", "IN TRANSIT  ->")
				: NSLOCTEXT("StarRoversHubUI", "RouteReturnTravel", "<-  RETURNING");
			Result.ProgressRatio = FMath::Clamp(Input.TravelProgressRatio, 0.0f, 1.0f);
			Result.bShowProgress = true;
			Result.PhaseDetail = FText::FromString(FString::Printf(
				TEXT("Travel %.1f / %.1fs  |  %.0f%%"),
				Input.TravelProgressSeconds,
				FMath::Max(0.0f, Input.TravelDurationSeconds),
				Result.ProgressRatio * 100.0f));
			break;
		case ESRSpaceLogisticsHubRoutePhase::ConditioningAtDestination:
		case ESRSpaceLogisticsHubRoutePhase::ConditioningAtSource:
			Result.Activity = ESRHubRouteCardActivity::Conditioning;
			Result.VisualState = ESRUIVisualState::Info;
			Result.StatusLabel = NSLOCTEXT("StarRoversHubUI", "RouteConditioning", "CONDITIONING");
			Result.ProgressRatio = Input.ConditioningDurationSeconds > 0.0f
				? FMath::Clamp(Input.ConditioningProgressSeconds / Input.ConditioningDurationSeconds, 0.0f, 1.0f)
				: 0.0f;
			Result.bShowProgress = true;
			Result.PhaseDetail = FText::FromString(FString::Printf(
				TEXT("Dock process %.1f / %.1fs  |  Fleet Load remains reserved"),
				Input.ConditioningProgressSeconds,
				Input.ConditioningDurationSeconds));
			break;
		case ESRSpaceLogisticsHubRoutePhase::UnloadingAtDestination:
		case ESRSpaceLogisticsHubRoutePhase::UnloadingAtSource:
			Result.Activity = ESRHubRouteCardActivity::Unloading;
			Result.VisualState = ESRUIVisualState::Selected;
			Result.StatusLabel = NSLOCTEXT("StarRoversHubUI", "RouteUnloading", "UNLOADING");
			Result.PhaseDetail = FText::FromString(FString::Printf(
				TEXT("Docked at %s; transferring cargo."),
				*DockSideLabel(Input.CurrentDockSide)));
			break;
		case ESRSpaceLogisticsHubRoutePhase::Blocked:
			Result.Activity = ESRHubRouteCardActivity::Blocked;
			Result.VisualState = ESRUIVisualState::Danger;
			Result.StatusLabel = NSLOCTEXT("StarRoversHubUI", "RouteBlocked", "ROUTE BLOCKED");
			Result.PhaseDetail = NSLOCTEXT("StarRoversHubUI", "RouteBlockedDetail", "A dock, path, or cargo contract is unavailable.");
			break;
		case ESRSpaceLogisticsHubRoutePhase::Idle:
		default:
			Result.Activity = ESRHubRouteCardActivity::Idle;
			Result.VisualState = ESRUIVisualState::Info;
			Result.StatusLabel = NSLOCTEXT("StarRoversHubUI", "RouteIdle", "DOCKED");
			Result.PhaseDetail = FText::FromString(FString::Printf(
				TEXT("Idle at %s; ready for the next logistics cycle."),
				*DockSideLabel(Input.CurrentDockSide)));
			break;
		}
	}

	Result.CargoGlyph = FSRResourceGlyphPresentationBuilder::Build(Input.Cargo);
	Result.bShowCargoGlyph = Result.CargoGlyph.bHasResource
		&& Input.Cargo.StackCount > 0;
	const FString CargoFilter = Input.CargoResourceId.IsNone()
		? TEXT("Any eligible resource")
		: Input.CargoResourceId.ToString();
	Result.CargoDetail = FText::FromString(Result.bShowCargoGlyph
		? FString::Printf(
			TEXT("Cargo policy: Filter %s  |  Limit x%d"),
			*CargoFilter,
			FMath::Max(1, Input.MaxCargoStackCount))
		: FString::Printf(
			TEXT("Cargo EMPTY  |  Filter %s  |  Limit x%d"),
			*CargoFilter,
			FMath::Max(1, Input.MaxCargoStackCount)));

	const FSRSpaceLogisticsRouteProfileRulesV2 ProfileRules =
		FSRFleetCapacityV2::GetRouteProfileRules(Input.RouteProfile);
	Result.ProfileDetail = FText::FromString(FString::Printf(
		TEXT("%s  |  %s  |  Cargo %d  |  Fleet Load %d  |  %.1f cargo/load  |  Empty return %s"),
		*ProfileRules.DisplayName.ToString(),
		*ProfileRules.CargoContractText.ToString(),
		ProfileRules.CargoCapacity,
		ProfileRules.FleetLoad,
		FSRFleetCapacityV2::ResolveMaximumCargoPerFleetLoad(ProfileRules.Profile),
		Input.bReturnEmptyWhenNoCargo ? TEXT("ON") : TEXT("OFF")));

	const FSRConditionedTransitModuleRulesV2 ModuleRules =
		FSRConditionedTransitV2::GetModuleRules(Input.ConditionedTransitModule);
	Result.ModuleDetail = ModuleRules.IsConditionedModule()
		? FText::FromString(FString::Printf(
			TEXT("Hold: %s  |  %s"),
			*ModuleRules.DisplayName.ToString(),
			*ModuleRules.PreviewText.ToString()))
		: NSLOCTEXT("StarRoversHubUI", "RouteNeutralHold", "Hold: State-neutral");
	return Result;
}
