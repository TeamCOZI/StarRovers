#include "SRSpaceLogisticsRouteProcessor.h"

#include "Automation/SRFacilityNetworkComponent.h"
#include "Automation/SRResourceInstanceOperations.h"
#include "Logistics/SRConditionedTransitV2.h"
#include "Logistics/SRFleetCapacityV2.h"
#include "Logistics/SRSpaceLogisticsSubsystem.h"
#include "SRSpaceLogisticsRoutePathResolver.h"
#include "SRSpaceLogisticsRouteVisualController.h"
#include "Simulation/SRAugmentSubsystem.h"
#include "Simulation/SRSimulationSettings.h"
#include "Utility/SRLog.h"

namespace
{
	bool AreHubEndpointKeysEqual(const FSRSpaceLogisticsHubEndpoint& Left, const FSRSpaceLogisticsHubEndpoint& Right)
	{
		return Left.BodyActor == Right.BodyActor && Left.HubOccupantId == Right.HubOccupantId;
	}

	bool HasCargo(const FSRResourceInstance& Cargo)
	{
		return !Cargo.ResourceId.IsNone() && Cargo.StackCount > 0;
	}

	FSRSpaceLogisticsHubEndpoint SelectRouteProcessorHubEndpointByDockSide(const FSRSpaceLogisticsHubRoute& HubRoute, ESRSpaceLogisticsHubRouteDockSide DockSide)
	{
		return DockSide == ESRSpaceLogisticsHubRouteDockSide::Destination ? HubRoute.DestinationHub : HubRoute.SourceHub;
	}

	FString BuildFleetCapacityHubKey(const FSRSpaceLogisticsHubEndpoint& HubEndpoint)
	{
		return FString::Printf(
			TEXT("%p|%s"),
			HubEndpoint.BodyActor.Get(),
			*HubEndpoint.HubOccupantId.ToString());
	}

	FSRFleetCapacityReportV2& FindOrBuildFleetCapacityReport(
		USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		const FSRSpaceLogisticsHubEndpoint& HubEndpoint,
		TMap<FString, FSRFleetCapacityReportV2>& ReportsByHub)
	{
		const FString HubKey = BuildFleetCapacityHubKey(HubEndpoint);
		if (FSRFleetCapacityReportV2* ExistingReport = ReportsByHub.Find(HubKey))
		{
			return *ExistingReport;
		}
		return ReportsByHub.Add(
			HubKey,
			SpaceLogisticsSubsystem.GetHubFleetCapacityReport(HubEndpoint));
	}

	void RemoveQueuedDepartureFromReport(FSRFleetCapacityReportV2& Report)
	{
		Report.QueuedDepartureCount = FMath::Max(0, Report.QueuedDepartureCount - 1);
	}

	void AddDepartureReservationToReport(
		FSRFleetCapacityReportV2& Report,
		const FSRSpaceLogisticsHubRoute& HubRoute)
	{
		Report.ReservedLoad += FSRFleetCapacityV2::ResolveFleetLoad(HubRoute);
		Report.AvailableCapacity = FMath::Max(0, Report.TotalCapacity - Report.ReservedLoad);
	}

