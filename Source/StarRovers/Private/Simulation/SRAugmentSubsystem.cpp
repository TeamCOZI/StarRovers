#include "Simulation/SRAugmentSubsystem.h"

#include "Utility/SRLog.h"
#include "Simulation/SRSimulationSettings.h"
#include "Simulation/SRTimeControlSubsystem.h"
#include "Structure/SRStructureDataAsset.h"

namespace
{
	bool HasRarity(const TArray<USRStructureDataAsset*>& Candidates, ESRFacilityRarity Rarity)
	{
		for (const USRStructureDataAsset* StructureDataAsset : Candidates)
		{
			if (!IsValid(StructureDataAsset))
			{
				continue;
			}

			const FSRStructureData StructureData = StructureDataAsset->BuildData();
			const USRFacilityDataAsset* FacilityDataAsset = StructureData.FacilityDataAsset.Get();
			if (IsValid(FacilityDataAsset) && FacilityDataAsset->Rarity == Rarity)
			{
				return true;
			}
		}

		return false;
	}

	bool HasRarity(const TArray<FSRAugmentChoice>& Choices, ESRFacilityRarity Rarity)
	{
		for (const FSRAugmentChoice& Choice : Choices)
		{
			if (Choice.Rarity == Rarity)
			{
				return true;
			}
		}

		return false;
	}
}

void USRAugmentSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (const USRSimulationSettings* SimulationSettings = GetDefault<USRSimulationSettings>())
	{
		AugmentIntervalCycles = FMath::Max(1, SimulationSettings->AugmentIntervalCycles);
		ChoicesPerOffer = FMath::Max(1, SimulationSettings->AugmentChoicesPerOffer);
		BasicChancePercent = FMath::Max(0.0f, SimulationSettings->AugmentBasicChancePercent);
		AdvancedChancePercent = FMath::Max(0.0f, SimulationSettings->AugmentAdvancedChancePercent);
		HighTechBaseChancePercent = FMath::Clamp(SimulationSettings->AugmentHighTechBaseChancePercent, 0.0f, 100.0f);
		HighTechPityIncreasePercent = FMath::Max(0.0f, SimulationSettings->AugmentHighTechPityIncreasePercent);
		HighTechPityCapPercent = FMath::Clamp(SimulationSettings->AugmentHighTechPityCapPercent, 0.0f, 100.0f);
		bPauseSimulationDuringChoice = SimulationSettings->bPauseSimulationDuringAugmentChoice;
		AugmentRandomSeed = SimulationSettings->AugmentRandomSeed;
		bDebugUnlockAllFacilitiesWithoutAugments = SimulationSettings->bDebugUnlockAllFacilitiesWithoutAugments;
	}

	Collection.InitializeDependency(USRTimeControlSubsystem::StaticClass());
	BindTimeControlSubsystem();
}

void USRAugmentSubsystem::Deinitialize()
{
	UnbindTimeControlSubsystem();

	Super::Deinitialize();
}

void USRAugmentSubsystem::RegisterStructureDataAssets(const TArray<USRStructureDataAsset*>& StructureDataAssets)
{
	bool bRegisteredAny = false;
	bool bUnlockedAny = false;

	TSet<FName> RegisteredStructureIds;
	for (const USRStructureDataAsset* RegisteredStructureDataAsset : RegisteredStructureDataAssets)
	{
		if (!IsValid(RegisteredStructureDataAsset))
		{
			continue;
		}

		const FSRStructureData StructureData = RegisteredStructureDataAsset->BuildData();
		if (!StructureData.StructureId.IsNone())
		{
			RegisteredStructureIds.Add(StructureData.StructureId);
		}
	}

	for (USRStructureDataAsset* StructureDataAsset : StructureDataAssets)
	{
		if (!IsValid(StructureDataAsset))
		{
			continue;
		}

		const FSRStructureData StructureData = StructureDataAsset->BuildData();
		if (StructureData.StructureId.IsNone() || RegisteredStructureIds.Contains(StructureData.StructureId))
		{
			continue;
		}

		RegisteredStructureDataAssets.Add(StructureDataAsset);
		RegisteredStructureIds.Add(StructureData.StructureId);
		bRegisteredAny = true;

		if (StructureData.bAvailableForConstruction
			&& !StructureData.bIsResourceDeposit
			&& !IsStructureUnlockControlled(StructureDataAsset)
			&& !UnlockedStructureIds.Contains(StructureData.StructureId))
		{
			UnlockedStructureIds.Add(StructureData.StructureId);
			bUnlockedAny = true;
		}
	}

	if (bRegisteredAny)
	{
		SR_LOG(Augment, LogTemp, Log, TEXT("USRAugmentSubsystem registered %d structure data assets."), RegisteredStructureDataAssets.Num());
	}

	if (bUnlockedAny || (bRegisteredAny && bDebugUnlockAllFacilitiesWithoutAugments))
	{
		OnUnlockedStructuresChanged.Broadcast();
	}
}

