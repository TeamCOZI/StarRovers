#include "Simulation/SRRunModifierSubsystem.h"

#include "Engine/World.h"
#include "Simulation/SRSimulationSettings.h"
#include "Simulation/SRTimeControlSubsystem.h"
#include "Utility/SRLog.h"

namespace
{
	FSRRunModifierSource BuildSource(
		FName SourceId,
		ESRRunModifierSourceKind SourceKind,
		int32 Priority,
		int32 StackCount,
		const TArray<FSRRunModifierEffect>& Effects)
	{
		FSRRunModifierSource Source;
		Source.SourceId = SourceId;
		Source.SourceKind = SourceKind;
		Source.Priority = Priority;
		Source.StackCount = FMath::Clamp(StackCount, 1, FSRRunModifierResolver::MaximumSourceStacks);
		Source.Effects = Effects;
		return Source;
	}
}

void USRRunModifierSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency(USRTimeControlSubsystem::StaticClass());
	RegisterConfiguredDataAssets();
	BindTimeControlSubsystem();
}

void USRRunModifierSubsystem::Deinitialize()
{
	UnbindTimeControlSubsystem();
	TechnologiesById.Reset();
	AugmentsById.Reset();
	TrialsById.Reset();
	UnlockedTechnologyIds.Reset();
	TechnologyUnlockedStructureIds.Reset();
	AugmentStacksById.Reset();
	ActiveTrialsById.Reset();
	CurrentContext = FSRRunModifierContext();
	Super::Deinitialize();
}

void USRRunModifierSubsystem::RegisterTechnologyDataAssets(const TArray<USRTechnologyDataAsset*>& DataAssets)
{
	bool bRegisteredAny = false;
	for (USRTechnologyDataAsset* DataAsset : DataAssets)
	{
		if (!IsValid(DataAsset) || DataAsset->TechnologyId.IsNone())
		{
			continue;
		}
		if (TechnologiesById.Contains(DataAsset->TechnologyId))
		{
			continue;
		}
		if (!ValidateEffects(DataAsset->TechnologyId, ESRRunModifierSourceKind::Technology, DataAsset->Effects))
		{
			continue;
		}

		TSet<FName> UniquePrerequisites;
		bool bPrerequisitesValid = true;
		for (const FName PrerequisiteId : DataAsset->PrerequisiteTechnologyIds)
		{
			bool bAlreadyPresent = false;
			UniquePrerequisites.Add(PrerequisiteId, &bAlreadyPresent);
			if (PrerequisiteId.IsNone() || PrerequisiteId == DataAsset->TechnologyId || bAlreadyPresent)
			{
				bPrerequisitesValid = false;
				break;
			}
		}
		if (!bPrerequisitesValid)
		{
			SR_LOG(Augment, LogTemp, Warning, TEXT("Technology '%s' has invalid or duplicate prerequisites and was not registered."), *DataAsset->TechnologyId.ToString());
			continue;
		}

		TechnologiesById.Add(DataAsset->TechnologyId, DataAsset);
		bRegisteredAny = true;
	}

	if (bRegisteredAny)
	{
		UnlockConfiguredDefaultTechnologies();
		RebuildTechnologyStructureUnlocks();
		RebuildContext();
	}
}

void USRRunModifierSubsystem::RegisterAugmentDataAssets(const TArray<USRRunAugmentDataAsset*>& DataAssets)
{
	for (USRRunAugmentDataAsset* DataAsset : DataAssets)
	{
		if (!IsValid(DataAsset)
			|| DataAsset->AugmentId.IsNone()
			|| DataAsset->Effects.IsEmpty()
			|| DataAsset->MaximumStacks < 1
			|| DataAsset->MaximumStacks > FSRRunModifierResolver::MaximumSourceStacks
			|| AugmentsById.Contains(DataAsset->AugmentId)
			|| !ValidateEffects(DataAsset->AugmentId, ESRRunModifierSourceKind::Augment, DataAsset->Effects))
		{
			continue;
		}
		AugmentsById.Add(DataAsset->AugmentId, DataAsset);
	}
}

