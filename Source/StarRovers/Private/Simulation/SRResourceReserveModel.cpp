#include "Simulation/SRResourceReserveModel.h"

#include "Automation/SRResourceSystemContent.h"

namespace
{
	struct FResourceIdentity
	{
		ESRResourceClass ResourceClass = ESRResourceClass::Unknown;
		ESRResourceFamily Family = ESRResourceFamily::None;
	};

	int64 SaturatingAdd(int64 Left, int64 Right)
	{
		return Right > 0 && Left > TNumericLimits<int64>::Max() - Right
			? TNumericLimits<int64>::Max()
			: Left + Right;
	}

	void BuildResourceIdentityMap(TMap<FName, FResourceIdentity>& OutIdentities)
	{
		OutIdentities.Reset();
		TArray<FSRReferenceResourceDefinitionV2> CardDefinitions;
		FSRResourceSystemContent::GetAllReferenceResourceDefinitions(CardDefinitions);
		for (const FSRReferenceResourceDefinitionV2& Definition : CardDefinitions)
		{
			FResourceIdentity& Identity = OutIdentities.FindOrAdd(Definition.ResourceId);
			Identity.ResourceClass = ESRResourceClass::Card;
			Identity.Family = Definition.Family;
		}

		TArray<FSRUtilityResourceDefinitionV2> UtilityDefinitions;
		FSRResourceSystemContent::GetAllUtilityResourceDefinitions(UtilityDefinitions);
		for (const FSRUtilityResourceDefinitionV2& Definition : UtilityDefinitions)
		{
			FResourceIdentity& Identity = OutIdentities.FindOrAdd(Definition.ResourceId);
			Identity.ResourceClass = ESRResourceClass::Utility;
			Identity.Family = ESRResourceFamily::None;
		}
	}

	const FSRResourceReserveEntry* FindEntry(
		const FSRResourceReserveSnapshot& Snapshot,
		FName ResourceId)
	{
		return Snapshot.Entries.FindByPredicate(
			[ResourceId](const FSRResourceReserveEntry& Entry)
			{
				return Entry.ResourceId == ResourceId;
			});
	}

	void ResolvePotentialPair(
		const FSRResourceReserveEntry* First,
		const FSRResourceReserveEntry* Second,
		int64& OutCount,
		bool& bOutInfinite)
	{
		OutCount = 0;
		bOutInfinite = false;
		if (!First || !Second)
		{
			return;
		}
		if (First->bHasInfiniteRemaining && Second->bHasInfiniteRemaining)
		{
			bOutInfinite = true;
			OutCount = TNumericLimits<int64>::Max();
			return;
		}
		const int64 FirstAmount = First->bHasInfiniteRemaining
			? TNumericLimits<int64>::Max()
			: First->RemainingAmount;
		const int64 SecondAmount = Second->bHasInfiniteRemaining
			? TNumericLimits<int64>::Max()
			: Second->RemainingAmount;
		OutCount = FMath::Min(FirstAmount, SecondAmount);
	}
}

