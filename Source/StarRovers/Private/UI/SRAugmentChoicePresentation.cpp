#include "UI/SRAugmentChoicePresentation.h"

#include "Automation/SRResourceSystemContent.h"
#include "Logistics/SRConditionedTransitV2.h"
#include "Logistics/SRFleetCapacityV2.h"

namespace
{
	FText DisplayNameFromId(FName Id)
	{
		return Id.IsNone()
			? FText::GetEmpty()
			: FText::FromString(FName::NameToDisplayString(Id.ToString(), false));
	}

	FText UpperText(const FText& Text)
	{
		return FText::FromString(Text.ToString().ToUpper());
	}

	FText ResolveProcessTagName(FName TagId)
	{
		FSRProcessTagDefinitionV2 Definition;
		return FSRResourceSystemContent::TryGetProcessTagDefinition(TagId, Definition)
			? Definition.DisplayName
			: DisplayNameFromId(TagId);
	}

	FText ResolveFuelImprintName(FName ImprintId)
	{
		FSRFuelImprintDefinitionV2 Definition;
		return FSRResourceSystemContent::TryGetFuelImprintDefinition(ImprintId, Definition)
			? Definition.DisplayName
			: DisplayNameFromId(ImprintId);
	}

	FText ResolveFacilityName(FName FacilityId)
	{
		TArray<FSRFacilityContentDefinitionV2> Definitions;
		FSRResourceSystemContent::GetAllFacilityDefinitions(Definitions);
		for (const FSRFacilityContentDefinitionV2& Definition : Definitions)
		{
			if (Definition.ContentId == FacilityId)
			{
				return Definition.DisplayName;
			}
		}
		return DisplayNameFromId(FacilityId);
	}

	bool TryResolveLogisticsModule(
		FName ModuleId,
		FSRConditionedTransitModuleRulesV2& OutRules)
	{
		TArray<ESRConditionedTransitModuleV2> Modules;
		FSRConditionedTransitV2::GetConditionedModules(Modules);
		for (const ESRConditionedTransitModuleV2 Module : Modules)
		{
			const FSRConditionedTransitModuleRulesV2 Rules = FSRConditionedTransitV2::GetModuleRules(Module);
			if (Rules.UnlockModuleId == ModuleId)
			{
				OutRules = Rules;
				return true;
			}
		}
		return false;
	}

	FText ResolveLogisticsModuleName(FName ModuleId)
	{
		FSRConditionedTransitModuleRulesV2 Rules;
		return TryResolveLogisticsModule(ModuleId, Rules)
			? Rules.DisplayName
			: DisplayNameFromId(ModuleId);
	}

	FText ResolveRouteProfileName(FName ProfileId)
	{
		ESRSpaceLogisticsRouteProfileV2 Profile;
		return FSRFleetCapacityV2::TryResolveRouteProfileId(ProfileId, Profile)
			? FSRFleetCapacityV2::GetRouteProfileRules(Profile).DisplayName
			: DisplayNameFromId(ProfileId);
	}

	FText ResolveRarityText(ESRFacilityRarity Rarity)
	{
		switch (Rarity)
		{
		case ESRFacilityRarity::Starting: return NSLOCTEXT("StarRoversAugmentChoice", "PresentationStarting", "STARTING");
		case ESRFacilityRarity::Basic: return NSLOCTEXT("StarRoversAugmentChoice", "PresentationBasic", "BASIC");
		case ESRFacilityRarity::Advanced: return NSLOCTEXT("StarRoversAugmentChoice", "PresentationAdvanced", "ADVANCED");
		case ESRFacilityRarity::HighTech: return NSLOCTEXT("StarRoversAugmentChoice", "PresentationHighTech", "HIGH TECH");
		case ESRFacilityRarity::Innovation: return NSLOCTEXT("StarRoversAugmentChoice", "PresentationInnovation", "INNOVATION");
		default: return FText::GetEmpty();
		}
	}

	FText ResolveOfferRoleText(ESRAugmentOfferRoleV2 OfferRole)
	{
		switch (OfferRole)
		{
		case ESRAugmentOfferRoleV2::Immediate: return NSLOCTEXT("StarRoversAugmentChoice", "PresentationImmediate", "IMMEDIATE");
		case ESRAugmentOfferRoleV2::Synergy: return NSLOCTEXT("StarRoversAugmentChoice", "PresentationSynergy", "SYNERGY");
		case ESRAugmentOfferRoleV2::Pivot: return NSLOCTEXT("StarRoversAugmentChoice", "PresentationPivot", "PIVOT");
		case ESRAugmentOfferRoleV2::Capstone: return NSLOCTEXT("StarRoversAugmentChoice", "PresentationCapstone", "CAPSTONE");
		case ESRAugmentOfferRoleV2::Legacy:
		default: return NSLOCTEXT("StarRoversAugmentChoice", "PresentationFacility", "FACILITY");
		}
	}

	ESRUIVisualState ResolveOfferState(ESRAugmentOfferRoleV2 OfferRole)
	{
		switch (OfferRole)
		{
		case ESRAugmentOfferRoleV2::Immediate: return ESRUIVisualState::Positive;
		case ESRAugmentOfferRoleV2::Synergy: return ESRUIVisualState::Info;
		case ESRAugmentOfferRoleV2::Pivot: return ESRUIVisualState::Warning;
		case ESRAugmentOfferRoleV2::Capstone: return ESRUIVisualState::Selected;
		case ESRAugmentOfferRoleV2::Legacy:
		default: return ESRUIVisualState::Neutral;
		}
	}

	FString JoinText(const TArray<FText>& Values, const TCHAR* Separator)
	{
		TArray<FString> Strings;
		Strings.Reserve(Values.Num());
		for (const FText& Value : Values)
		{
			if (!Value.IsEmpty())
			{
				Strings.Add(Value.ToString());
			}
		}
		return FString::Join(Strings, Separator);
	}