	void RecordRouteArrival(
		USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		FSRSpaceLogisticsHubRoute& HubRoute,
		FName SourceBodyId,
		FName DestinationBodyId,
		bool bResourceV2RulesActive)
	{
		if (!HasCargo(HubRoute.Cargo))
		{
			return;
		}
		if (!bResourceV2RulesActive)
		{
			StarRovers::Resources::RecordResourceTransit(
				HubRoute.Cargo,
				SourceBodyId,
				DestinationBodyId);
			return;
		}

		const FSRConditionedTransitModuleRulesV2 ModuleRules =
			FSRConditionedTransitV2::GetModuleRules(HubRoute.ConditionedTransitModule);
		const USRAugmentSubsystem* AugmentSubsystem = SpaceLogisticsSubsystem.GetWorld()
			? SpaceLogisticsSubsystem.GetWorld()->GetSubsystem<USRAugmentSubsystem>()
			: nullptr;
		const bool bModuleUnlocked = HubRoute.ConditionedTransitModule
			== ESRConditionedTransitModuleV2::None
			|| (ModuleRules.IsConditionedModule()
				&& IsValid(AugmentSubsystem)
				&& AugmentSubsystem->IsLogisticsModuleUnlockedV2(ModuleRules.UnlockModuleId));
		const FSRConditionedTransitResultV2 ArrivalResult = FSRConditionedTransitV2::EvaluateArrival(
			HubRoute.Cargo,
			HubRoute.RouteProfile,
			HubRoute.ConditionedTransitModule,
			SourceBodyId,
			DestinationBodyId,
			bModuleUnlocked);
		HubRoute.Cargo = ArrivalResult.OutputResource;
		if (ArrivalResult.bProcessApplied)
		{
			SR_LOG(SpaceLogistics,
				LogTemp,
				Display,
				TEXT("[SpaceLogistics] Conditioned arrival process applied: RouteId=%s Module=%s Resource=%s Energy=%.2f->%.2f Delta=%.2f"),
				*HubRoute.RouteId.ToString(),
				*ModuleRules.DisplayName.ToString(),
				*HubRoute.Cargo.ResourceId.ToString(),
				ArrivalResult.ProcessResult.InputEnergy,
				ArrivalResult.ProcessResult.OutputEnergy,
				ArrivalResult.ProcessResult.AppliedEnergyDelta);
		}
		else if (!ArrivalResult.IsSuccessful())
		{
			SR_LOG(SpaceLogistics,
				LogTemp,
				Warning,
				TEXT("[SpaceLogistics] Conditioned arrival stayed state-neutral: RouteId=%s Module=%s Reason=%s"),
				*HubRoute.RouteId.ToString(),
				*ModuleRules.DisplayName.ToString(),
				*ArrivalResult.FailureReason);
		}
	}
}

void FSRSpaceLogisticsRouteProcessor::ProcessRoutes(
	USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	float DeltaTime,
	TArray<FSRSpaceLogisticsHubRoute>& HubRoutes,
	TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId,
	int64& NextFleetDepartureQueueSequence)
{
	FSRFleetCapacityV2::RefreshQueuePositions(HubRoutes);
	TMap<FString, FSRFleetCapacityReportV2> FleetCapacityReportsByHub;
	if (SpaceLogisticsSubsystem.IsFleetCapacityRulesActive())
	{
		for (const FSRSpaceLogisticsHubRoute& HubRoute : HubRoutes)
		{
			if (!HubRoute.bEnabled || HubRoute.bDebugLocalOrbit)
			{
				continue;
			}
			FindOrBuildFleetCapacityReport(
				SpaceLogisticsSubsystem,
				HubRoute.SourceHub,
				FleetCapacityReportsByHub);
			FindOrBuildFleetCapacityReport(
				SpaceLogisticsSubsystem,
				HubRoute.DestinationHub,
				FleetCapacityReportsByHub);
		}
	}
	for (FSRSpaceLogisticsHubRoute& HubRoute : HubRoutes)
	{
		ProcessRoute(
			SpaceLogisticsSubsystem,
			HubRoute,
			DeltaTime,
			SpaceshipActorsByRouteId,
			HubRoutes,
			FleetCapacityReportsByHub,
			NextFleetDepartureQueueSequence);
	}
	FSRFleetCapacityV2::RefreshQueuePositions(HubRoutes);
}

