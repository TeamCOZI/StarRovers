#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityRuntimeData.h"

class FSRFacilityProcessingInventoryRouter
{
public:
	static bool GatherPendingInputResources(
		const FSRFacilityInstance& FacilityInstance,
		TArray<FSRResourceInstance>& OutInputResources);

	static bool CanStoreOutputResources(
		const FSRFacilityInstance& FacilityInstance,
		const TArray<FSRResourceInstance>& OutputResources);

	static void StoreOutputResources(
		FSRFacilityInstance& FacilityInstance,
		const TArray<FSRResourceInstance>& OutputResources);

	static bool TryMoveInputsToProcessingInventory(FSRFacilityInstance& FacilityInstance);
};
