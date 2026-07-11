#include "SRFacilityResourceCycleApplier.h"

#include "Automation/SRFacilityNetworkRuntimeState.h"

int32 FSRFacilityResourceCycleApplier::ApplyGameCycleToFacilities(FSRFacilityNetworkRuntimeState&)
{
	// Resource tag effects are applied during facility processing. No resource effect currently runs on game-cycle ticks.
	return 0;
}
