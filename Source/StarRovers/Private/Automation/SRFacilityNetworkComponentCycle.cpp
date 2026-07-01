#include "Automation/SRFacilityNetworkComponent.h"

#include "SRFacilityNetworkComponentInternal.h"

namespace StarRovers::FacilityNetwork::Cycle
{
	bool ApplyHalfLifeCycleToResource(FSRResourceInstance& ResourceInstance)
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
				TagStack.RemainingCycles = HalfLifeDefaultCycles;
			}

			--TagStack.RemainingCycles;
			if (TagStack.RemainingCycles > 0)
			{
				bChanged = true;
				continue;
			}

			const int32 SafeStackCount = FMath::Max(1, TagStack.StackCount);
			ResourceInstance.EnergyValue *= FMath::Pow(0.5, static_cast<double>(SafeStackCount));
			TagStack.RemainingCycles = HalfLifeDefaultCycles;
			bChanged = true;
		}

		return bChanged;
	}
}

void USRFacilityNetworkComponent::HandleGameCycleAdvanced(int32 CurrentCycleIndex)
{
	const int32 ChangedResourceCount = ApplyGameCycleToResources();
	if (ChangedResourceCount > 0 && bLogFacilityNetworkEvents)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[FacilityNetwork] Game cycle applied: Cycle=%d ChangedResources=%d Owner=%s"),
			CurrentCycleIndex,
			ChangedResourceCount,
			*GetNameSafe(GetOwner()));
	}
}

int32 USRFacilityNetworkComponent::DebugApplyGameCyclesToResources(int32 CycleCount)
{
	const int32 SafeCycleCount = FMath::Max(0, CycleCount);
	int32 ChangedResourceCount = 0;
	for (int32 CycleIndex = 0; CycleIndex < SafeCycleCount; ++CycleIndex)
	{
		ChangedResourceCount += ApplyGameCycleToResources();
	}
	return ChangedResourceCount;
}

int32 USRFacilityNetworkComponent::ApplyGameCycleToResources()
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
		RefreshFacilityAggregateInventories(FacilityInstance);
	}
	return ChangedResourceCount;
}

int32 USRFacilityNetworkComponent::ApplyGameCycleToInventory(TArray<FSRResourceInstance>& Inventory)
{
	int32 ChangedResourceCount = 0;
	for (FSRResourceInstance& ResourceInstance : Inventory)
	{
		if (StarRovers::FacilityNetwork::Cycle::ApplyHalfLifeCycleToResource(ResourceInstance))
		{
			++ChangedResourceCount;
		}
	}
	return ChangedResourceCount;
}
