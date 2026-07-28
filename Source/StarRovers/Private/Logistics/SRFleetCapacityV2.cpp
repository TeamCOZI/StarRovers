#include "Logistics/SRFleetCapacityV2.h"

namespace
{
	const FName NeutralShuttleProfileId(TEXT("NeutralShuttle"));
	const FName CardCourierProfileId(TEXT("CardCourier"));
	const FName BulkRawHoldProfileId(TEXT("BulkRawHold"));
	const FName ConditionedHoldProfileId(TEXT("ConditionedHold"));
}

void FSRFleetCapacityV2::GetRouteProfiles(
	TArray<ESRSpaceLogisticsRouteProfileV2>& OutProfiles)
{
	OutProfiles = {
		ESRSpaceLogisticsRouteProfileV2::NeutralShuttle,
		ESRSpaceLogisticsRouteProfileV2::CardCourier,
		ESRSpaceLogisticsRouteProfileV2::BulkRawHold,
		ESRSpaceLogisticsRouteProfileV2::ConditionedHold,
	};
}

FSRSpaceLogisticsRouteProfileRulesV2 FSRFleetCapacityV2::GetRouteProfileRules(
	ESRSpaceLogisticsRouteProfileV2 Profile)
{
	FSRSpaceLogisticsRouteProfileRulesV2 Rules;
	Rules.Profile = Profile;
	switch (Profile)
	{
	case ESRSpaceLogisticsRouteProfileV2::CardCourier:
		Rules.ProfileId = CardCourierProfileId;
		Rules.DisplayName = NSLOCTEXT("StarRoversFleetCapacityV2", "CardCourier", "Card Courier");
		Rules.CargoContractText = NSLOCTEXT("StarRoversFleetCapacityV2", "CardCourierContract", "Card cargo");
		Rules.CargoCapacity = 12;
		Rules.FleetLoad = 2;
		break;
	case ESRSpaceLogisticsRouteProfileV2::BulkRawHold:
		Rules.ProfileId = BulkRawHoldProfileId;
		Rules.DisplayName = NSLOCTEXT("StarRoversFleetCapacityV2", "BulkRawHold", "Bulk Raw Hold");
		Rules.CargoContractText = NSLOCTEXT("StarRoversFleetCapacityV2", "BulkRawHoldContract", "Utility or untouched Card cargo");
		Rules.CargoCapacity = 16;
		Rules.FleetLoad = 3;
		Rules.bRequiresAugmentUnlock = true;
		break;
	case ESRSpaceLogisticsRouteProfileV2::ConditionedHold:
		Rules.ProfileId = ConditionedHoldProfileId;
		Rules.DisplayName = NSLOCTEXT("StarRoversFleetCapacityV2", "ConditionedHold", "Conditioned Hold");
		Rules.CargoContractText = NSLOCTEXT("StarRoversFleetCapacityV2", "ConditionedHoldContract", "Card cargo matching the fitted Hold module");
		Rules.CargoCapacity = 4;
		Rules.FleetLoad = 3;
		Rules.bSupportsConditionedTransit = true;
		Rules.bRequiresAugmentUnlock = true;
		break;
	case ESRSpaceLogisticsRouteProfileV2::NeutralShuttle:
	default:
		Rules.Profile = ESRSpaceLogisticsRouteProfileV2::NeutralShuttle;
		Rules.ProfileId = NeutralShuttleProfileId;
		Rules.DisplayName = NSLOCTEXT("StarRoversFleetCapacityV2", "NeutralShuttle", "Neutral Shuttle");
		Rules.CargoContractText = NSLOCTEXT("StarRoversFleetCapacityV2", "NeutralShuttleContract", "Any cargo (compatibility hull)");
		Rules.CargoCapacity = 8;
		Rules.FleetLoad = 2;
		break;
	}
	return Rules;
}

FName FSRFleetCapacityV2::GetRouteProfileId(
	ESRSpaceLogisticsRouteProfileV2 Profile)
{
	return GetRouteProfileRules(Profile).ProfileId;
}