	FText JoinNames(const TArray<FName>& Values)
	{
		TArray<FText> Names;
		Names.Reserve(Values.Num());
		for (const FName Value : Values)
		{
			Names.Add(DisplayNameFromId(Value));
		}
		return FText::FromString(JoinText(Names, TEXT(" + ")));
	}

	FText BuildFamilyFitText(
		const FSRAugmentPackageDefinitionV2& Definition,
		const FSRAugmentBuildContextV2& Context)
	{
		if (Definition.CompatibleFamilies.IsEmpty())
		{
			return FText::GetEmpty();
		}

		TArray<FText> FamilyNames;
		for (const ESRResourceFamily Family : Definition.CompatibleFamilies)
		{
			FamilyNames.Add(StaticEnum<ESRResourceFamily>()->GetDisplayNameTextByValue(
				static_cast<int64>(Family)));
		}
		const FText FamilyList = FText::FromString(JoinText(FamilyNames, TEXT("/")));
		const bool bHasCompatibleFamily = Definition.CompatibleFamilies.ContainsByPredicate(
			[&Context](ESRResourceFamily Family)
			{
				return Context.AccessibleFamilies.Contains(Family);
			});
		return FText::Format(
			bHasCompatibleFamily
				? NSLOCTEXT("StarRoversAugmentChoice", "FamilyReadyFit", "{0} available")
				: NSLOCTEXT("StarRoversAugmentChoice", "FamilyMissingFit", "Needs {0}"),
			FamilyList);
	}

	FText BuildRoleFitText(ESRAugmentOfferRoleV2 OfferRole)
	{
		switch (OfferRole)
		{
		case ESRAugmentOfferRoleV2::Immediate:
			return NSLOCTEXT("StarRoversAugmentChoice", "ImmediateFit", "Works now");
		case ESRAugmentOfferRoleV2::Synergy:
			return NSLOCTEXT("StarRoversAugmentChoice", "SynergyFit", "Extends an owned strategy");
		case ESRAugmentOfferRoleV2::Pivot:
			return NSLOCTEXT("StarRoversAugmentChoice", "PivotFit", "Opens a new Line shape");
		case ESRAugmentOfferRoleV2::Capstone:
			return NSLOCTEXT("StarRoversAugmentChoice", "CapstoneFit", "Late payoff is ready");
		case ESRAugmentOfferRoleV2::Legacy:
		default:
			return NSLOCTEXT("StarRoversAugmentChoice", "LegacyFit", "Adds a Build Dock option");
		}
	}

	FText BuildRequirementEvidenceText(
		const FSRAugmentPackageDefinitionV2& Definition,
		const FSRAugmentBuildContextV2& Context,
		ESRAugmentOfferRoleV2 OfferRole)
	{
		TArray<FText> Evidence;
		Evidence.Add(BuildRoleFitText(OfferRole));
		if (!Definition.RequiredPackageIds.IsEmpty())
		{
			Evidence.Add(FText::Format(
				NSLOCTEXT("StarRoversAugmentChoice", "PackageFit", "Own {0}"),
				JoinNames(Definition.RequiredPackageIds)));
		}
		if (!Definition.RequiredFacilityContentIds.IsEmpty())
		{
			TArray<FText> FacilityNames;
			for (const FName FacilityId : Definition.RequiredFacilityContentIds)
			{
				FacilityNames.Add(ResolveFacilityName(FacilityId));
			}
			Evidence.Add(FText::Format(
				NSLOCTEXT("StarRoversAugmentChoice", "FacilityFit", "Tech: {0}"),
				FText::FromString(JoinText(FacilityNames, TEXT(" + ")))));
		}
		if (!Definition.RequiredGrades.IsEmpty())
		{
			TArray<FString> GradeStrings;
			for (const int32 Grade : Definition.RequiredGrades)
			{
				GradeStrings.Add(FString::FromInt(Grade));
			}
			Evidence.Add(FText::Format(
				NSLOCTEXT("StarRoversAugmentChoice", "GradeFit", "Grades {0} accessible"),
				FText::FromString(FString::Join(GradeStrings, TEXT("/")))));
		}
		const FText FamilyFit = BuildFamilyFitText(Definition, Context);
		if (!FamilyFit.IsEmpty())
		{
			Evidence.Add(FamilyFit);
		}
		if (!Definition.CompatibleSpectra.IsEmpty())
		{
			int32 AccessibleCount = 0;
			for (const ESRResourceSpectrum Spectrum : Definition.CompatibleSpectra)
			{
				AccessibleCount += Context.AccessibleSpectra.Contains(Spectrum) ? 1 : 0;
			}
			Evidence.Add(FText::Format(
				Definition.bRequiresAllCompatibleSpectra
					? NSLOCTEXT("StarRoversAugmentChoice", "AllSpectrumFit", "Spectra {0}/{1}")
					: NSLOCTEXT("StarRoversAugmentChoice", "AnySpectrumFit", "Spectra {0} available"),
				FText::AsNumber(AccessibleCount),
				FText::AsNumber(Definition.CompatibleSpectra.Num())));
		}
		if (Definition.MinimumHubEndpointCount > 0)
		{
			Evidence.Add(FText::Format(
				NSLOCTEXT("StarRoversAugmentChoice", "HubFit", "Hubs {0}/{1}"),
				FText::AsNumber(Context.HubEndpointCount),
				FText::AsNumber(Definition.MinimumHubEndpointCount)));
		}
		if (Definition.IsMacroDoctrine())
		{
			Evidence.Add(Context.ActiveMacroDoctrineId.IsNone()
				? NSLOCTEXT("StarRoversAugmentChoice", "DoctrineOpenFit", "Doctrine slot open")
				: NSLOCTEXT("StarRoversAugmentChoice", "DoctrineOccupiedFit", "Doctrine slot occupied"));
		}
		return FText::FromString(JoinText(Evidence, TEXT(" | ")));
	}