void FSRSpaceLogisticsRouteProcessor::ProcessRoute(
	USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	FSRSpaceLogisticsHubRoute& HubRoute,
	float DeltaTime,
	TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId,
	TArray<FSRSpaceLogisticsHubRoute>& HubRoutes,
	TMap<FString, FSRFleetCapacityReportV2>& FleetCapacityReportsByHub,
	int64& NextFleetDepartureQueueSequence)
{
	if (!HubRoute.bEnabled)
	{
		ClearFleetQueue(HubRoute);
		return;
	}

	if (!RefreshRouteEndpoints(SpaceLogisticsSubsystem, HubRoute))
	{
		HubRoute.Phase = ESRSpaceLogisticsHubRoutePhase::Blocked;
		FSRConditionedTransitV2::ClearConditioningDwell(HubRoute);
		ClearFleetQueue(HubRoute);
		return;
	}

	switch (HubRoute.Phase)
	{
	case ESRSpaceLogisticsHubRoutePhase::Idle:
		if (HubRoute.bDebugLocalOrbit)
		{
			StartTravel(SpaceLogisticsSubsystem, HubRoute, ESRSpaceLogisticsHubRoutePhase::TravelingToDestination);
			break;
		}
		HubRoute.CurrentDockSide = ESRSpaceLogisticsHubRouteDockSide::Source;
		TryDepartFromDock(
			SpaceLogisticsSubsystem,
			HubRoute,
			ESRSpaceLogisticsHubRouteDockSide::Source,
			HubRoutes,
			FleetCapacityReportsByHub,
			NextFleetDepartureQueueSequence);
		break;
	case ESRSpaceLogisticsHubRoutePhase::WaitingForCargo:
	case ESRSpaceLogisticsHubRoutePhase::WaitingForFleetCapacity:
		if (HubRoute.bDebugLocalOrbit)
		{
			StartTravel(SpaceLogisticsSubsystem, HubRoute, ESRSpaceLogisticsHubRoutePhase::TravelingToDestination);
			break;
		}
		TryDepartFromDock(
			SpaceLogisticsSubsystem,
			HubRoute,
			HubRoute.CurrentDockSide,
			HubRoutes,
			FleetCapacityReportsByHub,
			NextFleetDepartureQueueSequence);
		break;
	case ESRSpaceLogisticsHubRoutePhase::TravelingToDestination:
	case ESRSpaceLogisticsHubRoutePhase::TravelingToSource:
		AdvanceTravel(SpaceLogisticsSubsystem, HubRoute, DeltaTime, SpaceshipActorsByRouteId);
		break;
	case ESRSpaceLogisticsHubRoutePhase::ConditioningAtDestination:
	case ESRSpaceLogisticsHubRoutePhase::ConditioningAtSource:
		AdvanceConditioning(SpaceLogisticsSubsystem, HubRoute, DeltaTime);
		break;
	case ESRSpaceLogisticsHubRoutePhase::UnloadingAtDestination:
		if (TryUnloadAtDock(HubRoute, ESRSpaceLogisticsHubRouteDockSide::Destination))
		{
			TryDepartFromDock(
				SpaceLogisticsSubsystem,
				HubRoute,
				ESRSpaceLogisticsHubRouteDockSide::Destination,
				HubRoutes,
				FleetCapacityReportsByHub,
				NextFleetDepartureQueueSequence);
		}
		break;
	case ESRSpaceLogisticsHubRoutePhase::UnloadingAtSource:
		if (TryUnloadAtDock(HubRoute, ESRSpaceLogisticsHubRouteDockSide::Source))
		{
			TryDepartFromDock(
				SpaceLogisticsSubsystem,
				HubRoute,
				ESRSpaceLogisticsHubRouteDockSide::Source,
				HubRoutes,
				FleetCapacityReportsByHub,
				NextFleetDepartureQueueSequence);
		}
		break;
	case ESRSpaceLogisticsHubRoutePhase::Blocked:
		if (TryUnloadAtDock(HubRoute, HubRoute.CurrentDockSide))
		{
			TryDepartFromDock(
				SpaceLogisticsSubsystem,
				HubRoute,
				HubRoute.CurrentDockSide,
				HubRoutes,
				FleetCapacityReportsByHub,
				NextFleetDepartureQueueSequence);
		}
		break;
	default:
		break;
	}
}

bool FSRSpaceLogisticsRouteProcessor::RefreshRouteEndpoints(
	USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	FSRSpaceLogisticsHubRoute& HubRoute)
{
	FSRSpaceLogisticsHubEndpoint ResolvedSourceHub;
	FSRSpaceLogisticsHubEndpoint ResolvedDestinationHub;
	if (!SpaceLogisticsSubsystem.ResolveCurrentHubEndpoint(HubRoute.SourceHub, ResolvedSourceHub)
		|| !SpaceLogisticsSubsystem.ResolveCurrentHubEndpoint(HubRoute.DestinationHub, ResolvedDestinationHub)
		|| (!HubRoute.bDebugLocalOrbit && AreHubEndpointKeysEqual(ResolvedSourceHub, ResolvedDestinationHub)))
	{
		return false;
	}

	HubRoute.SourceHub = ResolvedSourceHub;
	HubRoute.DestinationHub = HubRoute.bDebugLocalOrbit ? ResolvedSourceHub : ResolvedDestinationHub;
	return true;
}

