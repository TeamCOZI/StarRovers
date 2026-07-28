#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityNetworkRuntimeState.h"
#include "Templates/Function.h"

class FSRFacilityProcessingTickRunner
{
public:
	static int32 ProcessFacilities(
		FSRFacilityNetworkRuntimeState& RuntimeState,
		float DeltaTime,
		int32 MaxFacilityTransitions,
		TFunctionRef<bool(FSRFacilityInstance&)> TryStartProcessing,
		TFunctionRef<void()> RefreshOperationalCapacity,
		TFunctionRef<float(const FSRFacilityInstance&)> ResolveOperationalSpeedFactor,
		TFunctionRef<bool(FSRFacilityInstance&)> TryCompleteProcessing);
};
