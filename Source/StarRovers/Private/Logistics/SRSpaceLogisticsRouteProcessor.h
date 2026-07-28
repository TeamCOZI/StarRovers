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
		TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId,
		int64& NextFleetDepartureQueueSequence);

private:
	static void ProcessRoute(
		USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		FSRSpaceLogisticsHubRoute& HubRoute,
		float DeltaTime,
		TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId,
		TArray<FSRSpaceLogisticsHubRoute>& HubRoutes,
		TMap<FString, FSRFleetCapacityReportV2>& FleetCapacityReportsByHub,
		int64& NextFleetDepartureQueueSequence);

	static bool RefreshRouteEndpoints(USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem, FSRSpaceLogisticsHubRoute& HubRoute);

	static bool TryDepartFromDock(
		USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		FSRSpaceLogisticsHubRoute& HubRoute,
		ESRSpaceLogisticsHubRouteDockSide DockSide,
		TArray<FSRSpaceLogisticsHubRoute>& HubRoutes,
		TMap<FString, FSRFleetCapacityReportV2>& FleetCapacityReportsByHub,
		int64& NextFleetDepartureQueueSequence);

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

	static void AdvanceConditioning(
		USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		FSRSpaceLogisticsHubRoute& HubRoute,
		float DeltaTime);

	static void CompleteRouteArrival(
		USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		FSRSpaceLogisticsHubRoute& HubRoute,
		ESRSpaceLogisticsHubRouteDockSide ArrivalDockSide,
		bool bResourceV2RulesActive);

	static bool TryLoadCargoFromHub(
		const FSRSpaceLogisticsHubEndpoint& HubEndpoint,
		const FSRSpaceLogisticsHubRoute& HubRoute,
		bool bFleetRulesActive,
		FSRResourceInstance& OutCargo);

	static bool HasLoadableCargoAtHub(
		const FSRSpaceLogisticsHubEndpoint& HubEndpoint,
		const FSRSpaceLogisticsHubRoute& HubRoute,
		bool bFleetRulesActive);

	static bool IsCargoEligibleForRoute(
		const FSRResourceInstance& Cargo,
		const FSRSpaceLogisticsHubRoute& HubRoute,
		bool bFleetRulesActive);

	static void ClearFleetQueue(FSRSpaceLogisticsHubRoute& HubRoute);

	static bool TryUnloadCargoToHub(const FSRSpaceLogisticsHubEndpoint& HubEndpoint, const FSRResourceInstance& Cargo);
};
