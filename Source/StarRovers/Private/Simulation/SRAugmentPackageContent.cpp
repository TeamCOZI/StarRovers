#include "Simulation/SRAugmentPackageContent.h"

#include "Automation/SRResourceSystemContent.h"
#include "Logistics/SRConditionedTransitV2.h"
#include "Logistics/SRFleetCapacityV2.h"

namespace
{
	const FName TagImprinterId(TEXT("TagImprinter"));
	const FName FuelImprinterId(TEXT("FuelImprinter"));
	const FName StellarFuelFabricatorId(TEXT("StellarFuelFabricator"));
	const FName MacroDoctrineGroup(TEXT("MacroDoctrine"));

	FSRAugmentPackageDefinitionV2 MakePackage(
		const TCHAR* PackageId,
		const TCHAR* StrategyId,
		const FText& DisplayName,
		const FText& Description,
		ESRAugmentPackageRoleV2 PackageRole,
		ESRFacilityRarity Rarity,
		const FText& ExampleLinePreview)
	{
		FSRAugmentPackageDefinitionV2 Definition;
		Definition.PackageId = FName(PackageId);
		Definition.StrategyId = FName(StrategyId);
		Definition.DisplayName = DisplayName;
		Definition.Description = Description;
		Definition.PackageRole = PackageRole;
		Definition.Rarity = Rarity;
		Definition.ExampleLinePreview = ExampleLinePreview;
		return Definition;
	}