void USRAugmentSubsystem::GenerateAugmentChoices(int32 CycleIndex)
{
	if (CycleIndex <= 0 || IsAugmentChoicePending())
	{
		return;
	}

	TArray<USRStructureDataAsset*> InitialCandidates = GetEligibleAugmentCandidates();
	if (InitialCandidates.IsEmpty())
	{
		SR_LOG(Augment, LogTemp, Warning, TEXT("USRAugmentSubsystem could not generate augment choices for cycle %d because no eligible structure candidates remain."), CycleIndex);
		return;
	}

	TArray<USRStructureDataAsset*> RemainingCandidates = InitialCandidates;
	TArray<FSRAugmentChoice> GeneratedChoices;
	const int32 SafeChoicesPerOffer = FMath::Max(1, ChoicesPerOffer);
	GeneratedChoices.Reserve(FMath::Min(SafeChoicesPerOffer, RemainingCandidates.Num()));

	const int32 Seed = HashCombineFast(
		GetTypeHash(AugmentRandomSeed),
		HashCombineFast(GetTypeHash(CycleIndex), GetTypeHash(UnlockedStructureIds.Num())));
	FRandomStream RandomStream(Seed);

	for (int32 ChoiceIndex = 0; ChoiceIndex < SafeChoicesPerOffer && !RemainingCandidates.IsEmpty(); ++ChoiceIndex)
	{
		int32 CandidateIndex = INDEX_NONE;
		if (!DrawCandidateIndex(RemainingCandidates, RandomStream, CandidateIndex) || !RemainingCandidates.IsValidIndex(CandidateIndex))
		{
			break;
		}

		USRStructureDataAsset* SelectedStructureDataAsset = RemainingCandidates[CandidateIndex];
		RemainingCandidates.RemoveAtSwap(CandidateIndex, 1, EAllowShrinking::No);

		if (IsValid(SelectedStructureDataAsset))
		{
			GeneratedChoices.Add(BuildAugmentChoice(SelectedStructureDataAsset));
		}
	}

	if (GeneratedChoices.IsEmpty())
	{
		return;
	}

	CurrentChoices = GeneratedChoices;
	CurrentAugmentChoiceCycleIndex = CycleIndex;
	UpdateHighTechBonusAfterOffer(InitialCandidates, GeneratedChoices);
	SR_LOG(Augment, LogTemp, Log, TEXT("USRAugmentSubsystem generated %d augment choices for cycle %d from %d candidates."),
		CurrentChoices.Num(),
		CurrentAugmentChoiceCycleIndex,
		InitialCandidates.Num());

	if (bPauseSimulationDuringChoice)
	{
		if (USRTimeControlSubsystem* TimeControlSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USRTimeControlSubsystem>() : nullptr)
		{
			TimeControlSubsystem->SetSimulationPaused(true);
			bPausedSimulationForCurrentChoice = true;
		}
	}

	OnAugmentChoicesReady.Broadcast(CurrentChoices, CurrentAugmentChoiceCycleIndex);
}

bool USRAugmentSubsystem::SelectAugmentChoiceByIndex(int32 ChoiceIndex)
{
	if (!CurrentChoices.IsValidIndex(ChoiceIndex))
	{
		return false;
	}

	const FSRAugmentChoice SelectedChoice = CurrentChoices[ChoiceIndex];
	if (!UnlockStructure(SelectedChoice.StructureDataAsset.Get()))
	{
		return false;
	}

	OnAugmentChoiceSelected.Broadcast(SelectedChoice);
	ClearPendingAugmentChoice();
	ResumeSimulationAfterChoiceIfNeeded();
	return true;
}

bool USRAugmentSubsystem::SelectAugmentChoiceByStructureId(FName StructureId)
{
	if (StructureId.IsNone())
	{
		return false;
	}

	for (int32 ChoiceIndex = 0; ChoiceIndex < CurrentChoices.Num(); ++ChoiceIndex)
	{
		if (CurrentChoices[ChoiceIndex].StructureId == StructureId)
		{
			return SelectAugmentChoiceByIndex(ChoiceIndex);
		}
	}

	return false;
}

