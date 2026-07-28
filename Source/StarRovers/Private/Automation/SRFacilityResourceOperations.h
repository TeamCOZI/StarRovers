#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityDataAsset.h"
#include "Automation/SRFacilityRuntimeData.h"
#include "Automation/SRResourceInstanceOperations.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"

namespace StarRovers::FacilityResources
{
	constexpr int32 HalfLifeDefaultCycles = 3;
	constexpr int32 ChargeStacksPerProcessingSecond = 1;
	constexpr int32 ChargeRequiredStacks = 5;
	constexpr double ChargeEnergyBonus = 3.0;

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
		const UEnum* ResourceClassEnum = StaticEnum<ESRResourceClass>();
		const UEnum* ResourceFamilyEnum = StaticEnum<ESRResourceFamily>();
		const UEnum* ResourceSpectrumEnum = StaticEnum<ESRResourceSpectrum>();
		const UEnum* SlotLifecycleEnum = StaticEnum<ESRResourceSlotLifecycle>();
		return FString::Printf(
			TEXT("ResourceId=%s Schema=%d Class=%s Family=%s CurrentEnergy=%.3f Seed=%s Spectrum=%s Grade=%d StateFlags=0x%X ProcessTag=%s/%s/%d Imprint=%s LegacyEnergy=%.3f RemainingProcessLimit=%d ProcessCount=%d EnergyChangeCount=%d StackCount=%d LegacyTags=%d"),
			*ResourceInstance.ResourceId.ToString(),
			ResourceInstance.ResourceSchemaVersion,
			ResourceClassEnum ? *ResourceClassEnum->GetNameStringByValue(static_cast<int64>(ResourceInstance.ResourceClass)) : TEXT("Unknown"),
			ResourceFamilyEnum ? *ResourceFamilyEnum->GetNameStringByValue(static_cast<int64>(ResourceInstance.Family)) : TEXT("None"),
			ResourceInstance.CurrentEnergy,
			ResourceInstance.bHasSeedEnergySnapshot
				? *FString::Printf(TEXT("%.3f"), ResourceInstance.SeedEnergySnapshot)
				: TEXT("Unset"),
			ResourceSpectrumEnum ? *ResourceSpectrumEnum->GetNameStringByValue(static_cast<int64>(ResourceInstance.Spectrum)) : TEXT("None"),
			ResourceInstance.Grade,
			ResourceInstance.ActiveFamilyStateFlags,
			*ResourceInstance.ProcessTagSlot.TagId.ToString(),
			SlotLifecycleEnum ? *SlotLifecycleEnum->GetNameStringByValue(static_cast<int64>(ResourceInstance.ProcessTagSlot.Lifecycle)) : TEXT("Unknown"),
			ResourceInstance.ProcessTagSlot.RemainingTriggers,
			*ResourceInstance.FuelImprintSlot.ImprintId.ToString(),
			ResourceInstance.EnergyValue,
			ResourceInstance.RemainingProcessLimit,
			ResourceInstance.ProcessCount,
			ResourceInstance.EnergyChangeCount,
			ResourceInstance.StackCount,
			ResourceInstance.Tags.Num());
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

	inline bool AreResourceTagStacksEquivalent(const TArray<FSRResourceTagStack>& LeftTags, const TArray<FSRResourceTagStack>& RightTags)
	{
		TArray<const FSRResourceTagStack*> LeftValidTags;
		TArray<const FSRResourceTagStack*> RightValidTags;
		for (const FSRResourceTagStack& LeftTag : LeftTags)
		{
			if (LeftTag.StackCount > 0)
			{
				LeftValidTags.Add(&LeftTag);
			}
		}
		for (const FSRResourceTagStack& RightTag : RightTags)
		{
			if (RightTag.StackCount > 0)
			{
				RightValidTags.Add(&RightTag);
			}
		}

		if (LeftValidTags.Num() != RightValidTags.Num())
		{
			return false;
		}

		for (int32 TagIndex = 0; TagIndex < LeftValidTags.Num(); ++TagIndex)
		{
			const FSRResourceTagStack& LeftTag = *LeftValidTags[TagIndex];
			const FSRResourceTagStack& RightTag = *RightValidTags[TagIndex];
			if (LeftTag.Tag != RightTag.Tag
				|| LeftTag.StackCount != RightTag.StackCount
				|| LeftTag.RemainingCycles != RightTag.RemainingCycles)
			{
				return false;
			}
		}

		return true;
	}

	inline bool AreResourceInstancesStackEquivalent(const FSRResourceInstance& Left, const FSRResourceInstance& Right)
	{
		return Left.ResourceId == Right.ResourceId
			&& Left.ResourceDataAsset == Right.ResourceDataAsset
			&& FMath::IsNearlyEqual(Left.EnergyValue, Right.EnergyValue)
			&& Left.RemainingProcessLimit == Right.RemainingProcessLimit
			&& Left.ProcessCount == Right.ProcessCount
			&& Left.EnergyChangeCount == Right.EnergyChangeCount
			&& AreResourceTagStacksEquivalent(Left.Tags, Right.Tags)
			&& StarRovers::Resources::AreResourceV2RuntimeFieldsEquivalent(Left, Right);
	}

	inline bool CanInventorySlotAcceptResource(const FSRFacilityPortInventory& PortInventory, const FSRResourceInstance& ResourceInstance)
	{
		FSRResourceInstance NormalizedResource = ResourceInstance;
		StarRovers::Resources::UpgradeResourceInstanceToCurrentSchema(NormalizedResource);
		const int32 ExistingStackCount = GetInventorySlotStackCount(PortInventory);
		if (ExistingStackCount >= FMath::Max(1, PortInventory.Capacity))
		{
			return false;
		}

		if (ExistingStackCount <= 0 || PortInventory.Inventory.IsEmpty())
		{
			return true;
		}

		return AreResourceInstancesStackEquivalent(PortInventory.Inventory[0], NormalizedResource);
	}

	inline int32 TryAddResourceToInventorySlot(FSRFacilityPortInventory& PortInventory, const FSRResourceInstance& ResourceInstance)
	{
		FSRResourceInstance NormalizedResource = ResourceInstance;
		StarRovers::Resources::UpgradeResourceInstanceToCurrentSchema(NormalizedResource);
		const int32 ResourceStackCount = GetResourceStackCount(NormalizedResource);
		if (NormalizedResource.ResourceId.IsNone() || ResourceStackCount <= 0 || !CanInventorySlotAcceptResource(PortInventory, NormalizedResource))
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
			FSRResourceInstance StoredResource = NormalizedResource;
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
			if (StoredResource.StackCount <= 0)
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
			if (StoredResource.ResourceId.IsNone() || StoredStackCount <= 0)
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
			if (ResourceInstance.StackCount > 0)
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
		if (RemainingStackCount <= 0 || ResourceInstance.ResourceId.IsNone())
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
