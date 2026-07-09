#include "SRFacilityProcessingInventoryRouter.h"

#include "Automation/SRFacilityDataAsset.h"
#include "SRFacilityResourceOperations.h"
#include "SRFacilityPortInventoryBuilder.h"

bool FSRFacilityProcessingInventoryRouter::GatherPendingInputResources(
	const FSRFacilityInstance& FacilityInstance,
	TArray<FSRResourceInstance>& OutInputResources)
{
	OutInputResources.Reset();
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset))
	{
		return false;
	}

	const int32 InputCount = FacilityInstance.InputPortInventories.Num();
	if (InputCount <= 0)
	{
		return false;
	}

	OutInputResources.Reserve(InputCount);
	for (int32 InputIndex = 0; InputIndex < InputCount; ++InputIndex)
	{
		const FSRFacilityPortInventory& InputPortInventory = FacilityInstance.InputPortInventories[InputIndex];
		const FSRResourceInstance ResourceInstance = StarRovers::FacilityResources::PeekSingleResourceFromInventorySlot(InputPortInventory);
		if (ResourceInstance.ResourceId.IsNone() || ResourceInstance.StackCount <= 0)
		{
			OutInputResources.Reset();
			return false;
		}

		OutInputResources.Add(ResourceInstance);
	}

	return !OutInputResources.IsEmpty();
}

bool FSRFacilityProcessingInventoryRouter::CanStoreOutputResources(
	const FSRFacilityInstance& FacilityInstance,
	const TArray<FSRResourceInstance>& OutputResources)
{
	if (OutputResources.IsEmpty())
	{
		return false;
	}

	TArray<FSRFacilityPortInventory> SimulatedOutputPortInventories = FacilityInstance.OutputPortInventories;
	for (const FSRResourceInstance& OutputResource : OutputResources)
	{
		const int32 RequiredStackCount = StarRovers::FacilityResources::GetResourceStackCount(OutputResource);
		if (RequiredStackCount <= 0
			|| StarRovers::FacilityResources::TryAddResourceToInventorySlots(SimulatedOutputPortInventories, OutputResource) != RequiredStackCount)
		{
			return false;
		}
	}

	return true;
}

void FSRFacilityProcessingInventoryRouter::StoreOutputResources(
	FSRFacilityInstance& FacilityInstance,
	const TArray<FSRResourceInstance>& OutputResources)
{
	for (const FSRResourceInstance& OutputResource : OutputResources)
	{
		StarRovers::FacilityResources::TryAddResourceToInventorySlots(FacilityInstance.OutputPortInventories, OutputResource);
	}

	FSRFacilityPortInventoryBuilder::RefreshAggregateInventories(FacilityInstance);
}

bool FSRFacilityProcessingInventoryRouter::TryMoveInputsToProcessingInventory(FSRFacilityInstance& FacilityInstance)
{
	const int32 InputCount = FacilityInstance.InputPortInventories.Num();
	if (InputCount <= 0)
	{
		return false;
	}

	FacilityInstance.ProcessingInventory.Reset();
	for (int32 InputIndex = 0; InputIndex < InputCount; ++InputIndex)
	{
		if (!FacilityInstance.InputPortInventories.IsValidIndex(InputIndex)
			|| StarRovers::FacilityResources::GetInventorySlotStackCount(FacilityInstance.InputPortInventories[InputIndex]) <= 0)
		{
			FacilityInstance.ProcessingInventory.Reset();
			FacilityInstance.bProcessing = false;
			FacilityInstance.ProcessProgressSeconds = 0.0f;
			return false;
		}

		FSRResourceInstance ResourceInstance;
		if (!StarRovers::FacilityResources::TryTakeSingleResourceFromInventorySlot(FacilityInstance.InputPortInventories[InputIndex], ResourceInstance))
		{
			FacilityInstance.ProcessingInventory.Reset();
			FacilityInstance.bProcessing = false;
			FacilityInstance.ProcessProgressSeconds = 0.0f;
			return false;
		}
		FacilityInstance.ProcessingInventory.Add(ResourceInstance);
	}
	FSRFacilityPortInventoryBuilder::RefreshAggregateInventories(FacilityInstance);

	if (FacilityInstance.ProcessingInventory.IsEmpty())
	{
		FacilityInstance.bProcessing = false;
		FacilityInstance.ProcessProgressSeconds = 0.0f;
		return false;
	}

	return true;
}
