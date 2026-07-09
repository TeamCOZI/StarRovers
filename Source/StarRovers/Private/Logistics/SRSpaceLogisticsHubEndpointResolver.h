#pragma once

#include "CoreMinimal.h"
#include "Logistics/SRSpaceLogisticsTypes.h"

class UWorld;

class FSRSpaceLogisticsHubEndpointResolver
{
public:
	static void Rebuild(UWorld* World, TArray<FSRSpaceLogisticsHubEndpoint>& OutHubEndpoints);

	static bool Build(AActor* BodyActor, FName HubOccupantId, FSRSpaceLogisticsHubEndpoint& OutHubEndpoint);

	static bool BuildSaveData(
		const FSRSpaceLogisticsHubEndpoint& HubEndpoint,
		FSRSpaceLogisticsHubEndpointSaveData& OutSaveData);

	static bool ResolveSaved(
		UWorld* World,
		TArray<FSRSpaceLogisticsHubEndpoint>& CachedHubEndpoints,
		const FSRSpaceLogisticsHubEndpointSaveData& SaveData,
		FSRSpaceLogisticsHubEndpoint& OutHubEndpoint);

	static bool ResolveCurrent(
		const FSRSpaceLogisticsHubEndpoint& CandidateHubEndpoint,
		FSRSpaceLogisticsHubEndpoint& OutHubEndpoint);

	static bool ResolveWorldLocationWithHeightOffset(
		const FSRSpaceLogisticsHubEndpoint& HubEndpoint,
		float HeightOffset,
		FVector& OutWorldLocation);

	static bool ResolveSurfaceWorldLocation(
		const FSRSpaceLogisticsHubEndpoint& HubEndpoint,
		float FallbackHeightOffset,
		FVector& OutWorldLocation);

	static FString BuildMotionKey(const FSRSpaceLogisticsHubEndpoint& HubEndpoint);
};