bool FSRFleetCapacityV2::TryResolveRouteProfileId(
	FName ProfileId,
	ESRSpaceLogisticsRouteProfileV2& OutProfile)
{
	TArray<ESRSpaceLogisticsRouteProfileV2> Profiles;
	GetRouteProfiles(Profiles);
	for (const ESRSpaceLogisticsRouteProfileV2 Profile : Profiles)
	{
		if (GetRouteProfileId(Profile) == ProfileId)
		{
			OutProfile = Profile;
			return true;
		}
	}

	OutProfile = ESRSpaceLogisticsRouteProfileV2::NeutralShuttle;
	return false;
}

bool FSRFleetCapacityV2::IsTechnologyRouteProfile(
	ESRSpaceLogisticsRouteProfileV2 Profile)
{
	return !GetRouteProfileRules(Profile).bRequiresAugmentUnlock;
}

ESRSpaceLogisticsRouteProfileV2 FSRFleetCapacityV2::GetNextRouteProfile(
	ESRSpaceLogisticsRouteProfileV2 Profile)
{
	switch (Profile)
	{
	case ESRSpaceLogisticsRouteProfileV2::NeutralShuttle:
		return ESRSpaceLogisticsRouteProfileV2::CardCourier;
	case ESRSpaceLogisticsRouteProfileV2::CardCourier:
		return ESRSpaceLogisticsRouteProfileV2::BulkRawHold;
	case ESRSpaceLogisticsRouteProfileV2::BulkRawHold:
		return ESRSpaceLogisticsRouteProfileV2::ConditionedHold;
	case ESRSpaceLogisticsRouteProfileV2::ConditionedHold:
	default:
		return ESRSpaceLogisticsRouteProfileV2::NeutralShuttle;
	}
}

int32 FSRFleetCapacityV2::ResolveEffectiveCargoCapacity(const FSRSpaceLogisticsHubRoute& Route)
{
	const FSRSpaceLogisticsRouteProfileRulesV2 Rules = GetRouteProfileRules(Route.RouteProfile);
	return FMath::Clamp(Route.MaxCargoStackCount, 1, FMath::Max(1, Rules.CargoCapacity));
}

int32 FSRFleetCapacityV2::ResolveFleetLoad(const FSRSpaceLogisticsHubRoute& Route)
{
	return FMath::Max(0, GetRouteProfileRules(Route.RouteProfile).FleetLoad);
}

double FSRFleetCapacityV2::ResolveMaximumCargoPerFleetLoad(
	ESRSpaceLogisticsRouteProfileV2 Profile)
{
	const FSRSpaceLogisticsRouteProfileRulesV2 Rules = GetRouteProfileRules(Profile);
	return static_cast<double>(FMath::Max(0, Rules.CargoCapacity))
		/ static_cast<double>(FMath::Max(1, Rules.FleetLoad));
}

bool FSRFleetCapacityV2::IsUntouchedCard(const FSRResourceInstance& Cargo)
{
	return Cargo.ResourceClass == ESRResourceClass::Card
		&& Cargo.ProcessingMemory.ProcessCount == 0
		&& Cargo.ProcessingMemory.EnergyChangeCount == 0
		&& Cargo.ActiveFamilyStateFlags == 0
		&& Cargo.ProcessTagSlot.TagId.IsNone()
		&& Cargo.ProcessTagSlot.Lifecycle == ESRResourceSlotLifecycle::Empty
		&& Cargo.ProcessTagSlot.RemainingTriggers == 0
		&& Cargo.FuelImprintSlot.ImprintId.IsNone();
}

bool FSRFleetCapacityV2::IsCargoEligible(
	ESRSpaceLogisticsRouteProfileV2 Profile,
	const FSRResourceInstance& Cargo)
{
	if (Cargo.ResourceId.IsNone() || Cargo.StackCount <= 0)
	{
		return false;
	}

	switch (Profile)
	{
	case ESRSpaceLogisticsRouteProfileV2::CardCourier:
	case ESRSpaceLogisticsRouteProfileV2::ConditionedHold:
		return Cargo.ResourceClass == ESRResourceClass::Card;
	case ESRSpaceLogisticsRouteProfileV2::BulkRawHold:
		return Cargo.ResourceClass == ESRResourceClass::Utility || IsUntouchedCard(Cargo);
	case ESRSpaceLogisticsRouteProfileV2::NeutralShuttle:
	default:
		return true;
	}
}

