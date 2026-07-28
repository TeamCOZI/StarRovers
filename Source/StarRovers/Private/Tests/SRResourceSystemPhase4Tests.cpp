#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Automation/SRFacilityResourceV2Processor.h"
#include "Automation/SROperationalEconomyProcessor.h"
#include "Automation/SRResourceInstanceOperations.h"
#include "Automation/SRResourceSystemContent.h"
#include "Automation/SRStellarFuelFabricator.h"
#include "Misc/AutomationTest.h"
#include "Simulation/SRSimulationSettings.h"
#include "../Automation/SRFacilityProcessingStepExecutor.h"

namespace StarRovers::ResourceSystemPhase4Tests
{
	USRFacilityDataAsset* MakeFacility(
		ESRFacilityContentPresetV2 Preset,
		FName PayloadId = NAME_None)
	{
		USRFacilityDataAsset* FacilityDataAsset = NewObject<USRFacilityDataAsset>(GetTransientPackage());
		if (!FSRResourceSystemContent::ApplyFacilityPreset(*FacilityDataAsset, Preset))
		{
			return nullptr;
		}
		if (FacilityDataAsset->ResourceV2Process.ProcessRole == ESRFacilityProcessRoleV2::ApplyProcessTag
			&& !PayloadId.IsNone())
		{
			FacilityDataAsset->ResourceV2Process.ProcessTagId = PayloadId;
		}
		else if (FacilityDataAsset->ResourceV2Process.ProcessRole == ESRFacilityProcessRoleV2::ApplyFuelImprint
			&& !PayloadId.IsNone())
		{
			FacilityDataAsset->ResourceV2Process.FuelImprintId = PayloadId;
		}
		return FacilityDataAsset;
	}

	FSRFacilityInstance MakeFacilityInstance(
		USRFacilityDataAsset* FacilityDataAsset,
		ESRFacilityTemperatureState Temperature = ESRFacilityTemperatureState::Normal,
		bool bAddPorts = false)
	{
		FSRFacilityInstance Facility;
		Facility.OccupantId = FName(TEXT("Phase4Facility"));
		Facility.FacilityDataAsset = FacilityDataAsset;
		Facility.TemperatureState = Temperature;
		Facility.bProcessEnabled = true;
		if (bAddPorts)
		{
			FSRFacilityPortInventory& InputPort = Facility.InputPortInventories.AddDefaulted_GetRef();
			InputPort.PortId = FName(TEXT("Input_0"));
			InputPort.PortKind = ESRFacilityPortKind::Input;
			InputPort.PortIndex = 0;
			InputPort.Capacity = 8;
			FSRFacilityPortInventory& OutputPort = Facility.OutputPortInventories.AddDefaulted_GetRef();
			OutputPort.PortId = FName(TEXT("Output_0"));
			OutputPort.PortKind = ESRFacilityPortKind::Output;
			OutputPort.PortIndex = 0;
			OutputPort.Capacity = 8;
		}
		return Facility;
	}

	bool ApplyFacility(
		FSRResourceInstance& Resource,
		ESRFacilityContentPresetV2 Preset,
		ESRFacilityTemperatureState Temperature = ESRFacilityTemperatureState::Normal,
		FName ProcessingBodyId = NAME_None,
		FName PayloadId = NAME_None,
		FSRFacilityResourceV2Evaluation* OutEvaluation = nullptr)
	{
		USRFacilityDataAsset* FacilityDataAsset = MakeFacility(Preset, PayloadId);
		if (!IsValid(FacilityDataAsset))
		{
			return false;
		}
		const FSRFacilityResourceV2Evaluation Evaluation = FSRFacilityResourceV2Processor::Evaluate(
			MakeFacilityInstance(FacilityDataAsset, Temperature),
			Resource,
			ProcessingBodyId);
		if (OutEvaluation)
		{
			*OutEvaluation = Evaluation;
		}
		if (!Evaluation.IsSuccess())
		{
			return false;
		}
		Resource = Evaluation.ResourceProcessResult.OutputResource;
		return true;
	}