void USRRunModifierSubsystem::RegisterTrialDataAssets(const TArray<USRTrialDataAsset*>& DataAssets)
{
	for (USRTrialDataAsset* DataAsset : DataAssets)
	{
		if (!IsValid(DataAsset)
			|| DataAsset->TrialId.IsNone()
			|| DataAsset->Effects.IsEmpty()
			|| DataAsset->DurationCycles < 1
			|| TrialsById.Contains(DataAsset->TrialId)
			|| !ValidateEffects(DataAsset->TrialId, ESRRunModifierSourceKind::Trial, DataAsset->Effects))
		{
			continue;
		}
		TrialsById.Add(DataAsset->TrialId, DataAsset);
	}
}

bool USRRunModifierSubsystem::UnlockTechnology(FName TechnologyId)
{
	if (TechnologyId.IsNone() || UnlockedTechnologyIds.Contains(TechnologyId))
	{
		return !TechnologyId.IsNone() && UnlockedTechnologyIds.Contains(TechnologyId);
	}

	const TObjectPtr<USRTechnologyDataAsset>* FoundDataAsset = TechnologiesById.Find(TechnologyId);
	const USRTechnologyDataAsset* DataAsset = FoundDataAsset ? FoundDataAsset->Get() : nullptr;
	if (!IsValid(DataAsset))
	{
		return false;
	}
	for (const FName PrerequisiteId : DataAsset->PrerequisiteTechnologyIds)
	{
		if (!UnlockedTechnologyIds.Contains(PrerequisiteId))
		{
			return false;
		}
	}

	UnlockedTechnologyIds.Add(TechnologyId);
	RebuildTechnologyStructureUnlocks();
	RebuildContext();
	OnTechnologyUnlocked.Broadcast(TechnologyId);
	return true;
}

bool USRRunModifierSubsystem::IsTechnologyUnlocked(FName TechnologyId) const
{
	return !TechnologyId.IsNone() && UnlockedTechnologyIds.Contains(TechnologyId);
}

bool USRRunModifierSubsystem::IsStructureUnlockedByTechnology(FName StructureId) const
{
	return !StructureId.IsNone() && TechnologyUnlockedStructureIds.Contains(StructureId);
}

bool USRRunModifierSubsystem::ApplyAugment(FName AugmentId)
{
	const TObjectPtr<USRRunAugmentDataAsset>* FoundDataAsset = AugmentsById.Find(AugmentId);
	const USRRunAugmentDataAsset* DataAsset = FoundDataAsset ? FoundDataAsset->Get() : nullptr;
	if (!IsValid(DataAsset))
	{
		return false;
	}

	const int32 CurrentStacks = GetAugmentStackCount(AugmentId);
	const int32 MaximumStacks = FMath::Clamp(DataAsset->MaximumStacks, 1, FSRRunModifierResolver::MaximumSourceStacks);
	if (CurrentStacks >= MaximumStacks)
	{
		return false;
	}

	AugmentStacksById.Add(AugmentId, CurrentStacks + 1);
	RebuildContext();
	return true;
}

int32 USRRunModifierSubsystem::GetAugmentStackCount(FName AugmentId) const
{
	const int32* FoundStacks = AugmentStacksById.Find(AugmentId);
	return FoundStacks ? FMath::Max(0, *FoundStacks) : 0;
}

int32 USRRunModifierSubsystem::GetTotalAugmentStackCount() const
{
	int32 TotalStacks = 0;
	for (const TPair<FName, int32>& Pair : AugmentStacksById)
	{
		TotalStacks += FMath::Max(0, Pair.Value);
	}
	return TotalStacks;
}

