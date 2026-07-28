#include "Simulation/SRAugmentSubsystem.h"

#include "Automation/SRResourceSystemContent.h"
#include "Automation/SRFacilityResourceV2Processor.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Logistics/SRSpaceLogisticsSubsystem.h"
#include "Engine/World.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "Utility/SRLog.h"
#include "Simulation/SRSimulationSettings.h"
#include "Simulation/SRTimeControlSubsystem.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"

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

	bool HasRarity(const TArray<FSRAugmentPackageDefinitionV2>& Definitions, ESRFacilityRarity Rarity)
	{
		for (const FSRAugmentPackageDefinitionV2& Definition : Definitions)
		{
			if (Definition.Rarity == Rarity)
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
		bDebugUnlockAllAugmentPackagesV2 = SimulationSettings->bDebugUnlockAllAugmentPackagesV2;
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
	RegisteredStructureIds.Reserve(RegisteredStructureDataAssets.Num() + StructureDataAssets.Num());
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

	RegisteredStructureDataAssets.Reserve(RegisteredStructureDataAssets.Num() + StructureDataAssets.Num());
	for (USRStructureDataAsset* StructureDataAsset : StructureDataAssets)
	{
		if (!IsValid(StructureDataAsset))
		{
			continue;
		}

		const FSRStructureData StructureData = StructureDataAsset->BuildData();
		if (StructureData.StructureId.IsNone())
		{
			continue;
		}

		bool bAlreadyRegistered = false;
		RegisteredStructureIds.Add(StructureData.StructureId, &bAlreadyRegistered);
		if (bAlreadyRegistered)
		{
			continue;
		}

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
	if (IsResourceV2RulesetActive())
	{
		GenerateResourceV2AugmentChoices(CycleIndex);
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

	UpdateHighTechBonusAfterOffer(InitialCandidates, GeneratedChoices);
	CurrentChoices = MoveTemp(GeneratedChoices);
	SR_LOG(Augment, LogTemp, Log, TEXT("USRAugmentSubsystem generated %d augment choices for cycle %d from %d candidates."),
		CurrentChoices.Num(),
		CycleIndex,
		InitialCandidates.Num());
	PublishCurrentChoices(CycleIndex);
}

bool USRAugmentSubsystem::SelectAugmentChoiceByIndex(int32 ChoiceIndex)
{
	if (!CurrentChoices.IsValidIndex(ChoiceIndex))
	{
		return false;
	}

	const FSRAugmentChoice SelectedChoice = CurrentChoices[ChoiceIndex];
	const bool bUnlocked = SelectedChoice.ChoiceKind == ESRAugmentChoiceKind::ResourceV2Package
		? UnlockAugmentPackageV2(SelectedChoice.PackageId)
		: UnlockStructure(SelectedChoice.StructureDataAsset.Get());
	if (!bUnlocked)
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

bool USRAugmentSubsystem::SelectAugmentChoiceByPackageId(FName PackageId)
{
	if (PackageId.IsNone())
	{
		return false;
	}
	for (int32 ChoiceIndex = 0; ChoiceIndex < CurrentChoices.Num(); ++ChoiceIndex)
	{
		if (CurrentChoices[ChoiceIndex].ChoiceKind == ESRAugmentChoiceKind::ResourceV2Package
			&& CurrentChoices[ChoiceIndex].PackageId == PackageId)
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

	bool bAlreadyUnlocked = false;
	UnlockedStructureIds.Add(StructureData.StructureId, &bAlreadyUnlocked);
	if (bAlreadyUnlocked)
	{
		return true;
	}

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

	const USRFacilityDataAsset* FacilityDataAsset = StructureData.FacilityDataAsset.Get();
	const bool bAvailableFacility = StructureData.bAvailableForConstruction
		&& !StructureData.bIsResourceDeposit
		&& IsValid(FacilityDataAsset);

	if (bDebugUnlockAllFacilitiesWithoutAugments && bAvailableFacility)
	{
		return true;
	}

	if (!bAvailableFacility
		|| FacilityDataAsset->FacilityKind != ESRFacilityKind::Standard
		|| FacilityDataAsset->Rarity == ESRFacilityRarity::Starting)
	{
		return true;
	}
	if (IsResourceV2RulesetActive())
	{
		// Legacy and unmapped assets stay available during migration. Resource V2
		// content is gated only when it declares a stable conditional Content id.
		if (FacilityDataAsset->FacilityDefinitionVersion
			< StarRovers::Facilities::CurrentFacilityDefinitionVersion
			|| FacilityDataAsset->ResourceV2ContentId.IsNone())
		{
			return true;
		}
		return IsFacilityContentUnlockedV2(FacilityDataAsset->ResourceV2ContentId);
	}

	return UnlockedStructureIds.Contains(StructureData.StructureId);
}

bool USRAugmentSubsystem::IsStructureUnlockedById(FName StructureId) const
{
	if (StructureId.IsNone())
	{
		return false;
	}

	for (const USRStructureDataAsset* StructureDataAsset : RegisteredStructureDataAssets)
	{
		if (IsValid(StructureDataAsset)
			&& StructureDataAsset->BuildData().StructureId == StructureId)
		{
			return IsStructureUnlocked(StructureDataAsset);
		}
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

bool USRAugmentSubsystem::UnlockAugmentPackageV2(FName PackageId)
{
	if (!IsResourceV2RulesetActive() || PackageId.IsNone())
	{
		return false;
	}
	if (SelectedAugmentPackageIdsV2.Contains(PackageId))
	{
		return true;
	}

	FSRAugmentPackageDefinitionV2 Definition;
	if (!FSRAugmentPackageContentV2::TryGetDefinition(PackageId, Definition))
	{
		return false;
	}
	const FSRAugmentBuildContextV2 Context = BuildResourceV2OfferContext();
	FString FailureReason;
	if (!FSRAugmentPackageContentV2::IsDefinitionEligible(Definition, Context, &FailureReason))
	{
		SR_LOG(Augment, LogTemp, Warning,
			TEXT("Resource V2 Augment Package %s could not be unlocked: %s"),
			*PackageId.ToString(),
			*FailureReason);
		return false;
	}

	SelectedAugmentPackageIdsV2.Add(PackageId);
	if (Definition.IsMacroDoctrine())
	{
		ActiveMacroDoctrineIdV2 = PackageId;
	}
	OnAugmentPackagesChanged.Broadcast();
	OnUnlockedStructuresChanged.Broadcast();
	SR_LOG(Augment, LogTemp, Display,
		TEXT("Resource V2 Augment Package unlocked: Package=%s Strategy=%s Role=%s Grants=(%s)"),
		*PackageId.ToString(),
		*Definition.StrategyId.ToString(),
		*StaticEnum<ESRAugmentPackageRoleV2>()->GetNameStringByValue(static_cast<int64>(Definition.PackageRole)),
		*FSRAugmentPackageContentV2::BuildGrantSummary(Definition));
	return true;
}

bool USRAugmentSubsystem::IsAugmentPackageUnlockedV2(FName PackageId) const
{
	if (SelectedAugmentPackageIdsV2.Contains(PackageId))
	{
		return true;
	}
	if (!bDebugUnlockAllAugmentPackagesV2)
	{
		return false;
	}
	FSRAugmentPackageDefinitionV2 Definition;
	return FSRAugmentPackageContentV2::TryGetDefinition(PackageId, Definition);
}

TArray<FName> USRAugmentSubsystem::GetSelectedAugmentPackageIdsV2() const
{
	TArray<FName> PackageIds = SelectedAugmentPackageIdsV2.Array();
	PackageIds.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});
	return PackageIds;
}

FName USRAugmentSubsystem::GetActiveMacroDoctrineIdV2() const
{
	return ActiveMacroDoctrineIdV2;
}

bool USRAugmentSubsystem::IsProcessTagRecipeUnlockedV2(FName TagId) const
{
	TArray<FName> PackageIds = GetSelectedAugmentPackageIdsV2();
	if (bDebugUnlockAllAugmentPackagesV2)
	{
		TArray<FSRAugmentPackageDefinitionV2> Definitions;
		FSRAugmentPackageContentV2::GetAllDefinitions(Definitions);
		for (const FSRAugmentPackageDefinitionV2& Definition : Definitions)
		{
			PackageIds.AddUnique(Definition.PackageId);
		}
	}
	return FSRAugmentPackageContentV2::IsProcessTagRecipeUnlocked(TagId, PackageIds);
}

bool USRAugmentSubsystem::IsFuelImprintRecipeUnlockedV2(FName ImprintId) const
{
	TArray<FName> PackageIds = GetSelectedAugmentPackageIdsV2();
	if (bDebugUnlockAllAugmentPackagesV2)
	{
		TArray<FSRAugmentPackageDefinitionV2> Definitions;
		FSRAugmentPackageContentV2::GetAllDefinitions(Definitions);
		for (const FSRAugmentPackageDefinitionV2& Definition : Definitions)
		{
			PackageIds.AddUnique(Definition.PackageId);
		}
	}
	return FSRAugmentPackageContentV2::IsFuelImprintRecipeUnlocked(ImprintId, PackageIds);
}

bool USRAugmentSubsystem::IsFacilityContentUnlockedV2(FName FacilityContentId) const
{
	if (bDebugUnlockAllFacilitiesWithoutAugments || bDebugUnlockAllAugmentPackagesV2)
	{
		return true;
	}
	return FSRAugmentPackageContentV2::IsFacilityContentUnlocked(
		FacilityContentId,
		GetSelectedAugmentPackageIdsV2());
}

bool USRAugmentSubsystem::IsLogisticsModuleUnlockedV2(FName ModuleId) const
{
	if (bDebugUnlockAllFacilitiesWithoutAugments || bDebugUnlockAllAugmentPackagesV2)
	{
		return true;
	}
	return FSRAugmentPackageContentV2::IsLogisticsModuleUnlocked(
		ModuleId,
		GetSelectedAugmentPackageIdsV2());
}

bool USRAugmentSubsystem::IsRouteProfileUnlockedV2(FName ProfileId) const
{
	if (bDebugUnlockAllFacilitiesWithoutAugments || bDebugUnlockAllAugmentPackagesV2)
	{
		return true;
	}
	return FSRAugmentPackageContentV2::IsRouteProfileUnlocked(
		ProfileId,
		GetSelectedAugmentPackageIdsV2());
}

FSRAugmentBuildContextV2 USRAugmentSubsystem::BuildResourceV2OfferContext() const
{
	FSRAugmentBuildContextV2 Context;
	Context.SelectedPackageIds = GetSelectedAugmentPackageIdsV2();
	Context.ActiveMacroDoctrineId = ActiveMacroDoctrineIdV2;
	FSRAugmentPackageContentV2::GetTechnologyFacilityContentIds(Context.AvailableFacilityContentIds);

	for (const USRStructureDataAsset* StructureDataAsset : RegisteredStructureDataAssets)
	{
		if (!IsValid(StructureDataAsset))
		{
			continue;
		}
		const FSRStructureData StructureData = StructureDataAsset->BuildData();
		if (const USRFacilityDataAsset* FacilityDataAsset = StructureData.FacilityDataAsset.Get();
			IsValid(FacilityDataAsset) && !FacilityDataAsset->ResourceV2ContentId.IsNone())
		{
			Context.AvailableFacilityContentIds.AddUnique(FacilityDataAsset->ResourceV2ContentId);
		}
	}

	// Registered Structure assets describe the project catalog, not the current
	// Run. Resource compatibility must come only from harvestable Card deposits
	// that were actually generated on constructible celestial bodies.
	if (const UWorld* World = GetWorld())
	{
		if (const USRCelestialBodyRegistrySubsystem* Registry =
			World->GetSubsystem<USRCelestialBodyRegistrySubsystem>())
		{
			TArray<AActor*> CelestialBodies;
			Registry->GetCelestialBodies(CelestialBodies);
			for (AActor* BodyActor : CelestialBodies)
			{
				if (!IsValid(BodyActor)
					|| !USRCelestialBodyRuntimeLibrary::GetCelestialCanConstruct(BodyActor))
				{
					continue;
				}
				const USRStructureInstanceManagerComponent* StructureManager =
					BodyActor->FindComponentByClass<USRStructureInstanceManagerComponent>();
				if (!IsValid(StructureManager))
				{
					continue;
				}

				TArray<FSRPlacedStructureInstance> PlacedStructures;
				StructureManager->GetPlacedStructures(PlacedStructures);
				for (const FSRPlacedStructureInstance& PlacedStructure : PlacedStructures)
				{
					FSRResourceDepositInstance Deposit;
					if (PlacedStructure.OccupantId.IsNone()
						|| !StructureManager->GetResourceDepositInstance(
							PlacedStructure.OccupantId,
							Deposit)
						|| !FSRResourceDepositAmountModel::CanHarvest(Deposit.RemainingAmount))
					{
						continue;
					}

					const USRResourceDataAsset* ResourceDataAsset =
						Deposit.ResourceDataAsset.Get();
					if (!IsValid(ResourceDataAsset)
						|| ResourceDataAsset->ResourceDefinitionVersion
							< StarRovers::Resources::CurrentResourceDefinitionVersion
						|| ResourceDataAsset->ResourceClass != ESRResourceClass::Card)
					{
						continue;
					}
					if (ResourceDataAsset->Family != ESRResourceFamily::None)
					{
						Context.AccessibleFamilies.AddUnique(ResourceDataAsset->Family);
					}
					if (ResourceDataAsset->NativeSpectrum != ESRResourceSpectrum::None)
					{
						Context.AccessibleSpectra.AddUnique(ResourceDataAsset->NativeSpectrum);
					}
					if (ResourceDataAsset->NativeGrade >= StarRovers::Resources::MinimumGrade
						&& ResourceDataAsset->NativeGrade <= StarRovers::Resources::MaximumGrade)
					{
						Context.AccessibleGrades.AddUnique(ResourceDataAsset->NativeGrade);
					}
				}
			}
		}
	}

	if (const USRSpaceLogisticsSubsystem* SpaceLogisticsSubsystem =
		GetWorld() ? GetWorld()->GetSubsystem<USRSpaceLogisticsSubsystem>() : nullptr)
	{
		TArray<FSRSpaceLogisticsHubEndpoint> HubEndpoints;
		SpaceLogisticsSubsystem->GetHubEndpoints(HubEndpoints);
		Context.HubEndpointCount = HubEndpoints.Num();
	}
	return Context;
}

bool USRAugmentSubsystem::IsFacilityRecipeUnlockedV2(
	const FSRFacilityInstance& FacilityInstance,
	FString& OutFailureReason) const
{
	OutFailureReason.Reset();
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsResourceV2RulesetActive() || !IsValid(FacilityDataAsset))
	{
		return true;
	}
	if (!FSRFacilityResourceV2Processor::ShouldRouteStandardProcessThroughResourceV2(FacilityDataAsset))
	{
		return true;
	}
	if (FacilityDataAsset->ResourceV2Process.ProcessRole == ESRFacilityProcessRoleV2::ApplyProcessTag
		&& !IsProcessTagRecipeUnlockedV2(
			FSRFacilityResourceV2Processor::ResolveProcessTagRecipeId(FacilityInstance)))
	{
		const FName RecipeId = FSRFacilityResourceV2Processor::ResolveProcessTagRecipeId(FacilityInstance);
		OutFailureReason = FString::Printf(
			TEXT("Process Tag recipe %s requires an Augment Package."),
			RecipeId.IsNone() ? TEXT("None") : *RecipeId.ToString());
		return false;
	}
	if (FacilityDataAsset->ResourceV2Process.ProcessRole == ESRFacilityProcessRoleV2::ApplyFuelImprint
		&& !IsFuelImprintRecipeUnlockedV2(
			FSRFacilityResourceV2Processor::ResolveFuelImprintRecipeId(FacilityInstance)))
	{
		const FName RecipeId = FSRFacilityResourceV2Processor::ResolveFuelImprintRecipeId(FacilityInstance);
		OutFailureReason = FString::Printf(
			TEXT("Fuel Imprint recipe %s requires an Augment Package."),
			RecipeId.IsNone() ? TEXT("None") : *RecipeId.ToString());
		return false;
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

bool USRAugmentSubsystem::IsResourceV2RulesetActive() const
{
	const USRSimulationSettings* SimulationSettings = GetDefault<USRSimulationSettings>();
	return IsValid(SimulationSettings)
		&& SimulationSettings->ResourceRulesetVersion == ESRResourceRulesetVersion::ResourceV2;
}

void USRAugmentSubsystem::GenerateResourceV2AugmentChoices(int32 CycleIndex)
{
	const FSRAugmentBuildContextV2 Context = BuildResourceV2OfferContext();
	TArray<FSRAugmentPackageDefinitionV2> EligibleDefinitions;
	FSRAugmentPackageContentV2::BuildEligibleDefinitions(Context, EligibleDefinitions);
	if (EligibleDefinitions.IsEmpty())
	{
		SR_LOG(Augment, LogTemp, Warning,
			TEXT("No eligible Resource V2 Augment Packages remain for cycle %d."),
			CycleIndex);
		return;
	}

	FSRAugmentOfferGenerationRulesV2 OfferRules;
	OfferRules.ChoiceCount = FMath::Max(1, ChoicesPerOffer);
	OfferRules.BasicWeight = FMath::Max(0.0f, BasicChancePercent);
	OfferRules.AdvancedWeight = FMath::Max(0.0f, AdvancedChancePercent);
	OfferRules.HighTechWeight = GetHighTechEffectiveChancePercent();
	OfferRules.RandomSeed = HashCombineFast(
		GetTypeHash(AugmentRandomSeed),
		HashCombineFast(GetTypeHash(CycleIndex), GetTypeHash(Context.SelectedPackageIds.Num())));
	OfferRules.RecentlyOfferedPackageIds = PreviousAugmentOfferPackageIdsV2;

	TArray<FSRAugmentPackageOfferV2> PackageOffers;
	FSRAugmentPackageContentV2::GenerateOffer(Context, OfferRules, PackageOffers);
	TArray<FSRAugmentChoice> GeneratedChoices;
	GeneratedChoices.Reserve(PackageOffers.Num());
	for (const FSRAugmentPackageOfferV2& PackageOffer : PackageOffers)
	{
		FSRAugmentPackageDefinitionV2 Definition;
		if (!FSRAugmentPackageContentV2::TryGetDefinition(PackageOffer.PackageId, Definition))
		{
			continue;
		}

		FSRAugmentChoice& Choice = GeneratedChoices.AddDefaulted_GetRef();
		Choice.ChoiceKind = ESRAugmentChoiceKind::ResourceV2Package;
		Choice.PackageId = Definition.PackageId;
		Choice.StrategyId = Definition.StrategyId;
		Choice.PackageRole = Definition.PackageRole;
		Choice.OfferRole = PackageOffer.OfferRole;
		Choice.StructureId = Definition.PackageId;
		Choice.DisplayName = Definition.DisplayName;
		Choice.Description = Definition.Description;
		Choice.Rarity = Definition.Rarity;
		Choice.GrantSummary = FText::FromString(
			FSRAugmentPackageContentV2::BuildGrantSummary(Definition));
		Choice.ExampleLinePreview = Definition.ExampleLinePreview;
	}
	if (GeneratedChoices.IsEmpty())
	{
		return;
	}

	PreviousAugmentOfferPackageIdsV2.Reset(PackageOffers.Num());
	for (const FSRAugmentPackageOfferV2& PackageOffer : PackageOffers)
	{
		PreviousAugmentOfferPackageIdsV2.Add(PackageOffer.PackageId);
	}
	UpdateResourceV2HighTechBonusAfterOffer(EligibleDefinitions, GeneratedChoices);
	CurrentChoices = MoveTemp(GeneratedChoices);
	SR_LOG(Augment, LogTemp, Display,
		TEXT("Generated %d Resource V2 Augment Package choices for cycle %d from %d eligible Packages."),
		CurrentChoices.Num(),
		CycleIndex,
		EligibleDefinitions.Num());
	PublishCurrentChoices(CycleIndex);
}

void USRAugmentSubsystem::PublishCurrentChoices(int32 CycleIndex)
{
	if (CurrentChoices.IsEmpty())
	{
		return;
	}
	CurrentAugmentChoiceCycleIndex = CycleIndex;
	if (bPauseSimulationDuringChoice)
	{
		if (USRTimeControlSubsystem* TimeControlSubsystem =
			GetWorld() ? GetWorld()->GetSubsystem<USRTimeControlSubsystem>() : nullptr)
		{
			TimeControlSubsystem->SetSimulationPaused(true);
			bPausedSimulationForCurrentChoice = true;
		}
	}
	OnAugmentChoicesReady.Broadcast(CurrentChoices, CurrentAugmentChoiceCycleIndex);
}

void USRAugmentSubsystem::UpdateResourceV2HighTechBonusAfterOffer(
	const TArray<FSRAugmentPackageDefinitionV2>& EligibleDefinitions,
	const TArray<FSRAugmentChoice>& GeneratedChoices)
{
	if (HasRarity(GeneratedChoices, ESRFacilityRarity::HighTech))
	{
		HighTechBonusChancePercent = 0.0f;
		return;
	}
	if (!HasRarity(EligibleDefinitions, ESRFacilityRarity::HighTech))
	{
		return;
	}
	HighTechBonusChancePercent = FMath::Clamp(
		HighTechBonusChancePercent + FMath::Max(0.0f, HighTechPityIncreasePercent),
		0.0f,
		FMath::Max(0.0f, HighTechPityCapPercent));
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
	const bool bStandardFacility = StructureData.bAvailableForConstruction
		&& !StructureData.bIsResourceDeposit
		&& IsValid(FacilityDataAsset)
		&& FacilityDataAsset->FacilityKind == ESRFacilityKind::Standard
		&& FacilityDataAsset->Rarity != ESRFacilityRarity::Starting;
	if (!bStandardFacility)
	{
		return false;
	}
	if (!IsResourceV2RulesetActive())
	{
		return true;
	}
	return FacilityDataAsset->FacilityDefinitionVersion
		>= StarRovers::Facilities::CurrentFacilityDefinitionVersion
		&& !FacilityDataAsset->ResourceV2ContentId.IsNone()
		&& !IsFacilityContentUnlockedV2(FacilityDataAsset->ResourceV2ContentId);
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
	if (!IsValid(StructureDataAsset))
	{
		return false;
	}

	const FSRStructureData StructureData = StructureDataAsset->BuildData();
	const USRFacilityDataAsset* FacilityDataAsset = StructureData.FacilityDataAsset.Get();
	if (!StructureData.bAvailableForConstruction
		|| StructureData.bIsResourceDeposit
		|| !IsValid(FacilityDataAsset)
		|| FacilityDataAsset->FacilityKind != ESRFacilityKind::Standard
		|| FacilityDataAsset->Rarity == ESRFacilityRarity::Starting)
	{
		return false;
	}

	if (bDebugUnlockAllFacilitiesWithoutAugments)
	{
		return false;
	}

	return !StructureData.StructureId.IsNone()
		&& !UnlockedStructureIds.Contains(StructureData.StructureId)
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
	Choice.DisplayName = StructureData.DisplayName;
	Choice.Description = StructureData.Description;
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