	FText FormatEnergyDelta(double EnergyDelta)
	{
		const FString Number = FMath::IsNearlyEqual(EnergyDelta, FMath::RoundToDouble(EnergyDelta))
			? FString::Printf(TEXT("%+.0f"), EnergyDelta)
			: FString::Printf(TEXT("%+.1f"), EnergyDelta);
		return FText::FromString(FString::Printf(TEXT("E %s"), *Number));
	}

	FText ResolveProcessTagCondition(ESRProcessTagTriggerV2 Trigger)
	{
		switch (Trigger)
		{
		case ESRProcessTagTriggerV2::PositiveFamilyStateActivated:
			return NSLOCTEXT("StarRoversAugmentChoice", "PositiveStateCondition", "POSITIVE STATE ACTIVATES");
		case ESRProcessTagTriggerV2::NegativeFamilyStateCleared:
			return NSLOCTEXT("StarRoversAugmentChoice", "NegativeStateCondition", "NEGATIVE STATE CLEARS");
		case ESRProcessTagTriggerV2::ProcessArchetypeChanged:
			return NSLOCTEXT("StarRoversAugmentChoice", "ArchetypeCondition", "ARCHETYPE CHANGES");
		case ESRProcessTagTriggerV2::FirstEnergyChangeAfterImport:
			return NSLOCTEXT("StarRoversAugmentChoice", "ImportCondition", "FIRST CHANGE AFTER IMPORT");
		case ESRProcessTagTriggerV2::FirstValidProcessOutsideOrigin:
			return NSLOCTEXT("StarRoversAugmentChoice", "OffOriginCondition", "FIRST PROCESS OFF-ORIGIN");
		default:
			return NSLOCTEXT("StarRoversAugmentChoice", "ValidProcessCondition", "VALID PROCESS");
		}
	}

	void AddProcessTagFlow(
		FName TagId,
		const FSRAugmentBuildContextV2& Context,
		bool bEligible,
		TArray<FSRAugmentConditionEffectPresentation>& OutRows)
	{
		FSRProcessTagDefinitionV2 TagDefinition;
		FSRAugmentConditionEffectPresentation& Row = OutRows.AddDefaulted_GetRef();
		const bool bKnown = FSRResourceSystemContent::TryGetProcessTagDefinition(TagId, TagDefinition);
		Row.ConditionText = bKnown
			? ResolveProcessTagCondition(TagDefinition.Trigger)
			: NSLOCTEXT("StarRoversAugmentChoice", "TagTriggerCondition", "TAG TRIGGER");
		const FText TagName = UpperText(bKnown ? TagDefinition.DisplayName : DisplayNameFromId(TagId));
		Row.EffectText = bKnown
			? FText::Format(
				NSLOCTEXT("StarRoversAugmentChoice", "TagEffect", "{0} | {1}"),
				TagName,
				FormatEnergyDelta(TagDefinition.EnergyDelta))
			: TagName;
		Row.DetailText = bKnown
			? FText::Format(
				NSLOCTEXT(
					"StarRoversAugmentChoice",
					"TagFlowDetail",
					"Tag recipe: prime {0} in a Tag Imprinter. Its trigger applies {1} once; this is not passive Energy on every process."),
				TagDefinition.DisplayName,
				FormatEnergyDelta(TagDefinition.EnergyDelta))
			: NSLOCTEXT("StarRoversAugmentChoice", "UnknownTagFlowDetail", "Unlocks a triggered Tag Imprinter recipe.");
		const bool bAlreadyAvailable = FSRAugmentPackageContentV2::IsProcessTagRecipeUnlocked(
			TagId,
			Context.SelectedPackageIds);
		Row.ConditionState = bEligible ? ESRUIVisualState::Positive : ESRUIVisualState::Warning;
		Row.EffectState = bAlreadyAvailable ? ESRUIVisualState::Disabled : ESRUIVisualState::Selected;
	}

