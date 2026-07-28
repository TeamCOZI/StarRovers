#include "SRFacilityProcessingInventoryRouter.h"

#include "Automation/SRFacilityDataAsset.h"
#include "SRFacilityResourceOperations.h"
#include "SRFacilityPortInventoryBuilder.h"

namespace
{
	int32 ResolveRequiredInputCount(const FSRFacilityInstance& FacilityInstance)
	{
		const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
		if (!IsValid(FacilityDataAsset))
		{
			return 0;
		}

		if (FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Process)
		{
			return FacilityInstance.InputPortInventories.IsEmpty() ? 0 : 1;
		}
		if (FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Synthesize)
		{
			return FacilityInstance.InputPortInventories.Num();
		}
		return 0;
	}
}

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

	const int32 InputCount = ResolveRequiredInputCount(FacilityInstance);
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
	// A running or otherwise-reserved batch owns its processing inputs until
	// completion. Never overwrite it with a second reservation attempt.
	if (!FacilityInstance.ProcessingInventory.IsEmpty())
	{
		return false;
	}

	TArray<FSRResourceInstance> PendingInputResources;
	if (!GatherPendingInputResources(FacilityInstance, PendingInputResources))
	{
		return false;
	}

	const int32 InputCount = ResolveRequiredInputCount(FacilityInstance);
	if (InputCount <= 0)
	{
		return false;
	}

	// Reserve against a copy first. The live input ports are committed only once
	// all required lanes succeed, making multi-input synthesis transactional.
	TArray<FSRFacilityPortInventory> SimulatedInputPortInventories =
		FacilityInstance.InputPortInventories;
	TArray<FSRResourceInstance> ReservedInputResources;
	ReservedInputResources.Reserve(InputCount);
	for (int32 InputIndex = 0; InputIndex < InputCount; ++InputIndex)
	{
		if (!SimulatedInputPortInventories.IsValidIndex(InputIndex)
			|| StarRovers::FacilityResources::GetInventorySlotStackCount(SimulatedInputPortInventories[InputIndex]) <= 0)
		{
			return false;
		}

		FSRResourceInstance ResourceInstance;
		if (!StarRovers::FacilityResources::TryTakeSingleResourceFromInventorySlot(
			SimulatedInputPortInventories[InputIndex],
			ResourceInstance))
		{
			return false;
		}
		ReservedInputResources.Add(ResourceInstance);
	}

	if (ReservedInputResources.Num() != InputCount)
	{
		return false;
	}

	FacilityInstance.InputPortInventories = MoveTemp(SimulatedInputPortInventories);
	FacilityInstance.ProcessingInventory = MoveTemp(ReservedInputResources);
	FSRFacilityPortInventoryBuilder::RefreshAggregateInventories(FacilityInstance);
	return !FacilityInstance.ProcessingInventory.IsEmpty();
}
