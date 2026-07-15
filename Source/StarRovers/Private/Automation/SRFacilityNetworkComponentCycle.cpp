#include "Automation/SRFacilityNetworkComponent.h"

#include "Utility/SRLog.h"
#include "SRFacilityResourceCycleApplier.h"

void USRFacilityNetworkComponent::HandleGameCycleAdvanced(int32 CurrentCycleIndex)
{
	const int32 ChangedResourceCount = FSRFacilityResourceCycleApplier::ApplyGameCycleToFacilities(RuntimeState);
	if (ChangedResourceCount > 0 && bLogFacilityNetworkEvents)
	{
		SR_LOG(FacilityNetwork, LogTemp,
			Display,
			TEXT("[FacilityNetwork] Game cycle applied: Cycle=%d ChangedResources=%d Owner=%s"),
			CurrentCycleIndex,
			ChangedResourceCount,
			*GetNameSafe(GetOwner()));
	}
}

int32 USRFacilityNetworkComponent::DebugApplyGameCyclesToResources(int32 CycleCount)
{
	const int32 SafeCycleCount = FMath::Max(0, CycleCount);
	int32 ChangedResourceCount = 0;
	for (int32 CycleIndex = 0; CycleIndex < SafeCycleCount; ++CycleIndex)
	{
		ChangedResourceCount += FSRFacilityResourceCycleApplier::ApplyGameCycleToFacilities(RuntimeState);
	}
	return ChangedResourceCount;
}