FSRFleetCapacityReportV2 FSRFleetCapacityV2::BuildReport(
	const FSRSpaceLogisticsHubEndpoint& HubEndpoint,
	const TArray<FSRSpaceLogisticsHubRoute>& Routes,
	bool bRulesActive,
	int32 ActiveFleetBerthCount,
	int32 BaseCapacity,
	int32 CapacityPerFleetBerth)
{
	FSRFleetCapacityReportV2 Report;
	Report.bRulesActive = bRulesActive;
	Report.BaseCapacity = FMath::Max(0, BaseCapacity);
	Report.ActiveFleetBerthCount = bRulesActive ? FMath::Max(0, ActiveFleetBerthCount) : 0;
	Report.FleetBerthCapacity = Report.ActiveFleetBerthCount * FMath::Max(0, CapacityPerFleetBerth);
	Report.TotalCapacity = Report.BaseCapacity + Report.FleetBerthCapacity;

	if (bRulesActive)
	{
		for (const FSRSpaceLogisticsHubRoute& Route : Routes)
		{
			if (!Route.bEnabled || Route.bDebugLocalOrbit)
			{
				continue;
			}

			FSRSpaceLogisticsHubEndpoint ReservationHub;
			if (TryGetReservedDepartureHub(Route, ReservationHub)
				&& AreEndpointKeysEqual(ReservationHub, HubEndpoint))
			{
				Report.ReservedLoad += ResolveFleetLoad(Route);
			}

			FSRSpaceLogisticsHubEndpoint QueueHub;
			if (TryGetQueuedDepartureHub(Route, QueueHub)
				&& AreEndpointKeysEqual(QueueHub, HubEndpoint))
			{
				++Report.QueuedDepartureCount;
			}
		}
	}

	Report.AvailableCapacity = FMath::Max(0, Report.TotalCapacity - Report.ReservedLoad);
	return Report;
}

bool FSRFleetCapacityV2::CanGrantDeparture(
	const FSRSpaceLogisticsHubRoute& Route,
	const FSRSpaceLogisticsHubEndpoint& DockHub,
	const TArray<FSRSpaceLogisticsHubRoute>& Routes,
	const FSRFleetCapacityReportV2& Report)
{
	if (!Report.bRulesActive || Route.bDebugLocalOrbit)
	{
		return true;
	}
	if (!Route.bEnabled || Report.AvailableCapacity < ResolveFleetLoad(Route))
	{
		return false;
	}
	if (Report.QueuedDepartureCount > 0)
	{
		if (Route.FleetDepartureQueueSequence <= 0)
		{
			return false;
		}
		if (Route.FleetQueuePosition > 0)
		{
			return Route.FleetQueuePosition == 1;
		}
	}

	// Queue positions are refreshed once before Route processing. Keep this
	// fallback for imported/test data queried before the first subsystem tick.
	for (const FSRSpaceLogisticsHubRoute& Candidate : Routes)
	{
		if (!Candidate.bEnabled
			|| Candidate.bDebugLocalOrbit
			|| Candidate.RouteId == Route.RouteId
			|| Candidate.Phase != ESRSpaceLogisticsHubRoutePhase::WaitingForFleetCapacity
			|| Candidate.FleetDepartureQueueSequence <= 0)
		{
			continue;
		}

		FSRSpaceLogisticsHubEndpoint CandidateHub;
		if (!TryGetQueuedDepartureHub(Candidate, CandidateHub)
			|| !AreEndpointKeysEqual(CandidateHub, DockHub))
		{
			continue;
		}

		const bool bCandidateIsOlder = Route.FleetDepartureQueueSequence <= 0
			|| Candidate.FleetDepartureQueueSequence < Route.FleetDepartureQueueSequence
			|| (Candidate.FleetDepartureQueueSequence == Route.FleetDepartureQueueSequence
				&& Candidate.RouteId.LexicalLess(Route.RouteId));
		if (bCandidateIsOlder)
		{
			return false;
		}
	}
	return true;
}

