#include "Simulation/SRAugmentSubsystem.h"

#include "Simulation/SRRunModifierSubsystem.h"
#include "Simulation/SRSimulationSettings.h"
#include "Simulation/SRTimeControlSubsystem.h"
#include "Structure/SRStructureDataAsset.h"
#include "Utility/SRLog.h"

namespace
{
	bool HasRarity(const TArray<USRRunAugmentDataAsset*>& Candidates, ESRRunAugmentRarity Rarity)
	{
		for (const USRRunAugmentDataAsset* Candidate : Candidates)
		{
			if (IsValid(Candidate) && Candidate->Rarity == Rarity)
			{
				return true;
			}
		}
		return false;
	}

	bool HasRarity(const TArray<FSRAugmentChoice>& Choices, ESRRunAugmentRarity Rarity)
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

	ESRRunAugmentOfferRole ResolveOfferRole(int32 ChoiceIndex)
	{
		switch (ChoiceIndex % 3)
		{
		case 0:
			return ESRRunAugmentOfferRole::Immediate;
		case 1:
			return ESRRunAugmentOfferRole::Synergy;
		default:
			return ESRRunAugmentOfferRole::Pivot;
		}
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
		bDebugUnlockAllFacilitiesWithoutTechnology = SimulationSettings->bDebugUnlockAllFacilitiesWithoutTechnology;
	}

	Collection.InitializeDependency(USRRunModifierSubsystem::StaticClass());
	Collection.InitializeDependency(USRTimeControlSubsystem::StaticClass());
	BindTimeControlSubsystem();
	if (USRRunModifierSubsystem* RunModifierSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USRRunModifierSubsystem>() : nullptr)
	{
		RunModifierSubsystem->OnTechnologyUnlocked.RemoveDynamic(this, &USRAugmentSubsystem::HandleTechnologyUnlocked);
		RunModifierSubsystem->OnTechnologyUnlocked.AddDynamic(this, &USRAugmentSubsystem::HandleTechnologyUnlocked);
	}
}

void USRAugmentSubsystem::Deinitialize()
{
	UnbindTimeControlSubsystem();
	if (USRRunModifierSubsystem* RunModifierSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USRRunModifierSubsystem>() : nullptr)
	{
		RunModifierSubsystem->OnTechnologyUnlocked.RemoveDynamic(this, &USRAugmentSubsystem::HandleTechnologyUnlocked);
	}
	Super::Deinitialize();
}

