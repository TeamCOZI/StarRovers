#include "Automation/SRFacilityNetworkComponent.h"

#include "SRFacilityNetworkComponentInternal.h"

FSRFacilityPortInventory* USRFacilityNetworkComponent::FindInputPortInventoryForDirectAdd(
	FSRFacilityInstance& FacilityInstance,
	const FSRResourceInstance& ResourceInstance,
	int32 PreferredPortIndex)
{
	if (FacilityInstance.InputPortInventories.IsValidIndex(PreferredPortIndex))
	{
		FSRFacilityPortInventory& PreferredPortInventory = FacilityInstance.InputPortInventories[PreferredPortIndex];
		if (StarRovers::FacilityNetwork::CanInventorySlotAcceptResource(PreferredPortInventory, ResourceInstance))
		{
			return &PreferredPortInventory;
		}
	}

	for (FSRFacilityPortInventory& InputPortInventory : FacilityInstance.InputPortInventories)
	{
		if (StarRovers::FacilityNetwork::GetInventorySlotStackCount(InputPortInventory) > 0
			&& StarRovers::FacilityNetwork::CanInventorySlotAcceptResource(InputPortInventory, ResourceInstance))
		{
			return &InputPortInventory;
		}
	}

	for (FSRFacilityPortInventory& InputPortInventory : FacilityInstance.InputPortInventories)
	{
		if (StarRovers::FacilityNetwork::GetInventorySlotStackCount(InputPortInventory) <= 0
			&& StarRovers::FacilityNetwork::CanInventorySlotAcceptResource(InputPortInventory, ResourceInstance))
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
		? FindInputPortInventoryForDirectAdd(*FacilityInstance, ResourceInstance)
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

	TArray<FSRFacilityPortInventory> SimulatedInputPortInventories = FacilityInstance->InputPortInventories;
	const int32 RequiredStackCount = StarRovers::FacilityNetwork::GetResourceStackCount(ResourceInstance);
	if (StarRovers::FacilityNetwork::TryAddResourceToInventorySlots(SimulatedInputPortInventories, ResourceInstance) != RequiredStackCount)
	{
		return false;
	}

	const int32 AddedStackCount = StarRovers::FacilityNetwork::TryAddResourceToInventorySlots(FacilityInstance->InputPortInventories, ResourceInstance);
	if (AddedStackCount != RequiredStackCount)
	{
		return false;
	}
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
			StarRovers::FacilityNetwork::GetInventorySlotStackCount(*InputPortInventory),
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
	FSRFacilityPortInventory SimulatedInputPortInventory = *InputPortInventory;
	const int32 RequiredStackCount = StarRovers::FacilityNetwork::GetResourceStackCount(ResourceInstance);
	if (StarRovers::FacilityNetwork::TryAddResourceToInventorySlot(SimulatedInputPortInventory, ResourceInstance) != RequiredStackCount)
	{
		return false;
	}

	if (StarRovers::FacilityNetwork::TryAddResourceToInventorySlot(*InputPortInventory, ResourceInstance) != RequiredStackCount)
	{
		return false;
	}
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
			return StarRovers::FacilityNetwork::GetInventorySlotStackCount(CandidatePortInventory) > 0;
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

	if (!StarRovers::FacilityNetwork::TryTakeSingleResourceFromInventorySlot(*OutputPortInventory, OutResourceInstance))
	{
		OutResourceInstance = FSRResourceInstance();
		return false;
	}
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
			StarRovers::FacilityNetwork::GetInventorySlotStackCount(*OutputPortInventory),
			FacilityInstance->OutputInventory.Num(),
			*GetNameSafe(GetOwner()));
	}
	return true;
}

bool USRFacilityNetworkComponent::IsHubFacility(FName OccupantId) const
{
	const FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance ? FacilityInstance->FacilityDataAsset.Get() : nullptr;
	return IsValid(FacilityDataAsset) && FacilityDataAsset->FacilityKind == ESRFacilityKind::Hub;
}

bool USRFacilityNetworkComponent::TryTakeHubOutboundCargo(FName OccupantId, int32 MaxStackCount, FSRResourceInstance& OutCargo)
{
	return TryTakeHubOutboundCargoByResource(OccupantId, NAME_None, MaxStackCount, OutCargo);
}

