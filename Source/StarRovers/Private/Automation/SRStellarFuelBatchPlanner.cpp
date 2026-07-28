#include "Automation/SRStellarFuelBatchPlanner.h"

#include "Automation/SRFacilityRuntimeData.h"
#include "Automation/SRStellarFuelFabricator.h"
#include "SRFacilityResourceOperations.h"

namespace
{
	int32 BuildCardKey(ESRResourceSpectrum Spectrum, int32 Grade)
	{
		return (static_cast<int32>(Spectrum) << 8) | (Grade & 0xFF);
	}

	ESRStellarFuelHandV2 ResolveCurrentPattern(const TMap<int32, int32>& UniqueKeysPerGrade)
	{
		int32 PairGradeCount = 0;
		bool bHasThree = false;
		bool bHasFour = false;
		bool bHasFiveGradeSequence = UniqueKeysPerGrade.Num()
			== StarRovers::StellarFuel::RequiredCardCount;
		for (int32 Grade = StarRovers::Resources::MinimumGrade;
			Grade <= StarRovers::Resources::MaximumGrade;
			++Grade)
		{
			const int32 Count = UniqueKeysPerGrade.FindRef(Grade);
			bHasFiveGradeSequence &= Count > 0;
			PairGradeCount += Count >= 2 ? 1 : 0;
			bHasThree |= Count >= 3;
			bHasFour |= Count >= 4;
		}

		if (bHasFour)
		{
			return ESRStellarFuelHandV2::FourOfAKind;
		}
		if (bHasThree && PairGradeCount >= 2)
		{
			return ESRStellarFuelHandV2::FullHouse;
		}
		if (bHasFiveGradeSequence)
		{
			return ESRStellarFuelHandV2::FiveGradeSequence;
		}
		if (bHasThree)
		{
			return ESRStellarFuelHandV2::ThreeOfAKind;
		}
		if (PairGradeCount >= 2)
		{
			return ESRStellarFuelHandV2::TwoPair;
		}
		if (PairGradeCount == 1)
		{
			return ESRStellarFuelHandV2::OnePair;
		}
		return ESRStellarFuelHandV2::Unranked;
	}

	FString GetHandLabel(ESRStellarFuelHandV2 Hand)
	{
		const UEnum* HandEnum = StaticEnum<ESRStellarFuelHandV2>();
		return HandEnum
			? HandEnum->GetDisplayNameTextByValue(static_cast<int64>(Hand)).ToString()
			: TEXT("Unranked");
	}

	FString GetCardKeyLabel(const FSRResourceInstance& Card)
	{
		const TCHAR* SpectrumPrefix = TEXT("?");
		switch (Card.Spectrum)
		{
		case ESRResourceSpectrum::Red: SpectrumPrefix = TEXT("R"); break;
		case ESRResourceSpectrum::Green: SpectrumPrefix = TEXT("G"); break;
		case ESRResourceSpectrum::Blue: SpectrumPrefix = TEXT("B"); break;
		case ESRResourceSpectrum::Yellow: SpectrumPrefix = TEXT("Y"); break;
		case ESRResourceSpectrum::None:
		default: break;
		}
		return FString::Printf(TEXT("%s%d"), SpectrumPrefix, Card.Grade);
	}

	FString BuildLaneNumberList(const TArray<int32>& ZeroBasedLaneIndices)
	{
		TArray<FString> Labels;
		Labels.Reserve(ZeroBasedLaneIndices.Num());
		for (const int32 LaneIndex : ZeroBasedLaneIndices)
		{
			Labels.Add(FString::FromInt(LaneIndex + 1));
		}
		return FString::Join(Labels, TEXT(", "));
	}