	const TArray<FSRAugmentPackageDefinitionV2>& GetDefinitions()
	{
		static const TArray<FSRAugmentPackageDefinitionV2> Definitions = []
		{
			TArray<FSRAugmentPackageDefinitionV2> Result;
			const TArray<ESRResourceFamily> CardFamilies = {
				ESRResourceFamily::Metal,
				ESRResourceFamily::Crystal,
				ESRResourceFamily::Organic,
				ESRResourceFamily::Plasma,
				ESRResourceFamily::Void,
			};

			FSRAugmentPackageDefinitionV2 StateResonator = MakePackage(
				TEXT("StateResonator"),
				TEXT("StateControl"),
				NSLOCTEXT("StarRoversAugmentV2", "StateResonator", "State Resonator"),
				NSLOCTEXT("StarRoversAugmentV2", "StateResonatorDescription", "Unlocks the Overtone recipe and its positive-State trigger preview."),
				ESRAugmentPackageRoleV2::Enabler,
				ESRFacilityRarity::Basic,
				NSLOCTEXT("StarRoversAugmentV2", "StateResonatorLine", "Tag Imprinter -> Family State cycle -> Overtone payoff"));
			StateResonator.RequiredFacilityContentIds.Add(TagImprinterId);
			StateResonator.CompatibleFamilies = CardFamilies;
			StateResonator.GrantedProcessTagIds.Add(
				FSRResourceSystemContent::GetProcessTagId(ESRProcessTagContentV2::Overtone));
			Result.Add(MoveTemp(StateResonator));

			FSRAugmentPackageDefinitionV2 RecoveryDividend = MakePackage(
				TEXT("RecoveryDividend"),
				TEXT("StateControl"),
				NSLOCTEXT("StarRoversAugmentV2", "RecoveryDividend", "Recovery Dividend"),
				NSLOCTEXT("StarRoversAugmentV2", "RecoveryDividendDescription", "Unlocks Reclamation so deliberate negative-State recovery becomes a viable Line."),
				ESRAugmentPackageRoleV2::Pivot,
				ESRFacilityRarity::Basic,
				NSLOCTEXT("StarRoversAugmentV2", "RecoveryDividendLine", "Negative State -> recovery Archetype -> Reclamation payoff"));
			RecoveryDividend.RequiredFacilityContentIds.Add(TagImprinterId);
			RecoveryDividend.CompatibleFamilies = CardFamilies;
			RecoveryDividend.GrantedProcessTagIds.Add(
				FSRResourceSystemContent::GetProcessTagId(ESRProcessTagContentV2::Reclamation));
			Result.Add(MoveTemp(RecoveryDividend));

			FSRAugmentPackageDefinitionV2 FullHouseMatrix = MakePackage(
				TEXT("FullHouseMatrix"),
				TEXT("Composition"),
				NSLOCTEXT("StarRoversAugmentV2", "FullHouseMatrix", "Full-House Matrix"),
				NSLOCTEXT("StarRoversAugmentV2", "FullHouseMatrixDescription", "Unlocks Twin Seal and exposes the Pair-plus-Triple fuel plan."),
				ESRAugmentPackageRoleV2::Payoff,
				ESRFacilityRarity::Advanced,
				NSLOCTEXT("StarRoversAugmentV2", "FullHouseMatrixLine", "R2 + B2 + G4 + Y4 + R4 -> Full House"));
			FullHouseMatrix.RequiredFacilityContentIds = { FuelImprinterId, StellarFuelFabricatorId };
			FullHouseMatrix.RequiredGrades = { 2, 4 };
			FullHouseMatrix.GrantedFuelImprintIds.Add(
				FSRResourceSystemContent::GetFuelImprintId(ESRFuelImprintContentV2::TwinSeal));
			Result.Add(MoveTemp(FullHouseMatrix));

			FSRAugmentPackageDefinitionV2 PrismaticFocus = MakePackage(
				TEXT("PrismaticFocus"),
				TEXT("Composition"),
				NSLOCTEXT("StarRoversAugmentV2", "PrismaticFocus", "Prismatic Focus"),
				NSLOCTEXT("StarRoversAugmentV2", "PrismaticFocusDescription", "Unlocks the one-per-batch Prismatic Catalyst for a four-Spectrum fuel batch."),
				ESRAugmentPackageRoleV2::Capstone,
				ESRFacilityRarity::HighTech,
				NSLOCTEXT("StarRoversAugmentV2", "PrismaticFocusLine", "Four Spectra -> Prismatic Catalyst -> final C +1"));
			PrismaticFocus.RequiredPackageIds.Add(FName(TEXT("FullHouseMatrix")));
			PrismaticFocus.RequiredFacilityContentIds = { FuelImprinterId, StellarFuelFabricatorId };
			PrismaticFocus.CompatibleSpectra = {
				ESRResourceSpectrum::Red,
				ESRResourceSpectrum::Green,
				ESRResourceSpectrum::Blue,
				ESRResourceSpectrum::Yellow,
			};
			PrismaticFocus.bRequiresAllCompatibleSpectra = true;
			PrismaticFocus.GrantedFuelImprintIds.Add(
				FSRResourceSystemContent::GetFuelImprintId(ESRFuelImprintContentV2::PrismaticCatalyst));
			Result.Add(MoveTemp(PrismaticFocus));

			auto AddMacroDoctrine = [&Result, &CardFamilies](
				const TCHAR* PackageId,
				const TCHAR* StrategyId,
				const FText& DisplayName,
				const FText& Description,
				const FText& Preview,
				FName ProcessTagId,
				FName FuelImprintId,
				int32 MinimumHubCount,
				FName GrantedRouteProfileId)
			{
				FSRAugmentPackageDefinitionV2 Definition = MakePackage(
					PackageId,
					StrategyId,
					DisplayName,
					Description,
					ESRAugmentPackageRoleV2::MacroDoctrine,
					ESRFacilityRarity::Advanced,
					Preview);
				Definition.RequiredFacilityContentIds = { FuelImprinterId, StellarFuelFabricatorId };
				Definition.CompatibleFamilies = CardFamilies;
				Definition.MinimumHubEndpointCount = MinimumHubCount;
				Definition.DoctrineExclusionGroup = MacroDoctrineGroup;
				if (!ProcessTagId.IsNone())
				{
					Definition.RequiredFacilityContentIds.AddUnique(TagImprinterId);
					Definition.GrantedProcessTagIds.Add(ProcessTagId);
				}
				Definition.GrantedFuelImprintIds.Add(FuelImprintId);
				if (!GrantedRouteProfileId.IsNone())
				{
					Definition.GrantedRouteProfileIds.Add(GrantedRouteProfileId);
				}
				Result.Add(MoveTemp(Definition));
			};

			AddMacroDoctrine(
				TEXT("ConvergenceProtocol"),
				TEXT("DistributedConvergence"),
				NSLOCTEXT("StarRoversAugmentV2", "ConvergenceProtocol", "Convergence Protocol"),
				NSLOCTEXT("StarRoversAugmentV2", "ConvergenceProtocolDescription", "Unlocks Convergence Seal for completed Cards exported from at least three Origins."),
				NSLOCTEXT("StarRoversAugmentV2", "ConvergenceProtocolLine", "Three local Card Lines -> synchronized export -> Convergence Seal"),
				NAME_None,
				FSRResourceSystemContent::GetFuelImprintId(ESRFuelImprintContentV2::ConvergenceSeal),
				3,
				NAME_None);
			AddMacroDoctrine(
				TEXT("CentralConvergence"),
				TEXT("CentralFactory"),
				NSLOCTEXT("StarRoversAugmentV2", "CentralConvergence", "Central Convergence"),
				NSLOCTEXT("StarRoversAugmentV2", "CentralConvergenceDescription", "Unlocks Landing Charge and Foundry Seal for a central assembly world."),
				NSLOCTEXT("StarRoversAugmentV2", "CentralConvergenceLine", "Raw imports -> central processing -> Foundry Seal"),
				FSRResourceSystemContent::GetProcessTagId(ESRProcessTagContentV2::LandingCharge),
				FSRResourceSystemContent::GetFuelImprintId(ESRFuelImprintContentV2::FoundrySeal),
				2,
				FSRFleetCapacityV2::GetRouteProfileId(ESRSpaceLogisticsRouteProfileV2::BulkRawHold));
			AddMacroDoctrine(
				TEXT("PilgrimCircuit"),
				TEXT("PilgrimRoute"),
				NSLOCTEXT("StarRoversAugmentV2", "PilgrimCircuit", "Pilgrim Circuit"),
				NSLOCTEXT("StarRoversAugmentV2", "PilgrimCircuitDescription", "Unlocks Pilgrim Charge and Pilgrim Seal for sequential inter-body processing."),
				NSLOCTEXT("StarRoversAugmentV2", "PilgrimCircuitLine", "Origin -> external processing -> route circuit -> Pilgrim Seal"),
				FSRResourceSystemContent::GetProcessTagId(ESRProcessTagContentV2::PilgrimCharge),
				FSRResourceSystemContent::GetFuelImprintId(ESRFuelImprintContentV2::PilgrimSeal),
				2,
				NAME_None);

			auto AddTransitEngine = [&Result](
				const TCHAR* PackageId,
				const TCHAR* StrategyId,
				const FText& DisplayName,
				const FText& Description,
				const FText& Preview,
				ESRResourceFamily Family,
				const TCHAR* GrantedModuleId)
			{
				FSRAugmentPackageDefinitionV2 Definition = MakePackage(
					PackageId,
					StrategyId,
					DisplayName,
					Description,
					ESRAugmentPackageRoleV2::Engine,
					ESRFacilityRarity::Advanced,
					Preview);
				Definition.CompatibleFamilies.Add(Family);
				Definition.MinimumHubEndpointCount = 2;
				Definition.GrantedLogisticsModuleIds.Add(FName(GrantedModuleId));
				Definition.GrantedRouteProfileIds.Add(
					FSRFleetCapacityV2::GetRouteProfileId(ESRSpaceLogisticsRouteProfileV2::ConditionedHold));
				Result.Add(MoveTemp(Definition));
			};

			AddTransitEngine(
				TEXT("DeepSpaceTempering"), TEXT("ConditionedTransit"),
				NSLOCTEXT("StarRoversAugmentV2", "DeepSpaceTempering", "Deep-Space Tempering"),
				NSLOCTEXT("StarRoversAugmentV2", "DeepSpaceTemperingDescription", "Unlocks Cryogenic Hold for an explicit Cold action during approved Metal transit."),
				NSLOCTEXT("StarRoversAugmentV2", "DeepSpaceTemperingLine", "Hot Metal export -> Cryogenic Hold -> Cold completion"),
				ESRResourceFamily::Metal, TEXT("CryogenicHold"));
			AddTransitEngine(
				TEXT("BioArkFreight"), TEXT("ConditionedTransit"),
				NSLOCTEXT("StarRoversAugmentV2", "BioArkFreight", "Bio-Ark Freight"),
				NSLOCTEXT("StarRoversAugmentV2", "BioArkFreightDescription", "Unlocks Bio-Culture Hold for one explicit Organic Growth cycle in transit."),
				NSLOCTEXT("StarRoversAugmentV2", "BioArkFreightLine", "Organic export -> Bio-Culture Hold -> Growth completion"),
				ESRResourceFamily::Organic, TEXT("BioCultureHold"));
			AddTransitEngine(
				TEXT("GroundedTransit"), TEXT("ConditionedTransit"),
				NSLOCTEXT("StarRoversAugmentV2", "GroundedTransit", "Grounded Transit"),
				NSLOCTEXT("StarRoversAugmentV2", "GroundedTransitDescription", "Unlocks Grounding Hold for one explicit Plasma Discharge in transit."),
				NSLOCTEXT("StarRoversAugmentV2", "GroundedTransitLine", "Amplified Plasma export -> Grounding Hold -> Discharge"),
				ESRResourceFamily::Plasma, TEXT("GroundingHold"));

			return Result;
		}();
		return Definitions;
	}

	bool ContainsAllNames(const TArray<FName>& Haystack, const TArray<FName>& Needles)
	{
		for (const FName Needle : Needles)
		{
			if (!Haystack.Contains(Needle))
			{
				return false;
			}
		}
		return true;
	}

	template <typename T>
	bool ContainsAny(const TArray<T>& Haystack, const TArray<T>& Needles)
	{
		if (Needles.IsEmpty())
		{
			return true;
		}
		for (const T& Needle : Needles)
		{
			if (Haystack.Contains(Needle))
			{
				return true;
			}
		}
		return false;
	}

