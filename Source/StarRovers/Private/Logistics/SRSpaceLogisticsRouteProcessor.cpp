#include "SRSpaceLogisticsRouteProcessor.h"

#include "Automation/SRFacilityNetworkComponent.h"
#include "Logistics/SRSpaceLogisticsSubsystem.h"
#include "SRSpaceLogisticsRoutePathResolver.h"
#include "SRSpaceLogisticsRouteVisualController.h"
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
}

void FSRSpaceLogisticsRouteProcessor::ProcessRoutes(
	USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	float DeltaTime,
	TArray<FSRSpaceLogisticsHubRoute>& HubRoutes,
	TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId)
{
	for (FSRSpaceLogisticsHubRoute& HubRoute : HubRoutes)
	{
		ProcessRoute(SpaceLogisticsSubsystem, HubRoute, DeltaTime, SpaceshipActorsByRouteId);
	}
}

void FSRSpaceLogisticsRouteProcessor::ProcessRoute(
	USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	FSRSpaceLogisticsHubRoute& HubRoute,
	float DeltaTime,
	TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId)
{
	if (!HubRoute.bEnabled)
	{
		return;
	}

	if (!RefreshRouteEndpoints(SpaceLogisticsSubsystem, HubRoute))
	{
		HubRoute.Phase = ESRSpaceLogisticsHubRoutePhase::Blocked;
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
		TryDepartFromDock(SpaceLogisticsSubsystem, HubRoute, ESRSpaceLogisticsHubRouteDockSide::Source);
		break;
	case ESRSpaceLogisticsHubRoutePhase::WaitingForCargo:
		if (HubRoute.bDebugLocalOrbit)
		{
			StartTravel(SpaceLogisticsSubsystem, HubRoute, ESRSpaceLogisticsHubRoutePhase::TravelingToDestination);
			break;
		}
		TryDepartFromDock(SpaceLogisticsSubsystem, HubRoute, HubRoute.CurrentDockSide);
		break;
	case ESRSpaceLogisticsHubRoutePhase::TravelingToDestination:
	case ESRSpaceLogisticsHubRoutePhase::TravelingToSource:
		AdvanceTravel(SpaceLogisticsSubsystem, HubRoute, DeltaTime, SpaceshipActorsByRouteId);
		break;
	case ESRSpaceLogisticsHubRoutePhase::UnloadingAtDestination:
		if (TryUnloadAtDock(HubRoute, ESRSpaceLogisticsHubRouteDockSide::Destination))
		{
			TryDepartFromDock(SpaceLogisticsSubsystem, HubRoute, ESRSpaceLogisticsHubRouteDockSide::Destination);
		}
		break;
	case ESRSpaceLogisticsHubRoutePhase::UnloadingAtSource:
		if (TryUnloadAtDock(HubRoute, ESRSpaceLogisticsHubRouteDockSide::Source))
		{
			TryDepartFromDock(SpaceLogisticsSubsystem, HubRoute, ESRSpaceLogisticsHubRouteDockSide::Source);
		}
		break;
	case ESRSpaceLogisticsHubRoutePhase::Blocked:
		if (TryUnloadAtDock(HubRoute, HubRoute.CurrentDockSide))
		{
			TryDepartFromDock(SpaceLogisticsSubsystem, HubRoute, HubRoute.CurrentDockSide);
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
	ESRSpaceLogisticsHubRouteDockSide DockSide)
{
	if (HubRoute.bDebugLocalOrbit)
	{
		return StartTravel(SpaceLogisticsSubsystem, HubRoute, ESRSpaceLogisticsHubRoutePhase::TravelingToDestination);
	}

	HubRoute.CurrentDockSide = DockSide;
	FSRResourceInstance LoadedCargo;
	const FSRSpaceLogisticsHubEndpoint DockHub = SelectRouteProcessorHubEndpointByDockSide(HubRoute, DockSide);
	if (TryLoadCargoFromHub(DockHub, HubRoute.MaxCargoStackCount, HubRoute.CargoResourceId, LoadedCargo))
	{
		HubRoute.Cargo = LoadedCargo;
		return StartTravel(
			SpaceLogisticsSubsystem,
			HubRoute,
			DockSide == ESRSpaceLogisticsHubRouteDockSide::Source
				? ESRSpaceLogisticsHubRoutePhase::TravelingToDestination
				: ESRSpaceLogisticsHubRoutePhase::TravelingToSource);
	}

	HubRoute.Cargo = FSRResourceInstance();
	if (DockSide == ESRSpaceLogisticsHubRouteDockSide::Destination && HubRoute.bReturnEmptyWhenNoCargo)
	{
		return StartTravel(SpaceLogisticsSubsystem, HubRoute, ESRSpaceLogisticsHubRoutePhase::TravelingToSource);
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
	if (HubRoute.Phase == ESRSpaceLogisticsHubRoutePhase::TravelingToDestination)
	{
		HubRoute.CurrentDockSide = ESRSpaceLogisticsHubRouteDockSide::Destination;
		HubRoute.Phase = ESRSpaceLogisticsHubRoutePhase::UnloadingAtDestination;
	}
	else if (HubRoute.Phase == ESRSpaceLogisticsHubRoutePhase::TravelingToSource)
	{
		HubRoute.CurrentDockSide = ESRSpaceLogisticsHubRouteDockSide::Source;
		HubRoute.Phase = ESRSpaceLogisticsHubRoutePhase::UnloadingAtSource;
	}
}

bool FSRSpaceLogisticsRouteProcessor::TryLoadCargoFromHub(
	const FSRSpaceLogisticsHubEndpoint& HubEndpoint,
	int32 MaxStackCount,
	FName CargoResourceId,
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

	return CargoResourceId.IsNone()
		? FacilityNetwork->TryTakeHubOutboundCargo(HubEndpoint.HubOccupantId, MaxStackCount, OutCargo)
		: FacilityNetwork->TryTakeHubOutboundCargoByResource(HubEndpoint.HubOccupantId, CargoResourceId, MaxStackCount, OutCargo);
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