	void ResolveFuelImprintFlow(FName ImprintId, FText& OutCondition, FText& OutEffect, FText& OutDetail)
	{
		const FText ImprintName = UpperText(ResolveFuelImprintName(ImprintId));
		if (ImprintId == FName(TEXT("TwinSeal")))
		{
			OutCondition = NSLOCTEXT("StarRoversAugmentChoice", "TwinSealCondition", "MATCHED GRADE SET");
			OutEffect = FText::Format(NSLOCTEXT("StarRoversAugmentChoice", "TwinSealEffect", "{0} | FINAL B"), ImprintName);
			OutDetail = NSLOCTEXT("StarRoversAugmentChoice", "TwinSealDetail", "One valid Twin Seal per matching Card key contributes its authored B bonus in the final Fabricator batch.");
		}
		else if (ImprintId == FName(TEXT("ConvergenceSeal")))
		{
			OutCondition = NSLOCTEXT("StarRoversAugmentChoice", "ConvergenceCondition", "3 EXPORTED ORIGINS");
			OutEffect = FText::Format(NSLOCTEXT("StarRoversAugmentChoice", "ConvergenceEffect", "{0} | FINAL B"), ImprintName);
			OutDetail = NSLOCTEXT("StarRoversAugmentChoice", "ConvergenceDetail", "At least three Cards must be completed at distinct Origins and exported before final fabrication.");
		}
		else if (ImprintId == FName(TEXT("FoundrySeal")))
		{
			OutCondition = NSLOCTEXT("StarRoversAugmentChoice", "FoundryCondition", "ALL CARDS AT FOUNDRY");
			OutEffect = FText::Format(NSLOCTEXT("StarRoversAugmentChoice", "FoundryEffect", "{0} | FINAL B"), ImprintName);
			OutDetail = NSLOCTEXT("StarRoversAugmentChoice", "FoundryDetail", "Every Card's last process must occur on the Stellar Fuel Fabricator body.");
		}
		else if (ImprintId == FName(TEXT("PilgrimSeal")))
		{
			OutCondition = NSLOCTEXT("StarRoversAugmentChoice", "PilgrimCondition", "ALL CARDS OFF-ORIGIN");
			OutEffect = FText::Format(NSLOCTEXT("StarRoversAugmentChoice", "PilgrimEffect", "{0} | FINAL B"), ImprintName);
			OutDetail = NSLOCTEXT("StarRoversAugmentChoice", "PilgrimDetail", "Every Card must complete a valid process away from its Origin before final fabrication.");
		}
		else if (ImprintId == FName(TEXT("PrismaticCatalyst")))
		{
			OutCondition = NSLOCTEXT("StarRoversAugmentChoice", "PrismaticCondition", "4 SPECTRA IN BATCH");
			OutEffect = FText::Format(NSLOCTEXT("StarRoversAugmentChoice", "PrismaticEffect", "{0} | FINAL C"), ImprintName);
			OutDetail = NSLOCTEXT("StarRoversAugmentChoice", "PrismaticDetail", "One Catalyst can contribute its authored C bonus when all four Spectra are present in the final batch.");
		}
		else
		{
			OutCondition = NSLOCTEXT("StarRoversAugmentChoice", "FinalBatchCondition", "FINAL 5-CARD BATCH");
			OutEffect = ImprintName;
			OutDetail = NSLOCTEXT("StarRoversAugmentChoice", "UnknownImprintDetail", "Unlocks a Fuel Imprinter recipe evaluated only by the final Stellar Fuel Fabricator.");
		}
	}

	void AddFuelImprintFlow(
		FName ImprintId,
		const FSRAugmentBuildContextV2& Context,
		bool bEligible,
		TArray<FSRAugmentConditionEffectPresentation>& OutRows)
	{
		FSRAugmentConditionEffectPresentation& Row = OutRows.AddDefaulted_GetRef();
		ResolveFuelImprintFlow(ImprintId, Row.ConditionText, Row.EffectText, Row.DetailText);
		const bool bAlreadyAvailable = FSRAugmentPackageContentV2::IsFuelImprintRecipeUnlocked(
			ImprintId,
			Context.SelectedPackageIds);
		Row.ConditionState = bEligible ? ESRUIVisualState::Positive : ESRUIVisualState::Warning;
		Row.EffectState = bAlreadyAvailable ? ESRUIVisualState::Disabled : ESRUIVisualState::Selected;
	}

	void AddLogisticsModuleFlow(
		FName ModuleId,
		const FSRAugmentBuildContextV2& Context,
		bool bEligible,
		TArray<FSRAugmentConditionEffectPresentation>& OutRows)
	{
		FSRAugmentConditionEffectPresentation& Row = OutRows.AddDefaulted_GetRef();
		FSRConditionedTransitModuleRulesV2 Rules;
		if (TryResolveLogisticsModule(ModuleId, Rules))
		{
			const FText FamilyName = UpperText(StaticEnum<ESRResourceFamily>()->GetDisplayNameTextByValue(
				static_cast<int64>(Rules.CompatibleFamily)));
			Row.ConditionText = FText::Format(
				NSLOCTEXT("StarRoversAugmentChoice", "ModuleCondition", "{0} IN CONDITIONED HOLD"),
				FamilyName);
			FText ActionText;
			if (Rules.FamilyAction != ESRResourceFamilyAction::None)
			{
				ActionText = UpperText(StaticEnum<ESRResourceFamilyAction>()->GetDisplayNameTextByValue(
					static_cast<int64>(Rules.FamilyAction)));
			}
			else
			{
				ActionText = UpperText(StaticEnum<ESRResourceProcessTemperatureState>()->GetDisplayNameTextByValue(
					static_cast<int64>(Rules.Temperature)));
			}
			Row.EffectText = FMath::IsNearlyZero(Rules.BaseEnergyDelta)
				? FText::Format(NSLOCTEXT("StarRoversAugmentChoice", "ModuleAction", "{0} PROCESS"), ActionText)
				: FText::Format(
					NSLOCTEXT("StarRoversAugmentChoice", "ModuleEnergyAction", "{0} | {1}"),
					ActionText,
					FormatEnergyDelta(Rules.BaseEnergyDelta));
			Row.DetailText = Rules.PreviewText;
		}
		else
		{
			Row.ConditionText = NSLOCTEXT("StarRoversAugmentChoice", "RouteCondition", "COMPATIBLE HUB ROUTE");
			Row.EffectText = UpperText(DisplayNameFromId(ModuleId));
			Row.DetailText = NSLOCTEXT("StarRoversAugmentChoice", "UnknownRouteDetail", "Unlocks one conditioned Route Module.");
		}
		const bool bAlreadyAvailable = FSRAugmentPackageContentV2::IsLogisticsModuleUnlocked(
			ModuleId,
			Context.SelectedPackageIds);
		Row.ConditionState = bEligible ? ESRUIVisualState::Positive : ESRUIVisualState::Warning;
		Row.EffectState = bAlreadyAvailable ? ESRUIVisualState::Disabled : ESRUIVisualState::Selected;
	}

