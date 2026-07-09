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
		int32 MaxFacilitiesProcessed,
		TFunctionRef<bool(FSRFacilityInstance&)> TryStartProcessing,
		TFunctionRef<bool(FSRFacilityInstance&)> TryCompleteProcessing);
};
