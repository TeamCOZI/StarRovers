#pragma once

#include "CoreMinimal.h"
#include "Logistics/SRSpaceLogisticsSubsystem.h"

class FSRSpaceLogisticsHubEndpointMotionTracker
{
public:
	static void Update(
		USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
		float SimulationDeltaTime,
		TArray<FSRSpaceLogisticsHubEndpoint>& CachedHubEndpoints,
		TMap<FString, FSRSpaceLogisticsHubEndpointMotionSample>& HubEndpointMotionSamples);

	static bool ResolveWorldVelocity(
		const FSRSpaceLogisticsHubEndpoint& HubEndpoint,
		const TMap<FString, FSRSpaceLogisticsHubEndpointMotionSample>& HubEndpointMotionSamples,
		FVector& OutWorldVelocity);
};