	bool ImprintProcessTag(
		FSRResourceInstance& Resource,
		ESRProcessTagContentV2 ProcessTag,
		FSRFacilityResourceV2Evaluation* OutEvaluation = nullptr)
	{
		return ApplyFacility(
			Resource,
			ESRFacilityContentPresetV2::TagImprinter,
			ESRFacilityTemperatureState::Normal,
			NAME_None,
			FSRResourceSystemContent::GetProcessTagId(ProcessTag),
			OutEvaluation);
	}

	FSRResourceInstance MakeReferenceCard(
		ESRResourceContentPresetV2 Preset,
		FName OriginBodyId)
	{
		FSRResourceInstance Resource;
		FSRResourceSystemContent::MakeReferenceResourceInstance(Preset, OriginBodyId, Resource);
		return Resource;
	}

	bool AreEquivalentExceptProcessTag(
		const FSRResourceInstance& Before,
		const FSRResourceInstance& After)
	{
		FSRResourceInstance NormalizedAfter = After;
		NormalizedAfter.ProcessTagSlot = Before.ProcessTagSlot;
		return StarRovers::Resources::AreResourceV2RuntimeFieldsEquivalent(Before, NormalizedAfter);
	}

	bool AreEquivalentExceptFuelImprint(
		const FSRResourceInstance& Before,
		const FSRResourceInstance& After)
	{
		FSRResourceInstance NormalizedAfter = After;
		NormalizedAfter.FuelImprintSlot = Before.FuelImprintSlot;
		return StarRovers::Resources::AreResourceV2RuntimeFieldsEquivalent(Before, NormalizedAfter);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourcePhase4ContentCatalogTest,
	"StarRovers.ResourceSystem.Phase4.ContentCatalog.Integrity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourcePhase4ContentCatalogTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::ResourceSystemPhase4Tests;

	TArray<FSRProcessTagDefinitionV2> ProcessTags;
	TArray<FSRFuelImprintDefinitionV2> FuelImprints;
	TArray<FSRReferenceResourceDefinitionV2> Resources;
	TArray<FSRFacilityContentDefinitionV2> Facilities;
	FSRResourceSystemContent::GetAllProcessTagDefinitions(ProcessTags);
	FSRResourceSystemContent::GetAllFuelImprintDefinitions(FuelImprints);
	FSRResourceSystemContent::GetAllReferenceResourceDefinitions(Resources);
	FSRResourceSystemContent::GetAllFacilityDefinitions(Facilities);
	TestEqual(TEXT("Five one-shot Process Tags are registered"), ProcessTags.Num(), 5);
	TestEqual(TEXT("Five Fuel Imprints are registered for the later Fabricator phase"), FuelImprints.Num(), 5);
	TestEqual(TEXT("The five-card Full House reference set is registered"), Resources.Num(), 5);
	TestEqual(TEXT("Six common, twelve Family, and three infrastructure facilities are registered"), Facilities.Num(), 21);

	TSet<FName> UniqueIds;
	for (const FSRProcessTagDefinitionV2& Definition : ProcessTags)
	{
		TestTrue(TEXT("Process Tag ids are non-empty and unique"),
			!Definition.TagId.IsNone() && !UniqueIds.Contains(Definition.TagId));
		UniqueIds.Add(Definition.TagId);
		TestEqual(TEXT("Prototype Process Tags are one-shot"), Definition.TriggerCount, 1);
		TestTrue(TEXT("Process Tag Energy is finite and additive"), FMath::IsFinite(Definition.EnergyDelta));
	}

	TMap<int32, int32> GradeCounts;
	TSet<ESRResourceSpectrum> Spectra;
	for (const FSRReferenceResourceDefinitionV2& Definition : Resources)
	{
		++GradeCounts.FindOrAdd(Definition.Grade);
		Spectra.Add(Definition.Spectrum);
		FSRResourceInstance Instance;
		TestTrue(TEXT("Every reference resource builds a V2 instance"),
			FSRResourceSystemContent::MakeReferenceResourceInstance(
				Definition.Preset,
				FName(TEXT("Origin")),
				Instance));
		TestEqual(TEXT("Reference instance keeps its Family"), Instance.Family, Definition.Family);
		TestEqual(TEXT("Reference instance keeps its Seed Energy"), Instance.CurrentEnergy, Definition.SeedEnergy);
	}
	TestEqual(TEXT("Reference set contains all four Spectra"), Spectra.Num(), 4);
	TestEqual(TEXT("Reference set contains a Grade 2 pair"), GradeCounts.FindRef(2), 2);
	TestEqual(TEXT("Reference set contains a Grade 4 triple"), GradeCounts.FindRef(4), 3);

	for (const FSRFacilityContentDefinitionV2& Definition : Facilities)
	{
		USRFacilityDataAsset* FacilityDataAsset = MakeFacility(Definition.Preset);
		FString FailureReason;
		const bool bValidContract = Definition.SynthesisRole == ESRFacilitySynthesisRoleV2::StellarFuelFabricator
			? FSRStellarFuelFabricator::ValidateFacilityDefinition(FacilityDataAsset, FailureReason)
			: (Definition.SynthesisRole == ESRFacilitySynthesisRoleV2::IndustrialSupplyFabricator
				|| Definition.SynthesisRole == ESRFacilitySynthesisRoleV2::ServiceCore
				|| Definition.SynthesisRole == ESRFacilitySynthesisRoleV2::FleetBerth)
					? FSROperationalEconomyProcessor::ValidateFacilityDefinition(FacilityDataAsset, FailureReason)
					: FSRFacilityResourceV2Processor::ValidateProcessDefinition(FacilityDataAsset, FailureReason);
		TestTrue(
			*FString::Printf(TEXT("Facility preset %s has a valid V2 contract"), *Definition.ContentId.ToString()),
			IsValid(FacilityDataAsset) && bValidContract);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourcePhase4OvertoneOrderingTest,
	"StarRovers.ResourceSystem.Phase4.ProcessTag.OvertoneAndOrdering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourcePhase4OvertoneOrderingTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::ResourceSystemPhase4Tests;

	FSRResourceInstance Metal = MakeReferenceCard(ESRResourceContentPresetV2::HeliosIron, TEXT("Cinder"));
	const FSRResourceInstance BeforeImprint = Metal;
	TestTrue(TEXT("Overtone can be imprinted"), ImprintProcessTag(Metal, ESRProcessTagContentV2::Overtone));
	TestTrue(TEXT("Tag Imprinter changes only the Process Tag slot"),
		AreEquivalentExceptProcessTag(BeforeImprint, Metal));
	TestTrue(TEXT("Overtone starts Primed"), Metal.ProcessTagSlot.Lifecycle == ESRResourceSlotLifecycle::Primed);

	FSRFacilityResourceV2Evaluation Evaluation;
	TestTrue(TEXT("Hot Forge succeeds"), ApplyFacility(
		Metal,
		ESRFacilityContentPresetV2::InductionForge,
		ESRFacilityTemperatureState::Hot,
		TEXT("Cinder"),
		NAME_None,
		&Evaluation));
	TestFalse(TEXT("Hot alone does not trigger Overtone"), Evaluation.ResourceProcessResult.bProcessTagTriggered);
	TestEqual(TEXT("Metal remains at nine after Forge"), Metal.CurrentEnergy, 9.0);
	TestTrue(TEXT("Overtone is still Primed"), Metal.ProcessTagSlot.Lifecycle == ESRResourceSlotLifecycle::Primed);

	TestTrue(TEXT("Cold Press succeeds"), ApplyFacility(
		Metal,
		ESRFacilityContentPresetV2::CryoPress,
		ESRFacilityTemperatureState::Cold,
		TEXT("Cinder"),
		NAME_None,
		&Evaluation));
	TestTrue(TEXT("Tempered activation triggers Overtone"), Evaluation.ResourceProcessResult.bProcessTagTriggered);
	TestEqual(TEXT("5 + 4 + 3 + 5 Tempered + 5 Overtone = 22"), Metal.CurrentEnergy, 22.0);
	TestEqual(TEXT("Triggered Overtone becomes Spent"), Metal.ProcessTagSlot.Lifecycle, ESRResourceSlotLifecycle::Spent);

	FSRResourceInstance Organic = MakeReferenceCard(ESRResourceContentPresetV2::VerdantSpore, TEXT("Viridia"));
	ImprintProcessTag(Organic, ESRProcessTagContentV2::Overtone);
	ApplyFacility(Organic, ESRFacilityContentPresetV2::GrowthVat, ESRFacilityTemperatureState::Normal, TEXT("Viridia"));
	TestEqual(TEXT("Zero-Energy Growth still triggers Overtone from Matured activation"), Organic.CurrentEnergy, 8.0);

	FSRResourceInstance Void = MakeReferenceCard(ESRResourceContentPresetV2::NullPearl, TEXT("Nadir"));
	ImprintProcessTag(Void, ESRProcessTagContentV2::Overtone);
	ApplyFacility(Void, ESRFacilityContentPresetV2::NullSink, ESRFacilityTemperatureState::Normal, TEXT("Nadir"), NAME_None, &Evaluation);
	TestEqual(TEXT("Void sacrifice records the available two before adding Overtone"), Void.ProcessingMemory.StoredFamilyMagnitude, 2.0);
	TestEqual(TEXT("2 - 2 actual sacrifice + 5 Overtone = 5"), Void.CurrentEnergy, 5.0);
	TestEqual(TEXT("The visible Tag contribution is exactly additive"), Evaluation.ResourceProcessResult.ProcessTagEnergyDelta, 5.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourcePhase4TriggerMatrixTest,
	"StarRovers.ResourceSystem.Phase4.ProcessTag.TriggerMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourcePhase4TriggerMatrixTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::ResourceSystemPhase4Tests;

	FSRFacilityResourceV2Evaluation Evaluation;
	FSRResourceInstance Reclamation = MakeReferenceCard(ESRResourceContentPresetV2::HeliosIron, TEXT("Cinder"));
	Reclamation.CurrentEnergy = 20.0;
	Reclamation.EnergyValue = 20.0;
	Reclamation.ActiveFamilyStateFlags = StarRovers::Resources::GetFamilyStateBit(ESRResourceFamilyState::Fatigued);
	Reclamation.ProcessingMemory.LastProcessArchetype = TEXT("Press");
	Reclamation.ProcessingMemory.LastTemperature = ESRResourceProcessTemperatureState::Cold;
	Reclamation.ProcessingMemory.ConsecutiveSameArchetypeCount = 3;
	Reclamation.ProcessingMemory.GeneralProcessesSinceReset = 3;
	ImprintProcessTag(Reclamation, ESRProcessTagContentV2::Reclamation);
	ApplyFacility(Reclamation, ESRFacilityContentPresetV2::AnnealingChamber, ESRFacilityTemperatureState::Normal, TEXT("Cinder"), NAME_None, &Evaluation);
	TestTrue(TEXT("Explicit Anneal clears Fatigued and triggers Reclamation"), Evaluation.ResourceProcessResult.bProcessTagTriggered);
	TestEqual(TEXT("Reclamation adds seven during the zero-Energy recovery"), Reclamation.CurrentEnergy, 27.0);
	TestEqual(TEXT("Anneal resets Metal Work Strain"), Reclamation.ProcessingMemory.GeneralProcessesSinceReset, 0);

	FSRResourceInstance Crosslink = MakeReferenceCard(ESRResourceContentPresetV2::HeliosIron, TEXT("Cinder"));
	Crosslink.ProcessingMemory.LastProcessArchetype = TEXT("Pulse");
	Crosslink.ProcessingMemory.LastTemperature = ESRResourceProcessTemperatureState::Normal;
	Crosslink.ProcessingMemory.ConsecutiveSameArchetypeCount = 1;
	ImprintProcessTag(Crosslink, ESRProcessTagContentV2::Crosslink);
	ApplyFacility(Crosslink, ESRFacilityContentPresetV2::CompressionMill, ESRFacilityTemperatureState::Normal, TEXT("Cinder"), NAME_None, &Evaluation);
	TestTrue(TEXT("Archetype change triggers Crosslink"), Evaluation.ResourceProcessResult.bProcessTagTriggered);
	TestEqual(TEXT("Bridge Compression plus Crosslink remain additive"), Crosslink.CurrentEnergy, 12.0);

	FSRResourceInstance Landing = MakeReferenceCard(ESRResourceContentPresetV2::VerdantSpore, TEXT("Viridia"));
	StarRovers::Resources::RecordResourceTransit(Landing, TEXT("Viridia"), TEXT("Concord"));
	ImprintProcessTag(Landing, ESRProcessTagContentV2::LandingCharge);
	ApplyFacility(Landing, ESRFacilityContentPresetV2::GrowthVat, ESRFacilityTemperatureState::Normal, TEXT("Concord"), NAME_None, &Evaluation);
	TestFalse(TEXT("A zero-Energy Growth does not consume Landing Charge"), Evaluation.ResourceProcessResult.bProcessTagTriggered);
	TestEqual(TEXT("Landing Charge waits after a zero-Energy process"), Landing.ProcessTagSlot.Lifecycle, ESRResourceSlotLifecycle::Primed);
	ApplyFacility(Landing, ESRFacilityContentPresetV2::EnzymeLoom, ESRFacilityTemperatureState::Normal, TEXT("Concord"), NAME_None, &Evaluation);
	TestTrue(TEXT("The first later Energy-changing process consumes Landing Charge"), Evaluation.ResourceProcessResult.bProcessTagTriggered);
	TestEqual(TEXT("3 + 0 + 2 + 6 Matured + 5 Landing = 16"), Landing.CurrentEnergy, 16.0);
	TestEqual(TEXT("Landing records the import count at its Energy change"), Landing.ProcessingMemory.TransitCountAtLastEnergyChange, 1);

	FSRResourceInstance Pilgrim = MakeReferenceCard(ESRResourceContentPresetV2::HeliosIron, TEXT("Cinder"));
	ImprintProcessTag(Pilgrim, ESRProcessTagContentV2::PilgrimCharge);
	ApplyFacility(Pilgrim, ESRFacilityContentPresetV2::PulseProcessor, ESRFacilityTemperatureState::Normal, TEXT("Cinder"), NAME_None, &Evaluation);
	TestFalse(TEXT("A valid process on Origin does not consume Pilgrim Charge"), Evaluation.ResourceProcessResult.bProcessTagTriggered);
	ApplyFacility(Pilgrim, ESRFacilityContentPresetV2::CompressionMill, ESRFacilityTemperatureState::Normal, TEXT("Prism"), NAME_None, &Evaluation);
	TestTrue(TEXT("The first valid process outside Origin consumes Pilgrim Charge"), Evaluation.ResourceProcessResult.bProcessTagTriggered);
	TestEqual(TEXT("5 + 1 + 3 + 6 Pilgrim = 15"), Pilgrim.CurrentEnergy, 15.0);
	TestTrue(TEXT("Outside-Origin processing becomes durable metadata"), Pilgrim.LogisticsMetadata.bHasBeenProcessedOutsideOrigin);

	FSRResourceInstance InvalidTag = MakeReferenceCard(ESRResourceContentPresetV2::HeliosIron, TEXT("Cinder"));
	InvalidTag.ProcessTagSlot.TagId = TEXT("RemovedContentTag");
	InvalidTag.ProcessTagSlot.Lifecycle = ESRResourceSlotLifecycle::Primed;
	InvalidTag.ProcessTagSlot.RemainingTriggers = 1;
	const FSRResourceInstance InvalidSnapshot = InvalidTag;
	USRFacilityDataAsset* PulseData = MakeFacility(ESRFacilityContentPresetV2::PulseProcessor);
	Evaluation = FSRFacilityResourceV2Processor::Evaluate(
		MakeFacilityInstance(PulseData),
		InvalidTag,
		TEXT("Cinder"));
	TestEqual(TEXT("Unknown Primed content is rejected transactionally"),
		Evaluation.ResourceProcessResult.Outcome,
		ESRResourceProcessOutcome::InvalidProcessTag);
	TestTrue(TEXT("Rejected Tag processing returns an unchanged snapshot"),
		StarRovers::Resources::AreResourceV2RuntimeFieldsEquivalent(
			InvalidSnapshot,
			Evaluation.ResourceProcessResult.OutputResource));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourcePhase4SlotFacilityExecutorTest,
	"StarRovers.ResourceSystem.Phase4.FacilitySlots.NeutralExecutorPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourcePhase4SlotFacilityExecutorTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::ResourceSystemPhase4Tests;

	USRSimulationSettings* Settings = GetMutableDefault<USRSimulationSettings>();
	if (!TestNotNull(TEXT("Simulation settings exist"), Settings))
	{
		return false;
	}
	TGuardValue<ESRResourceRulesetVersion> RulesetGuard(
		Settings->ResourceRulesetVersion,
		ESRResourceRulesetVersion::ResourceV2);

	FSRResourceInstance Input = MakeReferenceCard(ESRResourceContentPresetV2::HeliosIron, TEXT("Cinder"));
	Input.ActiveFamilyStateFlags = StarRovers::Resources::GetFamilyStateBit(ESRResourceFamilyState::Tempered);
	Input.ProcessingMemory.LastProcessArchetype = TEXT("Forge");
	Input.ProcessingMemory.LastTemperature = ESRResourceProcessTemperatureState::Hot;
	Input.ProcessingMemory.ProcessCount = 4;
	Input.LogisticsMetadata.LastProcessedBodyId = TEXT("Cinder");
	const FSRResourceInstance Snapshot = Input;

	FSRFacilityInstance Imprinter = MakeFacilityInstance(
		MakeFacility(
			ESRFacilityContentPresetV2::TagImprinter,
			FSRResourceSystemContent::GetProcessTagId(ESRProcessTagContentV2::Overtone)),
		ESRFacilityTemperatureState::Normal,
		true);
	Imprinter.InputPortInventories[0].Inventory.Add(Input);
	Imprinter.InputInventory.Add(Input);
	FSRFacilityProcessingStartResult StartResult;
	FSRFacilityProcessingCompletionResult CompletionResult;
	TestTrue(TEXT("Tag Imprinter starts through the normal inventory executor"),
		FSRFacilityProcessingStepExecutor::TryStartProcessing(nullptr, Imprinter, &StartResult));
	TestTrue(TEXT("Tag Imprinter completes through the normal inventory executor"),
		FSRFacilityProcessingStepExecutor::TryCompleteProcessing(nullptr, Imprinter, &CompletionResult));
	TestTrue(TEXT("Tag Imprinter reports the Resource V2 route"), CompletionResult.bUsedResourceV2);
	TestTrue(TEXT("Executor Tag imprint changes only its slot"),
		AreEquivalentExceptProcessTag(Snapshot, CompletionResult.PrimaryOutputResource));
	TestEqual(TEXT("Imprinter does not increment hidden Process Count"),
		CompletionResult.PrimaryOutputResource.ProcessingMemory.ProcessCount,
		4);
	TestEqual(TEXT("Imprinter does not rewrite Last Processed Body"),
		CompletionResult.PrimaryOutputResource.LogisticsMetadata.LastProcessedBodyId,
		FName(TEXT("Cinder")));

	FSRResourceInstance Tagged = CompletionResult.PrimaryOutputResource;
	FSRFacilityResourceV2Evaluation OccupiedEvaluation;
	TestFalse(TEXT("A second Tag cannot overwrite an occupied slot"),
		ImprintProcessTag(Tagged, ESRProcessTagContentV2::Crosslink, &OccupiedEvaluation));
	TestEqual(TEXT("Occupied Tag slot has a specific failure outcome"),
		OccupiedEvaluation.Outcome,
		ESRFacilityResourceV2Outcome::ProcessTagSlotOccupied);

	const FSRResourceInstance BeforeScrub = CompletionResult.PrimaryOutputResource;
	FSRResourceInstance Scrubbed = BeforeScrub;
	TestTrue(TEXT("Tag Scrubber clears the slot"),
		ApplyFacility(Scrubbed, ESRFacilityContentPresetV2::TagScrubber));
	TestTrue(TEXT("Scrubber preserves everything except the Process Tag slot"),
		AreEquivalentExceptProcessTag(BeforeScrub, Scrubbed));
	TestEqual(TEXT("Scrubber produces an Empty slot"),
		Scrubbed.ProcessTagSlot.Lifecycle,
		ESRResourceSlotLifecycle::Empty);

	const FSRResourceInstance BeforeFuel = Scrubbed;
	TestTrue(TEXT("Fuel Imprinter applies Twin Seal"),
		ApplyFacility(
			Scrubbed,
			ESRFacilityContentPresetV2::FuelImprinter,
			ESRFacilityTemperatureState::Normal,
			NAME_None,
			FSRResourceSystemContent::GetFuelImprintId(ESRFuelImprintContentV2::TwinSeal)));
	TestTrue(TEXT("Fuel Imprinter changes only the Fuel Imprint slot"),
		AreEquivalentExceptFuelImprint(BeforeFuel, Scrubbed));
	TestEqual(TEXT("Twin Seal is durable on the output"),
		Scrubbed.FuelImprintSlot.ImprintId,
		FSRResourceSystemContent::GetFuelImprintId(ESRFuelImprintContentV2::TwinSeal));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourcePhase4ReferenceLinesTest,
	"StarRovers.ResourceSystem.Phase4.ReferenceLines.AllFamilies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourcePhase4ReferenceLinesTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::ResourceSystemPhase4Tests;

	FSRResourceInstance Metal = MakeReferenceCard(ESRResourceContentPresetV2::HeliosIron, TEXT("Cinder"));
	ImprintProcessTag(Metal, ESRProcessTagContentV2::Overtone);
	ApplyFacility(Metal, ESRFacilityContentPresetV2::InductionForge, ESRFacilityTemperatureState::Hot, TEXT("Cinder"));
	ApplyFacility(Metal, ESRFacilityContentPresetV2::CryoPress, ESRFacilityTemperatureState::Cold, TEXT("Cinder"));
	ApplyFacility(Metal, ESRFacilityContentPresetV2::AnnealingChamber, ESRFacilityTemperatureState::Normal, TEXT("Cinder"));
	ApplyFacility(Metal, ESRFacilityContentPresetV2::InductionForge, ESRFacilityTemperatureState::Hot, TEXT("Cinder"));
	ApplyFacility(Metal, ESRFacilityContentPresetV2::CryoPress, ESRFacilityTemperatureState::Cold, TEXT("Cinder"));
	TestEqual(TEXT("Metal reference Line finishes at Energy 34"), Metal.CurrentEnergy, 34.0);

	FSRResourceInstance Crystal = MakeReferenceCard(ESRResourceContentPresetV2::EchoQuartz, TEXT("Prism"));
	ImprintProcessTag(Crystal, ESRProcessTagContentV2::Overtone);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		ApplyFacility(Crystal, ESRFacilityContentPresetV2::ResonanceMill, ESRFacilityTemperatureState::Normal, TEXT("Prism"));
	}
	for (int32 Index = 0; Index < 3; ++Index)
	{
		ApplyFacility(Crystal, ESRFacilityContentPresetV2::FacetShifter, ESRFacilityTemperatureState::Normal, TEXT("Prism"));
	}
	TestEqual(TEXT("Crystal reference Line finishes at Energy 40"), Crystal.CurrentEnergy, 40.0);

	FSRResourceInstance Organic = MakeReferenceCard(ESRResourceContentPresetV2::VerdantSpore, TEXT("Viridia"));
	ImprintProcessTag(Organic, ESRProcessTagContentV2::Overtone);
	ApplyFacility(Organic, ESRFacilityContentPresetV2::GrowthVat, ESRFacilityTemperatureState::Normal, TEXT("Viridia"));
	ApplyFacility(Organic, ESRFacilityContentPresetV2::EnzymeLoom, ESRFacilityTemperatureState::Normal, TEXT("Viridia"));
	ApplyFacility(Organic, ESRFacilityContentPresetV2::GrowthVat, ESRFacilityTemperatureState::Normal, TEXT("Viridia"));
	ApplyFacility(Organic, ESRFacilityContentPresetV2::EnzymeLoom, ESRFacilityTemperatureState::Normal, TEXT("Viridia"));
	ApplyFacility(Organic, ESRFacilityContentPresetV2::GrowthVat, ESRFacilityTemperatureState::Normal, TEXT("Viridia"));
	ApplyFacility(Organic, ESRFacilityContentPresetV2::SporePress, ESRFacilityTemperatureState::Normal, TEXT("Viridia"));
	TestEqual(TEXT("Organic reference Line finishes at Energy 35"), Organic.CurrentEnergy, 35.0);

	FSRResourceInstance Plasma = MakeReferenceCard(ESRResourceContentPresetV2::AuroraPlasma, TEXT("Tempest"));
	ImprintProcessTag(Plasma, ESRProcessTagContentV2::Overtone);
	for (int32 CycleIndex = 0; CycleIndex < 2; ++CycleIndex)
	{
		ApplyFacility(Plasma, ESRFacilityContentPresetV2::ArcAmplifier, ESRFacilityTemperatureState::Normal, TEXT("Tempest"));
		ApplyFacility(Plasma, ESRFacilityContentPresetV2::ArcAmplifier, ESRFacilityTemperatureState::Normal, TEXT("Tempest"));
		ApplyFacility(Plasma, ESRFacilityContentPresetV2::GroundingCoil, ESRFacilityTemperatureState::Normal, TEXT("Tempest"));
	}
	TestEqual(TEXT("Plasma reference Line finishes at Energy 39"), Plasma.CurrentEnergy, 39.0);

	FSRResourceInstance Void = MakeReferenceCard(ESRResourceContentPresetV2::NullPearl, TEXT("Nadir"));
	ImprintProcessTag(Void, ESRProcessTagContentV2::Overtone);
	for (int32 CycleIndex = 0; CycleIndex < 3; ++CycleIndex)
	{
		ApplyFacility(Void, ESRFacilityContentPresetV2::NullSink, ESRFacilityTemperatureState::Normal, TEXT("Nadir"));
		ApplyFacility(Void, ESRFacilityContentPresetV2::EchoChamber, ESRFacilityTemperatureState::Normal, TEXT("Nadir"));
	}
	TestEqual(TEXT("Void reference Line finishes at Energy 30"), Void.CurrentEnergy, 30.0);

	TestEqual(TEXT("All five reference Process Tags are spent"),
		static_cast<int32>(Metal.ProcessTagSlot.Lifecycle == ESRResourceSlotLifecycle::Spent)
			+ static_cast<int32>(Crystal.ProcessTagSlot.Lifecycle == ESRResourceSlotLifecycle::Spent)
			+ static_cast<int32>(Organic.ProcessTagSlot.Lifecycle == ESRResourceSlotLifecycle::Spent)
			+ static_cast<int32>(Plasma.ProcessTagSlot.Lifecycle == ESRResourceSlotLifecycle::Spent)
			+ static_cast<int32>(Void.ProcessTagSlot.Lifecycle == ESRResourceSlotLifecycle::Spent),
		5);
	TestEqual(TEXT("Reference pre-Fabricator Energy sum is 178"),
		Metal.CurrentEnergy + Crystal.CurrentEnergy + Organic.CurrentEnergy + Plasma.CurrentEnergy + Void.CurrentEnergy,
		178.0);
	return true;
}

#endif