void USRAugmentSubsystem::RegisterStructureDataAssets(const TArray<USRStructureDataAsset*>& StructureDataAssets)
{
	bool bRegisteredAny = false;
	bool bUnlockedAny = false;
	TSet<FName> RegisteredStructureIds;
	for (const USRStructureDataAsset* Registered : RegisteredStructureDataAssets)
	{
		if (IsValid(Registered))
		{
			RegisteredStructureIds.Add(Registered->BuildData().StructureId);
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
		RegisteredStructureIds.Add(StructureData.StructureId);
		RegisteredStructureDataAssets.Add(StructureDataAsset);
		bRegisteredAny = true;
		if (StructureData.bAvailableForConstruction
			&& !StructureData.bIsResourceDeposit
			&& !IsStructureUnlockControlled(StructureDataAsset))
		{
			bool bAlreadyUnlocked = false;
			UnlockedStructureIds.Add(StructureData.StructureId, &bAlreadyUnlocked);
			bUnlockedAny |= !bAlreadyUnlocked;
		}
	}

	if (bRegisteredAny)
	{
		SR_LOG(Augment, LogTemp, Log, TEXT("USRAugmentSubsystem registered %d structure data assets for Technology unlock checks."), RegisteredStructureDataAssets.Num());
	}
	if (bUnlockedAny || (bRegisteredAny && bDebugUnlockAllFacilitiesWithoutTechnology))
	{
		OnUnlockedStructuresChanged.Broadcast();
	}
}

void USRAugmentSubsystem::RegisterAugmentDataAssets(const TArray<USRRunAugmentDataAsset*>& AugmentDataAssets)
{
	if (USRRunModifierSubsystem* RunModifierSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USRRunModifierSubsystem>() : nullptr)
	{
		RunModifierSubsystem->RegisterAugmentDataAssets(AugmentDataAssets);
	}
}

void USRAugmentSubsystem::GenerateAugmentChoices(int32 CycleIndex)
{
	if (CycleIndex <= 0 || IsAugmentChoicePending())
	{
		return;
	}

	TArray<USRRunAugmentDataAsset*> InitialCandidates = GetEligibleAugmentCandidates();
	if (InitialCandidates.IsEmpty())
	{
		SR_LOG(Augment, LogTemp, Warning, TEXT("No eligible true Augment Data Assets are registered for cycle %d."), CycleIndex);
		return;
	}

	TArray<USRRunAugmentDataAsset*> RemainingCandidates = InitialCandidates;
	TArray<FSRAugmentChoice> GeneratedChoices;
	const int32 SafeChoicesPerOffer = FMath::Max(1, ChoicesPerOffer);
	GeneratedChoices.Reserve(FMath::Min(SafeChoicesPerOffer, RemainingCandidates.Num()));
	const USRRunModifierSubsystem* RunModifierSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USRRunModifierSubsystem>() : nullptr;
	const int32 AppliedStackCount = IsValid(RunModifierSubsystem) ? RunModifierSubsystem->GetTotalAugmentStackCount() : 0;
	const int32 Seed = HashCombineFast(GetTypeHash(AugmentRandomSeed), HashCombineFast(GetTypeHash(CycleIndex), GetTypeHash(AppliedStackCount)));
	FRandomStream RandomStream(Seed);

	for (int32 ChoiceIndex = 0; ChoiceIndex < SafeChoicesPerOffer && !RemainingCandidates.IsEmpty(); ++ChoiceIndex)
	{
		const ESRRunAugmentOfferRole DesiredRole = ResolveOfferRole(ChoiceIndex);
		TArray<USRRunAugmentDataAsset*> RoleCandidates;
		for (USRRunAugmentDataAsset* Candidate : RemainingCandidates)
		{
			if (IsValid(Candidate) && Candidate->OfferRole == DesiredRole)
			{
				RoleCandidates.Add(Candidate);
			}
		}
		const TArray<USRRunAugmentDataAsset*>& DrawCandidates = RoleCandidates.IsEmpty() ? RemainingCandidates : RoleCandidates;
		int32 CandidateIndex = INDEX_NONE;
		if (!DrawCandidateIndex(DrawCandidates, RandomStream, CandidateIndex) || !DrawCandidates.IsValidIndex(CandidateIndex))
		{
			break;
		}

		USRRunAugmentDataAsset* SelectedAugment = DrawCandidates[CandidateIndex];
		RemainingCandidates.RemoveSingleSwap(SelectedAugment, EAllowShrinking::No);
		if (IsValid(SelectedAugment))
		{
			GeneratedChoices.Add(BuildAugmentChoice(SelectedAugment));
		}
	}
	if (GeneratedChoices.IsEmpty())
	{
		return;
	}

	UpdateHighTechBonusAfterOffer(InitialCandidates, GeneratedChoices);
	CurrentChoices = MoveTemp(GeneratedChoices);
	CurrentAugmentChoiceCycleIndex = CycleIndex;
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
	USRRunModifierSubsystem* RunModifierSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USRRunModifierSubsystem>() : nullptr;
	if (!IsValid(RunModifierSubsystem) || !RunModifierSubsystem->ApplyAugment(SelectedChoice.AugmentId))
	{
		return false;
	}
	OnAugmentChoiceSelected.Broadcast(SelectedChoice);
	ClearPendingAugmentChoice();
	ResumeSimulationAfterChoiceIfNeeded();
	return true;
}

bool USRAugmentSubsystem::SelectAugmentChoiceByAugmentId(FName AugmentId)
{
	for (int32 ChoiceIndex = 0; ChoiceIndex < CurrentChoices.Num(); ++ChoiceIndex)
	{
		if (CurrentChoices[ChoiceIndex].AugmentId == AugmentId)
		{
			return SelectAugmentChoiceByIndex(ChoiceIndex);
		}
	}
	return false;
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
	const USRFacilityDataAsset* FacilityDataAsset = StructureData.FacilityDataAsset.Get();
	const bool bAvailableFacility = StructureData.bAvailableForConstruction && !StructureData.bIsResourceDeposit && IsValid(FacilityDataAsset);
	if (bDebugUnlockAllFacilitiesWithoutTechnology && bAvailableFacility)
	{
		return true;
	}
	if (!bAvailableFacility || FacilityDataAsset->FacilityKind != ESRFacilityKind::Standard || FacilityDataAsset->Rarity == ESRFacilityRarity::Starting)
	{
		return true;
	}
	if (UnlockedStructureIds.Contains(StructureData.StructureId))
	{
		return true;
	}
	const USRRunModifierSubsystem* RunModifierSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USRRunModifierSubsystem>() : nullptr;
	return IsValid(RunModifierSubsystem) && RunModifierSubsystem->IsStructureUnlockedByTechnology(StructureData.StructureId);
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
	const USRRunModifierSubsystem* RunModifierSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USRRunModifierSubsystem>() : nullptr;
	if (IsValid(RunModifierSubsystem) && RunModifierSubsystem->IsStructureUnlockedByTechnology(StructureId))
	{
		return true;
	}
	if (bDebugUnlockAllFacilitiesWithoutTechnology)
	{
		for (const USRStructureDataAsset* StructureDataAsset : RegisteredStructureDataAssets)
		{
			if (IsValid(StructureDataAsset) && StructureDataAsset->BuildData().StructureId == StructureId && IsDebugUnlockableFacility(StructureDataAsset))
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

void USRAugmentSubsystem::ExportSaveData(FSRAugmentOfferSaveData& OutSaveData) const
{
	OutSaveData = FSRAugmentOfferSaveData();
	OutSaveData.OfferCycleIndex = CurrentAugmentChoiceCycleIndex;
	OutSaveData.EpicPityBonusChancePercent = HighTechBonusChancePercent;
	OutSaveData.bPausedSimulationForOffer = bPausedSimulationForCurrentChoice;
	for (const FSRAugmentChoice& Choice : CurrentChoices)
	{
		OutSaveData.OfferedAugmentIds.Add(Choice.AugmentId);
	}
}

bool USRAugmentSubsystem::CanImportSaveData(
	const FSRAugmentOfferSaveData& SaveData,
	FString& OutFailureReason) const
{
	OutFailureReason.Reset();
	if (!StarRovers::Save::AugmentOffer::IsSupportedVersion(SaveData.Version))
	{
		OutFailureReason = FString::Printf(TEXT("Unsupported Augment-offer save version %d."), SaveData.Version);
		return false;
	}
	if (SaveData.OfferCycleIndex < 0
		|| !FMath::IsFinite(SaveData.EpicPityBonusChancePercent)
		|| SaveData.EpicPityBonusChancePercent < 0.0f
		|| SaveData.EpicPityBonusChancePercent > FMath::Max(0.0f, HighTechPityCapPercent))
	{
		OutFailureReason = TEXT("Augment offer Cycle or pity state is invalid.");
		return false;
	}
	if (SaveData.bPausedSimulationForOffer && SaveData.OfferedAugmentIds.IsEmpty())
	{
		OutFailureReason = TEXT("An Augment offer cannot pause the simulation without choices.");
		return false;
	}
	if (SaveData.OfferedAugmentIds.IsEmpty() && SaveData.OfferCycleIndex != 0)
	{
		OutFailureReason = TEXT("An empty Augment offer must have Cycle index zero.");
		return false;
	}

	const USRRunModifierSubsystem* RunModifiers = GetWorld() ? GetWorld()->GetSubsystem<USRRunModifierSubsystem>() : nullptr;
	if (!IsValid(RunModifiers) && !SaveData.OfferedAugmentIds.IsEmpty())
	{
		OutFailureReason = TEXT("Augment offers require the run-modifier registry.");
		return false;
	}
	TSet<FName> UniqueIds;
	for (const FName AugmentId : SaveData.OfferedAugmentIds)
	{
		bool bDuplicate = false;
		UniqueIds.Add(AugmentId, &bDuplicate);
		const USRRunAugmentDataAsset* Augment = RunModifiers ? RunModifiers->FindAugmentDataAsset(AugmentId) : nullptr;
		if (AugmentId.IsNone()
			|| bDuplicate
			|| !IsValid(Augment))
		{
			OutFailureReason = FString::Printf(TEXT("Saved Augment offer '%s' is unresolved or duplicated."), *AugmentId.ToString());
			return false;
		}
	}
	return true;
}

bool USRAugmentSubsystem::ImportSaveData(const FSRAugmentOfferSaveData& SaveData)
{
	FString FailureReason;
	if (!CanImportSaveData(SaveData, FailureReason))
	{
		return false;
	}
	USRRunModifierSubsystem* RunModifiers = GetWorld() ? GetWorld()->GetSubsystem<USRRunModifierSubsystem>() : nullptr;
	TArray<FSRAugmentChoice> ImportedChoices;
	for (const FName AugmentId : SaveData.OfferedAugmentIds)
	{
		ImportedChoices.Add(BuildAugmentChoice(const_cast<USRRunAugmentDataAsset*>(RunModifiers->FindAugmentDataAsset(AugmentId))));
	}
	CurrentChoices = MoveTemp(ImportedChoices);
	CurrentAugmentChoiceCycleIndex = SaveData.OfferCycleIndex;
	HighTechBonusChancePercent = SaveData.EpicPityBonusChancePercent;
	bPausedSimulationForCurrentChoice = SaveData.bPausedSimulationForOffer;
	if (!CurrentChoices.IsEmpty())
	{
		OnAugmentChoicesReady.Broadcast(CurrentChoices, CurrentAugmentChoiceCycleIndex);
	}
	return true;
}

void USRAugmentSubsystem::HandleGameCycleAdvanced(int32 CurrentCycleIndex)
{
	const int32 SafeInterval = FMath::Max(1, AugmentIntervalCycles);
	if (CurrentCycleIndex > 0 && CurrentCycleIndex % SafeInterval == 0)
	{
		GenerateAugmentChoices(CurrentCycleIndex);
	}
}

void USRAugmentSubsystem::HandleTechnologyUnlocked(FName TechnologyId)
{
	OnUnlockedStructuresChanged.Broadcast();
}

void USRAugmentSubsystem::BindTimeControlSubsystem()
{
	USRTimeControlSubsystem* TimeControlSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USRTimeControlSubsystem>() : nullptr;
	if (IsValid(TimeControlSubsystem))
	{
		TimeControlSubsystem->OnGameCycleAdvanced.RemoveDynamic(this, &USRAugmentSubsystem::HandleGameCycleAdvanced);
		TimeControlSubsystem->OnGameCycleAdvanced.AddDynamic(this, &USRAugmentSubsystem::HandleGameCycleAdvanced);
	}
}

void USRAugmentSubsystem::UnbindTimeControlSubsystem()
{
	USRTimeControlSubsystem* TimeControlSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USRTimeControlSubsystem>() : nullptr;
	if (IsValid(TimeControlSubsystem))
	{
		TimeControlSubsystem->OnGameCycleAdvanced.RemoveDynamic(this, &USRAugmentSubsystem::HandleGameCycleAdvanced);
	}
}

TArray<USRRunAugmentDataAsset*> USRAugmentSubsystem::GetEligibleAugmentCandidates() const
{
	TArray<USRRunAugmentDataAsset*> Candidates;
	const USRRunModifierSubsystem* RunModifierSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USRRunModifierSubsystem>() : nullptr;
	if (!IsValid(RunModifierSubsystem))
	{
		return Candidates;
	}
	for (USRRunAugmentDataAsset* DataAsset : RunModifierSubsystem->GetRegisteredAugmentDataAssets())
	{
		if (IsAugmentCandidate(DataAsset))
		{
			Candidates.Add(DataAsset);
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
	return StructureData.bAvailableForConstruction && !StructureData.bIsResourceDeposit && IsValid(FacilityDataAsset)
		&& FacilityDataAsset->FacilityKind == ESRFacilityKind::Standard && FacilityDataAsset->Rarity != ESRFacilityRarity::Starting;
}

bool USRAugmentSubsystem::IsDebugUnlockableFacility(const USRStructureDataAsset* StructureDataAsset) const
{
	if (!bDebugUnlockAllFacilitiesWithoutTechnology || !IsValid(StructureDataAsset))
	{
		return false;
	}
	const FSRStructureData StructureData = StructureDataAsset->BuildData();
	return StructureData.bAvailableForConstruction && !StructureData.bIsResourceDeposit && IsValid(StructureData.FacilityDataAsset.Get());
}

bool USRAugmentSubsystem::IsAugmentCandidate(const USRRunAugmentDataAsset* AugmentDataAsset) const
{
	const USRRunModifierSubsystem* RunModifierSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USRRunModifierSubsystem>() : nullptr;
	return IsValid(AugmentDataAsset)
		&& !AugmentDataAsset->AugmentId.IsNone()
		&& IsValid(RunModifierSubsystem)
		&& RunModifierSubsystem->GetAugmentStackCount(AugmentDataAsset->AugmentId) < FMath::Max(1, AugmentDataAsset->MaximumStacks);
}

bool USRAugmentSubsystem::DrawCandidateIndex(const TArray<USRRunAugmentDataAsset*>& Candidates, FRandomStream& RandomStream, int32& OutCandidateIndex) const
{
	OutCandidateIndex = INDEX_NONE;
	if (Candidates.IsEmpty())
	{
		return false;
	}
	TArray<int32> CommonIndices;
	TArray<int32> RareIndices;
	TArray<int32> EpicIndices;
	for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); ++CandidateIndex)
	{
		const USRRunAugmentDataAsset* Candidate = Candidates[CandidateIndex];
		if (!IsValid(Candidate))
		{
			continue;
		}
		switch (Candidate->Rarity)
		{
		case ESRRunAugmentRarity::Common:
			CommonIndices.Add(CandidateIndex);
			break;
		case ESRRunAugmentRarity::Rare:
			RareIndices.Add(CandidateIndex);
			break;
		case ESRRunAugmentRarity::Epic:
			EpicIndices.Add(CandidateIndex);
			break;
		default:
			break;
		}
	}

	const float EffectiveEpicChance = GetHighTechEffectiveChancePercent();
	const float CommonRareTotal = FMath::Max(UE_SMALL_NUMBER, BasicChancePercent + AdvancedChancePercent);
	const float NonEpicChance = FMath::Max(0.0f, 100.0f - EffectiveEpicChance);
	const float CommonWeight = CommonIndices.IsEmpty() ? 0.0f : NonEpicChance * FMath::Max(0.0f, BasicChancePercent) / CommonRareTotal;
	const float RareWeight = RareIndices.IsEmpty() ? 0.0f : NonEpicChance * FMath::Max(0.0f, AdvancedChancePercent) / CommonRareTotal;
	const float EpicWeight = EpicIndices.IsEmpty() ? 0.0f : EffectiveEpicChance;
	const float TotalWeight = CommonWeight + RareWeight + EpicWeight;
	if (TotalWeight <= UE_SMALL_NUMBER)
	{
		OutCandidateIndex = RandomStream.RandRange(0, Candidates.Num() - 1);
		return true;
	}

	const float Roll = RandomStream.FRandRange(0.0f, TotalWeight);
	const TArray<int32>* SelectedIndices = nullptr;
	if (Roll <= CommonWeight && !CommonIndices.IsEmpty())
	{
		SelectedIndices = &CommonIndices;
	}
	else if (Roll <= CommonWeight + RareWeight && !RareIndices.IsEmpty())
	{
		SelectedIndices = &RareIndices;
	}
	else if (!EpicIndices.IsEmpty())
	{
		SelectedIndices = &EpicIndices;
	}
	else if (!CommonIndices.IsEmpty())
	{
		SelectedIndices = &CommonIndices;
	}
	else if (!RareIndices.IsEmpty())
	{
		SelectedIndices = &RareIndices;
	}
	if (!SelectedIndices || SelectedIndices->IsEmpty())
	{
		return false;
	}
	OutCandidateIndex = (*SelectedIndices)[RandomStream.RandRange(0, SelectedIndices->Num() - 1)];
	return true;
}

FSRAugmentChoice USRAugmentSubsystem::BuildAugmentChoice(USRRunAugmentDataAsset* AugmentDataAsset) const
{
	FSRAugmentChoice Choice;
	if (!IsValid(AugmentDataAsset))
	{
		return Choice;
	}
	const USRRunModifierSubsystem* RunModifierSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USRRunModifierSubsystem>() : nullptr;
	Choice.AugmentId = AugmentDataAsset->AugmentId;
	Choice.DisplayName = AugmentDataAsset->DisplayName;
	Choice.Description = AugmentDataAsset->Description;
	Choice.AugmentDataAsset = AugmentDataAsset;
	Choice.Rarity = AugmentDataAsset->Rarity;
	Choice.OfferRole = AugmentDataAsset->OfferRole;
	Choice.CurrentStacks = IsValid(RunModifierSubsystem) ? RunModifierSubsystem->GetAugmentStackCount(AugmentDataAsset->AugmentId) : 0;
	Choice.MaximumStacks = FMath::Max(1, AugmentDataAsset->MaximumStacks);
	return Choice;
}

void USRAugmentSubsystem::UpdateHighTechBonusAfterOffer(const TArray<USRRunAugmentDataAsset*>& InitialCandidates, const TArray<FSRAugmentChoice>& GeneratedChoices)
{
	if (HasRarity(GeneratedChoices, ESRRunAugmentRarity::Epic))
	{
		HighTechBonusChancePercent = 0.0f;
		return;
	}
	if (HasRarity(InitialCandidates, ESRRunAugmentRarity::Epic))
	{
		HighTechBonusChancePercent = FMath::Clamp(HighTechBonusChancePercent + FMath::Max(0.0f, HighTechPityIncreasePercent), 0.0f, FMath::Max(0.0f, HighTechPityCapPercent));
	}
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
