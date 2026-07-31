#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityDataAsset.h"
#include "Automation/SRFacilityRuntimeData.h"
#include "Pattern/SRPatternRoutingFilter.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"

namespace StarRovers::FacilityResources
{
	inline FString BuildFacilityCellDebugString(const FSRPlanetSurfaceGridCellId& CellId)
	{
		return FString::Printf(
			TEXT("Face=%d X=%d Y=%d"),
			static_cast<int32>(CellId.Face),
			CellId.CellX,
			CellId.CellY);
	}

	inline FString BuildResourceDebugString(const FSRResourceInstance& ResourceInstance)
	{
		return FString::Printf(
			TEXT("ResourceId=%s SourcePatternId=%s SourcePatternSeed=%d Pattern=%s PatternHash=%u StackCount=%d"),
			*ResourceInstance.ResourceId.ToString(),
			*ResourceInstance.SourcePatternId.ToString(),
			ResourceInstance.SourcePatternSeed,
			*ResourceInstance.Pattern.ToCompactString(),
			ResourceInstance.Pattern.GetStableHash(),
			ResourceInstance.StackCount);
	}

	inline int32 GetResourceStackCount(const FSRResourceInstance& ResourceInstance)
	{
		return FMath::Max(0, ResourceInstance.StackCount);
	}

	inline int32 GetInventorySlotStackCount(const FSRFacilityPortInventory& PortInventory)
	{
		int32 StackCount = 0;
		for (const FSRResourceInstance& ResourceInstance : PortInventory.Inventory)
		{
			StackCount += GetResourceStackCount(ResourceInstance);
		}
		return StackCount;
	}

	inline bool AreResourceInstancesStackEquivalent(const FSRResourceInstance& Left, const FSRResourceInstance& Right)
	{
		return StarRovers::ResourcePatterns::ArePatternPayloadsEquivalent(Left, Right);
	}

	inline bool CanInventorySlotAcceptResource(const FSRFacilityPortInventory& PortInventory, const FSRResourceInstance& ResourceInstance)
	{
		if (!StarRovers::PatternRouting::MatchesRoutingFilter(
			ResourceInstance,
			PortInventory.PortSpec.RoutingFilter))
		{
			return false;
		}

		const int32 ExistingStackCount = GetInventorySlotStackCount(PortInventory);
		if (ExistingStackCount >= FMath::Max(1, PortInventory.Capacity))
		{
			return false;
		}

		if (ExistingStackCount <= 0 || PortInventory.Inventory.IsEmpty())
		{
			return true;
		}

		return AreResourceInstancesStackEquivalent(PortInventory.Inventory[0], ResourceInstance);
	}

	inline int32 TryAddResourceToInventorySlot(FSRFacilityPortInventory& PortInventory, const FSRResourceInstance& ResourceInstance)
	{
		const int32 ResourceStackCount = GetResourceStackCount(ResourceInstance);
		if (ResourceInstance.ResourceId.IsNone() || ResourceStackCount <= 0 || !CanInventorySlotAcceptResource(PortInventory, ResourceInstance))
		{
			return 0;
		}

		const int32 Capacity = FMath::Max(1, PortInventory.Capacity);
		const int32 ExistingStackCount = GetInventorySlotStackCount(PortInventory);
		const int32 AddedStackCount = FMath::Min(ResourceStackCount, Capacity - ExistingStackCount);
		if (AddedStackCount <= 0)
		{
			return 0;
		}

		if (ExistingStackCount <= 0 || PortInventory.Inventory.IsEmpty())
		{
			FSRResourceInstance StoredResource = ResourceInstance;
			StoredResource.StackCount = AddedStackCount;
			PortInventory.Inventory.Reset();
			PortInventory.Inventory.Add(StoredResource);
			return AddedStackCount;
		}

		PortInventory.Inventory[0].StackCount = FMath::Max(0, PortInventory.Inventory[0].StackCount) + AddedStackCount;
		return AddedStackCount;
	}

