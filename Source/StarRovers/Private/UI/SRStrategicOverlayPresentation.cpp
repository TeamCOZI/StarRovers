#include "UI/SRStrategicOverlayPresentation.h"

#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Engine/World.h"
#include "Logistics/SRSpaceLogisticsSubsystem.h"

namespace
{
	FText ResolveBodyName(const AActor* BodyActor)
	{
		if (!IsValid(BodyActor))
		{
			return NSLOCTEXT("StarRoversStrategy", "UnknownBody", "Unknown Body");
		}
		const FText VariableName = USRCelestialBodyRuntimeLibrary::GetCelestialVariableName(BodyActor);
		return VariableName.IsEmpty()
			? FText::FromString(BodyActor->GetName())
			: VariableName;
	}

	FString PluralSuffix(int32 Count)
	{
		return Count == 1 ? FString() : FString(TEXT("s"));
	}

	FSRStrategicRoutePresentation BuildRoutePresentation(const FSRStrategicRouteSnapshot& Snapshot)
	{
		FSRStrategicRoutePresentation Result;
		Result.RouteId = Snapshot.RouteId;
		Result.SourceBodyKey = Snapshot.SourceBodyKey;
		Result.DestinationBodyKey = Snapshot.DestinationBodyKey;
		Result.SourceBodyActor = Snapshot.SourceBodyActor;
		Result.DestinationBodyActor = Snapshot.DestinationBodyActor;
		Result.SourceBodyName = Snapshot.SourceBodyName;
		Result.DestinationBodyName = Snapshot.DestinationBodyName;
		Result.FleetQueuePosition = FMath::Max(0, Snapshot.FleetQueuePosition);
		Result.bEnabled = Snapshot.bEnabled;

		if (!Snapshot.bEnabled)
		{
			Result.Condition = ESRStrategicRouteCondition::Disabled;
			Result.VisualState = ESRUIVisualState::Disabled;
			Result.StatusLabel = NSLOCTEXT("StarRoversStrategy", "RouteDisabled", "OFF");
		}
		else
		{
			switch (Snapshot.Phase)
			{
			case ESRSpaceLogisticsHubRoutePhase::Blocked:
				Result.Condition = ESRStrategicRouteCondition::Blocked;
				Result.VisualState = ESRUIVisualState::Danger;
				Result.StatusLabel = NSLOCTEXT("StarRoversStrategy", "RouteBlocked", "BLOCKED");
				break;
			case ESRSpaceLogisticsHubRoutePhase::WaitingForFleetCapacity:
				Result.Condition = ESRStrategicRouteCondition::FleetQueue;
				Result.VisualState = ESRUIVisualState::Warning;
				Result.StatusLabel = FText::Format(
					NSLOCTEXT("StarRoversStrategy", "RouteQueue", "QUEUE #{0}"),
					FText::AsNumber(FMath::Max(1, Snapshot.FleetQueuePosition)));
				break;
			case ESRSpaceLogisticsHubRoutePhase::TravelingToDestination:
			case ESRSpaceLogisticsHubRoutePhase::TravelingToSource:
			case ESRSpaceLogisticsHubRoutePhase::UnloadingAtDestination:
			case ESRSpaceLogisticsHubRoutePhase::UnloadingAtSource:
				Result.Condition = ESRStrategicRouteCondition::Moving;
				Result.VisualState = ESRUIVisualState::Positive;
				Result.StatusLabel = NSLOCTEXT("StarRoversStrategy", "RouteMoving", "MOVING");
				break;
			case ESRSpaceLogisticsHubRoutePhase::ConditioningAtDestination:
			case ESRSpaceLogisticsHubRoutePhase::ConditioningAtSource:
				Result.Condition = ESRStrategicRouteCondition::Conditioning;
				Result.VisualState = ESRUIVisualState::Info;
				Result.StatusLabel = NSLOCTEXT("StarRoversStrategy", "RouteConditioning", "CONDITIONING");
				break;
			case ESRSpaceLogisticsHubRoutePhase::Idle:
			case ESRSpaceLogisticsHubRoutePhase::WaitingForCargo:
			default:
				Result.Condition = ESRStrategicRouteCondition::Ready;
				Result.VisualState = ESRUIVisualState::Neutral;
				Result.StatusLabel = NSLOCTEXT("StarRoversStrategy", "RouteReady", "READY");
				break;
			}
		}

		Result.ToolTipText = FText::Format(
			NSLOCTEXT("StarRoversStrategy", "RouteTooltip", "{0} > {1}\nRoute {2} | {3}"),
			Snapshot.SourceBodyName,
			Snapshot.DestinationBodyName,
			FText::FromName(Snapshot.RouteId),
			Result.StatusLabel);
		return Result;
	}

