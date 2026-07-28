#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Automation/SRFacilityResourceV2Processor.h"
#include "Automation/SRRefinementResistanceV2.h"
#include "Automation/SRResourceInstanceOperations.h"
#include "Automation/SRResourceSystemContent.h"
#include "Misc/AutomationTest.h"
#include "Simulation/SRAugmentPackageContent.h"
#include "Simulation/SRSimulationSettings.h"
#include "../Automation/SRFacilityProcessingRuleEvaluator.h"
#include "../Automation/SRFacilityProcessingStepExecutor.h"

namespace StarRovers::RefinementResistanceV2Tests
{
	FSRResourceInstance MakeHeliosIron(double CurrentEnergy)
	{
		FSRResourceInstance Resource;
		FSRResourceSystemContent::MakeReferenceResourceInstance(
			ESRResourceContentPresetV2::HeliosIron,
			FName(TEXT("Cinder")),
			Resource);
		Resource.CurrentEnergy = CurrentEnergy;
		Resource.EnergyValue = CurrentEnergy;
		return Resource;
	}

	FSRFacilityInstance MakeFacility(
		ESRFacilityContentPresetV2 Preset,
		ESRFacilityTemperatureState Temperature,
		const FSRResourceInstance& ProcessingResource)
	{
		USRFacilityDataAsset* Definition = NewObject<USRFacilityDataAsset>(GetTransientPackage());
		FSRResourceSystemContent::ApplyFacilityPreset(*Definition, Preset);
		FSRFacilityInstance Facility;
		Facility.FacilityDataAsset = Definition;
		Facility.TemperatureState = Temperature;
		Facility.bProcessEnabled = true;
		Facility.bProcessing = true;
		Facility.ProcessingInventory.Add(ProcessingResource);
		return Facility;
	}

	FSRFacilityInstance MakeIdleFacilityWithInput(
		ESRFacilityContentPresetV2 Preset,
		ESRFacilityTemperatureState Temperature,
		const FSRResourceInstance& InputResource)
	{
		FSRFacilityInstance Facility = MakeFacility(Preset, Temperature, InputResource);
		Facility.bProcessing = false;
		Facility.ProcessingInventory.Reset();
		FSRFacilityPortInventory& InputPort = Facility.InputPortInventories.AddDefaulted_GetRef();
		InputPort.PortId = FName(TEXT("Input_0"));
		InputPort.PortKind = ESRFacilityPortKind::Input;
		InputPort.PortIndex = 0;
		InputPort.Capacity = 8;
		InputPort.Inventory.Add(InputResource);
		FSRFacilityPortInventory& OutputPort = Facility.OutputPortInventories.AddDefaulted_GetRef();
		OutputPort.PortId = FName(TEXT("Output_0"));
		OutputPort.PortKind = ESRFacilityPortKind::Output;
		OutputPort.PortIndex = 0;
		OutputPort.Capacity = 8;
		return Facility;
	}

