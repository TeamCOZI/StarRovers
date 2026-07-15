#include "SRSpaceLogisticsRouteRegistry.h"

#include "Logistics/SRSpaceLogisticsSubsystem.h"
#include "SRSpaceLogisticsRoutePathResolver.h"
#include "SRSpaceLogisticsRouteVisualController.h"
#include "Utility/SRLog.h"

bool FSRSpaceLogisticsRouteRegistry::AreHubEndpointKeysEqual(
	const FSRSpaceLogisticsHubEndpoint& Left,
	const FSRSpaceLogisticsHubEndpoint& Right)
{
	return Left.BodyActor == Right.BodyActor && Left.HubOccupantId == Right.HubOccupantId;
}

bool FSRSpaceLogisticsRouteRegistry::DoesRouteAlreadyExist(
	const TArray<FSRSpaceLogisticsHubRoute>& HubRoutes,
	const FSRSpaceLogisticsHubEndpoint& SourceHub,
	const FSRSpaceLogisticsHubEndpoint& DestinationHub)
{
	for (const FSRSpaceLogisticsHubRoute& HubRoute : HubRoutes)
	{
		if (AreHubEndpointPairsEquivalent(HubRoute.SourceHub, HubRoute.DestinationHub, SourceHub, DestinationHub))
		{
			return true;
		}
	}

	return false;
}

bool FSRSpaceLogisticsRouteRegistry::CreateHubRoute(
	USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	const FSRSpaceLogisticsHubEndpoint& SourceHub,
	const FSRSpaceLogisticsHubEndpoint& DestinationHub,
	FName& OutRouteId,
	bool bReturnEmptyWhenNoCargo,
	int32 MaxCargoStackCount,
	float InitialSpeedUnitsPerSecond,
	float LaunchAccelerationUnitsPerSecondSquared,
	TArray<FSRSpaceLogisticsHubRoute>& HubRoutes,
	int32& NextHubRouteSequence)
{
	OutRouteId = NAME_None;

	FSRSpaceLogisticsHubEndpoint ResolvedSourceHub;
	FSRSpaceLogisticsHubEndpoint ResolvedDestinationHub;
	if (!SpaceLogisticsSubsystem.ResolveCurrentHubEndpoint(SourceHub, ResolvedSourceHub)
		|| !SpaceLogisticsSubsystem.ResolveCurrentHubEndpoint(DestinationHub, ResolvedDestinationHub)
		|| AreHubEndpointKeysEqual(ResolvedSourceHub, ResolvedDestinationHub))
	{
		return false;
	}

	if (DoesRouteAlreadyExist(HubRoutes, ResolvedSourceHub, ResolvedDestinationHub))
	{
		return false;
	}

	FSRSpaceLogisticsHubRoute& HubRoute = HubRoutes.AddDefaulted_GetRef();
	HubRoute.RouteId = MakeHubRouteId(ResolvedSourceHub, ResolvedDestinationHub, NextHubRouteSequence);
	HubRoute.SourceHub = ResolvedSourceHub;
	HubRoute.DestinationHub = ResolvedDestinationHub;
	HubRoute.bEnabled = true;
	HubRoute.bReturnEmptyWhenNoCargo = bReturnEmptyWhenNoCargo;
	HubRoute.MaxCargoStackCount = FMath::Max(1, MaxCargoStackCount);
	HubRoute.CargoResourceId = NAME_None;
	HubRoute.Phase = ESRSpaceLogisticsHubRoutePhase::WaitingForCargo;
	HubRoute.CurrentDockSide = ESRSpaceLogisticsHubRouteDockSide::Source;
	ApplyHubRouteFlightSettings(SpaceLogisticsSubsystem, HubRoute, InitialSpeedUnitsPerSecond, LaunchAccelerationUnitsPerSecondSquared);
	HubRoute.TravelDurationSeconds = FSRSpaceLogisticsRoutePathResolver::GetMinimumTravelDurationSeconds();
	HubRoute.TravelProgressSeconds = 0.0f;
	HubRoute.TravelProgressRatio = 0.0f;
	HubRoute.Cargo = FSRResourceInstance();
	OutRouteId = HubRoute.RouteId;

	SR_LOG(SpaceLogistics,
		LogTemp,
		Display,
		TEXT("[SpaceLogistics] Hub route created: RouteId=%s Source=%s/%s Destination=%s/%s MaxCargoStackCount=%d InitialSpeed=%.2f LaunchAcceleration=%.2f ReturnEmpty=%s"),
		*OutRouteId.ToString(),
		*GetNameSafe(ResolvedSourceHub.BodyActor.Get()),
		*ResolvedSourceHub.HubOccupantId.ToString(),
		*GetNameSafe(ResolvedDestinationHub.BodyActor.Get()),
		*ResolvedDestinationHub.HubOccupantId.ToString(),
		HubRoute.MaxCargoStackCount,
		HubRoute.InitialSpeedUnitsPerSecond,
		HubRoute.LaunchAccelerationUnitsPerSecondSquared,
		HubRoute.bReturnEmptyWhenNoCargo ? TEXT("true") : TEXT("false"));
	return true;
}

