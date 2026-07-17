#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityRuntimeData.h"

struct FSRFacilityInventoryTransferResult
{
	FName PortId = NAME_None;
	int32 PortIndex = INDEX_NONE;
	int32 PortStackCount = 0;
	int32 AggregateStackCount = 0;
};

class FSRFacilityDirectInventoryRouter
{
public:
	static bool TryAddInputResource(
		FSRFacilityInstance& FacilityInstance,
		const FSRResourceInstance& ResourceInstance,
		FSRFacilityInventoryTransferResult* OutTransferResult = nullptr);

	static bool TryAddInputResourceToPort(
		FSRFacilityInstance& FacilityInstance,
		int32 InputPortIndex,
		const FSRResourceInstance& ResourceInstance,
		FSRFacilityInventoryTransferResult* OutTransferResult = nullptr);

	static bool TryExtractOutputResource(
		FSRFacilityInstance& FacilityInstance,
		FSRResourceInstance& OutResourceInstance,
		FSRFacilityInventoryTransferResult* OutTransferResult = nullptr);
};
