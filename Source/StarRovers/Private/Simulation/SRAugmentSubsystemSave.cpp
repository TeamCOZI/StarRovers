#include "Simulation/SRAugmentSubsystem.h"

#include "Structure/SRStructureDataAsset.h"

namespace
{
	bool ValidateUniqueNames(
		TConstArrayView<FName> Names,
		const TCHAR* Label,
		FString& OutFailureReason)
	{
		TSet<FName> UniqueNames;
		for (const FName Name : Names)
		{
			if (Name.IsNone() || UniqueNames.Contains(Name))
			{
				OutFailureReason = FString::Printf(
					TEXT("Augment save contains a missing or duplicate %s id: %s."),
					Label,
					*Name.ToString());
				return false;
			}
			UniqueNames.Add(Name);
		}
		return true;
	}

	FSRAugmentChoice BuildPackageChoice(
		const FSRAugmentPackageDefinitionV2& Definition,
		ESRAugmentOfferRoleV2 OfferRole)
	{
		FSRAugmentChoice Choice;
		Choice.ChoiceKind = ESRAugmentChoiceKind::ResourceV2Package;
		Choice.PackageId = Definition.PackageId;
		Choice.StrategyId = Definition.StrategyId;
		Choice.PackageRole = Definition.PackageRole;
		Choice.OfferRole = OfferRole;
		Choice.StructureId = Definition.PackageId;
		Choice.DisplayName = Definition.DisplayName;
		Choice.Description = Definition.Description;
		Choice.Rarity = Definition.Rarity;
		Choice.GrantSummary = FText::FromString(
			FSRAugmentPackageContentV2::BuildGrantSummary(Definition));
		Choice.ExampleLinePreview = Definition.ExampleLinePreview;
		return Choice;
	}
}

void USRAugmentSubsystem::ExportSaveData(FSRAugmentSaveData& OutSaveData) const
{
	OutSaveData = FSRAugmentSaveData();
	OutSaveData.UnlockedStructureIds = UnlockedStructureIds.Array();
	OutSaveData.UnlockedStructureIds.Sort([](const FName Left, const FName Right)
	{
		return Left.LexicalLess(Right);
	});
	OutSaveData.SelectedPackageIdsV2 = GetSelectedAugmentPackageIdsV2();
	OutSaveData.PreviousOfferPackageIdsV2 = PreviousAugmentOfferPackageIdsV2;
	OutSaveData.ActiveMacroDoctrineIdV2 = ActiveMacroDoctrineIdV2;
	OutSaveData.CurrentChoiceCycleIndex = CurrentAugmentChoiceCycleIndex;
	OutSaveData.HighTechBonusChancePercent = HighTechBonusChancePercent;
	OutSaveData.bPausedSimulationForCurrentChoice =
		bPausedSimulationForCurrentChoice;
	OutSaveData.CurrentChoices.Reserve(CurrentChoices.Num());
	for (const FSRAugmentChoice& Choice : CurrentChoices)
	{
		FSRAugmentChoiceSaveData& SavedChoice =
			OutSaveData.CurrentChoices.AddDefaulted_GetRef();
		SavedChoice.ChoiceKind = Choice.ChoiceKind;
		SavedChoice.PackageId = Choice.PackageId;
		SavedChoice.StructureId = Choice.StructureId;
		SavedChoice.OfferRole = Choice.OfferRole;
	}
}