FSRResourceReserveSnapshot FSRResourceReserveModel::BuildSnapshot(
	const TArray<FSRResourceDepositInstance>& ResourceDeposits)
{
	FSRResourceReserveSnapshot Snapshot;
	TMap<FName, FResourceIdentity> Identities;
	BuildResourceIdentityMap(Identities);
	TMap<FName, int32> EntryIndices;

	for (const FSRResourceDepositInstance& Deposit : ResourceDeposits)
	{
		FName ResourceId = Deposit.ResourceId;
		if (ResourceId.IsNone() && IsValid(Deposit.ResourceDataAsset.Get()))
		{
			ResourceId = Deposit.ResourceDataAsset->ResourceId;
		}
		if (ResourceId.IsNone())
		{
			continue;
		}

		int32* ExistingIndex = EntryIndices.Find(ResourceId);
		if (!ExistingIndex)
		{
			const int32 NewIndex = Snapshot.Entries.AddDefaulted();
			EntryIndices.Add(ResourceId, NewIndex);
			ExistingIndex = EntryIndices.Find(ResourceId);
			FSRResourceReserveEntry& NewEntry = Snapshot.Entries[NewIndex];
			NewEntry.ResourceId = ResourceId;
			if (const FResourceIdentity* Identity = Identities.Find(ResourceId))
			{
				NewEntry.ResourceClass = Identity->ResourceClass;
				NewEntry.Family = Identity->Family;
			}
			else if (IsValid(Deposit.ResourceDataAsset.Get()))
			{
				NewEntry.ResourceClass = Deposit.ResourceDataAsset->ResourceClass;
				NewEntry.Family = Deposit.ResourceDataAsset->Family;
			}
		}

		FSRResourceReserveEntry& Entry = Snapshot.Entries[*ExistingIndex];
		const bool bInfiniteTotal =
			FSRResourceDepositAmountModel::IsInfinite(Deposit.TotalAmount);
		const bool bInfiniteRemaining =
			FSRResourceDepositAmountModel::IsInfinite(Deposit.RemainingAmount);
		const int64 SafeTotal = bInfiniteTotal
			? 0
			: static_cast<int64>(FMath::Max(0, Deposit.TotalAmount));
		const int64 SafeRemaining = bInfiniteRemaining
			? 0
			: FMath::Clamp(
				static_cast<int64>(Deposit.RemainingAmount),
				static_cast<int64>(0),
				SafeTotal);

		++Entry.DepositCount;
		++Snapshot.DepositCount;
		const bool bActive = bInfiniteRemaining || SafeRemaining > 0;
		Entry.ActiveDepositCount += bActive ? 1 : 0;
		Entry.DepletedDepositCount += bActive ? 0 : 1;
		Snapshot.ActiveDepositCount += bActive ? 1 : 0;
		Snapshot.DepletedDepositCount += bActive ? 0 : 1;
		Entry.bHasInfiniteDeposit |= bInfiniteTotal;
		Entry.bHasInfiniteRemaining |= bInfiniteRemaining;
		Snapshot.InfiniteDepositCount += bInfiniteTotal ? 1 : 0;
		Entry.TotalAmount = SaturatingAdd(Entry.TotalAmount, SafeTotal);
		Entry.RemainingAmount = SaturatingAdd(Entry.RemainingAmount, SafeRemaining);
		Snapshot.TotalFiniteAmount = SaturatingAdd(Snapshot.TotalFiniteAmount, SafeTotal);
		Snapshot.RemainingFiniteAmount = SaturatingAdd(
			Snapshot.RemainingFiniteAmount,
			SafeRemaining);
		if (Entry.ResourceClass == ESRResourceClass::Card)
		{
			Snapshot.RemainingCardAmount = SaturatingAdd(
				Snapshot.RemainingCardAmount,
				SafeRemaining);
		}
		else if (Entry.ResourceClass == ESRResourceClass::Utility)
		{
			Snapshot.RemainingUtilityAmount = SaturatingAdd(
				Snapshot.RemainingUtilityAmount,
				SafeRemaining);
		}
	}

	Snapshot.bHasDeposits = Snapshot.DepositCount > 0;
	Snapshot.Entries.Sort(
		[](const FSRResourceReserveEntry& Left, const FSRResourceReserveEntry& Right)
		{
			return Left.ResourceId.LexicalLess(Right.ResourceId);
		});

	TArray<FSRReferenceResourceDefinitionV2> CardDefinitions;
	FSRResourceSystemContent::GetAllReferenceResourceDefinitions(CardDefinitions);
	int64 MinimumRemaining = TNumericLimits<int64>::Max();
	int64 MinimumTotal = TNumericLimits<int64>::Max();
	bool bAllRemainingInfinite = !CardDefinitions.IsEmpty();
	bool bAllTotalInfinite = !CardDefinitions.IsEmpty();
	for (const FSRReferenceResourceDefinitionV2& Definition : CardDefinitions)
	{
		const FSRResourceReserveEntry* Entry = FindEntry(Snapshot, Definition.ResourceId);
		const bool bRemainingInfinite = Entry && Entry->bHasInfiniteRemaining;
		const bool bTotalInfinite = Entry && Entry->bHasInfiniteDeposit;
		const int64 Remaining = Entry
			? (bRemainingInfinite ? TNumericLimits<int64>::Max() : Entry->RemainingAmount)
			: 0;
		const int64 Total = Entry
			? (bTotalInfinite ? TNumericLimits<int64>::Max() : Entry->TotalAmount)
			: 0;
		if (Remaining > 0)
		{
			++Snapshot.CoveredReferenceCardTypeCount;
		}
		bAllRemainingInfinite &= bRemainingInfinite;
		bAllTotalInfinite &= bTotalInfinite;
		if (!bRemainingInfinite && Remaining < MinimumRemaining)
		{
			MinimumRemaining = Remaining;
			Snapshot.LimitingReferenceCardId = Definition.ResourceId;
		}
		if (!bTotalInfinite)
		{
			MinimumTotal = FMath::Min(MinimumTotal, Total);
		}
	}
	Snapshot.bPotentialFuelBatchesInfinite = bAllRemainingInfinite;
	Snapshot.PotentialFuelBatchCount = bAllRemainingInfinite
		? TNumericLimits<int64>::Max()
		: (MinimumRemaining == TNumericLimits<int64>::Max() ? 0 : MinimumRemaining);
	Snapshot.PotentialTotalFuelBatchCount = bAllTotalInfinite
		? TNumericLimits<int64>::Max()
		: (MinimumTotal == TNumericLimits<int64>::Max() ? 0 : MinimumTotal);

	FSRUtilityResourceDefinitionV2 CommonOre;
	FSRUtilityResourceDefinitionV2 Biomass;
	const FSRResourceReserveEntry* CommonOreEntry =
		FSRResourceSystemContent::TryGetUtilityResourceDefinition(
			ESRResourceContentPresetV2::CommonOre,
			CommonOre)
		? FindEntry(Snapshot, CommonOre.ResourceId)
		: nullptr;
	const FSRResourceReserveEntry* BiomassEntry =
		FSRResourceSystemContent::TryGetUtilityResourceDefinition(
			ESRResourceContentPresetV2::BiomassFeedstock,
			Biomass)
		? FindEntry(Snapshot, Biomass.ResourceId)
		: nullptr;
	ResolvePotentialPair(
		CommonOreEntry,
		BiomassEntry,
		Snapshot.PotentialIndustrialSupplyCycleCount,
		Snapshot.bPotentialIndustrialSupplyCyclesInfinite);

	if (Snapshot.TotalFiniteAmount > 0)
	{
		Snapshot.RemainingRatio = FMath::Clamp(
			static_cast<float>(Snapshot.RemainingFiniteAmount)
				/ static_cast<float>(Snapshot.TotalFiniteAmount),
			0.0f,
			1.0f);
	}
	else if (Snapshot.InfiniteDepositCount > 0)
	{
		Snapshot.RemainingRatio = 1.0f;
	}
	Snapshot.Pressure = ResolvePressure(Snapshot.bHasDeposits, Snapshot.RemainingRatio);
	return Snapshot;
}