	float ResolveRarityWeight(
		ESRFacilityRarity Rarity,
		const FSRAugmentOfferGenerationRulesV2& Rules)
	{
		switch (Rarity)
		{
		case ESRFacilityRarity::Basic: return FMath::Max(0.0f, Rules.BasicWeight);
		case ESRFacilityRarity::Advanced: return FMath::Max(0.0f, Rules.AdvancedWeight);
		case ESRFacilityRarity::HighTech: return FMath::Max(0.0f, Rules.HighTechWeight);
		default: return 0.0f;
		}
	}

	int32 PickWeightedIndex(
		const TArray<FSRAugmentPackageDefinitionV2>& Candidates,
		const TArray<int32>& CandidateIndices,
		const FSRAugmentOfferGenerationRulesV2& Rules,
		FRandomStream& RandomStream)
	{
		if (CandidateIndices.IsEmpty())
		{
			return INDEX_NONE;
		}

		float TotalWeight = 0.0f;
		for (const int32 CandidateIndex : CandidateIndices)
		{
			if (Candidates.IsValidIndex(CandidateIndex))
			{
				TotalWeight += ResolveRarityWeight(Candidates[CandidateIndex].Rarity, Rules);
			}
		}
		if (TotalWeight <= UE_SMALL_NUMBER)
		{
			return CandidateIndices[RandomStream.RandRange(0, CandidateIndices.Num() - 1)];
		}

		const float Roll = RandomStream.FRandRange(0.0f, TotalWeight);
		float AccumulatedWeight = 0.0f;
		for (const int32 CandidateIndex : CandidateIndices)
		{
			AccumulatedWeight += ResolveRarityWeight(Candidates[CandidateIndex].Rarity, Rules);
			if (Roll <= AccumulatedWeight)
			{
				return CandidateIndex;
			}
		}
		return CandidateIndices.Last();
	}

	bool DoesPackageGrantNewContent(
		const FSRAugmentPackageDefinitionV2& Definition,
		const FSRAugmentBuildContextV2& Context)
	{
		for (const FName TagId : Definition.GrantedProcessTagIds)
		{
			if (!FSRAugmentPackageContentV2::IsProcessTagRecipeUnlocked(
				TagId,
				Context.SelectedPackageIds))
			{
				return true;
			}
		}
		for (const FName ImprintId : Definition.GrantedFuelImprintIds)
		{
			if (!FSRAugmentPackageContentV2::IsFuelImprintRecipeUnlocked(
				ImprintId,
				Context.SelectedPackageIds))
			{
				return true;
			}
		}
		for (const FName FacilityId : Definition.GrantedFacilityContentIds)
		{
			if (!FSRAugmentPackageContentV2::IsFacilityContentUnlocked(
				FacilityId,
				Context.SelectedPackageIds))
			{
				return true;
			}
		}
		for (const FName ModuleId : Definition.GrantedLogisticsModuleIds)
		{
			if (!FSRAugmentPackageContentV2::IsLogisticsModuleUnlocked(
				ModuleId,
				Context.SelectedPackageIds))
			{
				return true;
			}
		}
		for (const FName ProfileId : Definition.GrantedRouteProfileIds)
		{
			if (!FSRAugmentPackageContentV2::IsRouteProfileUnlocked(
				ProfileId,
				Context.SelectedPackageIds))
			{
				return true;
			}
		}
		return false;
	}

	template <typename T>
	bool HasDuplicateValues(const TArray<T>& Values)
	{
		TSet<T> UniqueValues;
		for (const T& Value : Values)
		{
			if (UniqueValues.Contains(Value))
			{
				return true;
			}
			UniqueValues.Add(Value);
		}
		return false;
	}

	bool TryResolveTransitModuleRules(
		FName ModuleId,
		FSRConditionedTransitModuleRulesV2& OutRules)
	{
		TArray<ESRConditionedTransitModuleV2> Modules;
		FSRConditionedTransitV2::GetConditionedModules(Modules);
		for (const ESRConditionedTransitModuleV2 Module : Modules)
		{
			const FSRConditionedTransitModuleRulesV2 Rules =
				FSRConditionedTransitV2::GetModuleRules(Module);
			if (Rules.UnlockModuleId == ModuleId)
			{
				OutRules = Rules;
				return true;
			}
		}
		OutRules = FSRConditionedTransitModuleRulesV2();
		return false;
	}
}

void FSRAugmentPackageContentV2::GetAllDefinitions(
	TArray<FSRAugmentPackageDefinitionV2>& OutDefinitions)
{
	OutDefinitions = GetDefinitions();
}

bool FSRAugmentPackageContentV2::TryGetDefinition(
	FName PackageId,
	FSRAugmentPackageDefinitionV2& OutDefinition)
{
	for (const FSRAugmentPackageDefinitionV2& Definition : GetDefinitions())
	{
		if (Definition.PackageId == PackageId)
		{
			OutDefinition = Definition;
			return true;
		}
	}
	OutDefinition = FSRAugmentPackageDefinitionV2();
	return false;
}