bool FSRSpaceLogisticsRouteProcessor::TryDepartFromDock(
	USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	FSRSpaceLogisticsHubRoute& HubRoute,
	ESRSpaceLogisticsHubRouteDockSide DockSide,
	TArray<FSRSpaceLogisticsHubRoute>& HubRoutes,
	TMap<FString, FSRFleetCapacityReportV2>& FleetCapacityReportsByHub,
	int64& NextFleetDepartureQueueSequence)
{
	if (HubRoute.bDebugLocalOrbit)
	{
		return StartTravel(SpaceLogisticsSubsystem, HubRoute, ESRSpaceLogisticsHubRoutePhase::TravelingToDestination);
	}

	HubRoute.CurrentDockSide = DockSide;
	const FSRSpaceLogisticsHubEndpoint DockHub = SelectRouteProcessorHubEndpointByDockSide(HubRoute, DockSide);
	const bool bFleetRulesActive = SpaceLogisticsSubsystem.IsFleetCapacityRulesActive();
	FSRFleetCapacityReportV2* CapacityReport = bFleetRulesActive
		? &FindOrBuildFleetCapacityReport(
			SpaceLogisticsSubsystem,
			DockHub,
			FleetCapacityReportsByHub)
		: nullptr;
	const bool bWasQueuedForFleetCapacity =
		HubRoute.Phase == ESRSpaceLogisticsHubRoutePhase::WaitingForFleetCapacity;
	const bool bHasLoadableCargo = HasLoadableCargoAtHub(DockHub, HubRoute, bFleetRulesActive);
	const bool bCanReturnEmpty = DockSide == ESRSpaceLogisticsHubRouteDockSide::Destination
		&& HubRoute.bReturnEmptyWhenNoCargo;
	if (!bHasLoadableCargo && !bCanReturnEmpty)
	{
		HubRoute.Cargo = FSRResourceInstance();
		if (bWasQueuedForFleetCapacity && CapacityReport)
		{
			RemoveQueuedDepartureFromReport(*CapacityReport);
		}
		ClearFleetQueue(HubRoute);
		HubRoute.Phase = ESRSpaceLogisticsHubRoutePhase::WaitingForCargo;
		HubRoute.TravelProgressSeconds = 0.0f;
		HubRoute.TravelProgressRatio = 0.0f;
		HubRoute.bHasTravelStartWorldLocation = false;
		HubRoute.LaunchWorldVelocity = FVector::ZeroVector;
		HubRoute.bHasLaunchWorldVelocity = false;
		return false;
	}

	if (bFleetRulesActive && CapacityReport)
	{
		if (!FSRFleetCapacityV2::CanGrantDeparture(HubRoute, DockHub, HubRoutes, *CapacityReport))
		{
			if (HubRoute.FleetDepartureQueueSequence <= 0)
			{
				HubRoute.FleetDepartureQueueSequence = FMath::Max<int64>(1, NextFleetDepartureQueueSequence++);
			}
			if (!bWasQueuedForFleetCapacity)
			{
				++CapacityReport->QueuedDepartureCount;
			}
			HubRoute.Phase = ESRSpaceLogisticsHubRoutePhase::WaitingForFleetCapacity;
			HubRoute.TravelProgressSeconds = 0.0f;
			HubRoute.TravelProgressRatio = 0.0f;
			HubRoute.bHasTravelStartWorldLocation = false;
			HubRoute.LaunchWorldVelocity = FVector::ZeroVector;
			HubRoute.bHasLaunchWorldVelocity = false;
			return false;
		}
	}

	if (bWasQueuedForFleetCapacity && CapacityReport)
	{
		RemoveQueuedDepartureFromReport(*CapacityReport);
	}
	ClearFleetQueue(HubRoute);
	FSRConditionedTransitV2::ClearConditioningDwell(HubRoute);
	FSRResourceInstance LoadedCargo;
	if (bHasLoadableCargo
		&& TryLoadCargoFromHub(DockHub, HubRoute, bFleetRulesActive, LoadedCargo))
	{
		HubRoute.Cargo = LoadedCargo;
		const bool bStartedTravel = StartTravel(
			SpaceLogisticsSubsystem,
			HubRoute,
			DockSide == ESRSpaceLogisticsHubRouteDockSide::Source
				? ESRSpaceLogisticsHubRoutePhase::TravelingToDestination
				: ESRSpaceLogisticsHubRoutePhase::TravelingToSource);
		if (bStartedTravel && CapacityReport)
		{
			AddDepartureReservationToReport(*CapacityReport, HubRoute);
		}
		return bStartedTravel;
	}

	HubRoute.Cargo = FSRResourceInstance();
	if (bCanReturnEmpty)
	{
		const bool bStartedTravel = StartTravel(
			SpaceLogisticsSubsystem,
			HubRoute,
			ESRSpaceLogisticsHubRoutePhase::TravelingToSource);
		if (bStartedTravel && CapacityReport)
		{
			AddDepartureReservationToReport(*CapacityReport, HubRoute);
		}
		return bStartedTravel;
	}

	HubRoute.Phase = ESRSpaceLogisticsHubRoutePhase::WaitingForCargo;
	HubRoute.TravelProgressSeconds = 0.0f;
	HubRoute.TravelProgressRatio = 0.0f;
	HubRoute.bHasTravelStartWorldLocation = false;
	HubRoute.LaunchWorldVelocity = FVector::ZeroVector;
	HubRoute.bHasLaunchWorldVelocity = false;
	return false;
}