bool FSRSpaceLogisticsRouteRegistry::CreateDebugLocalOrbitRoute(
	USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	const FSRSpaceLogisticsHubEndpoint& SourceHub,
	FName& OutRouteId,
	float InitialSpeedUnitsPerSecond,
	float LaunchAccelerationUnitsPerSecondSquared,
	TArray<FSRSpaceLogisticsHubRoute>& HubRoutes,
	int32& NextHubRouteSequence)
{
	OutRouteId = NAME_None;

	FSRSpaceLogisticsHubEndpoint ResolvedSourceHub;
	if (!SpaceLogisticsSubsystem.ResolveCurrentHubEndpoint(SourceHub, ResolvedSourceHub))
	{
		return false;
	}

	for (const FSRSpaceLogisticsHubRoute& HubRoute : HubRoutes)
	{
		if (IsDebugLocalOrbitRouteForHub(HubRoute, ResolvedSourceHub))
		{
			return false;
		}
	}

	FSRSpaceLogisticsHubRoute& HubRoute = HubRoutes.AddDefaulted_GetRef();
	HubRoute.RouteId = MakeHubRouteId(ResolvedSourceHub, ResolvedSourceHub, NextHubRouteSequence);
	HubRoute.SourceHub = ResolvedSourceHub;
	HubRoute.DestinationHub = ResolvedSourceHub;
	HubRoute.bEnabled = true;
	HubRoute.bReturnEmptyWhenNoCargo = true;
	HubRoute.MaxCargoStackCount = 1;
	HubRoute.CargoResourceId = NAME_None;
	HubRoute.bDebugLocalOrbit = true;
	HubRoute.Phase = ESRSpaceLogisticsHubRoutePhase::TravelingToDestination;
	HubRoute.CurrentDockSide = ESRSpaceLogisticsHubRouteDockSide::Source;
	ApplyHubRouteFlightSettings(SpaceLogisticsSubsystem, HubRoute, InitialSpeedUnitsPerSecond, LaunchAccelerationUnitsPerSecondSquared);
	HubRoute.TravelProgressSeconds = 0.0f;
	HubRoute.TravelProgressRatio = 0.0f;
	HubRoute.Cargo = FSRResourceInstance();

	if (!SpaceLogisticsSubsystem.ResolveHubEndpointSurfaceWorldLocation(ResolvedSourceHub, HubRoute.TravelStartWorldLocation))
	{
		HubRoutes.RemoveAt(HubRoutes.Num() - 1);
		return false;
	}

	FVector LaunchWorldVelocity = FVector::ZeroVector;
	HubRoute.bHasLaunchWorldVelocity = SpaceLogisticsSubsystem.ResolveHubEndpointWorldVelocity(
		ResolvedSourceHub,
		LaunchWorldVelocity);
	HubRoute.LaunchWorldVelocity = HubRoute.bHasLaunchWorldVelocity
		? LaunchWorldVelocity
		: FVector::ZeroVector;
	HubRoute.bHasTravelStartWorldLocation = true;
	HubRoute.TravelDurationSeconds = FSRSpaceLogisticsRoutePathResolver::ResolveTravelDurationSeconds(
		SpaceLogisticsSubsystem,
		HubRoute);
	OutRouteId = HubRoute.RouteId;

	SR_LOG(SpaceLogistics,
		LogTemp,
		Display,
		TEXT("[SpaceLogistics] Debug local orbit route created: RouteId=%s Source=%s/%s InitialSpeed=%.2f LaunchAcceleration=%.2f Duration=%.2f"),
		*OutRouteId.ToString(),
		*GetNameSafe(ResolvedSourceHub.BodyActor.Get()),
		*ResolvedSourceHub.HubOccupantId.ToString(),
		HubRoute.InitialSpeedUnitsPerSecond,
		HubRoute.LaunchAccelerationUnitsPerSecondSquared,
		HubRoute.TravelDurationSeconds);
	return true;
}

