#pragma once

#include "CoreMinimal.h"
#include "Logistics/SRSpaceLogisticsTypes.h"

class ASRSpaceshipActor;
class USRSpaceLogisticsSubsystem;

class FSRSpaceLogisticsRouteRegistry
{
public:
	static bool AreHubEndpointKeysEqual(const FSRSpaceLogisticsHubEndpoint& Left, const FSRSpaceLogisticsHubEndpoint& Right);

	static bool DoesRouteAlreadyExist(
		const TArray<FSRSpaceLogisticsHubRoute>& HubRoutes,
		const FSRSpaceLogisticsHubEndpoint& SourceHub,
		const FSRSpaceLogisticsHubEndpoint& DestinationHub);

	static bool CreateHubRoute(
		USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		const FSRSpaceLogisticsHubEndpoint& SourceHub,
		const FSRSpaceLogisticsHubEndpoint& DestinationHub,
		FName& OutRouteId,
		bool bReturnEmptyWhenNoCargo,
		int32 MaxCargoStackCount,
		float InitialSpeedUnitsPerSecond,
		float LaunchAccelerationUnitsPerSecondSquared,
		TArray<FSRSpaceLogisticsHubRoute>& HubRoutes,
		int32& NextHubRouteSequence);

	static bool CreateDebugLocalOrbitRoute(
		USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		const FSRSpaceLogisticsHubEndpoint& SourceHub,
		FName& OutRouteId,
		float InitialSpeedUnitsPerSecond,
		float LaunchAccelerationUnitsPerSecondSquared,
		TArray<FSRSpaceLogisticsHubRoute>& HubRoutes,
		int32& NextHubRouteSequence);

	static bool RemoveHubRoute(
		FName RouteId,
		TArray<FSRSpaceLogisticsHubRoute>& HubRoutes,
		TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId);

	static bool SetHubRouteMaxCargoStackCount(
		FName RouteId,
		int32 MaxCargoStackCount,
		TArray<FSRSpaceLogisticsHubRoute>& HubRoutes);

	static bool SetHubRouteReturnEmptyWhenNoCargo(
		FName RouteId,
		bool bReturnEmptyWhenNoCargo,
		TArray<FSRSpaceLogisticsHubRoute>& HubRoutes);

	static bool SetHubRouteCargoResourceId(
		FName RouteId,
		FName CargoResourceId,
		TArray<FSRSpaceLogisticsHubRoute>& HubRoutes);

	static void ClearHubRoutes(
		TArray<FSRSpaceLogisticsHubRoute>& HubRoutes,
		TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId);

	static void GetHubRoutes(const TArray<FSRSpaceLogisticsHubRoute>& HubRoutes, TArray<FSRSpaceLogisticsHubRoute>& OutRoutes);
	static bool GetHubRoute(FName RouteId, const TArray<FSRSpaceLogisticsHubRoute>& HubRoutes, FSRSpaceLogisticsHubRoute& OutRoute);

private:
	static bool AreHubEndpointPairsEquivalent(
		const FSRSpaceLogisticsHubEndpoint& FirstSource,
		const FSRSpaceLogisticsHubEndpoint& FirstDestination,
		const FSRSpaceLogisticsHubEndpoint& SecondSource,
		const FSRSpaceLogisticsHubEndpoint& SecondDestination);

	static bool IsDebugLocalOrbitRouteForHub(const FSRSpaceLogisticsHubRoute& HubRoute, const FSRSpaceLogisticsHubEndpoint& HubEndpoint);

	static FSRSpaceLogisticsHubRoute* FindMutableRoute(FName RouteId, TArray<FSRSpaceLogisticsHubRoute>& HubRoutes);

	static FName MakeHubRouteId(
		const FSRSpaceLogisticsHubEndpoint& SourceHub,
		const FSRSpaceLogisticsHubEndpoint& DestinationHub,
		int32& NextHubRouteSequence);

	static void ApplyHubRouteFlightSettings(
		USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		FSRSpaceLogisticsHubRoute& HubRoute,
		float InitialSpeedUnitsPerSecond,
		float LaunchAccelerationUnitsPerSecondSquared);
};
