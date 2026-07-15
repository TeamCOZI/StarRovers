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

	static void RefreshStarFuelMissiles(
		const USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		UWorld* World,
		TArray<FSRSpaceLogisticsStarFuelMissile>& StarFuelMissiles,
		TMap<FName, TObjectPtr<ASRSpaceshipActor>>& MissileActorsByMissileId);

	static void Clear(TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId);
	static void DestroyRouteActor(FName RouteId, TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId);

	static void ResolveDefaultFlightSettings(
		UWorld* World,
		float& OutInitialSpeedUnitsPerSecond,
		float& OutLaunchAccelerationUnitsPerSecondSquared);

private:
	static ASRSpaceshipActor* FindOrSpawn(
		UWorld* World,
		FName VisualActorId,
		TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId);

	static TSubclassOf<ASRSpaceshipActor> ResolveActorClass(UWorld* World);
	static bool IsRouteTraveling(const FSRSpaceLogisticsHubRoute& HubRoute);
};
