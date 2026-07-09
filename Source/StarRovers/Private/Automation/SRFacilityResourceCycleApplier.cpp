#include "SRFacilityResourceCycleApplier.h"

#include "Automation/SRFacilityNetworkRuntimeState.h"
#include "SRFacilityResourceOperations.h"
#include "SRFacilityPortInventoryBuilder.h"

int32 FSRFacilityResourceCycleApplier::ApplyGameCycleToFacilities(FSRFacilityNetworkRuntimeState& RuntimeState)
{
	int32 ChangedResourceCount = 0;
	for (TPair<FName, FSRFacilityInstance>& FacilityPair : RuntimeState.FacilityInstancesByOccupantId)
	{
		FSRFacilityInstance& FacilityInstance = FacilityPair.Value;
		for (FSRFacilityPortInventory& InputPortInventory : FacilityInstance.InputPortInventories)
		{
			ChangedResourceCount += ApplyGameCycleToInventory(InputPortInventory.Inventory);
		}
		ChangedResourceCount += ApplyGameCycleToInventory(FacilityInstance.ProcessingInventory);
		for (FSRFacilityPortInventory& OutputPortInventory : FacilityInstance.OutputPortInventories)
		{
			ChangedResourceCount += ApplyGameCycleToInventory(OutputPortInventory.Inventory);
		}
		FSRFacilityPortInventoryBuilder::RefreshAggregateInventories(FacilityInstance);
	}
	return ChangedResourceCount;
}

int32 FSRFacilityResourceCycleApplier::ApplyGameCycleToInventory(TArray<FSRResourceInstance>& Inventory)
{
	int32 ChangedResourceCount = 0;
	for (FSRResourceInstance& ResourceInstance : Inventory)
	{
		if (ApplyHalfLifeCycleToResource(ResourceInstance))
		{
			++ChangedResourceCount;
		}
	}
	return ChangedResourceCount;
}

bool FSRFacilityResourceCycleApplier::ApplyHalfLifeCycleToResource(FSRResourceInstance& ResourceInstance)
{
	if (ResourceInstance.ResourceKind != ESRResourceKind::Energy)
	{
		return false;
	}

	bool bChanged = false;
	for (FSRResourceTagStack& TagStack : ResourceInstance.Tags)
	{
		if (TagStack.Tag != ESRResourceProcessTag::HalfLife || TagStack.StackCount <= 0)
		{
			continue;
		}

		if (TagStack.RemainingCycles <= 0)
		{
			TagStack.RemainingCycles = StarRovers::FacilityResources::HalfLifeDefaultCycles;
		}

		--TagStack.RemainingCycles;
		if (TagStack.RemainingCycles > 0)
		{
			bChanged = true;
			continue;
		}

		const int32 SafeStackCount = FMath::Max(1, TagStack.StackCount);
		ResourceInstance.EnergyValue *= FMath::Pow(0.5, static_cast<double>(SafeStackCount));
		TagStack.RemainingCycles = StarRovers::FacilityResources::HalfLifeDefaultCycles;
		bChanged = true;
	}

	return bChanged;
}