bool USRFacilityNetworkComponent::TryTakeHubOutboundCargoByResource(FName OccupantId, FName ResourceId, int32 MaxStackCount, FSRResourceInstance& OutCargo)
{
	OutCargo = FSRResourceInstance();
	FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	if (!FacilityInstance || !IsHubFacility(OccupantId) || MaxStackCount <= 0)
	{
		return false;
	}

	for (FSRFacilityPortInventory& InputPortInventory : FacilityInstance->InputPortInventories)
	{
		if (StarRovers::FacilityNetwork::GetInventorySlotStackCount(InputPortInventory) <= 0)
		{
			continue;
		}

		const int32 TakenStackCount = StarRovers::FacilityNetwork::TryTakeResourceStackFromInventorySlot(
			InputPortInventory,
			MaxStackCount,
			OutCargo,
			ResourceId);
		if (TakenStackCount <= 0)
		{
			continue;
		}

		RefreshFacilityAggregateInventories(*FacilityInstance);
		if (bLogFacilityNetworkEvents)
		{
			UE_LOG(
				LogTemp,
				Display,
				TEXT("[FacilityNetwork][Hub] Outbound cargo taken: OccupantId=%s Port=%s ResourceId=%s RequestedResourceId=%s StackCount=%d RemainingPortInput=%d Owner=%s"),
				*OccupantId.ToString(),
				*InputPortInventory.PortId.ToString(),
				*OutCargo.ResourceId.ToString(),
				ResourceId.IsNone() ? TEXT("Any") : *ResourceId.ToString(),
				OutCargo.StackCount,
				StarRovers::FacilityNetwork::GetInventorySlotStackCount(InputPortInventory),
				*GetNameSafe(GetOwner()));
		}
		return true;
	}

	return false;
}

void USRFacilityNetworkComponent::GetHubOutboundCargoResourceIds(FName OccupantId, TArray<FName>& OutResourceIds) const
{
	OutResourceIds.Reset();
	const FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	if (!FacilityInstance || !IsHubFacility(OccupantId))
	{
		return;
	}

	for (const FSRFacilityPortInventory& InputPortInventory : FacilityInstance->InputPortInventories)
	{
		for (const FSRResourceInstance& ResourceInstance : InputPortInventory.Inventory)
		{
			if (ResourceInstance.ResourceId.IsNone() || ResourceInstance.StackCount <= 0)
			{
				continue;
			}

			OutResourceIds.AddUnique(ResourceInstance.ResourceId);
		}
	}
}

bool USRFacilityNetworkComponent::CanStoreHubInboundCargo(FName OccupantId, const FSRResourceInstance& Cargo) const
{
	const FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	if (!FacilityInstance || !IsHubFacility(OccupantId) || Cargo.ResourceId.IsNone() || Cargo.StackCount <= 0)
	{
		return false;
	}

	TArray<FSRFacilityPortInventory> SimulatedOutputPortInventories = FacilityInstance->OutputPortInventories;
	const int32 RequiredStackCount = StarRovers::FacilityNetwork::GetResourceStackCount(Cargo);
	return RequiredStackCount > 0
		&& StarRovers::FacilityNetwork::TryAddResourceToInventorySlots(SimulatedOutputPortInventories, Cargo) == RequiredStackCount;
}

bool USRFacilityNetworkComponent::TryStoreHubInboundCargo(FName OccupantId, const FSRResourceInstance& Cargo)
{
	FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	if (!FacilityInstance || !CanStoreHubInboundCargo(OccupantId, Cargo))
	{
		return false;
	}

	const int32 RequiredStackCount = StarRovers::FacilityNetwork::GetResourceStackCount(Cargo);
	const int32 AddedStackCount = StarRovers::FacilityNetwork::TryAddResourceToInventorySlots(FacilityInstance->OutputPortInventories, Cargo);
	if (AddedStackCount != RequiredStackCount)
	{
		return false;
	}

	RefreshFacilityAggregateInventories(*FacilityInstance);
	if (bLogFacilityNetworkEvents)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[FacilityNetwork][Hub] Inbound cargo stored: OccupantId=%s ResourceId=%s StackCount=%d OutputCount=%d Owner=%s"),
			*OccupantId.ToString(),
			*Cargo.ResourceId.ToString(),
			Cargo.StackCount,
			FacilityInstance->OutputInventory.Num(),
			*GetNameSafe(GetOwner()));
	}
	return true;
}