bool FSRSpaceLogisticsRouteProcessor::TryUnloadAtDock(FSRSpaceLogisticsHubRoute& HubRoute, ESRSpaceLogisticsHubRouteDockSide DockSide)
{
	HubRoute.CurrentDockSide = DockSide;
	if (!HasCargo(HubRoute.Cargo))
	{
		HubRoute.Cargo = FSRResourceInstance();
		return true;
	}

	const FSRSpaceLogisticsHubEndpoint DockHub = SelectRouteProcessorHubEndpointByDockSide(HubRoute, DockSide);
	if (!TryUnloadCargoToHub(DockHub, HubRoute.Cargo))
	{
		HubRoute.Phase = ESRSpaceLogisticsHubRoutePhase::Blocked;
		return false;
	}

	HubRoute.Cargo = FSRResourceInstance();
	return true;
}

bool FSRSpaceLogisticsRouteProcessor::StartTravel(
	USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	FSRSpaceLogisticsHubRoute& HubRoute,
	ESRSpaceLogisticsHubRoutePhase TravelPhase)
{
	if (TravelPhase != ESRSpaceLogisticsHubRoutePhase::TravelingToDestination && TravelPhase != ESRSpaceLogisticsHubRoutePhase::TravelingToSource)
	{
		return false;
	}
	ClearFleetQueue(HubRoute);

	HubRoute.Phase = TravelPhase;
	HubRoute.CurrentDockSide = TravelPhase == ESRSpaceLogisticsHubRoutePhase::TravelingToDestination
		? ESRSpaceLogisticsHubRouteDockSide::Source
		: ESRSpaceLogisticsHubRouteDockSide::Destination;
	if (HubRoute.bDebugLocalOrbit)
	{
		HubRoute.CurrentDockSide = ESRSpaceLogisticsHubRouteDockSide::Source;
		HubRoute.DestinationHub = HubRoute.SourceHub;
		HubRoute.Cargo = FSRResourceInstance();
	}
	HubRoute.TravelProgressSeconds = 0.0f;
	HubRoute.TravelProgressRatio = 0.0f;

	const FSRSpaceLogisticsHubEndpoint StartHub = SelectRouteProcessorHubEndpointByDockSide(HubRoute, HubRoute.CurrentDockSide);
	if (!SpaceLogisticsSubsystem.ResolveHubEndpointSurfaceWorldLocation(StartHub, HubRoute.TravelStartWorldLocation))
	{
		HubRoute.bHasTravelStartWorldLocation = false;
		HubRoute.LaunchWorldVelocity = FVector::ZeroVector;
		HubRoute.bHasLaunchWorldVelocity = false;
		HubRoute.Phase = ESRSpaceLogisticsHubRoutePhase::Blocked;
		SR_LOG(SpaceLogistics,
			LogTemp,
			Warning,
			TEXT("[SpaceLogistics] Hub route blocked because start location could not be resolved: RouteId=%s"),
			*HubRoute.RouteId.ToString());
		return false;
	}

	FVector LaunchWorldVelocity = FVector::ZeroVector;
	HubRoute.bHasLaunchWorldVelocity = SpaceLogisticsSubsystem.ResolveHubEndpointWorldVelocity(StartHub, LaunchWorldVelocity);
	HubRoute.LaunchWorldVelocity = HubRoute.bHasLaunchWorldVelocity
		? LaunchWorldVelocity
		: FVector::ZeroVector;
	HubRoute.bHasTravelStartWorldLocation = true;
	HubRoute.TravelDurationSeconds = FSRSpaceLogisticsRoutePathResolver::ResolveTravelDurationSeconds(
		SpaceLogisticsSubsystem,
		HubRoute);
	SR_LOG(SpaceLogistics,
		LogTemp,
		Verbose,
		TEXT("[SpaceLogistics] Hub route travel duration resolved: RouteId=%s InitialSpeed=%.2f LaunchAcceleration=%.2f Duration=%.2f"),
		*HubRoute.RouteId.ToString(),
		HubRoute.InitialSpeedUnitsPerSecond,
		HubRoute.LaunchAccelerationUnitsPerSecondSquared,
		HubRoute.TravelDurationSeconds);
	return true;
}