	void ResolveBodyIssue(FSRStrategicBodyPresentation& Body)
	{
		const FSRCelestialBodyOperationsSummary& Summary = Body.Operations;
		const int32 Demand = Summary.OperationalCapacity.TotalDemand;
		const int32 Capacity = Summary.OperationalCapacity.TotalCapacity;
		const int32 Excess = FMath::Max(0, Demand - Capacity);
		const ESRCelestialBodyOperationsPressure Pressure =
			FSRCelestialBodyOperationsSummaryBuilder::ResolveOperationalPressure(Summary);

		if (Body.BlockedRouteCount > 0)
		{
			Body.BottleneckKind = ESRStrategicBottleneckKind::RouteBlocked;
			Body.VisualState = ESRUIVisualState::Danger;
			Body.StatusLabel = NSLOCTEXT("StarRoversStrategy", "BodyRouteBlocked", "ROUTE BLOCKED");
			Body.ShortBadgeText = FText::FromString(FString::Printf(
				TEXT("! ROUTE %d"),
				Body.BlockedRouteCount));
			Body.IssueDetailText = FText::FromString(FString::Printf(
				TEXT("%d connected Route%s cannot progress. Open a Hub on %s and inspect the blocked lane."),
				Body.BlockedRouteCount,
				*PluralSuffix(Body.BlockedRouteCount),
				*Body.BodyName.ToString()));
			Body.Priority = 100000
				+ Body.OutboundBlockedRouteCount * 1000
				+ Body.BlockedRouteCount * 10;
		}
		else if (Pressure == ESRCelestialBodyOperationsPressure::OverCapacity)
		{
			Body.BottleneckKind = ESRStrategicBottleneckKind::OperationalOverload;
			Body.VisualState = ESRUIVisualState::Danger;
			Body.StatusLabel = NSLOCTEXT("StarRoversStrategy", "BodyOverCapacity", "OVER CAPACITY");
			Body.ShortBadgeText = FText::FromString(FString::Printf(TEXT("! LOAD +%d"), Excess));
			Body.IssueDetailText = FText::FromString(FString::Printf(
				TEXT("Active Load %d exceeds Capacity %d by %d; %d Facilit%s throttled."),
				Demand,
				Capacity,
				Excess,
				Summary.ThrottledFacilityCount,
				Summary.ThrottledFacilityCount == 1 ? TEXT("y is") : TEXT("ies are")));
			Body.Priority = 90000 + Excess * 100 + Summary.ThrottledFacilityCount;
		}
		else if (Summary.ThrottledFacilityCount > 0)
		{
			Body.BottleneckKind = ESRStrategicBottleneckKind::OperationalThrottled;
			Body.VisualState = ESRUIVisualState::Danger;
			Body.StatusLabel = NSLOCTEXT("StarRoversStrategy", "BodyThrottled", "FACILITIES THROTTLED");
			Body.ShortBadgeText = FText::FromString(FString::Printf(
				TEXT("! SLOW %d"),
				Summary.ThrottledFacilityCount));
			Body.IssueDetailText = FText::FromString(FString::Printf(
				TEXT("%d active Facilit%s running below full speed. Inspect Capacity and operational priorities."),
				Summary.ThrottledFacilityCount,
				Summary.ThrottledFacilityCount == 1 ? TEXT("y is") : TEXT("ies are")));
			Body.Priority = 85000 + Summary.ThrottledFacilityCount * 100;
		}
		else if (FMath::Max(Body.QueuedRouteCount, Summary.FleetQueuedDepartureCount) > 0)
		{
			const int32 QueueCount = FMath::Max(Body.QueuedRouteCount, Summary.FleetQueuedDepartureCount);
			Body.BottleneckKind = ESRStrategicBottleneckKind::FleetQueue;
			Body.VisualState = ESRUIVisualState::Warning;
			Body.StatusLabel = NSLOCTEXT("StarRoversStrategy", "BodyFleetQueue", "FLEET QUEUE");
			Body.ShortBadgeText = FText::FromString(FString::Printf(TEXT("Q %d"), QueueCount));
			Body.IssueDetailText = FText::FromString(FString::Printf(
				TEXT("%d departure%s waiting for Fleet Capacity. Inspect this body's busiest Hub or supply a Fleet Berth."),
				QueueCount,
				*PluralSuffix(QueueCount)));
			Body.Priority = 80000 + QueueCount * 100;
		}
		else if (Summary.ConnectedRouteCount > 0
			&& Summary.BusiestHubTotalCapacity > 0
			&& Summary.BusiestHubReservedLoad >= Summary.BusiestHubTotalCapacity)
		{
			Body.BottleneckKind = ESRStrategicBottleneckKind::FleetAtCapacity;
			Body.VisualState = ESRUIVisualState::Warning;
			Body.StatusLabel = NSLOCTEXT("StarRoversStrategy", "BodyFleetFull", "FLEET FULL");
			Body.ShortBadgeText = FText::FromString(FString::Printf(
				TEXT("F %d/%d"),
				Summary.BusiestHubReservedLoad,
				Summary.BusiestHubTotalCapacity));
			Body.IssueDetailText = NSLOCTEXT(
				"StarRoversStrategy",
				"BodyFleetFullDetail",
				"The busiest Hub has no Fleet headroom. The next departure may queue until a vessel returns.");
			Body.Priority = 70000 + Summary.BusiestHubReservedLoad;
		}
		else if (Pressure == ESRCelestialBodyOperationsPressure::AtCapacity)
		{
			Body.BottleneckKind = ESRStrategicBottleneckKind::OperationalAtCapacity;
			Body.VisualState = ESRUIVisualState::Warning;
			Body.StatusLabel = NSLOCTEXT("StarRoversStrategy", "BodyCapacityFull", "CAPACITY FULL");
			Body.ShortBadgeText = NSLOCTEXT("StarRoversStrategy", "BodyCapacityFullBadge", "CAP FULL");
			Body.IssueDetailText = NSLOCTEXT(
				"StarRoversStrategy",
				"BodyCapacityFullDetail",
				"Operational Capacity has no headroom. New simultaneous work will be throttled.");
			Body.Priority = 60000 + Demand;
		}
		else if (Pressure == ESRCelestialBodyOperationsPressure::NearCapacity)
		{
			const int32 UtilizationPercent = Capacity > 0
				? FMath::RoundToInt(static_cast<float>(Demand) / static_cast<float>(Capacity) * 100.0f)
				: 100;
			Body.BottleneckKind = ESRStrategicBottleneckKind::OperationalNearCapacity;
			Body.VisualState = ESRUIVisualState::Warning;
			Body.StatusLabel = NSLOCTEXT("StarRoversStrategy", "BodyCapacityRisk", "CAPACITY RISK");
			Body.ShortBadgeText = FText::FromString(FString::Printf(TEXT("CAP %d%%"), UtilizationPercent));
			Body.IssueDetailText = FText::FromString(FString::Printf(
				TEXT("Operational Load is %d%%. Reserve Capacity before extending this Line."),
				UtilizationPercent));
			Body.Priority = 50000 + UtilizationPercent;
		}
		else
		{
			Body.BottleneckKind = ESRStrategicBottleneckKind::None;
			Body.VisualState = ESRUIVisualState::Neutral;
			Body.StatusLabel = NSLOCTEXT("StarRoversStrategy", "BodyNominal", "NOMINAL");
			Body.Priority = 0;
		}

		Body.bHasBottleneck = Body.BottleneckKind != ESRStrategicBottleneckKind::None;
		TArray<FString> ToolTipSections;
		if (!Body.IssueDetailText.IsEmpty())
		{
			ToolTipSections.Add(Body.IssueDetailText.ToString());
		}
		ToolTipSections.Add(FSRCelestialBodyOperationsSummaryBuilder::BuildOperationalToolTipText(Summary));
		if (Summary.HubCount > 0)
		{
			ToolTipSections.Add(FString::Printf(
				TEXT("Logistics: %d Hub%s, %d Route%s, %d blocked, Fleet %d/%d, queue %d."),
				Summary.HubCount,
				*PluralSuffix(Summary.HubCount),
				Summary.ConnectedRouteCount,
				*PluralSuffix(Summary.ConnectedRouteCount),
				Body.BlockedRouteCount,
				Summary.FleetReservedLoad,
				Summary.FleetTotalCapacity,
				FMath::Max(Body.QueuedRouteCount, Summary.FleetQueuedDepartureCount)));
		}
		Body.ToolTipText = FText::FromString(FString::Join(ToolTipSections, TEXT("\n\n")));
	}