	void AddRouteProfileFlow(
		FName ProfileId,
		const FSRAugmentBuildContextV2& Context,
		bool bEligible,
		TArray<FSRAugmentConditionEffectPresentation>& OutRows)
	{
		FSRAugmentConditionEffectPresentation& Row = OutRows.AddDefaulted_GetRef();
		ESRSpaceLogisticsRouteProfileV2 Profile;
		if (FSRFleetCapacityV2::TryResolveRouteProfileId(ProfileId, Profile))
		{
			const FSRSpaceLogisticsRouteProfileRulesV2 Rules =
				FSRFleetCapacityV2::GetRouteProfileRules(Profile);
			switch (Profile)
			{
			case ESRSpaceLogisticsRouteProfileV2::BulkRawHold:
				Row.ConditionText = NSLOCTEXT("StarRoversAugmentChoice", "BulkProfileCondition", "RAW CARD OR UTILITY ROUTE");
				break;
			case ESRSpaceLogisticsRouteProfileV2::ConditionedHold:
				Row.ConditionText = NSLOCTEXT("StarRoversAugmentChoice", "ConditionedProfileCondition", "FITTED HOLD + MATCHING CARD");
				break;
			default:
				Row.ConditionText = NSLOCTEXT("StarRoversAugmentChoice", "ProfileCondition", "HUB ROUTE CONFIGURATION");
				break;
			}
			Row.EffectText = FText::Format(
				NSLOCTEXT("StarRoversAugmentChoice", "ProfileEffect", "SELECT {0} | {1} CARGO/LOAD"),
				UpperText(Rules.DisplayName),
				FText::AsNumber(FSRFleetCapacityV2::ResolveMaximumCargoPerFleetLoad(Profile)));
			Row.DetailText = FText::Format(
				NSLOCTEXT(
					"StarRoversAugmentChoice",
					"ProfileDetail",
					"Unlocks a Route hull profile: {0}. Maximum Cargo {1}, Fleet Load {2}. A conditioned Module is fitted separately."),
				Rules.CargoContractText,
				FText::AsNumber(Rules.CargoCapacity),
				FText::AsNumber(Rules.FleetLoad));
		}
		else
		{
			Row.ConditionText = NSLOCTEXT("StarRoversAugmentChoice", "UnknownProfileCondition", "HUB ROUTE CONFIGURATION");
			Row.EffectText = UpperText(DisplayNameFromId(ProfileId));
			Row.DetailText = NSLOCTEXT("StarRoversAugmentChoice", "UnknownProfileDetail", "Unlocks one Route hull profile.");
		}
		const bool bAlreadyAvailable = FSRAugmentPackageContentV2::IsRouteProfileUnlocked(
			ProfileId,
			Context.SelectedPackageIds);
		Row.ConditionState = bEligible ? ESRUIVisualState::Positive : ESRUIVisualState::Warning;
		Row.EffectState = bAlreadyAvailable ? ESRUIVisualState::Disabled : ESRUIVisualState::Selected;
	}

	void BuildConditionEffectRows(
		const FSRAugmentPackageDefinitionV2& Definition,
		const FSRAugmentBuildContextV2& Context,
		bool bEligible,
		TArray<FSRAugmentConditionEffectPresentation>& OutRows)
	{
		OutRows.Reset();
		for (const FName TagId : Definition.GrantedProcessTagIds)
		{
			AddProcessTagFlow(TagId, Context, bEligible, OutRows);
		}
		for (const FName ImprintId : Definition.GrantedFuelImprintIds)
		{
			AddFuelImprintFlow(ImprintId, Context, bEligible, OutRows);
		}
		for (const FName ProfileId : Definition.GrantedRouteProfileIds)
		{
			AddRouteProfileFlow(ProfileId, Context, bEligible, OutRows);
		}
		for (const FName ModuleId : Definition.GrantedLogisticsModuleIds)
		{
			AddLogisticsModuleFlow(ModuleId, Context, bEligible, OutRows);
		}
		for (const FName FacilityId : Definition.GrantedFacilityContentIds)
		{
			FSRAugmentConditionEffectPresentation& Row = OutRows.AddDefaulted_GetRef();
			Row.ConditionText = NSLOCTEXT("StarRoversAugmentChoice", "FacilitySelectCondition", "SELECT PACKAGE");
			Row.EffectText = FText::Format(
				NSLOCTEXT("StarRoversAugmentChoice", "FacilityBuildEffect", "BUILD {0}"),
				UpperText(ResolveFacilityName(FacilityId)));
			Row.DetailText = NSLOCTEXT("StarRoversAugmentChoice", "FacilityBuildDetail", "Adds this Facility to the Build Dock; normal placement and Capacity costs still apply.");
			Row.ConditionState = bEligible ? ESRUIVisualState::Positive : ESRUIVisualState::Warning;
			Row.EffectState = FSRAugmentPackageContentV2::IsFacilityContentUnlocked(
				FacilityId,
				Context.SelectedPackageIds)
				? ESRUIVisualState::Disabled
				: ESRUIVisualState::Selected;
		}
	}