bool USRRunModifierSubsystem::ActivateTrial(FName TrialId, int32 StartCycleIndex)
{
	const TObjectPtr<USRTrialDataAsset>* FoundDataAsset = TrialsById.Find(TrialId);
	const USRTrialDataAsset* DataAsset = FoundDataAsset ? FoundDataAsset->Get() : nullptr;
	if (!IsValid(DataAsset))
	{
		return false;
	}
	if (StartCycleIndex == INDEX_NONE)
	{
		const USRTimeControlSubsystem* TimeControlSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USRTimeControlSubsystem>() : nullptr;
		StartCycleIndex = IsValid(TimeControlSubsystem) ? TimeControlSubsystem->GetCurrentCycleIndex() : 0;
	}

	FSRActiveTrialState TrialState;
	TrialState.TrialId = TrialId;
	TrialState.StartCycleIndex = FMath::Max(0, StartCycleIndex);
	TrialState.EndCycleIndexExclusive = TrialState.StartCycleIndex + FMath::Max(1, DataAsset->DurationCycles);
	ActiveTrialsById.Add(TrialId, TrialState);
	RebuildContext();
	OnTrialActivated.Broadcast(TrialState);
	return true;
}

bool USRRunModifierSubsystem::DeactivateTrial(FName TrialId)
{
	FSRActiveTrialState TrialState;
	if (!ActiveTrialsById.RemoveAndCopyValue(TrialId, TrialState))
	{
		return false;
	}
	RebuildContext();
	OnTrialExpired.Broadcast(TrialState);
	return true;
}

TArray<FSRActiveTrialState> USRRunModifierSubsystem::GetActiveTrials() const
{
	TArray<FSRActiveTrialState> Result;
	ActiveTrialsById.GenerateValueArray(Result);
	Result.Sort([](const FSRActiveTrialState& Left, const FSRActiveTrialState& Right)
	{
		if (Left.EndCycleIndexExclusive != Right.EndCycleIndexExclusive)
		{
			return Left.EndCycleIndexExclusive < Right.EndCycleIndexExclusive;
		}
		return Left.TrialId.ToString() < Right.TrialId.ToString();
	});
	return Result;
}

FSRRunModifierContext USRRunModifierSubsystem::GetRunModifierContext() const
{
	return CurrentContext;
}

FSRResolvedRunModifiers USRRunModifierSubsystem::ResolveModifiers(const FSRRunModifierQuery& Query) const
{
	return FSRRunModifierResolver::Resolve(CurrentContext, Query);
}

TArray<USRRunAugmentDataAsset*> USRRunModifierSubsystem::GetRegisteredAugmentDataAssets() const
{
	TArray<USRRunAugmentDataAsset*> Result;
	Result.Reserve(AugmentsById.Num());
	for (const TPair<FName, TObjectPtr<USRRunAugmentDataAsset>>& Pair : AugmentsById)
	{
		if (IsValid(Pair.Value.Get()))
		{
			Result.Add(Pair.Value.Get());
		}
	}
	Result.Sort([](const USRRunAugmentDataAsset& Left, const USRRunAugmentDataAsset& Right)
	{
		return Left.AugmentId.ToString() < Right.AugmentId.ToString();
	});
	return Result;
}

const USRRunAugmentDataAsset* USRRunModifierSubsystem::FindAugmentDataAsset(FName AugmentId) const
{
	const TObjectPtr<USRRunAugmentDataAsset>* FoundDataAsset = AugmentsById.Find(AugmentId);
	return FoundDataAsset ? FoundDataAsset->Get() : nullptr;
}

FSRRunModifierContext USRRunModifierSubsystem::GetContextForObject(const UObject* WorldContextObject)
{
	const UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	const USRRunModifierSubsystem* Subsystem = IsValid(World) ? World->GetSubsystem<USRRunModifierSubsystem>() : nullptr;
	return IsValid(Subsystem) ? Subsystem->GetRunModifierContext() : FSRRunModifierContext();
}

FSRResolvedRunModifiers USRRunModifierSubsystem::ResolveForObject(
	const UObject* WorldContextObject,
	const FSRRunModifierQuery& Query)
{
	return FSRRunModifierResolver::Resolve(GetContextForObject(WorldContextObject), Query);
}

