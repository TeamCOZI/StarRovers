#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Automation/SRResourceSystemContent.h"
#include "Automation/SRFacilityResourceV2Processor.h"
#include "Logistics/SRFleetCapacityV2.h"
#include "Misc/AutomationTest.h"
#include "Simulation/SRAugmentPackageContent.h"

namespace StarRovers::AugmentPackageV2Tests
{
	FSRAugmentBuildContextV2 MakeFullContext()
	{
		FSRAugmentBuildContextV2 Context;
		Context.AccessibleFamilies = {
			ESRResourceFamily::Metal,
			ESRResourceFamily::Crystal,
			ESRResourceFamily::Organic,
			ESRResourceFamily::Plasma,
			ESRResourceFamily::Void,
		};
		Context.AccessibleSpectra = {
			ESRResourceSpectrum::Red,
			ESRResourceSpectrum::Green,
			ESRResourceSpectrum::Blue,
			ESRResourceSpectrum::Yellow,
		};
		Context.AccessibleGrades = { 2, 4 };
		FSRAugmentPackageContentV2::GetTechnologyFacilityContentIds(
			Context.AvailableFacilityContentIds);
		Context.HubEndpointCount = 3;
		return Context;
	}

	bool ContainsDefinition(
		const TArray<FSRAugmentPackageDefinitionV2>& Definitions,
		FName PackageId)
	{
		return Definitions.ContainsByPredicate([PackageId](const FSRAugmentPackageDefinitionV2& Definition)
		{
			return Definition.PackageId == PackageId;
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRAugmentPackageCatalogIntegrityTest,
	"StarRovers.ResourceSystem.Phase6.Augment.CatalogIntegrity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRAugmentPackageCatalogIntegrityTest::RunTest(const FString& Parameters)
{
	TArray<FSRAugmentPackageDefinitionV2> Definitions;
	FSRAugmentPackageContentV2::GetAllDefinitions(Definitions);
	TestEqual(TEXT("The reference Augment catalog contains ten Packages"), Definitions.Num(), 10);
	FString FailureReason;
	TestTrue(TEXT("Every Package prerequisite and grant resolves"),
		FSRAugmentPackageContentV2::ValidateCatalog(FailureReason));
	if (!FailureReason.IsEmpty())
	{
		AddInfo(FailureReason);
	}

	TSet<FName> PackageIds;
	int32 MacroDoctrineCount = 0;
	for (const FSRAugmentPackageDefinitionV2& Definition : Definitions)
	{
		TestTrue(TEXT("Package ids are non-empty and unique"),
			!Definition.PackageId.IsNone() && !PackageIds.Contains(Definition.PackageId));
		PackageIds.Add(Definition.PackageId);
		MacroDoctrineCount += Definition.IsMacroDoctrine() ? 1 : 0;
		TestFalse(TEXT("Every Package explains a concrete grant"),
			FSRAugmentPackageContentV2::BuildGrantSummary(Definition).IsEmpty());
		TestFalse(TEXT("Every Package includes a Line preview"), Definition.ExampleLinePreview.IsEmpty());
	}
	TestEqual(TEXT("The prototype has exactly three mutually exclusive Macro Doctrines"),
		MacroDoctrineCount,
		3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRProcessTagAuthoringPolicyTest,
	"StarRovers.ResourceSystem.Phase17.Tag.AuthoringPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRProcessTagAuthoringPolicyTest::RunTest(const FString& Parameters)
{
	FString FailureReason;
	TestTrue(TEXT("The shipped Process Tag catalog obeys the additive one-shot policy"),
		FSRResourceSystemContent::ValidateProcessTagCatalog(FailureReason));
	if (!FailureReason.IsEmpty())
	{
		AddInfo(FailureReason);
	}

	TArray<FSRProcessTagDefinitionV2> Definitions;
	FSRResourceSystemContent::GetAllProcessTagDefinitions(Definitions);
	TSet<ESRProcessTagTriggerV2> UniqueTriggers;
	for (const FSRProcessTagDefinitionV2& Definition : Definitions)
	{
		TestEqual(TEXT("Every Process Tag is consumed by exactly one trigger"),
			Definition.TriggerCount,
			1);
		TestTrue(TEXT("Every Process Tag is a bounded positive additive reward"),
			Definition.EnergyDelta > 0.0 && Definition.EnergyDelta <= 8.0);
		TestFalse(TEXT("No two numeric Tags compete on the exact same trigger"),
			UniqueTriggers.Contains(Definition.Trigger));
		UniqueTriggers.Add(Definition.Trigger);
	}

	FSRProcessTagDefinitionV2 InvalidDefinition;
	TestTrue(TEXT("Overtone exists as the valid mutation baseline"),
		FSRResourceSystemContent::TryGetProcessTagDefinition(TEXT("Overtone"), InvalidDefinition));
	InvalidDefinition.TriggerCount = 2;
	TestFalse(TEXT("Repeatable or permanent Process Tags are rejected"),
		FSRResourceSystemContent::ValidateProcessTagDefinition(InvalidDefinition, FailureReason));
	TestTrue(TEXT("The rejection explains the one-shot contract"),
		FailureReason.Contains(TEXT("one-shot")));

	InvalidDefinition.TriggerCount = 1;
	InvalidDefinition.EnergyDelta = 9.0;
	TestFalse(TEXT("An oversized flat reward cannot dominate every structural choice"),
		FSRResourceSystemContent::ValidateProcessTagDefinition(InvalidDefinition, FailureReason));
	TestTrue(TEXT("The rejection exposes the additive balance range"),
		FailureReason.Contains(TEXT("range")));

	FSRAugmentPackageDefinitionV2 NoOpPackage;
	NoOpPackage.PackageId = TEXT("SyntheticNoOp");
	NoOpPackage.StrategyId = TEXT("SyntheticStrategy");
	NoOpPackage.GrantedProcessTagIds.Add(
		FSRResourceSystemContent::GetProcessTagId(ESRProcessTagContentV2::Crosslink));
	const FSRAugmentPackageEligibilityReportV2 NoOpReport =
		FSRAugmentPackageContentV2::EvaluateEligibility(
			NoOpPackage,
			StarRovers::AugmentPackageV2Tests::MakeFullContext());
	TestFalse(TEXT("A Package that only re-grants Technology content is not selectable"),
		NoOpReport.bEligible);
	TestFalse(TEXT("The no-op guard is explicit in the authoritative report"),
		NoOpReport.bNovelGrantReady);
	TestTrue(TEXT("A no-op rejection is actionable instead of silently consuming a choice"),
		NoOpReport.FailureReason.Contains(TEXT("already available")));

	FSRAugmentPackageDefinitionV2 StateResonator;
	TestTrue(TEXT("State Resonator exists for the no-resource guard"),
		FSRAugmentPackageContentV2::TryGetDefinition(TEXT("StateResonator"), StateResonator));
	FSRAugmentBuildContextV2 NoResourceContext;
	FSRAugmentPackageContentV2::GetTechnologyFacilityContentIds(
		NoResourceContext.AvailableFacilityContentIds);
	const FSRAugmentPackageEligibilityReportV2 NoResourceReport =
		FSRAugmentPackageContentV2::EvaluateEligibility(StateResonator, NoResourceContext);
	TestFalse(TEXT("A Tag Package is not offered before any compatible Card deposit exists"),
		NoResourceReport.bEligible);
	TestFalse(TEXT("The empty Run is rejected by visible Family evidence"),
		NoResourceReport.bCompatibleFamilyReady);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRAugmentPackageEligibilityTest,
	"StarRovers.ResourceSystem.Phase6.Augment.ContextEligibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRAugmentPackageEligibilityTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::AugmentPackageV2Tests;

	FSRAugmentBuildContextV2 LocalContext = MakeFullContext();
	LocalContext.AccessibleFamilies = { ESRResourceFamily::Metal };
	LocalContext.AccessibleSpectra = { ESRResourceSpectrum::Red };
	LocalContext.HubEndpointCount = 1;
	TArray<FSRAugmentPackageDefinitionV2> LocalEligible;
	FSRAugmentPackageContentV2::BuildEligibleDefinitions(LocalContext, LocalEligible);
	TestTrue(TEXT("State Resonator is usable on a local one-body Line"),
		ContainsDefinition(LocalEligible, FName(TEXT("StateResonator"))));
	TestTrue(TEXT("Full-House Matrix has an immediate guaranteed Technology path"),
		ContainsDefinition(LocalEligible, FName(TEXT("FullHouseMatrix"))));
	TestFalse(TEXT("Organic transit is filtered when Organic is inaccessible"),
		ContainsDefinition(LocalEligible, FName(TEXT("BioArkFreight"))));
	TestFalse(TEXT("Route Engines are filtered before two Hub endpoints exist"),
		ContainsDefinition(LocalEligible, FName(TEXT("DeepSpaceTempering"))));
	TestFalse(TEXT("Prismatic Capstone is filtered before its Engine and four Spectra"),
		ContainsDefinition(LocalEligible, FName(TEXT("PrismaticFocus"))));

	FSRAugmentBuildContextV2 DevelopedContext = MakeFullContext();
	DevelopedContext.SelectedPackageIds.Add(FName(TEXT("FullHouseMatrix")));
	TArray<FSRAugmentPackageDefinitionV2> DevelopedEligible;
	FSRAugmentPackageContentV2::BuildEligibleDefinitions(DevelopedContext, DevelopedEligible);
	TestTrue(TEXT("Prismatic Focus appears after Full-House Matrix and four Spectra"),
		ContainsDefinition(DevelopedEligible, FName(TEXT("PrismaticFocus"))));

	DevelopedContext.SelectedPackageIds.Add(FName(TEXT("ConvergenceProtocol")));
	DevelopedContext.ActiveMacroDoctrineId = FName(TEXT("ConvergenceProtocol"));
	FSRAugmentPackageContentV2::BuildEligibleDefinitions(DevelopedContext, DevelopedEligible);
	TestFalse(TEXT("Central Convergence is excluded while another Macro Doctrine is active"),
		ContainsDefinition(DevelopedEligible, FName(TEXT("CentralConvergence"))));
	TestFalse(TEXT("Pilgrim Circuit is excluded while another Macro Doctrine is active"),
		ContainsDefinition(DevelopedEligible, FName(TEXT("PilgrimCircuit"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRAugmentPackageEligibilityReportTest,
	"StarRovers.ResourceSystem.Phase7.Augment.EligibilityReport",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRAugmentPackageEligibilityReportTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::AugmentPackageV2Tests;

	FSRAugmentPackageDefinitionV2 Definition;
	TestTrue(TEXT("Deep-Space Tempering exists for eligibility-report validation"),
		FSRAugmentPackageContentV2::TryGetDefinition(FName(TEXT("DeepSpaceTempering")), Definition));

	FSRAugmentBuildContextV2 Context = MakeFullContext();
	Context.AccessibleFamilies = { ESRResourceFamily::Metal };
	Context.HubEndpointCount = 1;
	FSRAugmentPackageEligibilityReportV2 Report =
		FSRAugmentPackageContentV2::EvaluateEligibility(Definition, Context);
	TestEqual(TEXT("Transit Engine exposes Family and Hub as two authored requirement groups"),
		Report.TotalRequirementGroupCount,
		2);
	TestEqual(TEXT("Only the Family requirement is ready before the second Hub exists"),
		Report.SatisfiedRequirementGroupCount,
		1);
	TestTrue(TEXT("The Family evidence is ready"), Report.bCompatibleFamilyReady);
	TestFalse(TEXT("The Hub evidence is not ready"), Report.bHubNetworkReady);
	TestFalse(TEXT("The authoritative report rejects the Package"), Report.bEligible);
	TestTrue(TEXT("The report preserves the same actionable failure used by selection"),
		Report.FailureReason.Contains(TEXT("Hub")));

	Context.HubEndpointCount = 2;
	Report = FSRAugmentPackageContentV2::EvaluateEligibility(Definition, Context);
	TestTrue(TEXT("The Package becomes eligible at two Hub endpoints"), Report.bEligible);
	TestEqual(TEXT("Both authored requirement groups are now ready"),
		Report.SatisfiedRequirementGroupCount,
		2);

	Context.SelectedPackageIds.Add(Definition.PackageId);
	Report = FSRAugmentPackageContentV2::EvaluateEligibility(Definition, Context);
	TestFalse(TEXT("Already-selected Package is blocked by the selection guard"),
		Report.bPackageSelectionReady);
	TestFalse(TEXT("Already-selected Package is not selectable again"), Report.bEligible);
	TestEqual(TEXT("Run-fit evidence remains factual even when the offer itself is stale"),
		Report.SatisfiedRequirementGroupCount,
		2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRAugmentPackageOfferSafetyTest,
	"StarRovers.ResourceSystem.Phase17.Augment.OfferDiversityAndDeterminism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRAugmentPackageOfferSafetyTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::AugmentPackageV2Tests;
	FString CatalogFailure;
	TestTrue(TEXT("Offer generation starts from a policy-valid Package catalog"),
		FSRAugmentPackageContentV2::ValidateCatalog(CatalogFailure));
	if (!CatalogFailure.IsEmpty())
	{
		AddInfo(CatalogFailure);
	}

	const FSRAugmentBuildContextV2 Context = MakeFullContext();
	FSRAugmentOfferGenerationRulesV2 Rules;
	Rules.ChoiceCount = 3;
	Rules.RandomSeed = 9137;
	TArray<FSRAugmentPackageOfferV2> FirstOffer;
	TArray<FSRAugmentPackageOfferV2> RepeatedOffer;
	FSRAugmentPackageContentV2::GenerateOffer(Context, Rules, FirstOffer);
	FSRAugmentPackageContentV2::GenerateOffer(Context, Rules, RepeatedOffer);
	TestEqual(TEXT("A healthy Build Context receives three choices"), FirstOffer.Num(), 3);
	if (FirstOffer.IsEmpty())
	{
		return false;
	}
	TestEqual(TEXT("The first safe choice is marked Immediate"), FirstOffer[0].OfferRole, ESRAugmentOfferRoleV2::Immediate);
	TestEqual(TEXT("Repeated generation has the same number of choices"),
		RepeatedOffer.Num(),
		FirstOffer.Num());
	TSet<FName> UniquePackageIds;
	TSet<FName> UniqueStrategyIds;
	int32 ConditionedTransitChoiceCount = 0;
	for (int32 OfferIndex = 0; OfferIndex < FirstOffer.Num(); ++OfferIndex)
	{
		TestEqual(TEXT("The seeded Package Offer is deterministic"),
			FirstOffer[OfferIndex].PackageId,
			RepeatedOffer[OfferIndex].PackageId);
		TestFalse(TEXT("A Package cannot occupy two slots in one Offer"),
			UniquePackageIds.Contains(FirstOffer[OfferIndex].PackageId));
		UniquePackageIds.Add(FirstOffer[OfferIndex].PackageId);
		TestTrue(TEXT("Every offered Package is immediately selectable"),
			FirstOffer[OfferIndex].bImmediatelyUsable);

		FSRAugmentPackageDefinitionV2 OfferedDefinition;
		TestTrue(TEXT("Every offered Package resolves to catalog content"),
			FSRAugmentPackageContentV2::TryGetDefinition(
				FirstOffer[OfferIndex].PackageId,
				OfferedDefinition));
		TestFalse(TEXT("A healthy three-card Offer does not repeat a Strategy"),
			UniqueStrategyIds.Contains(OfferedDefinition.StrategyId));
		UniqueStrategyIds.Add(OfferedDefinition.StrategyId);
		ConditionedTransitChoiceCount += OfferedDefinition.StrategyId
			== FName(TEXT("ConditionedTransit")) ? 1 : 0;
	}
	TestTrue(TEXT("Family-specific transit Modules occupy at most one choice slot"),
		ConditionedTransitChoiceCount <= 1);

	FSRAugmentPackageDefinitionV2 FirstDefinition;
	TestTrue(TEXT("The first choice resolves for commitment validation"),
		FSRAugmentPackageContentV2::TryGetDefinition(FirstOffer[0].PackageId, FirstDefinition));
	TestFalse(TEXT("The guaranteed Immediate slot is not a Macro Doctrine commitment"),
		FirstDefinition.IsMacroDoctrine());
	TestTrue(TEXT("The guaranteed Immediate slot is not a late Capstone"),
		FirstDefinition.PackageRole != ESRAugmentPackageRoleV2::Capstone);

	FSRAugmentOfferGenerationRulesV2 FreshRules = Rules;
	FreshRules.RandomSeed = 19631;
	for (const FSRAugmentPackageOfferV2& Offer : FirstOffer)
	{
		FreshRules.RecentlyOfferedPackageIds.Add(Offer.PackageId);
	}
	TArray<FSRAugmentPackageOfferV2> FreshOffer;
	FSRAugmentPackageContentV2::GenerateOffer(Context, FreshRules, FreshOffer);
	TestEqual(TEXT("Recent-card avoidance does not shrink a healthy Offer"),
		FreshOffer.Num(),
		3);
	for (const FSRAugmentPackageOfferV2& Offer : FreshOffer)
	{
		TestFalse(TEXT("A rejected card stays out while a fresh eligible card exists"),
			FreshRules.RecentlyOfferedPackageIds.Contains(Offer.PackageId));
	}

	FSRAugmentBuildContextV2 CompositionContext = MakeFullContext();
	CompositionContext.SelectedPackageIds.Add(FName(TEXT("FullHouseMatrix")));
	TArray<FSRAugmentPackageOfferV2> CompositionOffer;
	FSRAugmentPackageContentV2::GenerateOffer(CompositionContext, Rules, CompositionOffer);
	const FSRAugmentPackageOfferV2* PrismaticOffer = CompositionOffer.FindByPredicate(
		[](const FSRAugmentPackageOfferV2& Offer)
		{
			return Offer.PackageId == FName(TEXT("PrismaticFocus"));
		});
	TestNotNull(TEXT("An eligible compatible Capstone replaces a generic Pivot slot"), PrismaticOffer);
	if (PrismaticOffer)
	{
		TestEqual(TEXT("Prismatic Focus is labelled Capstone"),
			PrismaticOffer->OfferRole,
			ESRAugmentOfferRoleV2::Capstone);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRAugmentPackageGrantAccessTest,
	"StarRovers.ResourceSystem.Phase6.Augment.ConcreteRecipeGrants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRAugmentPackageGrantAccessTest::RunTest(const FString& Parameters)
{
	const FName CrosslinkId = FSRResourceSystemContent::GetProcessTagId(ESRProcessTagContentV2::Crosslink);
	const FName OvertoneId = FSRResourceSystemContent::GetProcessTagId(ESRProcessTagContentV2::Overtone);
	const FName ReclamationId = FSRResourceSystemContent::GetProcessTagId(ESRProcessTagContentV2::Reclamation);
	const FName TwinSealId = FSRResourceSystemContent::GetFuelImprintId(ESRFuelImprintContentV2::TwinSeal);
	const FName FoundrySealId = FSRResourceSystemContent::GetFuelImprintId(ESRFuelImprintContentV2::FoundrySeal);

	TArray<FName> NoPackages;
	TestTrue(TEXT("Crosslink is a guaranteed Technology recipe"),
		FSRAugmentPackageContentV2::IsProcessTagRecipeUnlocked(CrosslinkId, NoPackages));
	TestFalse(TEXT("Overtone is not silently available without State Resonator"),
		FSRAugmentPackageContentV2::IsProcessTagRecipeUnlocked(OvertoneId, NoPackages));
	TestFalse(TEXT("Fuel Imprints are not silently available without a Package"),
		FSRAugmentPackageContentV2::IsFuelImprintRecipeUnlocked(TwinSealId, NoPackages));
	TestTrue(TEXT("Core Processor content is guaranteed by Technology"),
		FSRAugmentPackageContentV2::IsFacilityContentUnlocked(FName(TEXT("PulseProcessor")), NoPackages));
	TestFalse(TEXT("Conditioned Route Modules require their Engine Package"),
		FSRAugmentPackageContentV2::IsLogisticsModuleUnlocked(FName(TEXT("CryogenicHold")), NoPackages));
	TestTrue(TEXT("Neutral Shuttle is available from Technology"),
		FSRAugmentPackageContentV2::IsRouteProfileUnlocked(
			FSRFleetCapacityV2::GetRouteProfileId(ESRSpaceLogisticsRouteProfileV2::NeutralShuttle),
			NoPackages));
	TestTrue(TEXT("Card Courier is available from Technology"),
		FSRAugmentPackageContentV2::IsRouteProfileUnlocked(
			FSRFleetCapacityV2::GetRouteProfileId(ESRSpaceLogisticsRouteProfileV2::CardCourier),
			NoPackages));
	TestFalse(TEXT("Bulk Raw Hold requires Central Convergence"),
		FSRAugmentPackageContentV2::IsRouteProfileUnlocked(
			FSRFleetCapacityV2::GetRouteProfileId(ESRSpaceLogisticsRouteProfileV2::BulkRawHold),
			NoPackages));
	TestFalse(TEXT("Conditioned Hold requires a transit Engine"),
		FSRAugmentPackageContentV2::IsRouteProfileUnlocked(
			FSRFleetCapacityV2::GetRouteProfileId(ESRSpaceLogisticsRouteProfileV2::ConditionedHold),
			NoPackages));

	const TArray<FName> SelectedPackages = {
		FName(TEXT("StateResonator")),
		FName(TEXT("FullHouseMatrix")),
		FName(TEXT("CentralConvergence")),
		FName(TEXT("DeepSpaceTempering")),
	};
	TestTrue(TEXT("State Resonator concretely unlocks Overtone"),
		FSRAugmentPackageContentV2::IsProcessTagRecipeUnlocked(OvertoneId, SelectedPackages));
	TestFalse(TEXT("Recovery Dividend remains a separate decision"),
		FSRAugmentPackageContentV2::IsProcessTagRecipeUnlocked(ReclamationId, SelectedPackages));
	TestTrue(TEXT("Full-House Matrix concretely unlocks Twin Seal"),
		FSRAugmentPackageContentV2::IsFuelImprintRecipeUnlocked(TwinSealId, SelectedPackages));
	TestTrue(TEXT("Central Convergence concretely unlocks Foundry Seal"),
		FSRAugmentPackageContentV2::IsFuelImprintRecipeUnlocked(FoundrySealId, SelectedPackages));
	TestTrue(TEXT("Deep-Space Tempering concretely unlocks Cryogenic Hold"),
		FSRAugmentPackageContentV2::IsLogisticsModuleUnlocked(FName(TEXT("CryogenicHold")), SelectedPackages));
	TestTrue(TEXT("Central Convergence concretely unlocks Bulk Raw Hold"),
		FSRAugmentPackageContentV2::IsRouteProfileUnlocked(
			FSRFleetCapacityV2::GetRouteProfileId(ESRSpaceLogisticsRouteProfileV2::BulkRawHold),
			SelectedPackages));
	TestTrue(TEXT("A conditioned-transit Engine concretely unlocks Conditioned Hold"),
		FSRAugmentPackageContentV2::IsRouteProfileUnlocked(
			FSRFleetCapacityV2::GetRouteProfileId(ESRSpaceLogisticsRouteProfileV2::ConditionedHold),
			SelectedPackages));
	TestEqual(TEXT("Only one selected Macro Doctrine resolves as active"),
		FSRAugmentPackageContentV2::ResolveActiveMacroDoctrineId(SelectedPackages),
		FName(TEXT("CentralConvergence")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRAugmentRouteProfileGrantContractTest,
	"StarRovers.ResourceSystem.Phase18.Logistics.AugmentUnlockPaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRAugmentRouteProfileGrantContractTest::RunTest(const FString& Parameters)
{
	FSRAugmentPackageDefinitionV2 Central;
	TestTrue(TEXT("Central Convergence exists"),
		FSRAugmentPackageContentV2::TryGetDefinition(TEXT("CentralConvergence"), Central));
	TestTrue(TEXT("Central Convergence grants Bulk Raw Hold"),
		Central.GrantedRouteProfileIds.Contains(
			FSRFleetCapacityV2::GetRouteProfileId(ESRSpaceLogisticsRouteProfileV2::BulkRawHold)));

	const TArray<FName> TransitPackages = {
		TEXT("DeepSpaceTempering"),
		TEXT("BioArkFreight"),
		TEXT("GroundedTransit"),
	};
	for (const FName PackageId : TransitPackages)
	{
		FSRAugmentPackageDefinitionV2 Definition;
		TestTrue(TEXT("Each transit Engine resolves"),
			FSRAugmentPackageContentV2::TryGetDefinition(PackageId, Definition));
		TestTrue(TEXT("Each transit Engine grants the shared Conditioned Hold profile"),
			Definition.GrantedRouteProfileIds.Contains(
				FSRFleetCapacityV2::GetRouteProfileId(ESRSpaceLogisticsRouteProfileV2::ConditionedHold)));
		TestEqual(TEXT("Each transit Engine also grants exactly one concrete Hold module"),
			Definition.GrantedLogisticsModuleIds.Num(),
			1);
	}

	FString FailureReason;
	TestTrue(TEXT("The complete catalog has an unlock path for every strategic Route Profile"),
		FSRAugmentPackageContentV2::ValidateCatalog(FailureReason));
	if (!FailureReason.IsEmpty())
	{
		AddInfo(FailureReason);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRAugmentPackageInstanceRecipeSelectionTest,
	"StarRovers.ResourceSystem.Phase6.Augment.InstanceRecipeSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRAugmentPackageInstanceRecipeSelectionTest::RunTest(const FString& Parameters)
{
	USRFacilityDataAsset* TagImprinterData = NewObject<USRFacilityDataAsset>(GetTransientPackage());
	TestTrue(TEXT("Tag Imprinter reference content is authorable"),
		FSRResourceSystemContent::ApplyFacilityPreset(
			*TagImprinterData,
			ESRFacilityContentPresetV2::TagImprinter));

	FSRFacilityInstance TagImprinter;
	TagImprinter.FacilityDataAsset = TagImprinterData;
	const FName OvertoneId = FSRResourceSystemContent::GetProcessTagId(
		ESRProcessTagContentV2::Overtone);
	const FName CrosslinkId = FSRResourceSystemContent::GetProcessTagId(
		ESRProcessTagContentV2::Crosslink);
	TestEqual(TEXT("NAME_None resolves to the authored Tag recipe"),
		FSRFacilityResourceV2Processor::ResolveProcessTagRecipeId(TagImprinter),
		OvertoneId);

	TagImprinter.SelectedProcessTagRecipeId = CrosslinkId;
	TestEqual(TEXT("A Facility instance can select another unlocked Tag recipe"),
		FSRFacilityResourceV2Processor::ResolveProcessTagRecipeId(TagImprinter),
		CrosslinkId);
	TestEqual(TEXT("Selecting a runtime recipe does not mutate shared authored content"),
		TagImprinterData->ResourceV2Process.ProcessTagId,
		OvertoneId);

	FSRResourceInstance Card;
	TestTrue(TEXT("Reference Card is available for recipe evaluation"),
		FSRResourceSystemContent::MakeReferenceResourceInstance(
			ESRResourceContentPresetV2::HeliosIron,
			FName(TEXT("Phase6Body")),
			Card));
	const FSRFacilityResourceV2Evaluation TagEvaluation =
		FSRFacilityResourceV2Processor::Evaluate(TagImprinter, Card);
	TestTrue(TEXT("The selected instance Tag recipe executes"), TagEvaluation.IsSuccess());
	TestEqual(TEXT("The output receives the selected Crosslink recipe"),
		TagEvaluation.ResourceProcessResult.OutputResource.ProcessTagSlot.TagId,
		CrosslinkId);

	TagImprinter.SelectedProcessTagRecipeId = FName(TEXT("UnknownRecipe"));
	const FSRFacilityResourceV2Evaluation InvalidEvaluation =
		FSRFacilityResourceV2Processor::Evaluate(TagImprinter, Card);
	TestEqual(TEXT("An invalid instance override is rejected transactionally"),
		InvalidEvaluation.Outcome,
		ESRFacilityResourceV2Outcome::InvalidProcessTag);
	TestEqual(TEXT("A rejected recipe leaves the input Card untouched"),
		Card.ProcessTagSlot.TagId,
		NAME_None);

	USRFacilityDataAsset* FuelImprinterData = NewObject<USRFacilityDataAsset>(GetTransientPackage());
	TestTrue(TEXT("Fuel Imprinter reference content is authorable"),
		FSRResourceSystemContent::ApplyFacilityPreset(
			*FuelImprinterData,
			ESRFacilityContentPresetV2::FuelImprinter));
	FSRFacilityInstance FuelImprinter;
	FuelImprinter.FacilityDataAsset = FuelImprinterData;
	const FName FoundrySealId = FSRResourceSystemContent::GetFuelImprintId(
		ESRFuelImprintContentV2::FoundrySeal);
	FuelImprinter.SelectedFuelImprintRecipeId = FoundrySealId;
	const FSRFacilityResourceV2Evaluation FuelEvaluation =
		FSRFacilityResourceV2Processor::Evaluate(FuelImprinter, Card);
	TestTrue(TEXT("The selected instance Fuel Imprint recipe executes"), FuelEvaluation.IsSuccess());
	TestEqual(TEXT("The output receives the selected Foundry Seal"),
		FuelEvaluation.ResourceProcessResult.OutputResource.FuelImprintSlot.ImprintId,
		FoundrySealId);
	return true;
}

#endif