bool USRAugmentSubsystem::UnlockStructure(USRStructureDataAsset* StructureDataAsset)
{
	if (!IsValid(StructureDataAsset))
	{
		return false;
	}

	const FSRStructureData StructureData = StructureDataAsset->BuildData();
	if (StructureData.StructureId.IsNone())
	{
		return false;
	}

	if (UnlockedStructureIds.Contains(StructureData.StructureId))
	{
		return true;
	}

	UnlockedStructureIds.Add(StructureData.StructureId);
	OnUnlockedStructuresChanged.Broadcast();
	SR_LOG(Augment, LogTemp, Log, TEXT("USRAugmentSubsystem unlocked structure '%s'."), *StructureData.StructureId.ToString());
	return true;
}

bool USRAugmentSubsystem::IsStructureUnlocked(const USRStructureDataAsset* StructureDataAsset) const
{
	if (!IsValid(StructureDataAsset))
	{
		return false;
	}

	const FSRStructureData StructureData = StructureDataAsset->BuildData();
	if (StructureData.StructureId.IsNone())
	{
		return false;
	}

	if (IsDebugUnlockableFacility(StructureDataAsset))
	{
		return true;
	}

	if (!IsStructureUnlockControlled(StructureDataAsset))
	{
		return true;
	}

	return UnlockedStructureIds.Contains(StructureData.StructureId);
}

bool USRAugmentSubsystem::IsStructureUnlockedById(FName StructureId) const
{
	if (StructureId.IsNone())
	{
		return false;
	}

	if (UnlockedStructureIds.Contains(StructureId))
	{
		return true;
	}

	if (bDebugUnlockAllFacilitiesWithoutAugments)
	{
		for (const USRStructureDataAsset* StructureDataAsset : RegisteredStructureDataAssets)
		{
			if (!IsValid(StructureDataAsset))
			{
				continue;
			}

			const FSRStructureData StructureData = StructureDataAsset->BuildData();
			if (StructureData.StructureId == StructureId && IsDebugUnlockableFacility(StructureDataAsset))
			{
				return true;
			}
		}
	}

	return false;
}

bool USRAugmentSubsystem::IsAugmentChoicePending() const
{
	return !CurrentChoices.IsEmpty();
}

TArray<FSRAugmentChoice> USRAugmentSubsystem::GetCurrentAugmentChoices() const
{
	return CurrentChoices;
}

int32 USRAugmentSubsystem::GetCurrentAugmentChoiceCycleIndex() const
{
	return CurrentAugmentChoiceCycleIndex;
}

float USRAugmentSubsystem::GetHighTechEffectiveChancePercent() const
{
	return FMath::Clamp(HighTechBaseChancePercent + HighTechBonusChancePercent, 0.0f, 100.0f);
}

float USRAugmentSubsystem::GetHighTechBonusChancePercent() const
{
	return HighTechBonusChancePercent;
}

void USRAugmentSubsystem::HandleGameCycleAdvanced(int32 CurrentCycleIndex)
{
	const int32 SafeInterval = FMath::Max(1, AugmentIntervalCycles);
	if (CurrentCycleIndex > 0 && CurrentCycleIndex % SafeInterval == 0)
	{
		GenerateAugmentChoices(CurrentCycleIndex);
	}
}

void USRAugmentSubsystem::BindTimeControlSubsystem()
{
	USRTimeControlSubsystem* TimeControlSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USRTimeControlSubsystem>() : nullptr;
	if (!TimeControlSubsystem)
	{
		return;
	}

	TimeControlSubsystem->OnGameCycleAdvanced.RemoveDynamic(this, &USRAugmentSubsystem::HandleGameCycleAdvanced);
	TimeControlSubsystem->OnGameCycleAdvanced.AddDynamic(this, &USRAugmentSubsystem::HandleGameCycleAdvanced);
}

void USRAugmentSubsystem::UnbindTimeControlSubsystem()
{
	USRTimeControlSubsystem* TimeControlSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USRTimeControlSubsystem>() : nullptr;
	if (TimeControlSubsystem)
	{
		TimeControlSubsystem->OnGameCycleAdvanced.RemoveDynamic(this, &USRAugmentSubsystem::HandleGameCycleAdvanced);
	}
}

TArray<USRStructureDataAsset*> USRAugmentSubsystem::GetEligibleAugmentCandidates() const
{
	TArray<USRStructureDataAsset*> Candidates;
	Candidates.Reserve(RegisteredStructureDataAssets.Num());

	for (USRStructureDataAsset* StructureDataAsset : RegisteredStructureDataAssets)
	{
		if (IsAugmentCandidate(StructureDataAsset))
		{
			Candidates.Add(StructureDataAsset);
		}
	}

	return Candidates;
}