void FSRSpaceLogisticsRouteProcessor::AdvanceTravel(
	USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	FSRSpaceLogisticsHubRoute& HubRoute,
	float DeltaTime,
	TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId)
{
	const float TravelDurationSeconds = FMath::Max(0.01f, HubRoute.TravelDurationSeconds);
	HubRoute.TravelProgressSeconds = FMath::Max(0.0f, HubRoute.TravelProgressSeconds + FMath::Max(0.0f, DeltaTime));
	HubRoute.TravelProgressRatio = FSRSpaceLogisticsRoutePathResolver::ResolveMotionProgressRatio(
		SpaceLogisticsSubsystem,
		HubRoute,
		HubRoute.TravelProgressSeconds);
	if (HubRoute.TravelProgressRatio < 1.0f)
	{
		return;
	}

	if (HubRoute.bDebugLocalOrbit)
	{
		const float LaunchAscentLength = FSRSpaceLogisticsRoutePathResolver::EstimateRoutePathLengthRange(
			SpaceLogisticsSubsystem,
			HubRoute,
			0.0f,
			FSRSpaceLogisticsRoutePathResolver::GetLaunchAscentEndRatio());
		const float LaunchAscentDuration = FSRSpaceLogisticsRoutePathResolver::ResolveAcceleratedDuration(
			LaunchAscentLength,
			HubRoute.InitialSpeedUnitsPerSecond,
			HubRoute.LaunchAccelerationUnitsPerSecondSquared);
		const float OrbitDuration = FMath::Max(UE_SMALL_NUMBER, TravelDurationSeconds - LaunchAscentDuration);
		HubRoute.TravelProgressSeconds = LaunchAscentDuration
			+ FMath::Fmod(FMath::Max(0.0f, HubRoute.TravelProgressSeconds - LaunchAscentDuration), OrbitDuration);
		HubRoute.TravelProgressRatio = FSRSpaceLogisticsRoutePathResolver::ResolveMotionProgressRatio(
			SpaceLogisticsSubsystem,
			HubRoute,
			HubRoute.TravelProgressSeconds);
		return;
	}

	HubRoute.TravelProgressSeconds = TravelDurationSeconds;
	HubRoute.TravelProgressRatio = 1.0f;
	HubRoute.bHasTravelStartWorldLocation = false;
	HubRoute.LaunchWorldVelocity = FVector::ZeroVector;
	HubRoute.bHasLaunchWorldVelocity = false;
	FSRSpaceLogisticsRouteVisualController::DestroyRouteActor(HubRoute.RouteId, SpaceshipActorsByRouteId);
	const bool bTravelingToDestination =
		HubRoute.Phase == ESRSpaceLogisticsHubRoutePhase::TravelingToDestination;
	const ESRSpaceLogisticsHubRouteDockSide ArrivalDockSide = bTravelingToDestination
		? ESRSpaceLogisticsHubRouteDockSide::Destination
		: ESRSpaceLogisticsHubRouteDockSide::Source;
	const bool bResourceV2RulesActive = SpaceLogisticsSubsystem.IsFleetCapacityRulesActive();
	const USRSimulationSettings* Settings = GetDefault<USRSimulationSettings>();
	if (FSRConditionedTransitV2::TryBeginConditioningDwell(
		HubRoute,
		ArrivalDockSide,
		bResourceV2RulesActive,
		IsValid(Settings) ? Settings->RefinementResistanceEnergyScaleV2 : 40.0))
	{
		const FSRConditionedTransitModuleRulesV2 ModuleRules =
			FSRConditionedTransitV2::GetModuleRules(HubRoute.ConditionedTransitModule);
		SR_LOG(SpaceLogistics,
			LogTemp,
			Display,
			TEXT("[SpaceLogistics] Conditioned arrival dwell started: RouteId=%s Module=%s Duration=%.2f Cargo=%s Energy=%.2f"),
			*HubRoute.RouteId.ToString(),
			*ModuleRules.DisplayName.ToString(),
			HubRoute.ConditioningDurationSeconds,
			*HubRoute.Cargo.ResourceId.ToString(),
			HubRoute.Cargo.CurrentEnergy);
		return;
	}

	CompleteRouteArrival(
		SpaceLogisticsSubsystem,
		HubRoute,
		ArrivalDockSide,
		bResourceV2RulesActive);
}