	void BuildPlayerFacingText(FSRStellarFuelBatchStatusV2& Status)
	{
		const FString HandLabel = GetHandLabel(Status.Hand);
		if (Status.State == ESRStellarFuelBatchStateV2::Contaminated)
		{
			const int32 FirstLane = Status.ContaminatedLaneIndices.IsEmpty()
				? INDEX_NONE
				: Status.ContaminatedLaneIndices[0];
			const FString FirstReason = Status.Slots.IsValidIndex(FirstLane)
				? Status.Slots[FirstLane].FailureReason
				: TEXT("Reserved batch layout is invalid.");
			Status.Summary = FirstLane == INDEX_NONE
				? TEXT("BATCH CONTAMINATED | Invalid reserved batch")
				: FString::Printf(
					TEXT("BATCH CONTAMINATED | Lane %d blocked"),
					FirstLane + 1);
			Status.Detail = FString::Printf(
				TEXT("BLOCKED LANES | %s\n%s\nInput was preserved; clear or reroute the blocked lane."),
				*BuildLaneNumberList(Status.ContaminatedLaneIndices),
				*FirstReason);
			return;
		}

		if (Status.State == ESRStellarFuelBatchStateV2::Empty)
		{
			Status.Summary = TEXT("BATCH 0/5 | Connect five Card lanes");
		}
		else if (Status.State == ESRStellarFuelBatchStateV2::Collecting)
		{
			Status.Summary = FString::Printf(
				TEXT("BATCH %d/5 | Current: %s | Empty lanes: %s"),
				Status.ValidCardCount,
				*HandLabel,
				*BuildLaneNumberList(Status.EmptyLaneIndices));
		}
		else
		{
			Status.Summary = FString::Printf(
				TEXT("%s 5/5 | %s | Fuel E %.1f"),
				Status.State == ESRStellarFuelBatchStateV2::Reserved ? TEXT("RESERVED") : TEXT("READY"),
				*HandLabel,
				Status.FinalPreview.FuelEnergy);
		}

		TArray<FString> SlotLabels;
		SlotLabels.Reserve(Status.Slots.Num());
		for (const FSRStellarFuelBatchSlotV2& Slot : Status.Slots)
		{
			FString Value = TEXT("EMPTY");
			if (Slot.bOccupied)
			{
				Value = Slot.bValidCard ? GetCardKeyLabel(Slot.Resource) : TEXT("BLOCKED");
				if (Slot.bDuplicateCardKey)
				{
					Value += TEXT("*");
				}
			}
			SlotLabels.Add(FString::Printf(TEXT("%d:%s"), Slot.LaneIndex + 1, *Value));
		}

		Status.Detail = FString::Printf(TEXT("CARD LANES | %s"), *FString::Join(SlotLabels, TEXT("  ")));
		if (Status.DuplicateCardKeyCount > 0)
		{
			Status.Detail += FString::Printf(
				TEXT("\nDUPLICATE * | %d Card%s add Energy but score once"),
				Status.DuplicateCardKeyCount,
				Status.DuplicateCardKeyCount == 1 ? TEXT("") : TEXT("s"));
		}
		if (Status.bHasFinalPreview)
		{
			Status.Detail += FString::Printf(
				TEXT("\nFINAL | A %.1f + B %.1f x C %.1f = %.1f"),
				Status.FinalPreview.FormulaA,
				Status.FinalPreview.FormulaB,
				Status.FinalPreview.FormulaC,
				Status.FinalPreview.FuelEnergy);
		}
		else if (!Status.EmptyLaneIndices.IsEmpty())
		{
			Status.Detail += FString::Printf(
				TEXT("\nWAITING | Fill lane%s %s"),
				Status.EmptyLaneIndices.Num() == 1 ? TEXT("") : TEXT("s"),
				*BuildLaneNumberList(Status.EmptyLaneIndices));
		}
	}
}