	FText BuildRiskText(
		const FSRAugmentPackageDefinitionV2& Definition,
		int32 AlreadyAvailableCount,
		bool bEligible,
		const FString& EligibilityFailure)
	{
		TArray<FText> Risks;
		if (!bEligible && !EligibilityFailure.IsEmpty())
		{
			Risks.Add(FText::Format(
				NSLOCTEXT("StarRoversAugmentChoice", "IneligibleRisk", "Context changed: {0}"),
				FText::FromString(EligibilityFailure)));
		}
		if (Definition.IsMacroDoctrine())
		{
			Risks.Add(NSLOCTEXT(
				"StarRoversAugmentChoice",
				"DoctrineRisk",
				"Permanent for this Run: occupies the only Macro Doctrine slot and excludes the alternatives."));
		}
		if (!Definition.GrantedLogisticsModuleIds.IsEmpty())
		{
			Risks.Add(NSLOCTEXT(
				"StarRoversAugmentChoice",
				"LogisticsRisk",
				"Not passive: needs two Hub endpoints, compatible cargo, and conditioning dwell time."));
		}
		if (!Definition.GrantedRouteProfileIds.IsEmpty())
		{
			Risks.Add(NSLOCTEXT(
				"StarRoversAugmentChoice",
				"RouteProfileRisk",
				"Route hull unlock: it changes cargo eligibility, payload, and Fleet Load only after you select it on an empty docked route."));
		}
		if (!Definition.GrantedProcessTagIds.IsEmpty())
		{
			Risks.Add(NSLOCTEXT(
				"StarRoversAugmentChoice",
				"TagRisk",
				"Recipe only: it needs a Tag Imprinter and its stated trigger; it is not Energy on every process."));
		}
		if (!Definition.GrantedFuelImprintIds.IsEmpty())
		{
			Risks.Add(NSLOCTEXT(
				"StarRoversAugmentChoice",
				"ImprintRisk",
				"Recipe only: final multiplication still happens once, in the Stellar Fuel Fabricator."));
		}
		if (!Definition.GrantedFacilityContentIds.IsEmpty())
		{
			Risks.Add(NSLOCTEXT(
				"StarRoversAugmentChoice",
				"FacilityRisk",
				"Construction unlock only: placement, Operational Capacity, and throughput costs still apply."));
		}
		if (AlreadyAvailableCount > 0)
		{
			Risks.Add(FText::Format(
				NSLOCTEXT("StarRoversAugmentChoice", "DuplicateGrantRisk", "{0} grant(s) are already available and are not new unlocks."),
				FText::AsNumber(AlreadyAvailableCount)));
		}
		if (Risks.IsEmpty())
		{
			Risks.Add(NSLOCTEXT(
				"StarRoversAugmentChoice",
				"DefaultPackageRisk",
				"No passive multiplier: the unlocked content must be built into a functioning Line."));
		}
		return FText::FromString(JoinText(Risks, TEXT(" ")));
	}

	FText BuildWatchSummary(
		const FSRAugmentPackageDefinitionV2& Definition,
		bool bEligible,
		int32 AlreadyAvailableCount)
	{
		if (!bEligible)
		{
			return AlreadyAvailableCount > 0
				? NSLOCTEXT("StarRoversAugmentChoice", "OwnedWatch", "ALREADY OWNED")
				: NSLOCTEXT("StarRoversAugmentChoice", "ChangedWatch", "CONTEXT CHANGED");
		}
		if (Definition.IsMacroDoctrine())
		{
			return NSLOCTEXT("StarRoversAugmentChoice", "DoctrineWatch", "USES THE ONE DOCTRINE SLOT");
		}
		if (!Definition.GrantedLogisticsModuleIds.IsEmpty())
		{
			return NSLOCTEXT("StarRoversAugmentChoice", "TransitWatch", "REQUIRES DOCK DWELL");
		}
		if (!Definition.GrantedProcessTagIds.IsEmpty())
		{
			return NSLOCTEXT("StarRoversAugmentChoice", "TagWatch", "TRIGGERED RECIPE, NOT PASSIVE");
		}
		if (!Definition.GrantedFuelImprintIds.IsEmpty())
		{
			return NSLOCTEXT("StarRoversAugmentChoice", "ImprintWatch", "FINAL FABRICATOR ONLY");
		}
		if (!Definition.GrantedFacilityContentIds.IsEmpty())
		{
			return NSLOCTEXT("StarRoversAugmentChoice", "FacilityWatch", "PLACEMENT + CAPACITY COST");
		}
		return NSLOCTEXT("StarRoversAugmentChoice", "DefaultWatch", "BUILD IT INTO THE LINE");
	}

	FText BuildFullDetailText(const FSRAugmentChoicePresentation& Presentation)
	{
		TArray<FText> Sections;
		Sections.Add(Presentation.TitleText);
		Sections.Add(Presentation.DescriptionText);
		Sections.Add(FText::Format(
			NSLOCTEXT("StarRoversAugmentChoice", "TooltipFit", "RUN FIT: {0}\n{1}"),
			Presentation.RunFitBadgeText,
			Presentation.FitDetailText));
		Sections.Add(FText::Format(
			NSLOCTEXT("StarRoversAugmentChoice", "TooltipUnlocks", "UNLOCKS:\n{0}"),
			Presentation.UnlockDetailText));
		if (!Presentation.ConditionEffectRows.IsEmpty())
		{
			TArray<FText> FlowLines;
			for (const FSRAugmentConditionEffectPresentation& Row : Presentation.ConditionEffectRows)
			{
				FlowLines.Add(FText::Format(
					NSLOCTEXT("StarRoversAugmentChoice", "TooltipFlowLine", "{0} > {1}\n{2}"),
					Row.ConditionText,
					Row.EffectText,
					Row.DetailText));
			}
			Sections.Add(FText::Format(
				NSLOCTEXT("StarRoversAugmentChoice", "TooltipFlows", "WHEN > RESULT:\n{0}"),
				FText::FromString(JoinText(FlowLines, TEXT("\n")))));
		}
		if (!Presentation.ImpactDetailText.IsEmpty())
		{
			Sections.Add(FText::Format(
				NSLOCTEXT("StarRoversAugmentChoice", "TooltipLine", "LINE SHAPE: {0}"),
				Presentation.ImpactDetailText));
		}
		Sections.Add(FText::Format(
			NSLOCTEXT("StarRoversAugmentChoice", "TooltipWatch", "WATCH: {0}"),
			Presentation.RiskDetailText));
		return FText::FromString(JoinText(Sections, TEXT("\n\n")));
	}
}

