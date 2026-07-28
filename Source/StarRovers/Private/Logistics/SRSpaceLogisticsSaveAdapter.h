#pragma once

#include "CoreMinimal.h"
#include "Logistics/SRSpaceLogisticsSubsystem.h"

class FSRSpaceLogisticsSaveAdapter
{
public:
	static void ExportSaveData(
		const USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		const TArray<FSRSpaceLogisticsHubRoute>& HubRoutes,
		int32 NextHubRouteSequence,
		int64 NextFleetDepartureQueueSequence,
		const TArray<FSRSpaceLogisticsStarFuelMissile>& StarFuelMissiles,
		int32 NextStarFuelMissileSequence,
		FSRSpaceLogisticsSaveData& OutSaveData);

	static bool ImportSaveData(
		USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		const FSRSpaceLogisticsSaveData& SaveData,
		TArray<FSRSpaceLogisticsHubRoute>& HubRoutes,
		int32& NextHubRouteSequence,
		int64& NextFleetDepartureQueueSequence,
		TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId,
		TArray<FSRSpaceLogisticsStarFuelMissile>& StarFuelMissiles,
		int32& NextStarFuelMissileSequence,
		TMap<FName, TObjectPtr<ASRSpaceshipActor>>& StarFuelMissileActorsByMissileId,
		TMap<FString, FSRSpaceLogisticsHubEndpointMotionSample>& HubEndpointMotionSamples);

private:
	static bool BuildRouteSaveData(
		const USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		const FSRSpaceLogisticsHubRoute& HubRoute,
		FSRSpaceLogisticsHubRouteSaveData& OutRouteSaveData);

	static bool ImportRoute(
		USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		const FSRSpaceLogisticsHubRouteSaveData& RouteSaveData,
		int32 SourceSaveVersion,
		TArray<FSRSpaceLogisticsHubRoute>& HubRoutes);

	static bool BuildMissileSaveData(
		const USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		const FSRSpaceLogisticsStarFuelMissile& Missile,
		FSRSpaceLogisticsStarFuelMissileSaveData& OutMissileSaveData);

	static bool ImportMissile(
		USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		const FSRSpaceLogisticsStarFuelMissileSaveData& MissileSaveData,
		int32 SourceSaveVersion,
		TArray<FSRSpaceLogisticsStarFuelMissile>& StarFuelMissiles);

	static AActor* ResolveSavedStarActor(
		USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		FName TargetStarActorName,
		const FString& TargetStarVariableName);
};