bool FSRStellarFuelBatchPlanner::TryBuildStatus(
	const FSRFacilityInstance& FacilityInstance,
	FName FabricatorBodyId,
	FSRStellarFuelBatchStatusV2& OutStatus)
{
	OutStatus = FSRStellarFuelBatchStatusV2();
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!FSRStellarFuelFabricator::ShouldRouteThroughResourceV2(FacilityDataAsset))
	{
		return false;
	}

	OutStatus.bUsesReservedInputs = FacilityInstance.bProcessing
		|| !FacilityInstance.ProcessingInventory.IsEmpty();
	OutStatus.Slots.SetNum(StarRovers::StellarFuel::RequiredCardCount);
	for (int32 LaneIndex = 0; LaneIndex < OutStatus.Slots.Num(); ++LaneIndex)
	{
		OutStatus.Slots[LaneIndex].LaneIndex = LaneIndex;
	}

	bool bInvalidBatchLayout = false;
	if (OutStatus.bUsesReservedInputs)
	{
		bInvalidBatchLayout = FacilityInstance.ProcessingInventory.Num()
			!= StarRovers::StellarFuel::RequiredCardCount;
		for (int32 LaneIndex = 0;
			LaneIndex < OutStatus.Slots.Num() && FacilityInstance.ProcessingInventory.IsValidIndex(LaneIndex);
			++LaneIndex)
		{
			OutStatus.Slots[LaneIndex].Resource = FacilityInstance.ProcessingInventory[LaneIndex];
		}
	}
	else
	{
		bInvalidBatchLayout = FacilityInstance.InputPortInventories.Num()
			!= StarRovers::StellarFuel::RequiredCardCount;
		for (int32 LaneIndex = 0; LaneIndex < OutStatus.Slots.Num(); ++LaneIndex)
		{
			if (FacilityInstance.InputPortInventories.IsValidIndex(LaneIndex))
			{
				OutStatus.Slots[LaneIndex].Resource =
					StarRovers::FacilityResources::PeekSingleResourceFromInventorySlot(
						FacilityInstance.InputPortInventories[LaneIndex]);
			}
		}
	}

	TArray<FSRResourceInstance> ValidCards;
	TMap<int32, int32> FirstLanePerCardKey;
	TMap<int32, int32> UniqueKeysPerGrade;
	for (FSRStellarFuelBatchSlotV2& Slot : OutStatus.Slots)
	{
		Slot.bOccupied = !Slot.Resource.ResourceId.IsNone() && Slot.Resource.StackCount > 0;
		if (!Slot.bOccupied)
		{
			OutStatus.EmptyLaneIndices.Add(Slot.LaneIndex);
			continue;
		}

		++OutStatus.OccupiedLaneCount;
		Slot.bValidCard = FSRStellarFuelFabricator::ValidateInputCard(
			Slot.Resource,
			Slot.FailureReason,
			&Slot.ValidationOutcome);
		if (!Slot.bValidCard)
		{
			OutStatus.ContaminatedLaneIndices.Add(Slot.LaneIndex);
			continue;
		}

		++OutStatus.ValidCardCount;
		OutStatus.InputEnergySum += Slot.Resource.CurrentEnergy;
		ValidCards.Add(Slot.Resource);
		const int32 CardKey = BuildCardKey(Slot.Resource.Spectrum, Slot.Resource.Grade);
		if (const int32* FirstLane = FirstLanePerCardKey.Find(CardKey))
		{
			Slot.bDuplicateCardKey = true;
			OutStatus.Slots[*FirstLane].bDuplicateCardKey = true;
			++OutStatus.DuplicateCardKeyCount;
		}
		else
		{
			FirstLanePerCardKey.Add(CardKey, Slot.LaneIndex);
			++UniqueKeysPerGrade.FindOrAdd(Slot.Resource.Grade);
		}
	}

	OutStatus.UniqueCardKeyCount = FirstLanePerCardKey.Num();
	OutStatus.bHasCurrentPattern = OutStatus.ValidCardCount > 0;
	OutStatus.Hand = ResolveCurrentPattern(UniqueKeysPerGrade);
	OutStatus.bRankedHand = OutStatus.Hand != ESRStellarFuelHandV2::Unranked;

	if (bInvalidBatchLayout)
	{
		OutStatus.State = ESRStellarFuelBatchStateV2::Contaminated;
		if (OutStatus.ContaminatedLaneIndices.IsEmpty())
		{
			OutStatus.Summary = TEXT("BATCH CONTAMINATED | Fabricator requires exactly five runtime lanes");
			OutStatus.Detail = OutStatus.bUsesReservedInputs
				? TEXT("Reserved batch is incomplete; inputs were not consumed.")
				: TEXT("Runtime input-port layout does not match the five-lane Fabricator definition.");
			return true;
		}
	}
	if (!OutStatus.ContaminatedLaneIndices.IsEmpty())
	{
		OutStatus.State = ESRStellarFuelBatchStateV2::Contaminated;
		BuildPlayerFacingText(OutStatus);
		return true;
	}
	if (OutStatus.ValidCardCount == 0)
	{
		OutStatus.State = ESRStellarFuelBatchStateV2::Empty;
		BuildPlayerFacingText(OutStatus);
		return true;
	}
	if (OutStatus.ValidCardCount < StarRovers::StellarFuel::RequiredCardCount)
	{
		OutStatus.State = ESRStellarFuelBatchStateV2::Collecting;
		BuildPlayerFacingText(OutStatus);
		return true;
	}

	OutStatus.FinalPreview = FSRStellarFuelFabricator::Evaluate(
		FacilityInstance,
		ValidCards,
		FabricatorBodyId);
	if (!OutStatus.FinalPreview.IsSuccess())
	{
		OutStatus.State = ESRStellarFuelBatchStateV2::Contaminated;
		OutStatus.Summary = TEXT("BATCH CONTAMINATED | Final formula rejected the batch");
		OutStatus.Detail = OutStatus.FinalPreview.FailureReason;
		return true;
	}

	OutStatus.bHasFinalPreview = true;
	OutStatus.Hand = OutStatus.FinalPreview.Hand;
	OutStatus.bRankedHand = OutStatus.Hand != ESRStellarFuelHandV2::Unranked;
	OutStatus.State = OutStatus.bUsesReservedInputs
		? ESRStellarFuelBatchStateV2::Reserved
		: ESRStellarFuelBatchStateV2::Ready;
	BuildPlayerFacingText(OutStatus);
	return true;
}