bool USRAugmentSubsystem::ImportSaveData(
	const FSRAugmentSaveData& SaveData,
	FString& OutFailureReason)
{
	OutFailureReason.Reset();
	if (!SaveData.IsSupportedVersion())
	{
		OutFailureReason = FString::Printf(
			TEXT("Unsupported Augment save version %d."),
			SaveData.Version);
		return false;
	}
	if (!ValidateUniqueNames(
			SaveData.UnlockedStructureIds,
			TEXT("unlocked Structure"),
			OutFailureReason)
		|| !ValidateUniqueNames(
			SaveData.SelectedPackageIdsV2,
			TEXT("selected Package"),
			OutFailureReason)
		|| !ValidateUniqueNames(
			SaveData.PreviousOfferPackageIdsV2,
			TEXT("previous Offer Package"),
			OutFailureReason))
	{
		return false;
	}
	if (!FMath::IsFinite(SaveData.HighTechBonusChancePercent)
		|| SaveData.HighTechBonusChancePercent < 0.0f
		|| SaveData.HighTechBonusChancePercent > FMath::Max(0.0f, HighTechPityCapPercent)
		|| SaveData.CurrentChoiceCycleIndex < 0
		|| (!SaveData.CurrentChoices.IsEmpty()
			&& SaveData.CurrentChoiceCycleIndex <= 0))
	{
		OutFailureReason = TEXT("Augment save contains invalid pity or pending-cycle state.");
		return false;
	}

	TSet<FName> ImportedPackages;
	FName ResolvedMacroDoctrine = NAME_None;
	for (const FName PackageId : SaveData.SelectedPackageIdsV2)
	{
		FSRAugmentPackageDefinitionV2 Definition;
		if (!FSRAugmentPackageContentV2::TryGetDefinition(PackageId, Definition))
		{
			OutFailureReason = FString::Printf(
				TEXT("Augment save references unknown selected Package %s."),
				*PackageId.ToString());
			return false;
		}
		ImportedPackages.Add(PackageId);
		if (Definition.IsMacroDoctrine())
		{
			if (!ResolvedMacroDoctrine.IsNone())
			{
				OutFailureReason = TEXT("Augment save selects more than one Macro Doctrine.");
				return false;
			}
			ResolvedMacroDoctrine = PackageId;
		}
	}
	if (ResolvedMacroDoctrine != SaveData.ActiveMacroDoctrineIdV2)
	{
		OutFailureReason = TEXT("Augment save Macro Doctrine does not match its selected Packages.");
		return false;
	}
	for (const FName PackageId : SaveData.PreviousOfferPackageIdsV2)
	{
		FSRAugmentPackageDefinitionV2 Definition;
		if (!FSRAugmentPackageContentV2::TryGetDefinition(PackageId, Definition))
		{
			OutFailureReason = FString::Printf(
				TEXT("Augment save references unknown previous Offer Package %s."),
				*PackageId.ToString());
			return false;
		}
	}

	TArray<FSRAugmentChoice> ImportedChoices;
	TSet<FName> CurrentChoiceIds;
	ImportedChoices.Reserve(SaveData.CurrentChoices.Num());
	for (const FSRAugmentChoiceSaveData& SavedChoice : SaveData.CurrentChoices)
	{
		const FName StableChoiceId = SavedChoice.ChoiceKind
			== ESRAugmentChoiceKind::ResourceV2Package
			? SavedChoice.PackageId
			: SavedChoice.StructureId;
		if (StableChoiceId.IsNone() || CurrentChoiceIds.Contains(StableChoiceId))
		{
			OutFailureReason = TEXT("Augment save contains a missing or duplicate pending choice.");
			return false;
		}
		CurrentChoiceIds.Add(StableChoiceId);

		if (SavedChoice.ChoiceKind == ESRAugmentChoiceKind::ResourceV2Package)
		{
			FSRAugmentPackageDefinitionV2 Definition;
			if (ImportedPackages.Contains(SavedChoice.PackageId)
				|| !FSRAugmentPackageContentV2::TryGetDefinition(
					SavedChoice.PackageId,
					Definition))
			{
				OutFailureReason = FString::Printf(
					TEXT("Pending Augment Package %s is unknown or already selected."),
					*SavedChoice.PackageId.ToString());
				return false;
			}
			ImportedChoices.Add(BuildPackageChoice(Definition, SavedChoice.OfferRole));
			continue;
		}

		USRStructureDataAsset* MatchedStructure = nullptr;
		for (USRStructureDataAsset* RegisteredStructure : RegisteredStructureDataAssets)
		{
			if (IsValid(RegisteredStructure)
				&& RegisteredStructure->BuildData().StructureId
					== SavedChoice.StructureId)
			{
				MatchedStructure = RegisteredStructure;
				break;
			}
		}
		if (!IsValid(MatchedStructure))
		{
			OutFailureReason = FString::Printf(
				TEXT("Pending Legacy Structure choice %s is unavailable."),
				*SavedChoice.StructureId.ToString());
			return false;
		}
		ImportedChoices.Add(BuildAugmentChoice(MatchedStructure));
	}

	UnlockedStructureIds.Reset();
	for (const FName StructureId : SaveData.UnlockedStructureIds)
	{
		UnlockedStructureIds.Add(StructureId);
	}
	SelectedAugmentPackageIdsV2 = MoveTemp(ImportedPackages);
	PreviousAugmentOfferPackageIdsV2 = SaveData.PreviousOfferPackageIdsV2;
	ActiveMacroDoctrineIdV2 = SaveData.ActiveMacroDoctrineIdV2;
	CurrentChoices = MoveTemp(ImportedChoices);
	CurrentAugmentChoiceCycleIndex = SaveData.CurrentChoices.IsEmpty()
		? 0
		: SaveData.CurrentChoiceCycleIndex;
	HighTechBonusChancePercent = SaveData.HighTechBonusChancePercent;
	bPausedSimulationForCurrentChoice =
		SaveData.bPausedSimulationForCurrentChoice && !CurrentChoices.IsEmpty();

	OnAugmentPackagesChanged.Broadcast();
	OnUnlockedStructuresChanged.Broadcast();
	if (!CurrentChoices.IsEmpty())
	{
		OnAugmentChoicesReady.Broadcast(
			CurrentChoices,
			CurrentAugmentChoiceCycleIndex);
	}
	return true;
}