bool FSRAugmentPackageContentV2::ValidateCatalog(FString& OutFailureReason)
{
	OutFailureReason.Reset();
	FString ProcessTagFailure;
	if (!FSRResourceSystemContent::ValidateProcessTagCatalog(ProcessTagFailure))
	{
		OutFailureReason = FString::Printf(
			TEXT("Process Tag policy violation: %s"),
			*ProcessTagFailure);
		return false;
	}

	TSet<FName> PackageIds;
	for (const FSRAugmentPackageDefinitionV2& Definition : GetDefinitions())
	{
		if (Definition.PackageId.IsNone()
			|| Definition.StrategyId.IsNone()
			|| PackageIds.Contains(Definition.PackageId))
		{
			OutFailureReason = TEXT("Augment Package and Strategy ids must be non-empty, and Package ids must be unique.");
			return false;
		}
		if (Definition.DisplayName.IsEmpty()
			|| Definition.Description.IsEmpty()
			|| Definition.ExampleLinePreview.IsEmpty())
		{
			OutFailureReason = FString::Printf(
				TEXT("Package %s must explain its name, concrete effect, and Line shape."),
				*Definition.PackageId.ToString());
			return false;
		}
		if (Definition.Rarity != ESRFacilityRarity::Basic
			&& Definition.Rarity != ESRFacilityRarity::Advanced
			&& Definition.Rarity != ESRFacilityRarity::HighTech)
		{
			OutFailureReason = FString::Printf(
				TEXT("Package %s uses a rarity that the weighted Package offer cannot draw."),
				*Definition.PackageId.ToString());
			return false;
		}
		PackageIds.Add(Definition.PackageId);
	}

	TArray<FSRFacilityContentDefinitionV2> FacilityDefinitions;
	FSRResourceSystemContent::GetAllFacilityDefinitions(FacilityDefinitions);
	TSet<FName> FacilityContentIds;
	for (const FSRFacilityContentDefinitionV2& FacilityDefinition : FacilityDefinitions)
	{
		FacilityContentIds.Add(FacilityDefinition.ContentId);
	}
	TArray<FName> TechnologyFacilityIds;
	GetTechnologyFacilityContentIds(TechnologyFacilityIds);
	TArray<FName> TechnologyTagIds;
	GetTechnologyProcessTagIds(TechnologyTagIds);

	TSet<FName> GrantedTagIds;
	TSet<FName> GrantedImprintIds;
	TSet<FName> GrantedFacilityIds;
	TSet<FName> GrantedModuleIds;
	TSet<FName> GrantedRouteProfileIds;
	TMap<FName, FName> RouteProfileStrategyById;
	TSet<FName> MacroDoctrineStrategyIds;
	FName TransitStrategyId = NAME_None;
	auto RegisterUniqueGrant = [&OutFailureReason](
		FName GrantId,
		FName PackageId,
		const TCHAR* GrantKind,
		TSet<FName>& RegisteredIds)
	{
		if (GrantId.IsNone() || RegisteredIds.Contains(GrantId))
		{
			OutFailureReason = FString::Printf(
				TEXT("Package %s has an empty or duplicate %s grant %s."),
				*PackageId.ToString(),
				GrantKind,
				GrantId.IsNone() ? TEXT("None") : *GrantId.ToString());
			return false;
		}
		RegisteredIds.Add(GrantId);
		return true;
	};

	for (const FSRAugmentPackageDefinitionV2& Definition : GetDefinitions())
	{
		if (HasDuplicateValues(Definition.RequiredPackageIds)
			|| HasDuplicateValues(Definition.RequiredFacilityContentIds)
			|| HasDuplicateValues(Definition.RequiredGrades)
			|| HasDuplicateValues(Definition.CompatibleFamilies)
			|| HasDuplicateValues(Definition.CompatibleSpectra)
			|| HasDuplicateValues(Definition.GrantedProcessTagIds)
			|| HasDuplicateValues(Definition.GrantedFuelImprintIds)
			|| HasDuplicateValues(Definition.GrantedFacilityContentIds)
			|| HasDuplicateValues(Definition.GrantedLogisticsModuleIds)
			|| HasDuplicateValues(Definition.GrantedRouteProfileIds))
		{
			OutFailureReason = FString::Printf(
				TEXT("Package %s repeats a prerequisite, compatibility value, or grant."),
				*Definition.PackageId.ToString());
			return false;
		}
		for (const ESRResourceFamily Family : Definition.CompatibleFamilies)
		{
			if (Family == ESRResourceFamily::None)
			{
				OutFailureReason = FString::Printf(
					TEXT("Package %s declares None as a compatible Family."),
					*Definition.PackageId.ToString());
				return false;
			}
		}
		for (const ESRResourceSpectrum Spectrum : Definition.CompatibleSpectra)
		{
			if (Spectrum == ESRResourceSpectrum::None)
			{
				OutFailureReason = FString::Printf(
					TEXT("Package %s declares None as a compatible Spectrum."),
					*Definition.PackageId.ToString());
				return false;
			}
		}
		if (Definition.bRequiresAllCompatibleSpectra
			&& Definition.CompatibleSpectra.Num() < 2)
		{
			OutFailureReason = FString::Printf(
				TEXT("Package %s uses an all-Spectra gate without a meaningful Spectrum set."),
				*Definition.PackageId.ToString());
			return false;
		}
		for (const int32 RequiredGrade : Definition.RequiredGrades)
		{
			if (RequiredGrade < 1 || RequiredGrade > 5)
			{
				OutFailureReason = FString::Printf(
					TEXT("Package %s requires Grade %d outside the authored 1-5 deck."),
					*Definition.PackageId.ToString(),
					RequiredGrade);
				return false;
			}
		}
		bool bHasKnownPrerequisites = true;
		for (const FName RequiredPackageId : Definition.RequiredPackageIds)
		{
			bHasKnownPrerequisites &= PackageIds.Contains(RequiredPackageId)
				&& RequiredPackageId != Definition.PackageId;
		}
		if (!bHasKnownPrerequisites)
		{
			OutFailureReason = FString::Printf(TEXT("Package %s references an unknown or self prerequisite."), *Definition.PackageId.ToString());
			return false;
		}
		for (const FName FacilityId : Definition.RequiredFacilityContentIds)
		{
			if (!FacilityContentIds.Contains(FacilityId))
			{
				OutFailureReason = FString::Printf(TEXT("Package %s requires unknown Facility content %s."), *Definition.PackageId.ToString(), *FacilityId.ToString());
				return false;
			}
		}
		for (const FName TagId : Definition.GrantedProcessTagIds)
		{
			FSRProcessTagDefinitionV2 TagDefinition;
			if (!FSRResourceSystemContent::TryGetProcessTagDefinition(TagId, TagDefinition))
			{
				OutFailureReason = FString::Printf(TEXT("Package %s grants unknown Process Tag %s."), *Definition.PackageId.ToString(), *TagId.ToString());
				return false;
			}
			if (TechnologyTagIds.Contains(TagId))
			{
				OutFailureReason = FString::Printf(
					TEXT("Package %s redundantly grants Technology Tag %s."),
					*Definition.PackageId.ToString(),
					*TagId.ToString());
				return false;
			}
			if (!Definition.RequiredFacilityContentIds.Contains(TagImprinterId)
				|| !RegisterUniqueGrant(TagId, Definition.PackageId, TEXT("Process Tag"), GrantedTagIds))
			{
				if (OutFailureReason.IsEmpty())
				{
					OutFailureReason = FString::Printf(
						TEXT("Package %s grants Process Tag %s without requiring the Tag Imprinter."),
						*Definition.PackageId.ToString(),
						*TagId.ToString());
				}
				return false;
			}
		}
		for (const FName ImprintId : Definition.GrantedFuelImprintIds)
		{
			FSRFuelImprintDefinitionV2 ImprintDefinition;
			if (!FSRResourceSystemContent::TryGetFuelImprintDefinition(ImprintId, ImprintDefinition))
			{
				OutFailureReason = FString::Printf(TEXT("Package %s grants unknown Fuel Imprint %s."), *Definition.PackageId.ToString(), *ImprintId.ToString());
				return false;
			}
			if (!Definition.RequiredFacilityContentIds.Contains(FuelImprinterId)
				|| !Definition.RequiredFacilityContentIds.Contains(StellarFuelFabricatorId)
				|| !RegisterUniqueGrant(ImprintId, Definition.PackageId, TEXT("Fuel Imprint"), GrantedImprintIds))
			{
				if (OutFailureReason.IsEmpty())
				{
					OutFailureReason = FString::Printf(
						TEXT("Package %s grants Fuel Imprint %s without its Imprinter and final Fabricator path."),
						*Definition.PackageId.ToString(),
						*ImprintId.ToString());
				}
				return false;
			}
		}
		for (const FName FacilityId : Definition.GrantedFacilityContentIds)
		{
			if (!FacilityContentIds.Contains(FacilityId))
			{
				OutFailureReason = FString::Printf(
					TEXT("Package %s grants unknown Facility content %s."),
					*Definition.PackageId.ToString(),
					*FacilityId.ToString());
				return false;
			}
			if (TechnologyFacilityIds.Contains(FacilityId))
			{
				OutFailureReason = FString::Printf(
					TEXT("Package %s redundantly grants Technology Facility %s."),
					*Definition.PackageId.ToString(),
					*FacilityId.ToString());
				return false;
			}
			if (!RegisterUniqueGrant(FacilityId, Definition.PackageId, TEXT("Facility"), GrantedFacilityIds))
			{
				return false;
			}
		}
		for (const FName ModuleId : Definition.GrantedLogisticsModuleIds)
		{
			FSRConditionedTransitModuleRulesV2 ModuleRules;
			if (!TryResolveTransitModuleRules(ModuleId, ModuleRules))
			{
				OutFailureReason = FString::Printf(
					TEXT("Package %s grants unknown Logistics Module %s."),
					*Definition.PackageId.ToString(),
					*ModuleId.ToString());
				return false;
			}
			if (Definition.MinimumHubEndpointCount < 2
				|| !Definition.CompatibleFamilies.Contains(ModuleRules.CompatibleFamily))
			{
				OutFailureReason = FString::Printf(
					TEXT("Package %s grants Module %s without its compatible Family and two-Hub route gate."),
					*Definition.PackageId.ToString(),
					*ModuleId.ToString());
				return false;
			}
			if (TransitStrategyId.IsNone())
			{
				TransitStrategyId = Definition.StrategyId;
			}
			else if (TransitStrategyId != Definition.StrategyId)
			{
				OutFailureReason = FString::Printf(
					TEXT("Package %s splits interchangeable conditioned-transit Modules across competing Offer Strategies."),
					*Definition.PackageId.ToString());
				return false;
			}
			if (!RegisterUniqueGrant(ModuleId, Definition.PackageId, TEXT("Logistics Module"), GrantedModuleIds))
			{
				return false;
			}
		}
		for (const FName ProfileId : Definition.GrantedRouteProfileIds)
		{
			ESRSpaceLogisticsRouteProfileV2 Profile;
			if (!FSRFleetCapacityV2::TryResolveRouteProfileId(ProfileId, Profile))
			{
				OutFailureReason = FString::Printf(
					TEXT("Package %s grants unknown Route Profile %s."),
					*Definition.PackageId.ToString(),
					*ProfileId.ToString());
				return false;
			}
			if (FSRFleetCapacityV2::IsTechnologyRouteProfile(Profile))
			{
				OutFailureReason = FString::Printf(
					TEXT("Package %s redundantly grants Technology Route Profile %s."),
					*Definition.PackageId.ToString(),
					*ProfileId.ToString());
				return false;
			}
			if (Definition.MinimumHubEndpointCount < 2)
			{
				OutFailureReason = FString::Printf(
					TEXT("Package %s grants Route Profile %s without a two-Hub route gate."),
					*Definition.PackageId.ToString(),
					*ProfileId.ToString());
				return false;
			}

			if (Profile == ESRSpaceLogisticsRouteProfileV2::BulkRawHold
				&& !Definition.IsMacroDoctrine())
			{
				OutFailureReason = FString::Printf(
					TEXT("Bulk Raw Hold must be granted by a Macro Doctrine; Package %s is not one."),
					*Definition.PackageId.ToString());
				return false;
			}
			if (Profile == ESRSpaceLogisticsRouteProfileV2::ConditionedHold)
			{
				FSRConditionedTransitModuleRulesV2 ModuleRules;
				if (Definition.GrantedLogisticsModuleIds.Num() != 1
					|| !TryResolveTransitModuleRules(Definition.GrantedLogisticsModuleIds[0], ModuleRules))
				{
					OutFailureReason = FString::Printf(
						TEXT("Package %s grants Conditioned Hold without exactly one valid conditioned Module."),
						*Definition.PackageId.ToString());
					return false;
				}
			}

			if (const FName* ExistingStrategyId = RouteProfileStrategyById.Find(ProfileId))
			{
				if (*ExistingStrategyId != Definition.StrategyId)
				{
					OutFailureReason = FString::Printf(
						TEXT("Route Profile %s is split across competing Strategies."),
						*ProfileId.ToString());
					return false;
				}
			}
			else
			{
				RouteProfileStrategyById.Add(ProfileId, Definition.StrategyId);
			}
			GrantedRouteProfileIds.Add(ProfileId);
		}
		if (Definition.IsMacroDoctrine()
			&& (Definition.DoctrineExclusionGroup != MacroDoctrineGroup
				|| Definition.MinimumHubEndpointCount < 2
				|| Definition.GrantedFuelImprintIds.IsEmpty()))
		{
			OutFailureReason = FString::Printf(
				TEXT("Macro Doctrine %s must consume the shared Doctrine slot, require a network, and grant a topology Imprint."),
				*Definition.PackageId.ToString());
			return false;
		}
		if (Definition.IsMacroDoctrine())
		{
			if (MacroDoctrineStrategyIds.Contains(Definition.StrategyId))
			{
				OutFailureReason = FString::Printf(
					TEXT("Macro Doctrine %s duplicates another Doctrine Line Strategy."),
					*Definition.PackageId.ToString());
				return false;
			}
			MacroDoctrineStrategyIds.Add(Definition.StrategyId);
		}
		if (!Definition.IsMacroDoctrine() && !Definition.DoctrineExclusionGroup.IsNone())
		{
			OutFailureReason = FString::Printf(
				TEXT("Non-Doctrine Package %s cannot reserve a Doctrine exclusion group."),
				*Definition.PackageId.ToString());
			return false;
		}
		if (Definition.PackageRole == ESRAugmentPackageRoleV2::Capstone
			&& Definition.RequiredPackageIds.IsEmpty())
		{
			OutFailureReason = FString::Printf(TEXT("Capstone %s has no strategy prerequisite."), *Definition.PackageId.ToString());
			return false;
		}
		if (Definition.PackageRole == ESRAugmentPackageRoleV2::Capstone)
		{
			bool bExtendsSameStrategy = false;
			for (const FName RequiredPackageId : Definition.RequiredPackageIds)
			{
				FSRAugmentPackageDefinitionV2 RequiredDefinition;
				bExtendsSameStrategy |= TryGetDefinition(RequiredPackageId, RequiredDefinition)
					&& RequiredDefinition.StrategyId == Definition.StrategyId;
			}
			if (!bExtendsSameStrategy)
			{
				OutFailureReason = FString::Printf(
					TEXT("Capstone %s does not extend any prerequisite in its own Strategy."),
					*Definition.PackageId.ToString());
				return false;
			}
		}
		if (Definition.GrantedProcessTagIds.IsEmpty()
			&& Definition.GrantedFuelImprintIds.IsEmpty()
			&& Definition.GrantedFacilityContentIds.IsEmpty()
			&& Definition.GrantedLogisticsModuleIds.IsEmpty()
			&& Definition.GrantedRouteProfileIds.IsEmpty())
		{
			OutFailureReason = FString::Printf(TEXT("Package %s grants no usable content."), *Definition.PackageId.ToString());
			return false;
		}
	}

	for (const FSRAugmentPackageDefinitionV2& Definition : GetDefinitions())
	{
		TSet<FName> VisitedPackageIds;
		TArray<FName> PendingPackageIds = Definition.RequiredPackageIds;
		while (!PendingPackageIds.IsEmpty())
		{
			const FName PendingId = PendingPackageIds.Pop(EAllowShrinking::No);
			if (PendingId == Definition.PackageId)
			{
				OutFailureReason = FString::Printf(
					TEXT("Package %s participates in a prerequisite cycle."),
					*Definition.PackageId.ToString());
				return false;
			}
			if (VisitedPackageIds.Contains(PendingId))
			{
				continue;
			}
			VisitedPackageIds.Add(PendingId);
			FSRAugmentPackageDefinitionV2 PendingDefinition;
			if (TryGetDefinition(PendingId, PendingDefinition))
			{
				PendingPackageIds.Append(PendingDefinition.RequiredPackageIds);
			}
		}
	}

	TArray<FSRProcessTagDefinitionV2> TagDefinitions;
	FSRResourceSystemContent::GetAllProcessTagDefinitions(TagDefinitions);
	for (const FSRProcessTagDefinitionV2& TagDefinition : TagDefinitions)
	{
		if (!TechnologyTagIds.Contains(TagDefinition.TagId)
			&& !GrantedTagIds.Contains(TagDefinition.TagId))
		{
			OutFailureReason = FString::Printf(
				TEXT("Process Tag %s has no Technology or Augment unlock path."),
				*TagDefinition.TagId.ToString());
			return false;
		}
	}
	TArray<FSRFuelImprintDefinitionV2> ImprintDefinitions;
	FSRResourceSystemContent::GetAllFuelImprintDefinitions(ImprintDefinitions);
	for (const FSRFuelImprintDefinitionV2& ImprintDefinition : ImprintDefinitions)
	{
		if (!GrantedImprintIds.Contains(ImprintDefinition.ImprintId))
		{
			OutFailureReason = FString::Printf(
				TEXT("Fuel Imprint %s has no Augment unlock path."),
				*ImprintDefinition.ImprintId.ToString());
			return false;
		}
	}
	TArray<ESRConditionedTransitModuleV2> TransitModules;
	FSRConditionedTransitV2::GetConditionedModules(TransitModules);
	for (const ESRConditionedTransitModuleV2 TransitModule : TransitModules)
	{
		const FName ModuleId = FSRConditionedTransitV2::GetModuleRules(TransitModule).UnlockModuleId;
		if (!GrantedModuleIds.Contains(ModuleId))
		{
			OutFailureReason = FString::Printf(
				TEXT("Logistics Module %s has no Augment unlock path."),
				*ModuleId.ToString());
			return false;
		}
	}
	TArray<ESRSpaceLogisticsRouteProfileV2> RouteProfiles;
	FSRFleetCapacityV2::GetRouteProfiles(RouteProfiles);
	for (const ESRSpaceLogisticsRouteProfileV2 RouteProfile : RouteProfiles)
	{
		const FName ProfileId = FSRFleetCapacityV2::GetRouteProfileId(RouteProfile);
		if (!FSRFleetCapacityV2::IsTechnologyRouteProfile(RouteProfile)
			&& !GrantedRouteProfileIds.Contains(ProfileId))
		{
			OutFailureReason = FString::Printf(
				TEXT("Route Profile %s has no Augment unlock path."),
				*ProfileId.ToString());
			return false;
		}
	}
	return true;
}

