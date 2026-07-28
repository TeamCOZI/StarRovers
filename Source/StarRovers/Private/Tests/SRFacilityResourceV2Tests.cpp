#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Automation/SRFacilityResourceV2Processor.h"
#include "Automation/SRResourceInstanceOperations.h"
#include "Misc/AutomationTest.h"
#include "Simulation/SRSimulationSettings.h"
#include "../Automation/SRFacilityOutputResourceBuilder.h"
#include "../Automation/SRFacilityProcessingStepExecutor.h"

namespace StarRovers::FacilityResourceV2Tests
{
	USRFacilityDataAsset* MakeProcessFacility(
		const TCHAR* Archetype,
		double EnergyDelta,
		ESRResourceFamily AcceptedFamily = ESRResourceFamily::Metal,
		int32 DefinitionVersion = StarRovers::Facilities::CurrentFacilityDefinitionVersion)
	{
		USRFacilityDataAsset* FacilityDataAsset = NewObject<USRFacilityDataAsset>(GetTransientPackage());
		FacilityDataAsset->FacilityDefinitionVersion = DefinitionVersion;
		FacilityDataAsset->FacilityKind = ESRFacilityKind::Standard;
		FacilityDataAsset->OperationKind = ESRFacilityOperationKind::Process;
		FacilityDataAsset->BaseProcessSeconds = 1.0f;
		FacilityDataAsset->InputInventory.SlotCount = 1;
		FacilityDataAsset->InputInventory.SlotCapacity = 8;
		FacilityDataAsset->OutputInventory.SlotCount = 1;
		FacilityDataAsset->OutputInventory.SlotCapacity = 8;
		FacilityDataAsset->ResourceV2Process.ProcessArchetype = FName(Archetype);
		FacilityDataAsset->ResourceV2Process.AcceptedFamily = AcceptedFamily;
		FacilityDataAsset->ResourceV2Process.FamilyAction = ESRResourceFamilyAction::None;
		FacilityDataAsset->ResourceV2Process.FacilityEnergyDelta = EnergyDelta;
		return FacilityDataAsset;
	}

	FSRFacilityInstance MakeFacilityInstance(
		USRFacilityDataAsset* FacilityDataAsset,
		ESRFacilityTemperatureState Temperature)
	{
		FSRFacilityInstance Facility;
		Facility.OccupantId = FName(*FString::Printf(TEXT("Test_%s"), *GetNameSafe(FacilityDataAsset)));
		Facility.FacilityDataAsset = FacilityDataAsset;
		Facility.TemperatureState = Temperature;
		Facility.bProcessEnabled = true;

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
		return Facility;
	}

	FSRResourceInstance MakeCard(
		ESRResourceFamily Family = ESRResourceFamily::Metal,
		double Energy = 5.0)
	{
		FSRResourceInstance Resource;
		Resource.ResourceSchemaVersion = StarRovers::Resources::CurrentResourceSchemaVersion;
		Resource.ResourceId = FName(TEXT("StarIron_Test"));
		Resource.ResourceClass = ESRResourceClass::Card;
		Resource.Family = Family;
		Resource.CurrentEnergy = Energy;
		Resource.EnergyValue = Energy;
		Resource.Spectrum = ESRResourceSpectrum::Red;
		Resource.Grade = 2;
		Resource.RemainingProcessLimit = 0;
		Resource.StackCount = 1;
		return Resource;
	}