void USRRunModifierSubsystem::ExportSaveData(FSRRunModifierSaveData& OutSaveData) const
{
	OutSaveData = FSRRunModifierSaveData();
	OutSaveData.ContextRevision = CurrentContext.Revision;
	OutSaveData.NextContextRevision = FMath::Max(NextContextRevision, CurrentContext.Revision + 1);
	OutSaveData.UnlockedTechnologyIds = UnlockedTechnologyIds.Array();
	OutSaveData.UnlockedTechnologyIds.Sort([](const FName Left, const FName Right)
	{
		return Left.LexicalLess(Right);
	});

	TArray<FName> AugmentIds;
	AugmentStacksById.GetKeys(AugmentIds);
	AugmentIds.Sort([](const FName Left, const FName Right)
	{
		return Left.LexicalLess(Right);
	});
	for (const FName AugmentId : AugmentIds)
	{
		const int32 StackCount = GetAugmentStackCount(AugmentId);
		if (StackCount <= 0)
		{
			continue;
		}
		FSRRunModifierAugmentStackSaveData& StackSave = OutSaveData.AugmentStacks.AddDefaulted_GetRef();
		StackSave.AugmentId = AugmentId;
		StackSave.StackCount = StackCount;
	}
	OutSaveData.ActiveTrials = GetActiveTrials();
}

bool USRRunModifierSubsystem::CanImportSaveData(
	const FSRRunModifierSaveData& SaveData,
	FString& OutFailureReason) const
{
	TSet<FName> ImportedTechnologyIds;
	TMap<FName, int32> ImportedAugmentStacks;
	TMap<FName, FSRActiveTrialState> ImportedTrials;
	FSRRunModifierContext ImportedContext;
	return BuildStateFromSaveData(
		SaveData,
		ImportedTechnologyIds,
		ImportedAugmentStacks,
		ImportedTrials,
		ImportedContext,
		OutFailureReason);
}

bool USRRunModifierSubsystem::ImportSaveData(const FSRRunModifierSaveData& SaveData)
{
	TSet<FName> ImportedTechnologyIds;
	TMap<FName, int32> ImportedAugmentStacks;
	TMap<FName, FSRActiveTrialState> ImportedTrials;
	FSRRunModifierContext ImportedContext;
	FString FailureReason;
	if (!BuildStateFromSaveData(
		SaveData,
		ImportedTechnologyIds,
		ImportedAugmentStacks,
		ImportedTrials,
		ImportedContext,
		FailureReason))
	{
		SR_LOG(Augment, LogTemp, Error, TEXT("Run modifier save import rejected: %s"), *FailureReason);
		return false;
	}

	UnlockedTechnologyIds = MoveTemp(ImportedTechnologyIds);
	AugmentStacksById = MoveTemp(ImportedAugmentStacks);
	ActiveTrialsById = MoveTemp(ImportedTrials);
	CurrentContext = MoveTemp(ImportedContext);
	NextContextRevision = FMath::Max(SaveData.NextContextRevision, CurrentContext.Revision + 1);
	RebuildTechnologyStructureUnlocks();
	OnRunModifierContextChanged.Broadcast(CurrentContext);
	return true;
}

