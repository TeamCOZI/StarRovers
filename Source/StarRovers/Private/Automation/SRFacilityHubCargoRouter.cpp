#include "SRFacilityHubCargoRouter.h"

#include "Automation/SRFacilityDataAsset.h"
#include "SRFacilityResourceOperations.h"
#include "SRFacilityPortInventoryBuilder.h"

bool FSRFacilityHubCargoRouter::IsHubFacility(const FSRFacilityInstance& FacilityInstance)
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	return IsValid(FacilityDataAsset) && FacilityDataAsset->FacilityKind == ESRFacilityKind::Hub;
}

bool FSRFacilityHubCargoRouter::TryTakeOutboundCargo(
	FSRFacilityInstance& FacilityInstance,
	int32 MaxStackCount,
	FName ResourceId,
	FSRResourceInstance& OutCargo,
	FSRFacilityHubCargoTransferResult* OutTransferResult)
{
	OutCargo = FSRResourceInstance();
	if (OutTransferResult)
	{
		*OutTransferResult = FSRFacilityHubCargoTransferResult();
	}

	if (!IsHubFacility(FacilityInstance) || MaxStackCount <= 0)
	{
		return false;
	}

	for (FSRFacilityPortInventory& InputPortInventory : FacilityInstance.InputPortInventories)
	{
		if (StarRovers::FacilityResources::GetInventorySlotStackCount(InputPortInventory) <= 0)
		{
			continue;
		}

		const int32 TakenStackCount = StarRovers::FacilityResources::TryTakeResourceStackFromInventorySlot(
			InputPortInventory,
			MaxStackCount,
			OutCargo,
			ResourceId);
		if (TakenStackCount <= 0)
		{
			continue;
		}

		if (OutTransferResult)
		{
			OutTransferResult->PortId = InputPortInventory.PortId;
			OutTransferResult->RemainingPortStackCount = StarRovers::FacilityResources::GetInventorySlotStackCount(InputPortInventory);
		}
		FSRFacilityPortInventoryBuilder::RefreshAggregateInventories(FacilityInstance);
		return true;
	}

	return false;
}

void FSRFacilityHubCargoRouter::GetOutboundCargoResourceIds(
	const FSRFacilityInstance& FacilityInstance,
	TArray<FName>& OutResourceIds)
{
	OutResourceIds.Reset();
	if (!IsHubFacility(FacilityInstance))
	{
		return;
	}

	TSet<FName> ResourceIdSet;
	ResourceIdSet.Reserve(FacilityInstance.InputPortInventories.Num());
	for (const FSRFacilityPortInventory& InputPortInventory : FacilityInstance.InputPortInventories)
	{
		for (const FSRResourceInstance& ResourceInstance : InputPortInventory.Inventory)
		{
			if (ResourceInstance.ResourceId.IsNone() || ResourceInstance.StackCount <= 0)
			{
				continue;
			}

			if (!ResourceIdSet.Contains(ResourceInstance.ResourceId))
			{
				ResourceIdSet.Add(ResourceInstance.ResourceId);
				OutResourceIds.Add(ResourceInstance.ResourceId);
			}
		}
	}
}

bool FSRFacilityHubCargoRouter::CanStoreInboundCargo(
	const FSRFacilityInstance& FacilityInstance,
	const FSRResourceInstance& Cargo)
{
	if (!IsHubFacility(FacilityInstance) || Cargo.ResourceId.IsNone() || Cargo.StackCount <= 0)
	{
		return false;
	}

	TArray<FSRFacilityPortInventory> SimulatedOutputPortInventories = FacilityInstance.OutputPortInventories;
	const int32 RequiredStackCount = StarRovers::FacilityResources::GetResourceStackCount(Cargo);
	return RequiredStackCount > 0
		&& StarRovers::FacilityResources::TryAddResourceToInventorySlots(SimulatedOutputPortInventories, Cargo) == RequiredStackCount;
}

bool FSRFacilityHubCargoRouter::TryStoreInboundCargo(
	FSRFacilityInstance& FacilityInstance,
	const FSRResourceInstance& Cargo)
{
	if (!CanStoreInboundCargo(FacilityInstance, Cargo))
	{
		return false;
	}

	const int32 RequiredStackCount = StarRovers::FacilityResources::GetResourceStackCount(Cargo);
	const int32 AddedStackCount = StarRovers::FacilityResources::TryAddResourceToInventorySlots(FacilityInstance.OutputPortInventories, Cargo);
	if (AddedStackCount != RequiredStackCount)
	{
		return false;
	}

	FSRFacilityPortInventoryBuilder::RefreshAggregateInventories(FacilityInstance);
	return true;
}