bool FSRAugmentPackageContentV2::IsDefinitionEligible(
	const FSRAugmentPackageDefinitionV2& Definition,
	const FSRAugmentBuildContextV2& Context,
	FString* OutFailureReason)
{
	const FSRAugmentPackageEligibilityReportV2 Report = EvaluateEligibility(Definition, Context);
	if (OutFailureReason)
	{
		*OutFailureReason = Report.FailureReason;
	}
	return Report.bEligible;
}

FSRAugmentPackageEligibilityReportV2 FSRAugmentPackageContentV2::EvaluateEligibility(
	const FSRAugmentPackageDefinitionV2& Definition,
	const FSRAugmentBuildContextV2& Context)
{
	FSRAugmentPackageEligibilityReportV2 Report;
	Report.bPackageSelectionReady = !Definition.PackageId.IsNone()
		&& !Context.SelectedPackageIds.Contains(Definition.PackageId);
	Report.bRequiredPackagesReady = ContainsAllNames(
		Context.SelectedPackageIds,
		Definition.RequiredPackageIds);
	Report.bRequiredFacilitiesReady = ContainsAllNames(
		Context.AvailableFacilityContentIds,
		Definition.RequiredFacilityContentIds);

	for (const int32 RequiredGrade : Definition.RequiredGrades)
	{
		if (!Context.AccessibleGrades.Contains(RequiredGrade))
		{
			Report.bRequiredGradesReady = false;
			break;
		}
	}
	Report.bCompatibleFamilyReady = ContainsAny(
		Context.AccessibleFamilies,
		Definition.CompatibleFamilies);
	if (Definition.bRequiresAllCompatibleSpectra)
	{
		for (const ESRResourceSpectrum Spectrum : Definition.CompatibleSpectra)
		{
			if (!Context.AccessibleSpectra.Contains(Spectrum))
			{
				Report.bCompatibleSpectrumReady = false;
				break;
			}
		}
	}
	else
	{
		Report.bCompatibleSpectrumReady = ContainsAny(
			Context.AccessibleSpectra,
			Definition.CompatibleSpectra);
	}
	Report.bHubNetworkReady = Context.HubEndpointCount >= Definition.MinimumHubEndpointCount;
	Report.bDoctrineSlotReady = !Definition.IsMacroDoctrine()
		|| Context.ActiveMacroDoctrineId.IsNone()
		|| Context.ActiveMacroDoctrineId == Definition.PackageId;
	Report.bNovelGrantReady = DoesPackageGrantNewContent(Definition, Context);

	auto CountRequirementGroup = [&Report](bool bAuthored, bool bReady)
	{
		if (!bAuthored)
		{
			return;
		}
		++Report.TotalRequirementGroupCount;
		Report.SatisfiedRequirementGroupCount += bReady ? 1 : 0;
	};
	CountRequirementGroup(!Definition.RequiredPackageIds.IsEmpty(), Report.bRequiredPackagesReady);
	CountRequirementGroup(!Definition.RequiredFacilityContentIds.IsEmpty(), Report.bRequiredFacilitiesReady);
	CountRequirementGroup(!Definition.RequiredGrades.IsEmpty(), Report.bRequiredGradesReady);
	CountRequirementGroup(!Definition.CompatibleFamilies.IsEmpty(), Report.bCompatibleFamilyReady);
	CountRequirementGroup(!Definition.CompatibleSpectra.IsEmpty(), Report.bCompatibleSpectrumReady);
	CountRequirementGroup(Definition.MinimumHubEndpointCount > 0, Report.bHubNetworkReady);
	CountRequirementGroup(Definition.IsMacroDoctrine(), Report.bDoctrineSlotReady);

	Report.bEligible = Report.bPackageSelectionReady
		&& Report.bRequiredPackagesReady
		&& Report.bRequiredFacilitiesReady
		&& Report.bRequiredGradesReady
		&& Report.bCompatibleFamilyReady
		&& Report.bCompatibleSpectrumReady
		&& Report.bHubNetworkReady
		&& Report.bDoctrineSlotReady
		&& Report.bNovelGrantReady;

	if (!Report.bPackageSelectionReady)
	{
		Report.FailureReason = TEXT("Package is invalid or already selected.");
	}
	else if (!Report.bRequiredPackagesReady)
	{
		Report.FailureReason = TEXT("Required Augment Package is missing.");
	}
	else if (!Report.bRequiredFacilitiesReady)
	{
		Report.FailureReason = TEXT("Required Technology Facility is unavailable.");
	}
	else if (!Report.bRequiredGradesReady)
	{
		Report.FailureReason = TEXT("A required Grade is not yet accessible.");
	}
	else if (!Report.bCompatibleFamilyReady)
	{
		Report.FailureReason = TEXT("No compatible Resource Family is accessible.");
	}
	else if (!Report.bCompatibleSpectrumReady)
	{
		Report.FailureReason = Definition.bRequiresAllCompatibleSpectra
			? TEXT("All compatible Spectra are not yet accessible.")
			: TEXT("No compatible Spectrum is accessible.");
	}
	else if (!Report.bHubNetworkReady)
	{
		Report.FailureReason = TEXT("The Package requires more usable Hub endpoints.");
	}
	else if (!Report.bDoctrineSlotReady)
	{
		Report.FailureReason = TEXT("The Macro Doctrine slot is already occupied.");
	}
	else if (!Report.bNovelGrantReady)
	{
		Report.FailureReason = TEXT("Every Package grant is already available in this Run.");
	}
	return Report;
}