bool USRRunModifierSubsystem::BuildStateFromSaveData(
	const FSRRunModifierSaveData& SaveData,
	TSet<FName>& OutUnlockedTechnologyIds,
	TMap<FName, int32>& OutAugmentStacksById,
	TMap<FName, FSRActiveTrialState>& OutActiveTrialsById,
	FSRRunModifierContext& OutContext,
	FString& OutFailureReason) const
{
	OutUnlockedTechnologyIds.Reset();
	OutAugmentStacksById.Reset();
	OutActiveTrialsById.Reset();
	OutContext = FSRRunModifierContext();
	OutFailureReason.Reset();
	if (!StarRovers::Save::RunModifiers::IsSupportedVersion(SaveData.Version))
	{
		OutFailureReason = FString::Printf(TEXT("Unsupported run-modifier save version %d."), SaveData.Version);
		return false;
	}
	if (SaveData.ContextRevision < 0 || SaveData.NextContextRevision <= SaveData.ContextRevision)
	{
		OutFailureReason = TEXT("Run-modifier context revisions are invalid.");
		return false;
	}

	for (const FName TechnologyId : SaveData.UnlockedTechnologyIds)
	{
		bool bAlreadyPresent = false;
		OutUnlockedTechnologyIds.Add(TechnologyId, &bAlreadyPresent);
		if (TechnologyId.IsNone() || bAlreadyPresent || !TechnologiesById.Contains(TechnologyId))
		{
			OutFailureReason = FString::Printf(TEXT("Technology '%s' is missing, empty, or duplicated."), *TechnologyId.ToString());
			return false;
		}
	}
	for (const FName TechnologyId : OutUnlockedTechnologyIds)
	{
		const USRTechnologyDataAsset* Technology = TechnologiesById.FindRef(TechnologyId).Get();
		for (const FName PrerequisiteId : Technology->PrerequisiteTechnologyIds)
		{
			if (!OutUnlockedTechnologyIds.Contains(PrerequisiteId))
			{
				OutFailureReason = FString::Printf(
					TEXT("Technology '%s' is missing prerequisite '%s'."),
					*TechnologyId.ToString(),
					*PrerequisiteId.ToString());
				return false;
			}
		}
	}

	for (const FSRRunModifierAugmentStackSaveData& StackSave : SaveData.AugmentStacks)
	{
		const USRRunAugmentDataAsset* Augment = AugmentsById.FindRef(StackSave.AugmentId).Get();
		if (StackSave.AugmentId.IsNone()
			|| !IsValid(Augment)
			|| StackSave.StackCount < 1
			|| StackSave.StackCount > FMath::Clamp(Augment->MaximumStacks, 1, FSRRunModifierResolver::MaximumSourceStacks)
			|| OutAugmentStacksById.Contains(StackSave.AugmentId))
		{
			OutFailureReason = FString::Printf(TEXT("Augment stack '%s' is unresolved, duplicated, or outside its cap."), *StackSave.AugmentId.ToString());
			return false;
		}
		OutAugmentStacksById.Add(StackSave.AugmentId, StackSave.StackCount);
	}

	for (const FSRActiveTrialState& TrialState : SaveData.ActiveTrials)
	{
		const USRTrialDataAsset* Trial = TrialsById.FindRef(TrialState.TrialId).Get();
		if (TrialState.TrialId.IsNone()
			|| !IsValid(Trial)
			|| TrialState.StartCycleIndex < 0
			|| TrialState.EndCycleIndexExclusive <= TrialState.StartCycleIndex
			|| TrialState.EndCycleIndexExclusive - TrialState.StartCycleIndex != Trial->DurationCycles
			|| OutActiveTrialsById.Contains(TrialState.TrialId))
		{
			OutFailureReason = FString::Printf(TEXT("Trial state '%s' is unresolved, duplicated, or has invalid Cycle bounds."), *TrialState.TrialId.ToString());
			return false;
		}
		OutActiveTrialsById.Add(TrialState.TrialId, TrialState);
	}

	TArray<FSRRunModifierSource> Sources;
	for (const FName TechnologyId : OutUnlockedTechnologyIds)
	{
		const USRTechnologyDataAsset* Technology = TechnologiesById.FindRef(TechnologyId).Get();
		if (!Technology->Effects.IsEmpty())
		{
			Sources.Add(BuildSource(TechnologyId, ESRRunModifierSourceKind::Technology, Technology->Priority, 1, Technology->Effects));
		}
	}
	for (const TPair<FName, int32>& Pair : OutAugmentStacksById)
	{
		const USRRunAugmentDataAsset* Augment = AugmentsById.FindRef(Pair.Key).Get();
		Sources.Add(BuildSource(Pair.Key, ESRRunModifierSourceKind::Augment, Augment->Priority, Pair.Value, Augment->Effects));
	}
	for (const TPair<FName, FSRActiveTrialState>& Pair : OutActiveTrialsById)
	{
		const USRTrialDataAsset* Trial = TrialsById.FindRef(Pair.Key).Get();
		Sources.Add(BuildSource(Pair.Key, ESRRunModifierSourceKind::Trial, Trial->Priority, 1, Trial->Effects));
	}
	return FSRRunModifierResolver::BuildContext(
		Sources,
		SaveData.ContextRevision,
		OutContext,
		OutFailureReason);
}