bool USRAugmentSubsystem::IsStructureUnlockControlled(const USRStructureDataAsset* StructureDataAsset) const
{
	if (!IsValid(StructureDataAsset))
	{
		return false;
	}

	const FSRStructureData StructureData = StructureDataAsset->BuildData();
	const USRFacilityDataAsset* FacilityDataAsset = StructureData.FacilityDataAsset.Get();
	return StructureData.bAvailableForConstruction
		&& !StructureData.bIsResourceDeposit
		&& IsValid(FacilityDataAsset)
		&& FacilityDataAsset->FacilityKind == ESRFacilityKind::Standard;
}

bool USRAugmentSubsystem::IsDebugUnlockableFacility(const USRStructureDataAsset* StructureDataAsset) const
{
	if (!bDebugUnlockAllFacilitiesWithoutAugments || !IsValid(StructureDataAsset))
	{
		return false;
	}

	const FSRStructureData StructureData = StructureDataAsset->BuildData();
	return StructureData.bAvailableForConstruction
		&& !StructureData.bIsResourceDeposit
		&& IsValid(StructureData.FacilityDataAsset.Get());
}

bool USRAugmentSubsystem::IsAugmentCandidate(const USRStructureDataAsset* StructureDataAsset) const
{
	if (!IsStructureUnlockControlled(StructureDataAsset))
	{
		return false;
	}

	if (IsDebugUnlockableFacility(StructureDataAsset))
	{
		return false;
	}

	const FSRStructureData StructureData = StructureDataAsset->BuildData();
	const USRFacilityDataAsset* FacilityDataAsset = StructureData.FacilityDataAsset.Get();
	return !StructureData.StructureId.IsNone()
		&& !UnlockedStructureIds.Contains(StructureData.StructureId)
		&& IsValid(FacilityDataAsset)
		&& FacilityDataAsset->Rarity != ESRFacilityRarity::Innovation;
}

bool USRAugmentSubsystem::DrawCandidateIndex(const TArray<USRStructureDataAsset*>& Candidates, FRandomStream& RandomStream, int32& OutCandidateIndex) const
{
	OutCandidateIndex = INDEX_NONE;
	if (Candidates.IsEmpty())
	{
		return false;
	}

	TArray<int32> BasicCandidateIndices;
	TArray<int32> AdvancedCandidateIndices;
	TArray<int32> HighTechCandidateIndices;
	BasicCandidateIndices.Reserve(Candidates.Num());
	AdvancedCandidateIndices.Reserve(Candidates.Num());
	HighTechCandidateIndices.Reserve(Candidates.Num());

	for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); ++CandidateIndex)
	{
		const USRStructureDataAsset* StructureDataAsset = Candidates[CandidateIndex];
		if (!IsValid(StructureDataAsset))
		{
			continue;
		}

		const FSRStructureData StructureData = StructureDataAsset->BuildData();
		const USRFacilityDataAsset* FacilityDataAsset = StructureData.FacilityDataAsset.Get();
		if (!IsValid(FacilityDataAsset))
		{
			continue;
		}

		switch (FacilityDataAsset->Rarity)
		{
		case ESRFacilityRarity::Basic:
			BasicCandidateIndices.Add(CandidateIndex);
			break;
		case ESRFacilityRarity::Advanced:
			AdvancedCandidateIndices.Add(CandidateIndex);
			break;
		case ESRFacilityRarity::HighTech:
			HighTechCandidateIndices.Add(CandidateIndex);
			break;
		default:
			break;
		}
	}

	const float EffectiveHighTechChance = GetHighTechEffectiveChancePercent();
	const float BasicAdvancedBaseTotal = FMath::Max(UE_SMALL_NUMBER, BasicChancePercent + AdvancedChancePercent);
	const float RemainingNonHighTechChance = FMath::Max(0.0f, 100.0f - EffectiveHighTechChance);

	const float BasicWeight = BasicCandidateIndices.IsEmpty()
		? 0.0f
		: RemainingNonHighTechChance * (FMath::Max(0.0f, BasicChancePercent) / BasicAdvancedBaseTotal);
	const float AdvancedWeight = AdvancedCandidateIndices.IsEmpty()
		? 0.0f
		: RemainingNonHighTechChance * (FMath::Max(0.0f, AdvancedChancePercent) / BasicAdvancedBaseTotal);
	const float HighTechWeight = HighTechCandidateIndices.IsEmpty() ? 0.0f : EffectiveHighTechChance;
	const float TotalWeight = BasicWeight + AdvancedWeight + HighTechWeight;

	if (TotalWeight <= UE_SMALL_NUMBER)
	{
		OutCandidateIndex = RandomStream.RandRange(0, Candidates.Num() - 1);
		return true;
	}

	const float Roll = RandomStream.FRandRange(0.0f, TotalWeight);
	const TArray<int32>* SelectedCandidateIndices = nullptr;
	float AccumulatedWeight = BasicWeight;
	if (Roll <= AccumulatedWeight && !BasicCandidateIndices.IsEmpty())
	{
		SelectedCandidateIndices = &BasicCandidateIndices;
	}
	else
	{
		AccumulatedWeight += AdvancedWeight;
		if (Roll <= AccumulatedWeight && !AdvancedCandidateIndices.IsEmpty())
		{
			SelectedCandidateIndices = &AdvancedCandidateIndices;
		}
		else if (!HighTechCandidateIndices.IsEmpty())
		{
			SelectedCandidateIndices = &HighTechCandidateIndices;
		}
	}

	if (!SelectedCandidateIndices || SelectedCandidateIndices->IsEmpty())
	{
		if (!BasicCandidateIndices.IsEmpty())
		{
			SelectedCandidateIndices = &BasicCandidateIndices;
		}
		else if (!AdvancedCandidateIndices.IsEmpty())
		{
			SelectedCandidateIndices = &AdvancedCandidateIndices;
		}
		else if (!HighTechCandidateIndices.IsEmpty())
		{
			SelectedCandidateIndices = &HighTechCandidateIndices;
		}
	}

	if (!SelectedCandidateIndices || SelectedCandidateIndices->IsEmpty())
	{
		return false;
	}

	const int32 RarityLocalIndex = RandomStream.RandRange(0, SelectedCandidateIndices->Num() - 1);
	OutCandidateIndex = (*SelectedCandidateIndices)[RarityLocalIndex];
	return true;
}

