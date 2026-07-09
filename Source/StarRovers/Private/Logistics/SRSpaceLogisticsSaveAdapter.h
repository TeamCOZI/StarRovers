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
		FSRSpaceLogisticsSaveData& OutSaveData);

	static bool ImportSaveData(
		USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		const FSRSpaceLogisticsSaveData& SaveData,
		TArray<FSRSpaceLogisticsHubRoute>& HubRoutes,
		int32& NextHubRouteSequence,
		TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId,
		TMap<FString, FSRSpaceLogisticsHubEndpointMotionSample>& HubEndpointMotionSamples);

private:
	static bool BuildRouteSaveData(
		const USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		const FSRSpaceLogisticsHubRoute& HubRoute,
		FSRSpaceLogisticsHubRouteSaveData& OutRouteSaveData);

	static bool ImportRoute(
		USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		const FSRSpaceLogisticsHubRouteSaveData& RouteSaveData,
		TArray<FSRSpaceLogisticsHubRoute>& HubRoutes);
};