void USRRunModifierSubsystem::HandleGameCycleAdvanced(int32 CurrentCycleIndex)
{
	TArray<FSRActiveTrialState> ExpiredTrials;
	for (const TPair<FName, FSRActiveTrialState>& Pair : ActiveTrialsById)
	{
		if (CurrentCycleIndex >= Pair.Value.EndCycleIndexExclusive)
		{
			ExpiredTrials.Add(Pair.Value);
		}
	}
	if (ExpiredTrials.IsEmpty())
	{
		return;
	}

	ExpiredTrials.Sort([](const FSRActiveTrialState& Left, const FSRActiveTrialState& Right)
	{
		return Left.TrialId.ToString() < Right.TrialId.ToString();
	});
	for (const FSRActiveTrialState& TrialState : ExpiredTrials)
	{
		ActiveTrialsById.Remove(TrialState.TrialId);
	}
	RebuildContext();
	for (const FSRActiveTrialState& TrialState : ExpiredTrials)
	{
		OnTrialExpired.Broadcast(TrialState);
	}
}

void USRRunModifierSubsystem::RegisterConfiguredDataAssets()
{
	const USRSimulationSettings* Settings = GetDefault<USRSimulationSettings>();
	if (!Settings)
	{
		RebuildContext();
		return;
	}

	TArray<USRTechnologyDataAsset*> Technologies;
	for (const TSoftObjectPtr<USRTechnologyDataAsset>& AssetReference : Settings->TechnologyDataAssets)
	{
		if (USRTechnologyDataAsset* DataAsset = AssetReference.LoadSynchronous())
		{
			Technologies.Add(DataAsset);
		}
	}
	RegisterTechnologyDataAssets(Technologies);

	TArray<USRRunAugmentDataAsset*> Augments;
	for (const TSoftObjectPtr<USRRunAugmentDataAsset>& AssetReference : Settings->RunAugmentDataAssets)
	{
		if (USRRunAugmentDataAsset* DataAsset = AssetReference.LoadSynchronous())
		{
			Augments.Add(DataAsset);
		}
	}
	RegisterAugmentDataAssets(Augments);

	TArray<USRTrialDataAsset*> Trials;
	for (const TSoftObjectPtr<USRTrialDataAsset>& AssetReference : Settings->TrialDataAssets)
	{
		if (USRTrialDataAsset* DataAsset = AssetReference.LoadSynchronous())
		{
			Trials.Add(DataAsset);
		}
	}
	RegisterTrialDataAssets(Trials);

	if (CurrentContext.Revision == 0)
	{
		RebuildContext();
	}
}

void USRRunModifierSubsystem::UnlockConfiguredDefaultTechnologies()
{
	bool bUnlockedAny = true;
	while (bUnlockedAny)
	{
		bUnlockedAny = false;
		for (const TPair<FName, TObjectPtr<USRTechnologyDataAsset>>& Pair : TechnologiesById)
		{
			const USRTechnologyDataAsset* DataAsset = Pair.Value.Get();
			if (!IsValid(DataAsset) || !DataAsset->bUnlockedByDefault || UnlockedTechnologyIds.Contains(Pair.Key))
			{
				continue;
			}

			bool bHasAllPrerequisites = true;
			for (const FName PrerequisiteId : DataAsset->PrerequisiteTechnologyIds)
			{
				bHasAllPrerequisites &= UnlockedTechnologyIds.Contains(PrerequisiteId);
			}
			if (bHasAllPrerequisites)
			{
				UnlockedTechnologyIds.Add(Pair.Key);
				OnTechnologyUnlocked.Broadcast(Pair.Key);
				bUnlockedAny = true;
			}
		}
	}
}

bool USRRunModifierSubsystem::ValidateEffects(
	FName SourceId,
	ESRRunModifierSourceKind SourceKind,
	const TArray<FSRRunModifierEffect>& Effects) const
{
	if (Effects.IsEmpty())
	{
		return true;
	}
	FString FailureReason;
	const FSRRunModifierSource ValidationSource = BuildSource(SourceId, SourceKind, 0, 1, Effects);
	if (FSRRunModifierResolver::ValidateSource(ValidationSource, FailureReason))
	{
		return true;
	}
	SR_LOG(Augment, LogTemp, Warning, TEXT("Run modifier source '%s' was rejected: %s"), *SourceId.ToString(), *FailureReason);
	return false;
}