bool FSRSpaceLogisticsRouteRegistry::RemoveHubRoute(
	FName RouteId,
	TArray<FSRSpaceLogisticsHubRoute>& HubRoutes,
	TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId)
{
	if (RouteId.IsNone())
	{
		return false;
	}

	const int32 RemovedCount = HubRoutes.RemoveAll([RouteId](const FSRSpaceLogisticsHubRoute& HubRoute)
	{
		return HubRoute.RouteId == RouteId;
	});

	if (RemovedCount > 0)
	{
		FSRSpaceLogisticsRouteVisualController::DestroyRouteActor(RouteId, SpaceshipActorsByRouteId);
		SR_LOG(SpaceLogistics, LogTemp, Display, TEXT("[SpaceLogistics] Hub route removed: RouteId=%s"), *RouteId.ToString());
	}
	return RemovedCount > 0;
}

bool FSRSpaceLogisticsRouteRegistry::SetHubRouteMaxCargoStackCount(
	FName RouteId,
	int32 MaxCargoStackCount,
	TArray<FSRSpaceLogisticsHubRoute>& HubRoutes)
{
	FSRSpaceLogisticsHubRoute* HubRoute = FindMutableRoute(RouteId, HubRoutes);
	if (!HubRoute)
	{
		return false;
	}

	HubRoute->MaxCargoStackCount = FMath::Max(1, MaxCargoStackCount);
	SR_LOG(SpaceLogistics,
		LogTemp,
		Display,
		TEXT("[SpaceLogistics] Hub route max cargo stack count updated: RouteId=%s MaxCargoStackCount=%d"),
		*RouteId.ToString(),
		HubRoute->MaxCargoStackCount);
	return true;
}

bool FSRSpaceLogisticsRouteRegistry::SetHubRouteReturnEmptyWhenNoCargo(
	FName RouteId,
	bool bReturnEmptyWhenNoCargo,
	TArray<FSRSpaceLogisticsHubRoute>& HubRoutes)
{
	FSRSpaceLogisticsHubRoute* HubRoute = FindMutableRoute(RouteId, HubRoutes);
	if (!HubRoute)
	{
		return false;
	}

	HubRoute->bReturnEmptyWhenNoCargo = bReturnEmptyWhenNoCargo;
	SR_LOG(SpaceLogistics,
		LogTemp,
		Display,
		TEXT("[SpaceLogistics] Hub route return empty setting updated: RouteId=%s ReturnEmpty=%s"),
		*RouteId.ToString(),
		HubRoute->bReturnEmptyWhenNoCargo ? TEXT("true") : TEXT("false"));
	return true;
}

bool FSRSpaceLogisticsRouteRegistry::SetHubRouteCargoResourceId(
	FName RouteId,
	FName CargoResourceId,
	TArray<FSRSpaceLogisticsHubRoute>& HubRoutes)
{
	FSRSpaceLogisticsHubRoute* HubRoute = FindMutableRoute(RouteId, HubRoutes);
	if (!HubRoute)
	{
		return false;
	}

	HubRoute->CargoResourceId = CargoResourceId;
	SR_LOG(SpaceLogistics,
		LogTemp,
		Display,
		TEXT("[SpaceLogistics] Hub route cargo resource filter updated: RouteId=%s CargoResourceId=%s"),
		*RouteId.ToString(),
		HubRoute->CargoResourceId.IsNone() ? TEXT("Any") : *HubRoute->CargoResourceId.ToString());
	return true;
}

void FSRSpaceLogisticsRouteRegistry::ClearHubRoutes(
	TArray<FSRSpaceLogisticsHubRoute>& HubRoutes,
	TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId)
{
	const int32 RemovedCount = HubRoutes.Num();
	HubRoutes.Reset();
	FSRSpaceLogisticsRouteVisualController::Clear(SpaceshipActorsByRouteId);
	if (RemovedCount > 0)
	{
		SR_LOG(SpaceLogistics, LogTemp, Display, TEXT("[SpaceLogistics] Hub routes cleared: Removed=%d"), RemovedCount);
	}
}

void FSRSpaceLogisticsRouteRegistry::GetHubRoutes(
	const TArray<FSRSpaceLogisticsHubRoute>& HubRoutes,
	TArray<FSRSpaceLogisticsHubRoute>& OutRoutes)
{
	OutRoutes = HubRoutes;
}