void FSRAugmentPackageContentV2::BuildEligibleDefinitions(
	const FSRAugmentBuildContextV2& Context,
	TArray<FSRAugmentPackageDefinitionV2>& OutDefinitions)
{
	OutDefinitions.Reset();
	for (const FSRAugmentPackageDefinitionV2& Definition : GetDefinitions())
	{
		if (IsDefinitionEligible(Definition, Context))
		{
			OutDefinitions.Add(Definition);
		}
	}
	OutDefinitions.Sort([](const FSRAugmentPackageDefinitionV2& Left, const FSRAugmentPackageDefinitionV2& Right)
	{
		return Left.PackageId.LexicalLess(Right.PackageId);
	});
}

void FSRAugmentPackageContentV2::GenerateOffer(
	const FSRAugmentBuildContextV2& Context,
	const FSRAugmentOfferGenerationRulesV2& Rules,
	TArray<FSRAugmentPackageOfferV2>& OutOffers)
{
	OutOffers.Reset();
	TArray<FSRAugmentPackageDefinitionV2> Remaining;
	BuildEligibleDefinitions(Context, Remaining);
	if (Remaining.IsEmpty())
	{
		return;
	}

	TSet<FName> SelectedStrategyIds;
	for (const FName SelectedPackageId : Context.SelectedPackageIds)
	{
		FSRAugmentPackageDefinitionV2 SelectedDefinition;
		if (TryGetDefinition(SelectedPackageId, SelectedDefinition))
		{
			SelectedStrategyIds.Add(SelectedDefinition.StrategyId);
		}
	}
	TSet<FName> OfferedStrategyIds;
	FRandomStream RandomStream(Rules.RandomSeed);
	const int32 SafeChoiceCount = FMath::Max(1, Rules.ChoiceCount);

	auto AddFromPool = [
		&Remaining,
		&OutOffers,
		&OfferedStrategyIds,
		&Rules,
		&RandomStream](
		auto&& Predicate,
		ESRAugmentOfferRoleV2 OfferRole,
		bool bRequireUnrepresentedStrategy)
	{
		auto BuildCandidateIndices = [
			&Remaining,
			&OfferedStrategyIds,
			&Rules,
			&Predicate,
			bRequireUnrepresentedStrategy](
			bool bFreshOnly,
			TArray<int32>& OutCandidateIndices)
		{
			OutCandidateIndices.Reset();
			for (int32 Index = 0; Index < Remaining.Num(); ++Index)
			{
				const FSRAugmentPackageDefinitionV2& Candidate = Remaining[Index];
				if (!Predicate(Candidate)
					|| (bRequireUnrepresentedStrategy
						&& OfferedStrategyIds.Contains(Candidate.StrategyId))
					|| (bFreshOnly
						&& Rules.RecentlyOfferedPackageIds.Contains(Candidate.PackageId)))
				{
					continue;
				}
				OutCandidateIndices.Add(Index);
			}
		};

		TArray<int32> CandidateIndices;
		BuildCandidateIndices(true, CandidateIndices);
		if (CandidateIndices.IsEmpty())
		{
			// Recent-offer avoidance is deliberately soft. A small remaining
			// catalog must still produce every choice that is actually available.
			BuildCandidateIndices(false, CandidateIndices);
		}
		const int32 SelectedIndex = PickWeightedIndex(Remaining, CandidateIndices, Rules, RandomStream);
		if (!Remaining.IsValidIndex(SelectedIndex))
		{
			return false;
		}

		const FSRAugmentPackageDefinitionV2 SelectedDefinition = Remaining[SelectedIndex];
		FSRAugmentPackageOfferV2& Offer = OutOffers.AddDefaulted_GetRef();
		Offer.PackageId = SelectedDefinition.PackageId;
		Offer.OfferRole = OfferRole;
		Offer.bImmediatelyUsable = true;
		OfferedStrategyIds.Add(SelectedDefinition.StrategyId);
		Remaining.RemoveAt(SelectedIndex);
		return true;
	};

	// The first slot is a usable, non-exclusive Line component whenever one
	// exists. A player is never forced to spend the only clearly safe slot on a
	// Macro Doctrine commitment or a late Capstone.
	if (!AddFromPool(
		[](const FSRAugmentPackageDefinitionV2& Definition)
		{
			return Definition.PackageRole != ESRAugmentPackageRoleV2::Capstone
				&& Definition.PackageRole != ESRAugmentPackageRoleV2::MacroDoctrine;
		},
		ESRAugmentOfferRoleV2::Immediate,
		true))
	{
		AddFromPool(
			[](const FSRAugmentPackageDefinitionV2& Definition)
			{
				return Definition.PackageRole != ESRAugmentPackageRoleV2::Capstone;
			},
			ESRAugmentOfferRoleV2::Immediate,
			true);
	}

	while (OutOffers.Num() < SafeChoiceCount && !Remaining.IsEmpty())
	{
		if (AddFromPool(
			[&SelectedStrategyIds](const FSRAugmentPackageDefinitionV2& Definition)
			{
				return Definition.PackageRole == ESRAugmentPackageRoleV2::Capstone
					&& SelectedStrategyIds.Contains(Definition.StrategyId);
			},
			ESRAugmentOfferRoleV2::Capstone,
			true))
		{
			continue;
		}
		if (AddFromPool(
			[&SelectedStrategyIds](const FSRAugmentPackageDefinitionV2& Definition)
			{
				return Definition.PackageRole != ESRAugmentPackageRoleV2::Capstone
					&& SelectedStrategyIds.Contains(Definition.StrategyId);
			},
			ESRAugmentOfferRoleV2::Synergy,
			true))
		{
			continue;
		}
		if (AddFromPool(
			[](const FSRAugmentPackageDefinitionV2& Definition)
			{
				return Definition.PackageRole != ESRAugmentPackageRoleV2::Capstone;
			},
			ESRAugmentOfferRoleV2::Pivot,
			true))
		{
			continue;
		}

		if (AddFromPool(
			[](const FSRAugmentPackageDefinitionV2& Definition)
			{
				return Definition.PackageRole == ESRAugmentPackageRoleV2::Capstone;
			},
			ESRAugmentOfferRoleV2::Capstone,
			true))
		{
			continue;
		}
		if (AddFromPool(
			[](const FSRAugmentPackageDefinitionV2& Definition)
			{
				return Definition.PackageRole != ESRAugmentPackageRoleV2::Capstone;
			},
			ESRAugmentOfferRoleV2::Pivot,
			false))
		{
			continue;
		}
		if (!AddFromPool(
			[](const FSRAugmentPackageDefinitionV2& Definition)
			{
				return Definition.PackageRole == ESRAugmentPackageRoleV2::Capstone;
			},
			ESRAugmentOfferRoleV2::Capstone,
			false))
		{
			break;
		}
	}
}