	inline bool TryTakeSingleResourceFromInventorySlot(FSRFacilityPortInventory& PortInventory, FSRResourceInstance& OutResourceInstance)
	{
		OutResourceInstance = FSRResourceInstance();
		for (int32 ResourceIndex = 0; ResourceIndex < PortInventory.Inventory.Num(); ++ResourceIndex)
		{
			FSRResourceInstance& StoredResource = PortInventory.Inventory[ResourceIndex];
			if (!StarRovers::PatternRouting::IsValidPatternPayload(StoredResource))
			{
				continue;
			}

			OutResourceInstance = StoredResource;
			OutResourceInstance.StackCount = 1;
			--StoredResource.StackCount;
			if (StoredResource.StackCount <= 0)
			{
				PortInventory.Inventory.RemoveAt(ResourceIndex);
			}
			return true;
		}

		PortInventory.Inventory.RemoveAll([](const FSRResourceInstance& ResourceInstance)
		{
			return ResourceInstance.StackCount <= 0;
		});
		return false;
	}

	inline int32 TryTakeResourceStackFromInventorySlot(
		FSRFacilityPortInventory& PortInventory,
		int32 MaxStackCount,
		FSRResourceInstance& OutResourceInstance,
		FName DesiredResourceId = NAME_None)
	{
		OutResourceInstance = FSRResourceInstance();
		const int32 SafeMaxStackCount = FMath::Max(0, MaxStackCount);
		if (SafeMaxStackCount <= 0)
		{
			return 0;
		}

		for (int32 ResourceIndex = 0; ResourceIndex < PortInventory.Inventory.Num(); ++ResourceIndex)
		{
			FSRResourceInstance& StoredResource = PortInventory.Inventory[ResourceIndex];
			const int32 StoredStackCount = GetResourceStackCount(StoredResource);
			if (!StarRovers::PatternRouting::IsValidPatternPayload(StoredResource)
				|| StoredStackCount <= 0)
			{
				continue;
			}
			if (!DesiredResourceId.IsNone() && StoredResource.ResourceId != DesiredResourceId)
			{
				continue;
			}

			const int32 TakenStackCount = FMath::Min(StoredStackCount, SafeMaxStackCount);
			OutResourceInstance = StoredResource;
			OutResourceInstance.StackCount = TakenStackCount;
			StoredResource.StackCount = StoredStackCount - TakenStackCount;
			if (StoredResource.StackCount <= 0)
			{
				PortInventory.Inventory.RemoveAt(ResourceIndex);
			}
			return TakenStackCount;
		}

		PortInventory.Inventory.RemoveAll([](const FSRResourceInstance& ResourceInstance)
		{
			return ResourceInstance.StackCount <= 0 || ResourceInstance.ResourceId.IsNone();
		});
		return 0;
	}

	inline FSRResourceInstance PeekSingleResourceFromInventorySlot(const FSRFacilityPortInventory& PortInventory)
	{
		for (const FSRResourceInstance& ResourceInstance : PortInventory.Inventory)
		{
			if (StarRovers::PatternRouting::IsValidPatternPayload(ResourceInstance))
			{
				FSRResourceInstance Result = ResourceInstance;
				Result.StackCount = 1;
				return Result;
			}
		}
		return FSRResourceInstance();
	}

	inline int32 TryAddResourceToInventorySlots(TArray<FSRFacilityPortInventory>& PortInventories, const FSRResourceInstance& ResourceInstance)
	{
		int32 RemainingStackCount = GetResourceStackCount(ResourceInstance);
		if (RemainingStackCount <= 0
			|| !StarRovers::PatternRouting::IsValidPatternPayload(ResourceInstance))
		{
			return 0;
		}

		int32 AddedStackCount = 0;
		auto TryAddToSlot = [&ResourceInstance, &RemainingStackCount, &AddedStackCount](FSRFacilityPortInventory& PortInventory)
		{
			if (RemainingStackCount <= 0)
			{
				return;
			}

			FSRResourceInstance RemainingResource = ResourceInstance;
			RemainingResource.StackCount = RemainingStackCount;
			const int32 AddedToSlot = TryAddResourceToInventorySlot(PortInventory, RemainingResource);
			RemainingStackCount -= AddedToSlot;
			AddedStackCount += AddedToSlot;
		};

		for (FSRFacilityPortInventory& PortInventory : PortInventories)
		{
			if (GetInventorySlotStackCount(PortInventory) > 0
				&& CanInventorySlotAcceptResource(PortInventory, ResourceInstance))
			{
				TryAddToSlot(PortInventory);
			}
		}

		for (FSRFacilityPortInventory& PortInventory : PortInventories)
		{
			if (GetInventorySlotStackCount(PortInventory) <= 0)
			{
				TryAddToSlot(PortInventory);
			}
		}

		return AddedStackCount;
	}
}
