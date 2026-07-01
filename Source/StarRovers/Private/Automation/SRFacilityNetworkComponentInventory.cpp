#include "Automation/SRFacilityNetworkComponent.h"

FSRFacilityPortInventory* USRFacilityNetworkComponent::FindInputPortInventoryForDirectAdd(FSRFacilityInstance& FacilityInstance, int32 PreferredPortIndex)
{
	if (FacilityInstance.InputPortInventories.IsValidIndex(PreferredPortIndex))
	{
		FSRFacilityPortInventory& PreferredPortInventory = FacilityInstance.InputPortInventories[PreferredPortIndex];
		if (PreferredPortInventory.Inventory.Num() < FMath::Max(1, PreferredPortInventory.Capacity))
		{
			return &PreferredPortInventory;
		}
	}

	for (FSRFacilityPortInventory& InputPortInventory : FacilityInstance.InputPortInventories)
	{
		if (InputPortInventory.Inventory.IsEmpty()
			&& InputPortInventory.Inventory.Num() < FMath::Max(1, InputPortInventory.Capacity))
		{
			return &InputPortInventory;
		}
	}

	for (FSRFacilityPortInventory& InputPortInventory : FacilityInstance.InputPortInventories)
	{
		if (InputPortInventory.Inventory.Num() < FMath::Max(1, InputPortInventory.Capacity))
		{
			return &InputPortInventory;
		}
	}

	return nullptr;
}

bool USRFacilityNetworkComponent::AddInputResource(FName OccupantId, const FSRResourceInstance& ResourceInstance)
{
	FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	FSRFacilityPortInventory* InputPortInventory = FacilityInstance
		? FindInputPortInventoryForDirectAdd(*FacilityInstance)
		: nullptr;
	if (!FacilityInstance || !InputPortInventory || ResourceInstance.ResourceId.IsNone() || ResourceInstance.StackCount <= 0)
	{
		if (bLogFacilityNetworkEvents)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[FacilityNetwork] AddInputResource failed: OccupantId=%s ResourceId=%s StackCount=%d Owner=%s Reason=InvalidInputOrFacility"),
				*OccupantId.ToString(),
				*ResourceInstance.ResourceId.ToString(),
				ResourceInstance.StackCount,
				*GetNameSafe(GetOwner()));
		}
		return false;
	}

	InputPortInventory->Inventory.Add(ResourceInstance);
	RefreshFacilityAggregateInventories(*FacilityInstance);
	SetComponentTickEnabled(bAutoProcessFacilities);
	if (bLogFacilityNetworkEvents)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[FacilityNetwork] Input added: OccupantId=%s Port=%s ResourceId=%s StackCount=%d PortInputCount=%d InputCount=%d Owner=%s"),
			*OccupantId.ToString(),
			*InputPortInventory->PortId.ToString(),
			*ResourceInstance.ResourceId.ToString(),
			ResourceInstance.StackCount,
			InputPortInventory->Inventory.Num(),
			FacilityInstance->InputInventory.Num(),
			*GetNameSafe(GetOwner()));
	}
	return true;
}

bool USRFacilityNetworkComponent::AddInputResourceToPort(FName OccupantId, int32 InputPortIndex, const FSRResourceInstance& ResourceInstance)
{
	FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	FSRFacilityPortInventory* InputPortInventory = FacilityInstance && FacilityInstance->InputPortInventories.IsValidIndex(InputPortIndex)
		? &FacilityInstance->InputPortInventories[InputPortIndex]
		: nullptr;
	if (!FacilityInstance || !InputPortInventory || ResourceInstance.ResourceId.IsNone() || ResourceInstance.StackCount <= 0)
	{
		return false;
	}
	if (InputPortInventory->Inventory.Num() >= FMath::Max(1, InputPortInventory->Capacity))
	{
		return false;
	}

	InputPortInventory->Inventory.Add(ResourceInstance);
	RefreshFacilityAggregateInventories(*FacilityInstance);
	SetComponentTickEnabled(bAutoProcessFacilities);
	return true;
}

bool USRFacilityNetworkComponent::ExtractOutputResource(FName OccupantId, FSRResourceInstance& OutResourceInstance)
{
	FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	FSRFacilityPortInventory* OutputPortInventory = nullptr;
	if (FacilityInstance)
	{
		OutputPortInventory = FacilityInstance->OutputPortInventories.FindByPredicate([](const FSRFacilityPortInventory& CandidatePortInventory)
		{
			return !CandidatePortInventory.Inventory.IsEmpty();
		});
	}

	if (!FacilityInstance || !OutputPortInventory)
	{
		OutResourceInstance = FSRResourceInstance();
		if (bLogFacilityNetworkEvents)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[FacilityNetwork] ExtractOutputResource failed: OccupantId=%s Owner=%s Reason=MissingFacilityOrEmptyOutput"),
				*OccupantId.ToString(),
				*GetNameSafe(GetOwner()));
		}
		return false;
	}

	OutResourceInstance = OutputPortInventory->Inventory[0];
	OutputPortInventory->Inventory.RemoveAt(0);
	RefreshFacilityAggregateInventories(*FacilityInstance);
	if (bLogFacilityNetworkEvents)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[FacilityNetwork] Output extracted: OccupantId=%s Port=%s ResourceId=%s RemainingPortOutput=%d RemainingOutputCount=%d Owner=%s"),
			*OccupantId.ToString(),
			*OutputPortInventory->PortId.ToString(),
			*OutResourceInstance.ResourceId.ToString(),
			OutputPortInventory->Inventory.Num(),
			FacilityInstance->OutputInventory.Num(),
			*GetNameSafe(GetOwner()));
	}
	return true;
}
