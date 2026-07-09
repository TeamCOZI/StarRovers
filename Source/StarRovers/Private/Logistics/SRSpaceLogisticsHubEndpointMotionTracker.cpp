#include "SRSpaceLogisticsHubEndpointMotionTracker.h"

#include "SRSpaceLogisticsHubEndpointResolver.h"

void FSRSpaceLogisticsHubEndpointMotionTracker::Update(
	USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	float SimulationDeltaTime,
	TArray<FSRSpaceLogisticsHubEndpoint>& CachedHubEndpoints,
	TMap<FString, FSRSpaceLogisticsHubEndpointMotionSample>& HubEndpointMotionSamples)
{
	if (SimulationDeltaTime <= UE_SMALL_NUMBER)
	{
		return;
	}

	FSRSpaceLogisticsHubEndpointResolver::Rebuild(SpaceLogisticsSubsystem.GetWorld(), CachedHubEndpoints);

	TSet<FString> ActiveSampleKeys;
	ActiveSampleKeys.Reserve(CachedHubEndpoints.Num());
	for (const FSRSpaceLogisticsHubEndpoint& HubEndpoint : CachedHubEndpoints)
	{
		FVector CurrentWorldLocation = FVector::ZeroVector;
		if (!SpaceLogisticsSubsystem.ResolveHubEndpointWorldLocation(HubEndpoint, CurrentWorldLocation))
		{
			continue;
		}

		const FString SampleKey = FSRSpaceLogisticsHubEndpointResolver::BuildMotionKey(HubEndpoint);
		ActiveSampleKeys.Add(SampleKey);

		FSRSpaceLogisticsHubEndpointMotionSample& MotionSample = HubEndpointMotionSamples.FindOrAdd(SampleKey);
		if (MotionSample.bHasWorldLocation)
		{
			MotionSample.WorldVelocity = (CurrentWorldLocation - MotionSample.LastWorldLocation) / SimulationDeltaTime;
		}
		else
		{
			MotionSample.WorldVelocity = FVector::ZeroVector;
			MotionSample.bHasWorldLocation = true;
		}
		MotionSample.LastWorldLocation = CurrentWorldLocation;
	}

	TArray<FString> StaleSampleKeys;
	for (const TPair<FString, FSRSpaceLogisticsHubEndpointMotionSample>& Pair : HubEndpointMotionSamples)
	{
		if (!ActiveSampleKeys.Contains(Pair.Key))
		{
			StaleSampleKeys.Add(Pair.Key);
		}
	}

	for (const FString& StaleSampleKey : StaleSampleKeys)
	{
		HubEndpointMotionSamples.Remove(StaleSampleKey);
	}
}

bool FSRSpaceLogisticsHubEndpointMotionTracker::ResolveWorldVelocity(
	const FSRSpaceLogisticsHubEndpoint& HubEndpoint,
	const TMap<FString, FSRSpaceLogisticsHubEndpointMotionSample>& HubEndpointMotionSamples,
	FVector& OutWorldVelocity)
{
	OutWorldVelocity = FVector::ZeroVector;
	const FSRSpaceLogisticsHubEndpointMotionSample* MotionSample = HubEndpointMotionSamples.Find(
		FSRSpaceLogisticsHubEndpointResolver::BuildMotionKey(HubEndpoint));
	if (!MotionSample || !MotionSample->bHasWorldLocation)
	{
		return false;
	}

	OutWorldVelocity = MotionSample->WorldVelocity;
	return !OutWorldVelocity.IsNearlyZero();
}