	bool HasState(const FSRResourceInstance& Resource, ESRResourceFamilyState State)
	{
		return (Resource.ActiveFamilyStateFlags
			& StarRovers::Resources::GetFamilyStateBit(State)) != 0;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRRefinementResistanceFormulaTest,
	"StarRovers.ResourceSystem.Phase10.RefinementResistance.FormulaAndSeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRRefinementResistanceFormulaTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::RefinementResistanceV2Tests;

	FSRResourceInstance Resource = MakeHeliosIron(5.0);
	const FSRResourceInstance Snapshot = Resource;
	FSRRefinementResistanceResultV2 Result = FSRRefinementResistanceV2::Evaluate(Resource, 4.0f, 40.0);
	TestTrue(TEXT("Reference ResourceId resolves Seed Energy without a Data Asset pointer"), Result.bSeedEnergyResolved);
	TestEqual(TEXT("Helios Iron Seed Energy is five"), Result.SeedEnergy, 5.0);
	TestEqual(TEXT("An unrefined Card keeps the base cycle"), Result.EffectiveProcessSeconds, 4.0f);
	TestTrue(
		TEXT("Formula evaluation is pure"),
		StarRovers::Resources::AreResourceV2RuntimeFieldsEquivalent(Resource, Snapshot));

	Resource.CurrentEnergy = 9.0;
	Result = FSRRefinementResistanceV2::Evaluate(Resource, 4.0f, 40.0);
	TestEqual(TEXT("Energy nine produces the 4.4 second prototype cycle"), Result.EffectiveProcessSeconds, 4.4f);
	TestEqual(TEXT("The multiplier is 1.1"), Result.CycleMultiplier, 1.1);

	Resource.CurrentEnergy = 17.0;
	Result = FSRRefinementResistanceV2::Evaluate(Resource, 4.0f, 40.0);
	TestEqual(TEXT("Energy seventeen produces the 5.2 second prototype cycle"), Result.EffectiveProcessSeconds, 5.2f);

	Resource.ResourceId = FName(TEXT("UnknownCustomCard"));
	Result = FSRRefinementResistanceV2::Evaluate(Resource, 4.0f, 40.0);
	TestTrue(TEXT("Changing identity cannot discard an already captured Seed"), Result.bApplied);
	TestEqual(TEXT("The immutable snapshot remains five after the id changes"), Result.SeedEnergy, 5.0);
	TestEqual(TEXT("Captured Seed keeps the same resistance result"), Result.EffectiveProcessSeconds, 5.2f);

	FSRResourceInstance DynamicCard;
	DynamicCard.ResourceSchemaVersion = StarRovers::Resources::CurrentResourceSchemaVersion;
	DynamicCard.ResourceId = FName(TEXT("ProceduralAlloy_01"));
	DynamicCard.ResourceClass = ESRResourceClass::Card;
	DynamicCard.Family = ESRResourceFamily::Metal;
	DynamicCard.CurrentEnergy = 12.0;
	DynamicCard.Spectrum = ESRResourceSpectrum::Red;
	DynamicCard.Grade = 1;
	StarRovers::Resources::EnsureResourceSeedEnergySnapshot(DynamicCard);
	TestTrue(TEXT("A definition-free dynamic Card captures a Seed"), DynamicCard.bHasSeedEnergySnapshot);
	TestEqual(TEXT("Its creation Energy becomes the Seed"), DynamicCard.SeedEnergySnapshot, 12.0);
	DynamicCard.CurrentEnergy = 52.0;
	StarRovers::Resources::EnsureResourceSeedEnergySnapshot(DynamicCard);
	TestEqual(TEXT("Later Energy changes cannot rewrite the captured Seed"), DynamicCard.SeedEnergySnapshot, 12.0);
	Result = FSRRefinementResistanceV2::Evaluate(DynamicCard, 4.0f, 40.0);
	TestEqual(TEXT("Dynamic Cards receive normal resistance after creation"), Result.EffectiveProcessSeconds, 8.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRRefinementResistanceFacilityTimingTest,
	"StarRovers.ResourceSystem.Phase10.RefinementResistance.RuntimePreviewAndExclusions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRRefinementResistanceFacilityTimingTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::RefinementResistanceV2Tests;

	USRSimulationSettings* Settings = GetMutableDefault<USRSimulationSettings>();
	if (!TestNotNull(TEXT("Simulation settings exist"), Settings))
	{
		return false;
	}
	TGuardValue<ESRResourceRulesetVersion> RulesetGuard(
		Settings->ResourceRulesetVersion,
		ESRResourceRulesetVersion::ResourceV2);
	TGuardValue<float> ScaleGuard(Settings->RefinementResistanceEnergyScaleV2, 40.0f);

	FSRFacilityInstance Forge = MakeFacility(
		ESRFacilityContentPresetV2::InductionForge,
		ESRFacilityTemperatureState::Hot,
		MakeHeliosIron(9.0));
	FSRRefinementResistanceResultV2 Timing =
		FSRFacilityProcessingRuleEvaluator::ResolveRefinementResistance(Forge);
	TestTrue(TEXT("Reserved processing input activates Refinement Resistance"), Timing.bApplied);
	TestEqual(TEXT("Runtime and preview share the 4.4 second result"),
		FSRFacilityProcessingRuleEvaluator::ResolveProcessSeconds(Forge), 4.4f);

	const FSRFacilityInstance CopiedRuntimeSnapshot = Forge;
	TestEqual(TEXT("A copied in-flight runtime snapshot resolves the identical deterministic cycle"),
		FSRFacilityProcessingRuleEvaluator::ResolveProcessSeconds(CopiedRuntimeSnapshot), 4.4f);

	FSRFacilityInstance Anneal = MakeFacility(
		ESRFacilityContentPresetV2::AnnealingChamber,
		ESRFacilityTemperatureState::Normal,
		MakeHeliosIron(100.0));
	Timing = FSRFacilityProcessingRuleEvaluator::ResolveRefinementResistance(Anneal);
	TestFalse(TEXT("Zero-Energy Anneal is excluded from Refinement Resistance"), Timing.bApplied);
	TestEqual(TEXT("Anneal keeps its six second recovery cycle"),
		FSRFacilityProcessingRuleEvaluator::ResolveProcessSeconds(Anneal), 6.0f);

	FSRFacilityInstance TagImprinter = MakeFacility(
		ESRFacilityContentPresetV2::TagImprinter,
		ESRFacilityTemperatureState::Normal,
		MakeHeliosIron(100.0));
	TestEqual(TEXT("Tag imprinting remains a fixed two second mutation"),
		FSRFacilityProcessingRuleEvaluator::ResolveProcessSeconds(TagImprinter), 2.0f);

	Settings->ResourceRulesetVersion = ESRResourceRulesetVersion::Legacy;
	TestEqual(TEXT("The Legacy ruleset is unchanged by the V2 resistance system"),
		FSRFacilityProcessingRuleEvaluator::ResolveProcessSeconds(Forge), 4.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRProcessCycleStartSnapshotTest,
	"StarRovers.ResourceSystem.Phase13.FacilityTiming.StartSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRProcessCycleStartSnapshotTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::RefinementResistanceV2Tests;
	USRSimulationSettings* Settings = GetMutableDefault<USRSimulationSettings>();
	if (!TestNotNull(TEXT("Simulation settings exist"), Settings))
	{
		return false;
	}
	TGuardValue<ESRResourceRulesetVersion> RulesetGuard(
		Settings->ResourceRulesetVersion,
		ESRResourceRulesetVersion::ResourceV2);
	TGuardValue<float> ScaleGuard(Settings->RefinementResistanceEnergyScaleV2, 40.0f);

	FSRFacilityInstance Forge = MakeIdleFacilityWithInput(
		ESRFacilityContentPresetV2::InductionForge,
		ESRFacilityTemperatureState::Hot,
		MakeHeliosIron(9.0));
	TestTrue(TEXT("A real processing start reserves its input"),
		FSRFacilityProcessingStepExecutor::TryStartProcessing(nullptr, Forge));
	TestTrue(TEXT("Processing start captures its resolved duration"), Forge.bHasResolvedProcessSeconds);
	TestEqual(TEXT("The captured prototype duration is 4.4 seconds"), Forge.ResolvedProcessSeconds, 4.4f);

	Settings->RefinementResistanceEnergyScaleV2 = 4.0f;
	Forge.FacilityDataAsset->BaseProcessSeconds = 40.0f;
	TestEqual(TEXT("Runtime setting and authored balance edits cannot move an in-flight finish line"),
		FSRFacilityProcessingRuleEvaluator::ResolveProcessSeconds(Forge),
		4.4f);
	const FSRRefinementResistanceResultV2 InFlightTiming =
		FSRFacilityProcessingRuleEvaluator::ResolveRefinementResistance(Forge);
	TestEqual(TEXT("The in-flight timing explanation also uses the captured duration"),
		InFlightTiming.EffectiveProcessSeconds,
		4.4f);

	const FSRFacilityInstance CopiedSnapshot = Forge;
	TestEqual(TEXT("A copied runtime snapshot retains the same finish line"),
		FSRFacilityProcessingRuleEvaluator::ResolveProcessSeconds(CopiedSnapshot),
		4.4f);

	Forge.ProcessProgressSeconds = 4.4f;
	TestTrue(TEXT("The operation can complete against its captured duration"),
		FSRFacilityProcessingStepExecutor::TryCompleteProcessing(nullptr, Forge));
	TestFalse(TEXT("Completion clears the duration snapshot for the next job"), Forge.bHasResolvedProcessSeconds);
	TestEqual(TEXT("Completion clears the stored resolved seconds"), Forge.ResolvedProcessSeconds, 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRAnnealingChamberContractTest,
	"StarRovers.ResourceSystem.Phase10.Metal.AnnealingChamberContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRAnnealingChamberContractTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::RefinementResistanceV2Tests;

	FSRFacilityContentDefinitionV2 Definition;
	TestTrue(TEXT("Annealing Chamber is present in the reference catalog"),
		FSRResourceSystemContent::TryGetFacilityDefinition(
			ESRFacilityContentPresetV2::AnnealingChamber,
			Definition));
	TestEqual(TEXT("Annealing Chamber accepts Metal"), Definition.AcceptedFamily, ESRResourceFamily::Metal);
	TestEqual(TEXT("Annealing Chamber uses the Anneal action"), Definition.FamilyAction, ESRResourceFamilyAction::Anneal);
	TestEqual(TEXT("Annealing Chamber has no Facility Energy gain"), Definition.FacilityEnergyDelta, 0.0);
	TestEqual(TEXT("Annealing Chamber uses two Operational Load"), Definition.OperationalLoad, 2);

	TArray<FName> TechnologyFacilityIds;
	FSRAugmentPackageContentV2::GetTechnologyFacilityContentIds(TechnologyFacilityIds);
	TestTrue(TEXT("Annealing Chamber is a baseline Technology facility"),
		TechnologyFacilityIds.Contains(FName(TEXT("AnnealingChamber"))));

	FSRResourceInstance FatiguedMetal = MakeHeliosIron(12.0);
	FatiguedMetal.ActiveFamilyStateFlags =
		StarRovers::Resources::GetFamilyStateBit(ESRResourceFamilyState::Fatigued)
		| StarRovers::Resources::GetFamilyStateBit(ESRResourceFamilyState::Tempered);
	FatiguedMetal.ProcessingMemory.GeneralProcessesSinceReset = 4;
	FSRFacilityInstance Anneal = MakeFacility(
		ESRFacilityContentPresetV2::AnnealingChamber,
		ESRFacilityTemperatureState::Normal,
		FatiguedMetal);
	const FSRFacilityResourceV2Evaluation Evaluation =
		FSRFacilityResourceV2Processor::Evaluate(Anneal, FatiguedMetal);
	TestTrue(TEXT("Annealing Chamber evaluation succeeds"), Evaluation.IsSuccess());
	TestEqual(TEXT("Anneal leaves Current Energy unchanged"),
		Evaluation.ResourceProcessResult.OutputEnergy, 12.0);
	TestFalse(TEXT("Anneal clears Fatigued"),
		HasState(Evaluation.ResourceProcessResult.OutputResource, ESRResourceFamilyState::Fatigued));
	TestFalse(TEXT("Anneal clears Tempered"),
		HasState(Evaluation.ResourceProcessResult.OutputResource, ESRResourceFamilyState::Tempered));
	TestEqual(TEXT("Anneal resets Work Strain to zero"),
		Evaluation.ResourceProcessResult.OutputResource.ProcessingMemory.GeneralProcessesSinceReset, 0);
	TestTrue(TEXT("Anneal preview exposes the Work Strain reset"),
		FSRFacilityResourceV2Processor::BuildPreviewSummary(Evaluation).Contains(TEXT("4 -> 0")));
	return true;
}

#endif
