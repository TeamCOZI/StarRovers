#pragma once

#include "CoreMinimal.h"

class UActorComponent;
struct FSRFacilityNetworkRuntimeState;
struct FSRResourceInstance;

class FSRFacilityOutputPreviewQuery
{
public:
	static bool GetOutputPreview(
		const UActorComponent* OwnerComponent,
		const FSRFacilityNetworkRuntimeState& RuntimeState,
		FName OccupantId,
		FSRResourceInstance& OutPrimaryOutput,
		TArray<FSRResourceInstance>& OutAdditionalOutputs,
		int32& OutOutputCount);
};