FSRAugmentChoicePresentation FSRAugmentChoicePresentationBuilder::Build(
	const FSRAugmentChoice& Choice,
	const FSRAugmentBuildContextV2& Context)
{
	FSRAugmentChoicePresentation Result;
	Result.TitleText = Choice.DisplayName.IsEmpty()
		? DisplayNameFromId(Choice.StructureId)
		: Choice.DisplayName;
	Result.DescriptionText = Choice.Description;
	Result.OfferBadgeText = ResolveOfferRoleText(Choice.OfferRole);
	Result.RarityBadgeText = ResolveRarityText(Choice.Rarity);
	Result.OfferState = ResolveOfferState(Choice.OfferRole);
	Result.CardState = Result.OfferState;
	Result.SelectActionText = NSLOCTEXT("StarRoversAugmentChoice", "SelectPackageAction", "SELECT PACKAGE");

	if (Choice.ChoiceKind != ESRAugmentChoiceKind::ResourceV2Package)
	{
		Result.RunFitBadgeText = NSLOCTEXT("StarRoversAugmentChoice", "LegacyReadyBadge", "READY");
		Result.StrategyBadgeText = NSLOCTEXT("StarRoversAugmentChoice", "LegacyBuildStrategy", "BUILD OPTION");
		Result.UnlockSectionText = NSLOCTEXT("StarRoversAugmentChoice", "LegacyUnlockSection", "NEW UNLOCK | 1");
		Result.UnlockDetailText = FText::Format(
			NSLOCTEXT("StarRoversAugmentChoice", "LegacyUnlockDetail", "FACILITY | {0}"),
			Result.TitleText);
		Result.FitSectionText = NSLOCTEXT("StarRoversAugmentChoice", "LegacyFitSection", "RUN FIT");
		Result.FitDetailText = BuildRoleFitText(ESRAugmentOfferRoleV2::Legacy);
		Result.ImpactDetailText = NSLOCTEXT(
			"StarRoversAugmentChoice",
			"LegacyImpact",
			"Build Dock -> place Facility -> connect Line");
		Result.RiskDetailText = NSLOCTEXT(
			"StarRoversAugmentChoice",
			"LegacyRisk",
			"Placement, construction, Operational Capacity, and throughput costs still apply.");
		Result.WatchSummaryText = NSLOCTEXT("StarRoversAugmentChoice", "LegacyWatch", "PLACEMENT + CAPACITY COST");
		Result.SelectActionText = NSLOCTEXT("StarRoversAugmentChoice", "SelectFacilityAction", "UNLOCK FACILITY");
		Result.NewUnlockCount = 1;
		FSRAugmentConditionEffectPresentation& Flow = Result.ConditionEffectRows.AddDefaulted_GetRef();
		Flow.ConditionText = NSLOCTEXT("StarRoversAugmentChoice", "LegacySelectCondition", "SELECT");
		Flow.EffectText = FText::Format(
			NSLOCTEXT("StarRoversAugmentChoice", "LegacyBuildEffect", "BUILD {0}"),
			UpperText(Result.TitleText));
		Flow.DetailText = Result.RiskDetailText;
		Result.FullDetailText = BuildFullDetailText(Result);
		return Result;
	}

	Result.bIsResourceV2Package = true;
	FSRAugmentPackageDefinitionV2 Definition;
	if (!FSRAugmentPackageContentV2::TryGetDefinition(Choice.PackageId, Definition))
	{
		Result.bEligibleInContext = false;
		Result.CardState = ESRUIVisualState::Disabled;
		Result.RunFitState = ESRUIVisualState::Warning;
		Result.RunFitBadgeText = NSLOCTEXT("StarRoversAugmentChoice", "UnknownFitBadge", "UNKNOWN");
		Result.StrategyBadgeText = NSLOCTEXT("StarRoversAugmentChoice", "UnknownStrategy", "CATALOG ERROR");
		Result.UnlockSectionText = NSLOCTEXT("StarRoversAugmentChoice", "UnknownUnlockSection", "PACKAGE CONTENT");
		Result.UnlockDetailText = Choice.GrantSummary;
		Result.FitSectionText = NSLOCTEXT("StarRoversAugmentChoice", "UnknownFitSection", "RUN FIT");
		Result.FitDetailText = BuildRoleFitText(Choice.OfferRole);
		Result.ImpactDetailText = Choice.ExampleLinePreview;
		Result.RiskDetailText = NSLOCTEXT(
			"StarRoversAugmentChoice",
			"UnknownPackageRisk",
			"Package definition is unavailable; verify the content catalog before selecting.");
		Result.WatchSummaryText = NSLOCTEXT("StarRoversAugmentChoice", "UnknownWatch", "CATALOG MISSING");
		Result.RiskState = ESRUIVisualState::Warning;
		Result.SelectActionText = NSLOCTEXT("StarRoversAugmentChoice", "UnknownAction", "UNAVAILABLE");
		FSRAugmentConditionEffectPresentation& Flow = Result.ConditionEffectRows.AddDefaulted_GetRef();
		Flow.ConditionText = NSLOCTEXT("StarRoversAugmentChoice", "UnknownCatalogCondition", "CATALOG ENTRY");
		Flow.EffectText = NSLOCTEXT("StarRoversAugmentChoice", "UnknownCatalogEffect", "MISSING");
		Flow.DetailText = Result.RiskDetailText;
		Flow.ConditionState = ESRUIVisualState::Warning;
		Flow.EffectState = ESRUIVisualState::Disabled;
		Result.FullDetailText = BuildFullDetailText(Result);
		return Result;
	}

	Result.bIsMacroDoctrine = Definition.IsMacroDoctrine();
	Result.StrategyBadgeText = UpperText(DisplayNameFromId(Definition.StrategyId));
	TArray<FText> NewUnlocks;
	auto AddUnlock = [&Result, &NewUnlocks](const FText& Kind, const FText& Name, bool bAlreadyAvailable)
	{
		if (bAlreadyAvailable)
		{
			++Result.AlreadyAvailableCount;
			return;
		}
		++Result.NewUnlockCount;
		NewUnlocks.Add(FText::Format(
			NSLOCTEXT("StarRoversAugmentChoice", "UnlockItemFormat", "{0} | {1}"),
			Kind,
			Name));
	};

	for (const FName TagId : Definition.GrantedProcessTagIds)
	{
		AddUnlock(
			NSLOCTEXT("StarRoversAugmentChoice", "ProcessTagKind", "TAG RECIPE"),
			ResolveProcessTagName(TagId),
			FSRAugmentPackageContentV2::IsProcessTagRecipeUnlocked(TagId, Context.SelectedPackageIds));
	}
	for (const FName ImprintId : Definition.GrantedFuelImprintIds)
	{
		AddUnlock(
			NSLOCTEXT("StarRoversAugmentChoice", "FuelImprintKind", "FUEL RECIPE"),
			ResolveFuelImprintName(ImprintId),
			FSRAugmentPackageContentV2::IsFuelImprintRecipeUnlocked(ImprintId, Context.SelectedPackageIds));
	}
	for (const FName FacilityId : Definition.GrantedFacilityContentIds)
	{
		AddUnlock(
			NSLOCTEXT("StarRoversAugmentChoice", "FacilityKind", "FACILITY"),
			ResolveFacilityName(FacilityId),
			FSRAugmentPackageContentV2::IsFacilityContentUnlocked(FacilityId, Context.SelectedPackageIds));
	}
	for (const FName ModuleId : Definition.GrantedLogisticsModuleIds)
	{
		AddUnlock(
			NSLOCTEXT("StarRoversAugmentChoice", "RouteModuleKind", "ROUTE MODULE"),
			ResolveLogisticsModuleName(ModuleId),
			FSRAugmentPackageContentV2::IsLogisticsModuleUnlocked(ModuleId, Context.SelectedPackageIds));
	}
	for (const FName ProfileId : Definition.GrantedRouteProfileIds)
	{
		AddUnlock(
			NSLOCTEXT("StarRoversAugmentChoice", "RouteProfileKind", "ROUTE PROFILE"),
			ResolveRouteProfileName(ProfileId),
			FSRAugmentPackageContentV2::IsRouteProfileUnlocked(ProfileId, Context.SelectedPackageIds));
	}

	Result.UnlockSectionText = FText::Format(
		NSLOCTEXT("StarRoversAugmentChoice", "UnlockCountSection", "NEW UNLOCKS | {0}"),
		FText::AsNumber(Result.NewUnlockCount));
	Result.UnlockDetailText = NewUnlocks.IsEmpty()
		? NSLOCTEXT("StarRoversAugmentChoice", "NoNewUnlocks", "NO NEW CONTENT IN THIS RUN STATE")
		: FText::FromString(JoinText(NewUnlocks, TEXT("\n")));

	const FSRAugmentPackageEligibilityReportV2 Eligibility =
		FSRAugmentPackageContentV2::EvaluateEligibility(Definition, Context);
	Result.bEligibleInContext = Eligibility.bEligible;
	Result.SatisfiedRequirementGroupCount = Eligibility.SatisfiedRequirementGroupCount;
	Result.TotalRequirementGroupCount = Eligibility.TotalRequirementGroupCount;
	Result.RunFitState = Result.bEligibleInContext ? ESRUIVisualState::Positive : ESRUIVisualState::Warning;
	if (Eligibility.TotalRequirementGroupCount > 0)
	{
		Result.RunFitBadgeText = FText::Format(
			Result.bEligibleInContext
				? NSLOCTEXT("StarRoversAugmentChoice", "ReadyCountBadge", "READY {0}/{1}")
				: NSLOCTEXT("StarRoversAugmentChoice", "ChangedCountBadge", "CHANGED {0}/{1}"),
			FText::AsNumber(Eligibility.SatisfiedRequirementGroupCount),
			FText::AsNumber(Eligibility.TotalRequirementGroupCount));
	}
	else
	{
		Result.RunFitBadgeText = Result.bEligibleInContext
			? NSLOCTEXT("StarRoversAugmentChoice", "ReadyOpenBadge", "READY | OPEN")
			: NSLOCTEXT("StarRoversAugmentChoice", "ChangedOpenBadge", "CONTEXT CHANGED");
	}

	Result.FitSectionText = NSLOCTEXT("StarRoversAugmentChoice", "RunFitSection", "RUN FIT");
	Result.FitDetailText = BuildRequirementEvidenceText(Definition, Context, Choice.OfferRole);
	Result.ImpactDetailText = Choice.ExampleLinePreview.IsEmpty()
		? Definition.ExampleLinePreview
		: Choice.ExampleLinePreview;
	Result.RiskDetailText = BuildRiskText(
		Definition,
		Result.AlreadyAvailableCount,
		Result.bEligibleInContext,
		Eligibility.FailureReason);
	Result.WatchSummaryText = BuildWatchSummary(
		Definition,
		Result.bEligibleInContext,
		Result.AlreadyAvailableCount);
	Result.RiskState = (!Result.bEligibleInContext
		|| Definition.IsMacroDoctrine()
		|| !Definition.GrantedLogisticsModuleIds.IsEmpty()
		|| !Definition.GrantedRouteProfileIds.IsEmpty())
		? ESRUIVisualState::Warning
		: ESRUIVisualState::Neutral;
	BuildConditionEffectRows(Definition, Context, Result.bEligibleInContext, Result.ConditionEffectRows);
	if (!Result.bEligibleInContext)
	{
		Result.CardState = ESRUIVisualState::Disabled;
		Result.SelectActionText = NSLOCTEXT("StarRoversAugmentChoice", "ContextChangedAction", "CONTEXT CHANGED");
	}
	Result.FullDetailText = BuildFullDetailText(Result);
	return Result;
}