void FSRFleetCapacityV2::RefreshQueuePositions(TArray<FSRSpaceLogisticsHubRoute>& Routes)
{
	TMap<FString, TArray<FSRSpaceLogisticsHubRoute*>> QueuesByHub;
	for (FSRSpaceLogisticsHubRoute& Route : Routes)
	{
		Route.FleetQueuePosition = 0;
		if (!Route.bEnabled
			|| Route.Phase != ESRSpaceLogisticsHubRoutePhase::WaitingForFleetCapacity
			|| Route.FleetDepartureQueueSequence <= 0)
		{
			continue;
		}

		FSRSpaceLogisticsHubEndpoint RouteHub;
		if (!TryGetQueuedDepartureHub(Route, RouteHub))
		{
			continue;
		}
		const FString HubKey = FString::Printf(
			TEXT("%p|%s"),
			RouteHub.BodyActor.Get(),
			*RouteHub.HubOccupantId.ToString());
		QueuesByHub.FindOrAdd(HubKey).Add(&Route);
	}

	for (TPair<FString, TArray<FSRSpaceLogisticsHubRoute*>>& QueuePair : QueuesByHub)
	{
		QueuePair.Value.Sort([](
			const FSRSpaceLogisticsHubRoute& Left,
			const FSRSpaceLogisticsHubRoute& Right)
		{
			if (Left.FleetDepartureQueueSequence != Right.FleetDepartureQueueSequence)
			{
				return Left.FleetDepartureQueueSequence < Right.FleetDepartureQueueSequence;
			}
			return Left.RouteId.LexicalLess(Right.RouteId);
		});
		for (int32 QueueIndex = 0; QueueIndex < QueuePair.Value.Num(); ++QueueIndex)
		{
			if (FSRSpaceLogisticsHubRoute* Route = QueuePair.Value[QueueIndex])
			{
				Route->FleetQueuePosition = QueueIndex + 1;
			}
		}
	}
}

bool FSRFleetCapacityV2::AreEndpointKeysEqual(
	const FSRSpaceLogisticsHubEndpoint& Left,
	const FSRSpaceLogisticsHubEndpoint& Right)
{
	return Left.BodyActor == Right.BodyActor && Left.HubOccupantId == Right.HubOccupantId;
}

bool FSRFleetCapacityV2::TryGetReservedDepartureHub(
	const FSRSpaceLogisticsHubRoute& Route,
	FSRSpaceLogisticsHubEndpoint& OutHubEndpoint)
{
	if (Route.Phase == ESRSpaceLogisticsHubRoutePhase::TravelingToDestination
		|| Route.Phase == ESRSpaceLogisticsHubRoutePhase::ConditioningAtDestination)
	{
		OutHubEndpoint = Route.SourceHub;
		return true;
	}
	if (Route.Phase == ESRSpaceLogisticsHubRoutePhase::TravelingToSource
		|| Route.Phase == ESRSpaceLogisticsHubRoutePhase::ConditioningAtSource)
	{
		OutHubEndpoint = Route.DestinationHub;
		return true;
	}
	OutHubEndpoint = FSRSpaceLogisticsHubEndpoint();
	return false;
}

bool FSRFleetCapacityV2::TryGetQueuedDepartureHub(
	const FSRSpaceLogisticsHubRoute& Route,
	FSRSpaceLogisticsHubEndpoint& OutHubEndpoint)
{
	if (Route.Phase != ESRSpaceLogisticsHubRoutePhase::WaitingForFleetCapacity)
	{
		OutHubEndpoint = FSRSpaceLogisticsHubEndpoint();
		return false;
	}
	OutHubEndpoint = Route.CurrentDockSide == ESRSpaceLogisticsHubRouteDockSide::Destination
		? Route.DestinationHub
		: Route.SourceHub;
	return true;
}
