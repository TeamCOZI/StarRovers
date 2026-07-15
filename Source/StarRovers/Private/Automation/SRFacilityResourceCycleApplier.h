#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityNetworkRuntimeState.h"

class FSRFacilityResourceCycleApplier
{
public:
	static int32 ApplyGameCycleToFacilities(FSRFacilityNetworkRuntimeState& RuntimeState);
};