	bool IsHigherPriorityBody(
		const FSRStrategicBodyPresentation& Candidate,
		const FSRStrategicBodyPresentation& Current)
	{
		if (Candidate.Priority != Current.Priority)
		{
			return Candidate.Priority > Current.Priority;
		}
		const FString CandidateName = Candidate.BodyName.ToString();
		const FString CurrentName = Current.BodyName.ToString();
		if (CandidateName != CurrentName)
		{
			return CandidateName < CurrentName;
		}
		return Candidate.BodyKey.LexicalLess(Current.BodyKey);
	}
}

const FSRStrategicBodyPresentation* FSRStrategicOverlayPresentation::FindBody(FName BodyKey) const
{
	return Bodies.FindByPredicate([BodyKey](const FSRStrategicBodyPresentation& Body)
	{
		return Body.BodyKey == BodyKey;
	});
}

const FSRStrategicBodyPresentation* FSRStrategicOverlayPresentation::FindBody(
	const AActor* BodyActor) const
{
	return Bodies.FindByPredicate([BodyActor](const FSRStrategicBodyPresentation& Body)
	{
		return Body.BodyActor.Get() == BodyActor;
	});
}

const FSRStrategicRoutePresentation* FSRStrategicOverlayPresentation::FindRoute(FName RouteId) const
{
	return Routes.FindByPredicate([RouteId](const FSRStrategicRoutePresentation& Route)
	{
		return Route.RouteId == RouteId;
	});
}

