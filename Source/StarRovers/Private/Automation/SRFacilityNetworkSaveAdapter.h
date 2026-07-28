#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityNetworkRuntimeState.h"
#include "Automation/SRFacilityNetworkSaveData.h"

class FSRFacilityNetworkSaveAdapter
{
public:
	static void ExportSaveData(
		const FSRFacilityNetworkRuntimeState& RuntimeState,
		FSRFacilityNetworkSaveData& OutSaveData);

	static bool ImportSaveData(
		const FSRFacilityNetworkSaveData& SaveData,
		FSRFacilityNetworkRuntimeState& OutRuntimeState,
		FString* OutFailureReason = nullptr);
};