void FSRSpaceLogisticsRouteProcessor::AdvanceConditioning(
	USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	FSRSpaceLogisticsHubRoute& HubRoute,
	float DeltaTime)
{
	if (!FSRConditionedTransitV2::AdvanceConditioningDwell(HubRoute, DeltaTime))
	{
		return;
	}

	const ESRSpaceLogisticsHubRouteDockSide ArrivalDockSide =
		HubRoute.Phase == ESRSpaceLogisticsHubRoutePhase::ConditioningAtDestination
		? ESRSpaceLogisticsHubRouteDockSide::Destination
		: ESRSpaceLogisticsHubRouteDockSide::Source;
	SR_LOG(SpaceLogistics,
		LogTemp,
		Display,
		TEXT("[SpaceLogistics] Conditioned arrival dwell completed: RouteId=%s Duration=%.2f"),
		*HubRoute.RouteId.ToString(),
		HubRoute.ConditioningDurationSeconds);
	CompleteRouteArrival(
		SpaceLogisticsSubsystem,
		HubRoute,
		ArrivalDockSide,
		SpaceLogisticsSubsystem.IsFleetCapacityRulesActive());
}

void FSRSpaceLogisticsRouteProcessor::CompleteRouteArrival(
	USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	FSRSpaceLogisticsHubRoute& HubRoute,
	ESRSpaceLogisticsHubRouteDockSide ArrivalDockSide,
	bool bResourceV2RulesActive)
{
	const bool bArrivedAtDestination =
		ArrivalDockSide == ESRSpaceLogisticsHubRouteDockSide::Destination;
	RecordRouteArrival(
		SpaceLogisticsSubsystem,
		HubRoute,
		StarRovers::Resources::ResolveCelestialBodyResourceId(
			bArrivedAtDestination
				? HubRoute.SourceHub.BodyActor.Get()
				: HubRoute.DestinationHub.BodyActor.Get()),
		StarRovers::Resources::ResolveCelestialBodyResourceId(
			bArrivedAtDestination
				? HubRoute.DestinationHub.BodyActor.Get()
				: HubRoute.SourceHub.BodyActor.Get()),
		bResourceV2RulesActive);
	FSRConditionedTransitV2::ClearConditioningDwell(HubRoute);
	HubRoute.CurrentDockSide = ArrivalDockSide;
	HubRoute.Phase = bArrivedAtDestination
		? ESRSpaceLogisticsHubRoutePhase::UnloadingAtDestination
		: ESRSpaceLogisticsHubRoutePhase::UnloadingAtSource;
}

