#pragma once

#include "CoreMinimal.h"
#include "Logistics/SRSpaceLogisticsTypes.h"

class ASRSpaceshipActor;
class USRSpaceLogisticsSubsystem;

class FSRSpaceLogisticsRouteVisualController
{
public:
	static void Refresh(
		const USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		UWorld* World,
		TArray<FSRSpaceLogisticsHubRoute>& HubRoutes,
		TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId);

	static void Clear(TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId);
	static void DestroyRouteActor(FName RouteId, TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId);

	static void ResolveDefaultFlightSettings(
		float& OutInitialSpeedUnitsPerSecond,
		float& OutLaunchAccelerationUnitsPerSecondSquared);

private:
	static ASRSpaceshipActor* FindOrSpawn(
		UWorld* World,
		FSRSpaceLogisticsHubRoute& HubRoute,
		TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId);

	static TSubclassOf<ASRSpaceshipActor> ResolveActorClass();
	static bool IsRouteTraveling(const FSRSpaceLogisticsHubRoute& HubRoute);
};
