#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityNetworkRuntimeState.h"

class STARROVERS_API FSROperationalCapacity final
{
public:
	static FSROperationalCapacityReportV2 BuildReport(
		const FSRFacilityNetworkRuntimeState& RuntimeState,
		bool bRulesActive,
		int32 BaseCapacity = 30,
		int32 ServiceCoreCapacity = 18,
		int32 AugmentCapacity = 0);
};