FSRStrategicOverlayPresentation FSRStrategicOverlayPresentationBuilder::Build(
	const FSRStrategicOverlayInput& Input)
{
	FSRStrategicOverlayPresentation Result;
	Result.Bodies.Reserve(Input.Bodies.Num());
	for (const FSRStrategicBodySnapshot& Snapshot : Input.Bodies)
	{
		FSRStrategicBodyPresentation& Body = Result.Bodies.AddDefaulted_GetRef();
		Body.BodyKey = Snapshot.BodyKey;
		Body.BodyActor = Snapshot.BodyActor;
		Body.BodyName = Snapshot.BodyName;
		Body.Operations = Snapshot.Operations;
	}

	TMap<FName, int32> BodyIndexByKey;
	for (int32 BodyIndex = 0; BodyIndex < Result.Bodies.Num(); ++BodyIndex)
	{
		if (!Result.Bodies[BodyIndex].BodyKey.IsNone())
		{
			BodyIndexByKey.Add(Result.Bodies[BodyIndex].BodyKey, BodyIndex);
		}
	}

	Result.Routes.Reserve(Input.Routes.Num());
	for (const FSRStrategicRouteSnapshot& RouteSnapshot : Input.Routes)
	{
		FSRStrategicRoutePresentation& Route = Result.Routes.Add_GetRef(
			BuildRoutePresentation(RouteSnapshot));
		if (Route.Condition == ESRStrategicRouteCondition::Blocked)
		{
			++Result.BlockedRouteCount;
			if (const int32* SourceIndex = BodyIndexByKey.Find(Route.SourceBodyKey))
			{
				++Result.Bodies[*SourceIndex].BlockedRouteCount;
				++Result.Bodies[*SourceIndex].OutboundBlockedRouteCount;
			}
			if (Route.DestinationBodyKey != Route.SourceBodyKey)
			{
				if (const int32* DestinationIndex = BodyIndexByKey.Find(Route.DestinationBodyKey))
				{
					++Result.Bodies[*DestinationIndex].BlockedRouteCount;
				}
			}
		}
		else if (Route.Condition == ESRStrategicRouteCondition::FleetQueue)
		{
			++Result.QueuedRouteCount;
			const FName DockBodyKey = RouteSnapshot.CurrentDockSide
				== ESRSpaceLogisticsHubRouteDockSide::Destination
				? Route.DestinationBodyKey
				: Route.SourceBodyKey;
			if (const int32* DockIndex = BodyIndexByKey.Find(DockBodyKey))
			{
				++Result.Bodies[*DockIndex].QueuedRouteCount;
			}
		}
	}

	const FSRStrategicBodyPresentation* RecommendedBody = nullptr;
	for (FSRStrategicBodyPresentation& Body : Result.Bodies)
	{
		ResolveBodyIssue(Body);
		if (Body.VisualState == ESRUIVisualState::Danger)
		{
			++Result.CriticalBodyCount;
		}
		else if (Body.VisualState == ESRUIVisualState::Warning)
		{
			++Result.WarningBodyCount;
		}
		if (Body.bHasBottleneck
			&& (!RecommendedBody || IsHigherPriorityBody(Body, *RecommendedBody)))
		{
			RecommendedBody = &Body;
		}
	}

	if (RecommendedBody)
	{
		Result.bHasRecommendation = true;
		Result.RecommendedBodyKey = RecommendedBody->BodyKey;
		Result.RecommendedBodyActor = RecommendedBody->BodyActor;
		Result.SummaryState = RecommendedBody->VisualState;
		Result.SummaryLabel = FText::Format(
			NSLOCTEXT("StarRoversStrategy", "BottleneckSummary", "BOTTLENECK | {0} | {1}"),
			RecommendedBody->BodyName,
			RecommendedBody->StatusLabel);
		Result.SummaryDetailText = FText::Format(
			NSLOCTEXT("StarRoversStrategy", "BottleneckDetailSummary", "{0}\n{1} critical | {2} watch"),
			RecommendedBody->IssueDetailText,
			FText::AsNumber(Result.CriticalBodyCount),
			FText::AsNumber(Result.WarningBodyCount));
		Result.FocusActionText = FText::Format(
			NSLOCTEXT("StarRoversStrategy", "FocusBodyAction", "FOCUS {0}"),
			RecommendedBody->BodyName);
	}
	else
	{
		Result.SummaryState = ESRUIVisualState::Positive;
		Result.SummaryLabel = NSLOCTEXT("StarRoversStrategy", "NominalSummary", "NETWORK NOMINAL");
		Result.SummaryDetailText = FText::Format(
			NSLOCTEXT("StarRoversStrategy", "NominalDetail", "{0} bodies | {1} Routes | no active hard stop"),
			FText::AsNumber(Result.Bodies.Num()),
			FText::AsNumber(Result.Routes.Num()));
	}
	return Result;
}