void FSRAugmentPackageContentV2::GetTechnologyFacilityContentIds(TArray<FName>& OutContentIds)
{
	OutContentIds.Reset();
	TArray<FSRFacilityContentDefinitionV2> FacilityDefinitions;
	FSRResourceSystemContent::GetAllFacilityDefinitions(FacilityDefinitions);
	for (const FSRFacilityContentDefinitionV2& Definition : FacilityDefinitions)
	{
		OutContentIds.AddUnique(Definition.ContentId);
	}
}

void FSRAugmentPackageContentV2::GetTechnologyProcessTagIds(TArray<FName>& OutTagIds)
{
	OutTagIds = {
		FSRResourceSystemContent::GetProcessTagId(ESRProcessTagContentV2::Crosslink),
	};
}

bool FSRAugmentPackageContentV2::IsProcessTagRecipeUnlocked(
	FName TagId,
	const TArray<FName>& SelectedPackageIds)
{
	TArray<FName> TechnologyTagIds;
	GetTechnologyProcessTagIds(TechnologyTagIds);
	if (TechnologyTagIds.Contains(TagId))
	{
		return true;
	}
	for (const FName PackageId : SelectedPackageIds)
	{
		FSRAugmentPackageDefinitionV2 Definition;
		if (TryGetDefinition(PackageId, Definition) && Definition.GrantedProcessTagIds.Contains(TagId))
		{
			return true;
		}
	}
	return false;
}

