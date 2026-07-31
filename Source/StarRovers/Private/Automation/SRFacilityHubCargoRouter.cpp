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

bool FSRFacilityHubCargoRouter::TryTakeOutboundCargoMatching(
	FSRFacilityInstance& FacilityInstance,
	int32 MaxStackCount,
	TFunctionRef<bool(const FSRResourceInstance&)> CargoPredicate,
	FSRResourceInstance& OutCargo,
	FSRFacilityHubCargoTransferResult* OutTransferResult)
{
	OutCargo = FSRResourceInstance();
	if (OutTransferResult)
	{
		*OutTransferResult = FSRFacilityHubCargoTransferResult();
	}

	const int32 SafeMaxStackCount = FMath::Max(0, MaxStackCount);
	if (!IsHubFacility(FacilityInstance) || SafeMaxStackCount <= 0)
	{
		return false;
	}

	for (FSRFacilityPortInventory& InputPortInventory : FacilityInstance.InputPortInventories)
	{
		for (int32 ResourceIndex = 0; ResourceIndex < InputPortInventory.Inventory.Num(); ++ResourceIndex)
		{
			FSRResourceInstance& StoredResource = InputPortInventory.Inventory[ResourceIndex];
			const int32 StoredStackCount = StarRovers::FacilityResources::GetResourceStackCount(StoredResource);
			if (!StarRovers::PatternRouting::IsValidPatternPayload(StoredResource)
				|| StoredStackCount <= 0)
			{
				continue;
			}

			FSRResourceInstance CandidateCargo = StoredResource;
			CandidateCargo.StackCount = FMath::Min(StoredStackCount, SafeMaxStackCount);
			if (!CargoPredicate(CandidateCargo))
			{
				continue;
			}

			const int32 TakenStackCount = CandidateCargo.StackCount;
			OutCargo = CandidateCargo;
			StoredResource.StackCount = StoredStackCount - TakenStackCount;
			if (StoredResource.StackCount <= 0)
			{
				InputPortInventory.Inventory.RemoveAt(ResourceIndex);
			}

			if (OutTransferResult)
			{
				OutTransferResult->PortId = InputPortInventory.PortId;
				OutTransferResult->RemainingPortStackCount =
					StarRovers::FacilityResources::GetInventorySlotStackCount(InputPortInventory);
			}
			FSRFacilityPortInventoryBuilder::RefreshAggregateInventories(FacilityInstance);
			return true;
		}

		InputPortInventory.Inventory.RemoveAll([](const FSRResourceInstance& ResourceInstance)
		{
			return ResourceInstance.StackCount <= 0 || ResourceInstance.ResourceId.IsNone();
		});
	}

	return false;
}

bool FSRFacilityHubCargoRouter::TryTakeOutboundCargoMatchingFromInputPort(
	FSRFacilityInstance& FacilityInstance,
	int32 InputPortIndex,
	int32 MaxStackCount,
	TFunctionRef<bool(const FSRResourceInstance&)> CargoPredicate,
	FSRResourceInstance& OutCargo,
	FSRFacilityHubCargoTransferResult* OutTransferResult)
{
	OutCargo = FSRResourceInstance();
	if (OutTransferResult)
	{
		*OutTransferResult = FSRFacilityHubCargoTransferResult();
	}

	const int32 SafeMaxStackCount = FMath::Max(0, MaxStackCount);
	if (!IsHubFacility(FacilityInstance)
		|| SafeMaxStackCount <= 0
		|| !FacilityInstance.InputPortInventories.IsValidIndex(InputPortIndex))
	{
		return false;
	}

	FSRFacilityPortInventory& InputPortInventory = FacilityInstance.InputPortInventories[InputPortIndex];
	for (int32 ResourceIndex = 0; ResourceIndex < InputPortInventory.Inventory.Num(); ++ResourceIndex)
	{
		FSRResourceInstance& StoredResource = InputPortInventory.Inventory[ResourceIndex];
		const int32 StoredStackCount = StarRovers::FacilityResources::GetResourceStackCount(StoredResource);
		if (!StarRovers::PatternRouting::IsValidPatternPayload(StoredResource)
			|| StoredStackCount <= 0)
		{
			continue;
		}

		FSRResourceInstance CandidateCargo = StoredResource;
		CandidateCargo.StackCount = FMath::Min(StoredStackCount, SafeMaxStackCount);
		if (!CargoPredicate(CandidateCargo))
		{
			continue;
		}

		const int32 TakenStackCount = CandidateCargo.StackCount;
		OutCargo = CandidateCargo;
		StoredResource.StackCount = StoredStackCount - TakenStackCount;
		if (StoredResource.StackCount <= 0)
		{
			InputPortInventory.Inventory.RemoveAt(ResourceIndex);
		}

		if (OutTransferResult)
		{
			OutTransferResult->PortId = InputPortInventory.PortId;
			OutTransferResult->RemainingPortStackCount =
				StarRovers::FacilityResources::GetInventorySlotStackCount(InputPortInventory);
		}
		FSRFacilityPortInventoryBuilder::RefreshAggregateInventories(FacilityInstance);
		return true;
	}

	InputPortInventory.Inventory.RemoveAll([](const FSRResourceInstance& ResourceInstance)
	{
		return ResourceInstance.StackCount <= 0 || ResourceInstance.ResourceId.IsNone();
	});
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
			const FName ResourceId = ResourceInstance.ResourceId;
			if (!StarRovers::PatternRouting::IsValidPatternPayload(ResourceInstance))
			{
				continue;
			}

			bool bAlreadyAdded = false;
			ResourceIdSet.Add(ResourceId, &bAlreadyAdded);
			if (!bAlreadyAdded)
			{
				OutResourceIds.Add(ResourceId);
			}
		}
	}
}

bool FSRFacilityHubCargoRouter::CanStoreInboundCargo(
	const FSRFacilityInstance& FacilityInstance,
	const FSRResourceInstance& Cargo)
{
	if (!IsHubFacility(FacilityInstance)
		|| !StarRovers::PatternRouting::IsValidPatternPayload(Cargo))
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
