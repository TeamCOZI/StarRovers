#pragma once

#include "CoreMinimal.h"
#include "Logistics/SRSpaceLogisticsTypes.h"

class ASRSpaceshipActor;
class USRSpaceLogisticsSubsystem;

class FSRSpaceLogisticsRouteProcessor
{
public:
	static void ProcessRoutes(
		USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		float DeltaTime,
		TArray<FSRSpaceLogisticsHubRoute>& HubRoutes,
		TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId);

private:
	static void ProcessRoute(
		USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		FSRSpaceLogisticsHubRoute& HubRoute,
		float DeltaTime,
		TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId);

	static bool RefreshRouteEndpoints(USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem, FSRSpaceLogisticsHubRoute& HubRoute);

	static bool TryDepartFromDock(
		USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		FSRSpaceLogisticsHubRoute& HubRoute,
		ESRSpaceLogisticsHubRouteDockSide DockSide);

	static bool TryUnloadAtDock(FSRSpaceLogisticsHubRoute& HubRoute, ESRSpaceLogisticsHubRouteDockSide DockSide);

	static bool StartTravel(
		USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		FSRSpaceLogisticsHubRoute& HubRoute,
		ESRSpaceLogisticsHubRoutePhase TravelPhase);

	static void AdvanceTravel(
		USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		FSRSpaceLogisticsHubRoute& HubRoute,
		float DeltaTime,
		TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId);

	static bool TryLoadCargoFromHub(
		const FSRSpaceLogisticsHubEndpoint& HubEndpoint,
		int32 MaxStackCount,
		FName CargoResourceId,
		FSRResourceInstance& OutCargo);

	static bool TryUnloadCargoToHub(const FSRSpaceLogisticsHubEndpoint& HubEndpoint, const FSRResourceInstance& Cargo);
};