	void AddInput(FSRFacilityInstance& Facility, const FSRResourceInstance& Resource)
	{
		Facility.InputPortInventories[0].Inventory.Add(Resource);
		Facility.InputInventory.Add(Resource);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRFacilityResourceV2DefinitionGateTest,
	"StarRovers.ResourceSystem.FacilityV2.DefinitionAndRulesetGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRFacilityResourceV2DefinitionGateTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::FacilityResourceV2Tests;

	USRSimulationSettings* Settings = GetMutableDefault<USRSimulationSettings>();
	TestNotNull(TEXT("Simulation settings exist"), Settings);
	if (!Settings)
	{
		return false;
	}
	TGuardValue<ESRResourceRulesetVersion> RulesetGuard(
		Settings->ResourceRulesetVersion,
		ESRResourceRulesetVersion::ResourceV2);

	USRFacilityDataAsset* ForgeDataAsset = MakeProcessFacility(TEXT("InductionForge"), 4.0);
	FSRFacilityInstance Forge = MakeFacilityInstance(ForgeDataAsset, ESRFacilityTemperatureState::Hot);
	const FSRResourceInstance Metal = MakeCard();
	TestTrue(
		TEXT("Resource V2 accepts a Card even when the Legacy process limit is zero"),
		FSRFacilityOutputResourceBuilder::DoesInputSetMatchOperation(
			ForgeDataAsset,
			TArray<FSRResourceInstance>({Metal}),
			Forge.TemperatureState));

	USRFacilityDataAsset* LegacyFacility = MakeProcessFacility(
		TEXT("LegacyForge"),
		4.0,
		ESRResourceFamily::Metal,
		StarRovers::Facilities::LegacyFacilityDefinitionVersion);
	TestFalse(
		TEXT("Resource V2 never falls back to an unmigrated standard Process facility"),
		FSRFacilityOutputResourceBuilder::DoesInputSetMatchOperation(
			LegacyFacility,
			TArray<FSRResourceInstance>({Metal}),
			ESRFacilityTemperatureState::Hot));

	const FSRResourceInstance Crystal = MakeCard(ESRResourceFamily::Crystal);
	TestFalse(
		TEXT("A Metal facility rejects a different Family"),
		FSRFacilityOutputResourceBuilder::DoesInputSetMatchOperation(
			ForgeDataAsset,
			TArray<FSRResourceInstance>({Crystal}),
			Forge.TemperatureState));

	Settings->ResourceRulesetVersion = ESRResourceRulesetVersion::Legacy;
	FSRResourceInstance LegacyEligibleMetal = Metal;
	LegacyEligibleMetal.RemainingProcessLimit = 1;
	TArray<FSRResourceInstance> LegacyOutputs;
	FSRFacilityOutputResourceBuilder::BuildOutputResources(
		Forge,
		TArray<FSRResourceInstance>({LegacyEligibleMetal}),
		LegacyOutputs);
	TestEqual(TEXT("Legacy route still emits one primary output"), LegacyOutputs.Num(), 1);
	if (LegacyOutputs.Num() == 1)
	{
		TestTrue(
			TEXT("Legacy route ignores the V2 additive facility contract"),
			FMath::IsNearlyEqual(LegacyOutputs[0].EnergyValue, 5.0));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRFacilityResourceV2MetalLineExecutorTest,
	"StarRovers.ResourceSystem.FacilityV2.Metal.HotForgeToColdPress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRFacilityResourceV2MetalLineExecutorTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::FacilityResourceV2Tests;

	USRSimulationSettings* Settings = GetMutableDefault<USRSimulationSettings>();
	if (!TestNotNull(TEXT("Simulation settings exist"), Settings))
	{
		return false;
	}
	TGuardValue<ESRResourceRulesetVersion> RulesetGuard(
		Settings->ResourceRulesetVersion,
		ESRResourceRulesetVersion::ResourceV2);

	FSRFacilityInstance Forge = MakeFacilityInstance(
		MakeProcessFacility(TEXT("InductionForge"), 4.0),
		ESRFacilityTemperatureState::Hot);
	AddInput(Forge, MakeCard());
	FSRFacilityProcessingStartResult ForgeStart;
	TestTrue(
		TEXT("Induction Forge starts with a zero-limit V2 Metal Card"),
		FSRFacilityProcessingStepExecutor::TryStartProcessing(nullptr, Forge, &ForgeStart));
	FSRFacilityProcessingCompletionResult ForgeCompletion;
	TestTrue(
		TEXT("Induction Forge completes through the normal facility inventory executor"),
		FSRFacilityProcessingStepExecutor::TryCompleteProcessing(nullptr, Forge, &ForgeCompletion));
	TestTrue(TEXT("Forge completion reports the V2 path"), ForgeCompletion.bUsedResourceV2);
	TestTrue(
		TEXT("Forge applies only its +4 additive delta"),
		FMath::IsNearlyEqual(ForgeCompletion.PrimaryOutputResource.CurrentEnergy, 9.0));
	TestEqual(
		TEXT("V2 processing preserves the ignored Legacy process limit"),
		ForgeCompletion.PrimaryOutputResource.RemainingProcessLimit,
		0);

	FSRFacilityInstance Press = MakeFacilityInstance(
		MakeProcessFacility(TEXT("CryoPress"), 3.0),
		ESRFacilityTemperatureState::Cold);
	AddInput(Press, ForgeCompletion.PrimaryOutputResource);
	FSRFacilityProcessingStartResult PressStart;
	TestTrue(
		TEXT("Cryo Press starts with Forge output"),
		FSRFacilityProcessingStepExecutor::TryStartProcessing(nullptr, Press, &PressStart));
	FSRFacilityProcessingCompletionResult PressCompletion;
	TestTrue(
		TEXT("Cryo Press completes through the normal facility inventory executor"),
		FSRFacilityProcessingStepExecutor::TryCompleteProcessing(nullptr, Press, &PressCompletion));
	TestTrue(TEXT("Press completion reports the V2 path"), PressCompletion.bUsedResourceV2);
	TestTrue(
		TEXT("Hot to Cold yields 5 + 4 + 3 + 5 Tempered = 17"),
		FMath::IsNearlyEqual(PressCompletion.PrimaryOutputResource.CurrentEnergy, 17.0));
	TestTrue(
		TEXT("Cryo Press output carries Tempered"),
		(PressCompletion.PrimaryOutputResource.ActiveFamilyStateFlags
			& StarRovers::Resources::GetFamilyStateBit(ESRResourceFamilyState::Tempered)) != 0);
	TestEqual(
		TEXT("Two successful facilities increment hidden process history twice"),
		PressCompletion.PrimaryOutputResource.ProcessingMemory.ProcessCount,
		2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRFacilityResourceV2PreviewTest,
	"StarRovers.ResourceSystem.FacilityV2.Preview.IsPureAndExplainsState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRFacilityResourceV2PreviewTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::FacilityResourceV2Tests;

	USRSimulationSettings* Settings = GetMutableDefault<USRSimulationSettings>();
	if (!TestNotNull(TEXT("Simulation settings exist"), Settings))
	{
		return false;
	}
	TGuardValue<ESRResourceRulesetVersion> RulesetGuard(
		Settings->ResourceRulesetVersion,
		ESRResourceRulesetVersion::ResourceV2);

	FSRFacilityInstance Press = MakeFacilityInstance(
		MakeProcessFacility(TEXT("CryoPress"), 3.0),
		ESRFacilityTemperatureState::Cold);
	FSRResourceInstance Input = MakeCard(ESRResourceFamily::Metal, 9.0);
	Input.ProcessingMemory.LastProcessArchetype = FName(TEXT("InductionForge"));
	Input.ProcessingMemory.LastTemperature = ESRResourceProcessTemperatureState::Hot;
	Input.ProcessingMemory.ConsecutiveSameArchetypeCount = 1;
	const FSRResourceInstance Snapshot = Input;

	TArray<FSRResourceInstance> Outputs;
	TArray<FString> FormulaTexts;
	FSRResourceProcessResult ProcessResult;
	FSRFacilityOutputResourceBuilder::BuildOutputResources(
		Press,
		TArray<FSRResourceInstance>({Input}),
		Outputs,
		nullptr,
		nullptr,
		&FormulaTexts,
		&ProcessResult);
	TestEqual(TEXT("Preview produces one output"), Outputs.Num(), 1);
	TestTrue(TEXT("Preview returns the detailed V2 process result"), ProcessResult.IsSuccess());
	TestEqual(TEXT("Preview produces one explanation"), FormulaTexts.Num(), 1);
	if (FormulaTexts.Num() == 1)
	{
		TestTrue(TEXT("Explanation declares additive processing"), FormulaTexts[0].Contains(TEXT("Resource V2 Additive Process")));
		TestTrue(TEXT("Explanation identifies Tempered"), FormulaTexts[0].Contains(TEXT("Tempered")));
		TestTrue(TEXT("Explanation does not contain a multiplier stage"), !FormulaTexts[0].Contains(TEXT("Multiplier")));
	}
	TestTrue(
		TEXT("Preview never mutates its input"),
		StarRovers::Resources::AreResourceV2RuntimeFieldsEquivalent(Input, Snapshot));
	return true;
}

#endif