ESRResourceReservePressure FSRResourceReserveModel::ResolvePressure(
	bool bHasDeposits,
	float RemainingRatio)
{
	if (!bHasDeposits)
	{
		return ESRResourceReservePressure::Unavailable;
	}
	const float SafeRatio = FMath::Clamp(
		FMath::IsFinite(RemainingRatio) ? RemainingRatio : 0.0f,
		0.0f,
		1.0f);
	if (SafeRatio <= KINDA_SMALL_NUMBER)
	{
		return ESRResourceReservePressure::Depleted;
	}
	if (SafeRatio <= 0.10f)
	{
		return ESRResourceReservePressure::Critical;
	}
	if (SafeRatio <= 0.25f)
	{
		return ESRResourceReservePressure::Low;
	}
	return ESRResourceReservePressure::Healthy;
}

FString FSRResourceReserveModel::BuildPressureLabel(
	ESRResourceReservePressure Pressure)
{
	switch (Pressure)
	{
	case ESRResourceReservePressure::Healthy:
		return TEXT("HEALTHY");
	case ESRResourceReservePressure::Low:
		return TEXT("LOW");
	case ESRResourceReservePressure::Critical:
		return TEXT("CRITICAL");
	case ESRResourceReservePressure::Depleted:
		return TEXT("DEPLETED");
	case ESRResourceReservePressure::Unavailable:
	default:
		return TEXT("N/A");
	}
}