FSRStrategicOverlayPresentation FSRStrategicOverlayPresentationBuilder::BuildFromWorld(
	UWorld* World,
	const TArray<AActor*>& CelestialBodies)
{
	FSRStrategicOverlayInput Input;
	Input.Bodies.Reserve(CelestialBodies.Num());
	for (AActor* BodyActor : CelestialBodies)
	{
		if (!IsValid(BodyActor))
		{
			continue;
		}
		FSRStrategicBodySnapshot& Body = Input.Bodies.AddDefaulted_GetRef();
		Body.BodyKey = BodyActor->GetFName();
		Body.BodyActor = BodyActor;
		Body.BodyName = ResolveBodyName(BodyActor);
		FSRCelestialBodyOperationsSummaryBuilder::BuildSummary(BodyActor, Body.Operations);
	}

	const USRSpaceLogisticsSubsystem* Logistics = IsValid(World)
		? World->GetSubsystem<USRSpaceLogisticsSubsystem>()
		: nullptr;
	if (IsValid(Logistics))
	{
		TArray<FSRSpaceLogisticsHubRoute> Routes;
		Logistics->GetHubRoutes(Routes);
		for (const FSRSpaceLogisticsHubRoute& Route : Routes)
		{
			if (!Route.IsValid() || Route.bDebugLocalOrbit)
			{
				continue;
			}
			FSRStrategicRouteSnapshot& Snapshot = Input.Routes.AddDefaulted_GetRef();
			Snapshot.RouteId = Route.RouteId;
			Snapshot.SourceBodyActor = Route.SourceHub.BodyActor.Get();
			Snapshot.DestinationBodyActor = Route.DestinationHub.BodyActor.Get();
			Snapshot.SourceBodyKey = IsValid(Route.SourceHub.BodyActor.Get())
				? Route.SourceHub.BodyActor->GetFName()
				: NAME_None;
			Snapshot.DestinationBodyKey = IsValid(Route.DestinationHub.BodyActor.Get())
				? Route.DestinationHub.BodyActor->GetFName()
				: NAME_None;
			Snapshot.SourceBodyName = ResolveBodyName(Route.SourceHub.BodyActor.Get());
			Snapshot.DestinationBodyName = ResolveBodyName(Route.DestinationHub.BodyActor.Get());
			Snapshot.bEnabled = Route.bEnabled;
			Snapshot.Phase = Route.Phase;
			Snapshot.CurrentDockSide = Route.CurrentDockSide;
			Snapshot.FleetQueuePosition = Route.FleetQueuePosition;
		}
	}
	return Build(Input);
}