FSRAugmentChoice USRAugmentSubsystem::BuildAugmentChoice(USRStructureDataAsset* StructureDataAsset) const
{
	FSRAugmentChoice Choice;
	if (!IsValid(StructureDataAsset))
	{
		return Choice;
	}

	const FSRStructureData StructureData = StructureDataAsset->BuildData();
	const USRFacilityDataAsset* FacilityDataAsset = StructureData.FacilityDataAsset.Get();

	Choice.StructureId = StructureData.StructureId;
	Choice.DisplayName = StructureData.DisplayName.IsEmpty() && IsValid(FacilityDataAsset)
		? FacilityDataAsset->DisplayName
		: StructureData.DisplayName;
	Choice.Description = StructureData.Description.IsEmpty() && IsValid(FacilityDataAsset)
		? FacilityDataAsset->Description
		: StructureData.Description;
	Choice.StructureDataAsset = StructureDataAsset;
	Choice.Rarity = IsValid(FacilityDataAsset) ? FacilityDataAsset->Rarity : ESRFacilityRarity::Basic;
	return Choice;
}

void USRAugmentSubsystem::UpdateHighTechBonusAfterOffer(const TArray<USRStructureDataAsset*>& InitialCandidates, const TArray<FSRAugmentChoice>& GeneratedChoices)
{
	if (HasRarity(GeneratedChoices, ESRFacilityRarity::HighTech))
	{
		HighTechBonusChancePercent = 0.0f;
		return;
	}

	if (!HasRarity(InitialCandidates, ESRFacilityRarity::HighTech))
	{
		return;
	}

	HighTechBonusChancePercent = FMath::Clamp(
		HighTechBonusChancePercent + FMath::Max(0.0f, HighTechPityIncreasePercent),
		0.0f,
		FMath::Max(0.0f, HighTechPityCapPercent));
}

void USRAugmentSubsystem::ClearPendingAugmentChoice()
{
	CurrentChoices.Reset();
	CurrentAugmentChoiceCycleIndex = 0;
}

void USRAugmentSubsystem::ResumeSimulationAfterChoiceIfNeeded()
{
	if (!bPausedSimulationForCurrentChoice)
	{
		return;
	}

	if (USRTimeControlSubsystem* TimeControlSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USRTimeControlSubsystem>() : nullptr)
	{
		TimeControlSubsystem->SetSimulationPaused(false);
	}

	bPausedSimulationForCurrentChoice = false;
}
