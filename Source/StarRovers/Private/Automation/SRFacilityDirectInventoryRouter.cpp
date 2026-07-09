#include "SRFacilityDirectInventoryRouter.h"

#include "SRFacilityResourceOperations.h"
#include "SRFacilityPortInventoryBuilder.h"

namespace
{
	FSRFacilityPortInventory* FindInputPortInventoryForDirectAdd(
		FSRFacilityInstance& FacilityInstance,
		const FSRResourceInstance& ResourceInstance,
		int32 PreferredPortIndex = INDEX_NONE)
	{
		if (FacilityInstance.InputPortInventories.IsValidIndex(PreferredPortIndex))
		{
			FSRFacilityPortInventory& PreferredPortInventory = FacilityInstance.InputPortInventories[PreferredPortIndex];
			if (StarRovers::FacilityResources::CanInventorySlotAcceptResource(PreferredPortInventory, ResourceInstance))
			{
				return &PreferredPortInventory;
			}
		}

		for (FSRFacilityPortInventory& InputPortInventory : FacilityInstance.InputPortInventories)
		{
			if (StarRovers::FacilityResources::GetInventorySlotStackCount(InputPortInventory) > 0
				&& StarRovers::FacilityResources::CanInventorySlotAcceptResource(InputPortInventory, ResourceInstance))
			{
				return &InputPortInventory;
			}
		}

		for (FSRFacilityPortInventory& InputPortInventory : FacilityInstance.InputPortInventories)
		{
			if (StarRovers::FacilityResources::GetInventorySlotStackCount(InputPortInventory) <= 0
				&& StarRovers::FacilityResources::CanInventorySlotAcceptResource(InputPortInventory, ResourceInstance))
			{
				return &InputPortInventory;
			}
		}

		return nullptr;
	}

	void ResetTransferResult(FSRFacilityInventoryTransferResult* OutTransferResult)
	{
		if (OutTransferResult)
		{
			*OutTransferResult = FSRFacilityInventoryTransferResult();
		}
	}
}

bool FSRFacilityDirectInventoryRouter::TryAddInputResource(
	FSRFacilityInstance& FacilityInstance,
	const FSRResourceInstance& ResourceInstance,
	FSRFacilityInventoryTransferResult* OutTransferResult)
{
	ResetTransferResult(OutTransferResult);

	FSRFacilityPortInventory* InputPortInventory = FindInputPortInventoryForDirectAdd(FacilityInstance, ResourceInstance);
	if (!InputPortInventory || ResourceInstance.ResourceId.IsNone() || ResourceInstance.StackCount <= 0)
	{
		return false;
	}

	TArray<FSRFacilityPortInventory> SimulatedInputPortInventories = FacilityInstance.InputPortInventories;
	const int32 RequiredStackCount = StarRovers::FacilityResources::GetResourceStackCount(ResourceInstance);
	if (StarRovers::FacilityResources::TryAddResourceToInventorySlots(SimulatedInputPortInventories, ResourceInstance) != RequiredStackCount)
	{
		return false;
	}

	const int32 AddedStackCount = StarRovers::FacilityResources::TryAddResourceToInventorySlots(FacilityInstance.InputPortInventories, ResourceInstance);
	if (AddedStackCount != RequiredStackCount)
	{
		return false;
	}

	FSRFacilityPortInventoryBuilder::RefreshAggregateInventories(FacilityInstance);
	if (OutTransferResult)
	{
		OutTransferResult->PortId = InputPortInventory->PortId;
		OutTransferResult->PortStackCount = StarRovers::FacilityResources::GetInventorySlotStackCount(*InputPortInventory);
		OutTransferResult->AggregateStackCount = FacilityInstance.InputInventory.Num();
	}
	return true;
}

bool FSRFacilityDirectInventoryRouter::TryAddInputResourceToPort(
	FSRFacilityInstance& FacilityInstance,
	int32 InputPortIndex,
	const FSRResourceInstance& ResourceInstance)
{
	FSRFacilityPortInventory* InputPortInventory = FacilityInstance.InputPortInventories.IsValidIndex(InputPortIndex)
		? &FacilityInstance.InputPortInventories[InputPortIndex]
		: nullptr;
	if (!InputPortInventory || ResourceInstance.ResourceId.IsNone() || ResourceInstance.StackCount <= 0)
	{
		return false;
	}

	FSRFacilityPortInventory SimulatedInputPortInventory = *InputPortInventory;
	const int32 RequiredStackCount = StarRovers::FacilityResources::GetResourceStackCount(ResourceInstance);
	if (StarRovers::FacilityResources::TryAddResourceToInventorySlot(SimulatedInputPortInventory, ResourceInstance) != RequiredStackCount)
	{
		return false;
	}

	if (StarRovers::FacilityResources::TryAddResourceToInventorySlot(*InputPortInventory, ResourceInstance) != RequiredStackCount)
	{
		return false;
	}

	FSRFacilityPortInventoryBuilder::RefreshAggregateInventories(FacilityInstance);
	return true;
}

bool FSRFacilityDirectInventoryRouter::TryExtractOutputResource(
	FSRFacilityInstance& FacilityInstance,
	FSRResourceInstance& OutResourceInstance,
	FSRFacilityInventoryTransferResult* OutTransferResult)
{
	OutResourceInstance = FSRResourceInstance();
	ResetTransferResult(OutTransferResult);

	FSRFacilityPortInventory* OutputPortInventory = FacilityInstance.OutputPortInventories.FindByPredicate([](const FSRFacilityPortInventory& CandidatePortInventory)
	{
		return StarRovers::FacilityResources::GetInventorySlotStackCount(CandidatePortInventory) > 0;
	});
	if (!OutputPortInventory)
	{
		return false;
	}

	if (!StarRovers::FacilityResources::TryTakeSingleResourceFromInventorySlot(*OutputPortInventory, OutResourceInstance))
	{
		OutResourceInstance = FSRResourceInstance();
		return false;
	}

	FSRFacilityPortInventoryBuilder::RefreshAggregateInventories(FacilityInstance);
	if (OutTransferResult)
	{
		OutTransferResult->PortId = OutputPortInventory->PortId;
		OutTransferResult->PortStackCount = StarRovers::FacilityResources::GetInventorySlotStackCount(*OutputPortInventory);
		OutTransferResult->AggregateStackCount = FacilityInstance.OutputInventory.Num();
	}
	return true;
}