bool FSRAugmentPackageContentV2::IsFuelImprintRecipeUnlocked(
	FName ImprintId,
	const TArray<FName>& SelectedPackageIds)
{
	for (const FName PackageId : SelectedPackageIds)
	{
		FSRAugmentPackageDefinitionV2 Definition;
		if (TryGetDefinition(PackageId, Definition) && Definition.GrantedFuelImprintIds.Contains(ImprintId))
		{
			return true;
		}
	}
	return false;
}

bool FSRAugmentPackageContentV2::IsFacilityContentUnlocked(
	FName ContentId,
	const TArray<FName>& SelectedPackageIds)
{
	TArray<FName> TechnologyContentIds;
	GetTechnologyFacilityContentIds(TechnologyContentIds);
	if (TechnologyContentIds.Contains(ContentId))
	{
		return true;
	}
	for (const FName PackageId : SelectedPackageIds)
	{
		FSRAugmentPackageDefinitionV2 Definition;
		if (TryGetDefinition(PackageId, Definition) && Definition.GrantedFacilityContentIds.Contains(ContentId))
		{
			return true;
		}
	}
	return false;
}

bool FSRAugmentPackageContentV2::IsLogisticsModuleUnlocked(
	FName ModuleId,
	const TArray<FName>& SelectedPackageIds)
{
	for (const FName PackageId : SelectedPackageIds)
	{
		FSRAugmentPackageDefinitionV2 Definition;
		if (TryGetDefinition(PackageId, Definition)
			&& Definition.GrantedLogisticsModuleIds.Contains(ModuleId))
		{
			return true;
		}
	}
	return false;
}

bool FSRAugmentPackageContentV2::IsRouteProfileUnlocked(
	FName ProfileId,
	const TArray<FName>& SelectedPackageIds)
{
	ESRSpaceLogisticsRouteProfileV2 Profile;
	if (!FSRFleetCapacityV2::TryResolveRouteProfileId(ProfileId, Profile))
	{
		return false;
	}
	if (FSRFleetCapacityV2::IsTechnologyRouteProfile(Profile))
	{
		return true;
	}
	for (const FName PackageId : SelectedPackageIds)
	{
		FSRAugmentPackageDefinitionV2 Definition;
		if (TryGetDefinition(PackageId, Definition)
			&& Definition.GrantedRouteProfileIds.Contains(ProfileId))
		{
			return true;
		}
	}
	return false;
}

FName FSRAugmentPackageContentV2::ResolveActiveMacroDoctrineId(
	const TArray<FName>& SelectedPackageIds)
{
	for (const FName PackageId : SelectedPackageIds)
	{
		FSRAugmentPackageDefinitionV2 Definition;
		if (TryGetDefinition(PackageId, Definition) && Definition.IsMacroDoctrine())
		{
			return PackageId;
		}
	}
	return NAME_None;
}

FString FSRAugmentPackageContentV2::BuildGrantSummary(
	const FSRAugmentPackageDefinitionV2& Definition)
{
	TArray<FString> Grants;
	for (const FName TagId : Definition.GrantedProcessTagIds)
	{
		Grants.Add(FString::Printf(TEXT("Tag: %s"), *TagId.ToString()));
	}
	for (const FName ImprintId : Definition.GrantedFuelImprintIds)
	{
		Grants.Add(FString::Printf(TEXT("Imprint: %s"), *ImprintId.ToString()));
	}
	for (const FName FacilityId : Definition.GrantedFacilityContentIds)
	{
		Grants.Add(FString::Printf(TEXT("Facility: %s"), *FacilityId.ToString()));
	}
	for (const FName ModuleId : Definition.GrantedLogisticsModuleIds)
	{
		Grants.Add(FString::Printf(TEXT("Route Module: %s"), *ModuleId.ToString()));
	}
	for (const FName ProfileId : Definition.GrantedRouteProfileIds)
	{
		Grants.Add(FString::Printf(TEXT("Route Profile: %s"), *ProfileId.ToString()));
	}
	return FString::Join(Grants, TEXT(" | "));
}