bool FSRSpaceLogisticsRouteRegistry::GetHubRoute(
	FName RouteId,
	const TArray<FSRSpaceLogisticsHubRoute>& HubRoutes,
	FSRSpaceLogisticsHubRoute& OutRoute)
{
	OutRoute = FSRSpaceLogisticsHubRoute();
	if (RouteId.IsNone())
	{
		return false;
	}

	for (const FSRSpaceLogisticsHubRoute& HubRoute : HubRoutes)
	{
		if (HubRoute.RouteId == RouteId)
		{
			OutRoute = HubRoute;
			return true;
		}
	}

	return false;
}

bool FSRSpaceLogisticsRouteRegistry::AreHubEndpointPairsEquivalent(
	const FSRSpaceLogisticsHubEndpoint& FirstSource,
	const FSRSpaceLogisticsHubEndpoint& FirstDestination,
	const FSRSpaceLogisticsHubEndpoint& SecondSource,
	const FSRSpaceLogisticsHubEndpoint& SecondDestination)
{
	return (AreHubEndpointKeysEqual(FirstSource, SecondSource) && AreHubEndpointKeysEqual(FirstDestination, SecondDestination))
		|| (AreHubEndpointKeysEqual(FirstSource, SecondDestination) && AreHubEndpointKeysEqual(FirstDestination, SecondSource));
}

bool FSRSpaceLogisticsRouteRegistry::IsDebugLocalOrbitRouteForHub(
	const FSRSpaceLogisticsHubRoute& HubRoute,
	const FSRSpaceLogisticsHubEndpoint& HubEndpoint)
{
	return HubRoute.bDebugLocalOrbit && AreHubEndpointKeysEqual(HubRoute.SourceHub, HubEndpoint);
}

FSRSpaceLogisticsHubRoute* FSRSpaceLogisticsRouteRegistry::FindMutableRoute(FName RouteId, TArray<FSRSpaceLogisticsHubRoute>& HubRoutes)
{
	if (RouteId.IsNone())
	{
		return nullptr;
	}

	for (FSRSpaceLogisticsHubRoute& HubRoute : HubRoutes)
	{
		if (HubRoute.RouteId == RouteId)
		{
			return &HubRoute;
		}
	}

	return nullptr;
}

FName FSRSpaceLogisticsRouteRegistry::MakeHubRouteId(
	const FSRSpaceLogisticsHubEndpoint& SourceHub,
	const FSRSpaceLogisticsHubEndpoint& DestinationHub,
	int32& NextHubRouteSequence)
{
	return FName(*FString::Printf(
		TEXT("HubRoute_%s_%s_%d"),
		*SourceHub.HubOccupantId.ToString(),
		*DestinationHub.HubOccupantId.ToString(),
		NextHubRouteSequence++));
}

void FSRSpaceLogisticsRouteRegistry::ApplyHubRouteFlightSettings(
	USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	FSRSpaceLogisticsHubRoute& HubRoute,
	float InitialSpeedUnitsPerSecond,
	float LaunchAccelerationUnitsPerSecondSquared)
{
	float DefaultInitialSpeedUnitsPerSecond = FSRSpaceLogisticsRoutePathResolver::GetDefaultInitialSpeedUnitsPerSecond();
	float DefaultLaunchAccelerationUnitsPerSecondSquared =
		FSRSpaceLogisticsRoutePathResolver::GetDefaultLaunchAccelerationUnitsPerSecondSquared();
	FSRSpaceLogisticsRouteVisualController::ResolveDefaultFlightSettings(
		SpaceLogisticsSubsystem.GetWorld(),
		DefaultInitialSpeedUnitsPerSecond,
		DefaultLaunchAccelerationUnitsPerSecondSquared);

	HubRoute.InitialSpeedUnitsPerSecond = FSRSpaceLogisticsRoutePathResolver::ClampInitialSpeed(
		InitialSpeedUnitsPerSecond > 0.0f
			? InitialSpeedUnitsPerSecond
			: DefaultInitialSpeedUnitsPerSecond);
	HubRoute.LaunchAccelerationUnitsPerSecondSquared = FSRSpaceLogisticsRoutePathResolver::ClampLaunchAcceleration(
		LaunchAccelerationUnitsPerSecondSquared > 0.0f
			? LaunchAccelerationUnitsPerSecondSquared
			: DefaultLaunchAccelerationUnitsPerSecondSquared);
}