bool FSRSpaceLogisticsRouteProcessor::TryLoadCargoFromHub(
	const FSRSpaceLogisticsHubEndpoint& HubEndpoint,
	const FSRSpaceLogisticsHubRoute& HubRoute,
	bool bFleetRulesActive,
	FSRResourceInstance& OutCargo)
{
	OutCargo = FSRResourceInstance();
	AActor* BodyActor = HubEndpoint.BodyActor.Get();
	USRFacilityNetworkComponent* FacilityNetwork = IsValid(BodyActor)
		? BodyActor->FindComponentByClass<USRFacilityNetworkComponent>()
		: nullptr;
	if (!IsValid(FacilityNetwork))
	{
		return false;
	}

	const int32 MaxStackCount = bFleetRulesActive
		? FSRFleetCapacityV2::ResolveEffectiveCargoCapacity(HubRoute)
		: FMath::Max(1, HubRoute.MaxCargoStackCount);
	return FacilityNetwork->TryTakeHubOutboundCargoMatching(
		HubEndpoint.HubOccupantId,
		MaxStackCount,
		[&HubRoute, bFleetRulesActive](const FSRResourceInstance& Cargo)
		{
			return IsCargoEligibleForRoute(Cargo, HubRoute, bFleetRulesActive);
		},
		OutCargo);
}

bool FSRSpaceLogisticsRouteProcessor::HasLoadableCargoAtHub(
	const FSRSpaceLogisticsHubEndpoint& HubEndpoint,
	const FSRSpaceLogisticsHubRoute& HubRoute,
	bool bFleetRulesActive)
{
	AActor* BodyActor = HubEndpoint.BodyActor.Get();
	USRFacilityNetworkComponent* FacilityNetwork = IsValid(BodyActor)
		? BodyActor->FindComponentByClass<USRFacilityNetworkComponent>()
		: nullptr;
	FSRFacilityInstance HubFacility;
	if (!IsValid(FacilityNetwork)
		|| !FacilityNetwork->GetFacilityInstance(HubEndpoint.HubOccupantId, HubFacility))
	{
		return false;
	}

	for (const FSRFacilityPortInventory& Port : HubFacility.InputPortInventories)
	{
		for (const FSRResourceInstance& Cargo : Port.Inventory)
		{
			if (IsCargoEligibleForRoute(Cargo, HubRoute, bFleetRulesActive))
			{
				return true;
			}
		}
	}
	return false;
}

bool FSRSpaceLogisticsRouteProcessor::IsCargoEligibleForRoute(
	const FSRResourceInstance& Cargo,
	const FSRSpaceLogisticsHubRoute& HubRoute,
	bool bFleetRulesActive)
{
	return !Cargo.ResourceId.IsNone()
		&& Cargo.StackCount > 0
		&& (HubRoute.CargoResourceId.IsNone() || Cargo.ResourceId == HubRoute.CargoResourceId)
		&& (!bFleetRulesActive
			|| (FSRFleetCapacityV2::IsCargoEligible(HubRoute.RouteProfile, Cargo)
				&& FSRConditionedTransitV2::IsCargoCompatible(
					HubRoute.ConditionedTransitModule,
					Cargo)));
}

void FSRSpaceLogisticsRouteProcessor::ClearFleetQueue(FSRSpaceLogisticsHubRoute& HubRoute)
{
	HubRoute.FleetDepartureQueueSequence = 0;
	HubRoute.FleetQueuePosition = 0;
}

bool FSRSpaceLogisticsRouteProcessor::TryUnloadCargoToHub(
	const FSRSpaceLogisticsHubEndpoint& HubEndpoint,
	const FSRResourceInstance& Cargo)
{
	AActor* BodyActor = HubEndpoint.BodyActor.Get();
	USRFacilityNetworkComponent* FacilityNetwork = IsValid(BodyActor)
		? BodyActor->FindComponentByClass<USRFacilityNetworkComponent>()
		: nullptr;
	return IsValid(FacilityNetwork)
		&& FacilityNetwork->TryStoreHubInboundCargo(HubEndpoint.HubOccupantId, Cargo);
}