void USRRunModifierSubsystem::RebuildContext()
{
	TArray<FSRRunModifierSource> Sources;
	for (const FName TechnologyId : UnlockedTechnologyIds)
	{
		const TObjectPtr<USRTechnologyDataAsset>* FoundDataAsset = TechnologiesById.Find(TechnologyId);
		const USRTechnologyDataAsset* DataAsset = FoundDataAsset ? FoundDataAsset->Get() : nullptr;
		if (IsValid(DataAsset) && !DataAsset->Effects.IsEmpty())
		{
			Sources.Add(BuildSource(TechnologyId, ESRRunModifierSourceKind::Technology, DataAsset->Priority, 1, DataAsset->Effects));
		}
	}
	for (const TPair<FName, int32>& Pair : AugmentStacksById)
	{
		const TObjectPtr<USRRunAugmentDataAsset>* FoundDataAsset = AugmentsById.Find(Pair.Key);
		const USRRunAugmentDataAsset* DataAsset = FoundDataAsset ? FoundDataAsset->Get() : nullptr;
		if (IsValid(DataAsset) && Pair.Value > 0)
		{
			Sources.Add(BuildSource(Pair.Key, ESRRunModifierSourceKind::Augment, DataAsset->Priority, Pair.Value, DataAsset->Effects));
		}
	}
	for (const TPair<FName, FSRActiveTrialState>& Pair : ActiveTrialsById)
	{
		const TObjectPtr<USRTrialDataAsset>* FoundDataAsset = TrialsById.Find(Pair.Key);
		const USRTrialDataAsset* DataAsset = FoundDataAsset ? FoundDataAsset->Get() : nullptr;
		if (IsValid(DataAsset))
		{
			Sources.Add(BuildSource(Pair.Key, ESRRunModifierSourceKind::Trial, DataAsset->Priority, 1, DataAsset->Effects));
		}
	}

	FSRRunModifierContext NewContext;
	FString FailureReason;
	if (!FSRRunModifierResolver::BuildContext(Sources, NextContextRevision, NewContext, FailureReason))
	{
		SR_LOG(Augment, LogTemp, Error, TEXT("Run modifier context rebuild failed: %s"), *FailureReason);
		return;
	}
	++NextContextRevision;
	CurrentContext = MoveTemp(NewContext);
	OnRunModifierContextChanged.Broadcast(CurrentContext);
}

void USRRunModifierSubsystem::RebuildTechnologyStructureUnlocks()
{
	TechnologyUnlockedStructureIds.Reset();
	for (const FName TechnologyId : UnlockedTechnologyIds)
	{
		const TObjectPtr<USRTechnologyDataAsset>* FoundDataAsset = TechnologiesById.Find(TechnologyId);
		const USRTechnologyDataAsset* DataAsset = FoundDataAsset ? FoundDataAsset->Get() : nullptr;
		if (!IsValid(DataAsset))
		{
			continue;
		}
		for (const FName StructureId : DataAsset->UnlockedStructureIds)
		{
			if (!StructureId.IsNone())
			{
				TechnologyUnlockedStructureIds.Add(StructureId);
			}
		}
	}
}

void USRRunModifierSubsystem::BindTimeControlSubsystem()
{
	USRTimeControlSubsystem* TimeControlSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USRTimeControlSubsystem>() : nullptr;
	if (IsValid(TimeControlSubsystem))
	{
		TimeControlSubsystem->OnGameCycleAdvanced.RemoveDynamic(this, &USRRunModifierSubsystem::HandleGameCycleAdvanced);
		TimeControlSubsystem->OnGameCycleAdvanced.AddDynamic(this, &USRRunModifierSubsystem::HandleGameCycleAdvanced);
	}
}

void USRRunModifierSubsystem::UnbindTimeControlSubsystem()
{
	USRTimeControlSubsystem* TimeControlSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USRTimeControlSubsystem>() : nullptr;
	if (IsValid(TimeControlSubsystem))
	{
		TimeControlSubsystem->OnGameCycleAdvanced.RemoveDynamic(this, &USRRunModifierSubsystem::HandleGameCycleAdvanced);
	}
}
