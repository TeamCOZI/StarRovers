#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Automation/SROperationalCapacity.h"
#include "Automation/SROperationalEconomyProcessor.h"
#include "Automation/SRResourceSystemContent.h"
#include "Misc/AutomationTest.h"
#include "Simulation/SRSimulationSettings.h"
#include "../Automation/SRFacilityProcessingStepExecutor.h"
#include "../Automation/SRFacilityProcessingTickRunner.h"
#include "../Automation/SRFacilityOutputResourceBuilder.h"

namespace StarRovers::OperationalCapacityV2Tests
{
	FSRResourceInstance MakeUtility(ESRResourceContentPresetV2 Preset, int32 StackCount = 1)
	{
		FSRResourceInstance Resource;
		FSRResourceSystemContent::MakeReferenceResourceInstance(Preset, NAME_None, Resource);
		Resource.ResourceInstanceId = FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		Resource.StackCount = FMath::Max(1, StackCount);
		return Resource;
	}

	USRFacilityDataAsset* MakePresetFacility(ESRFacilityContentPresetV2 Preset)
	{
		USRFacilityDataAsset* Facility = NewObject<USRFacilityDataAsset>(GetTransientPackage());
		FSRResourceSystemContent::ApplyFacilityPreset(*Facility, Preset);
		return Facility;
	}

	FSRFacilityInstance MakeRuntimeFacility(USRFacilityDataAsset* Definition, const TCHAR* OccupantId)
	{
		FSRFacilityInstance Facility;
		Facility.OccupantId = FName(OccupantId);
		Facility.FacilityDataAsset = Definition;
		Facility.bProcessEnabled = true;
		Facility.OperationalPriority = IsValid(Definition)
			? Definition->DefaultOperationalPriority
			: ESROperationalPriorityV2::Normal;
		if (IsValid(Definition))
		{
			for (int32 InputIndex = 0; InputIndex < Definition->InputInventory.SlotCount; ++InputIndex)
			{
				FSRFacilityPortInventory& Port = Facility.InputPortInventories.AddDefaulted_GetRef();
				Port.PortId = FName(*FString::Printf(TEXT("Input_%d"), InputIndex));
				Port.PortKind = ESRFacilityPortKind::Input;
				Port.PortIndex = InputIndex;
				Port.Capacity = Definition->InputInventory.SlotCapacity;
			}
			for (int32 OutputIndex = 0; OutputIndex < Definition->OutputInventory.SlotCount; ++OutputIndex)
			{
				FSRFacilityPortInventory& Port = Facility.OutputPortInventories.AddDefaulted_GetRef();
				Port.PortId = FName(*FString::Printf(TEXT("Output_%d"), OutputIndex));
				Port.PortKind = ESRFacilityPortKind::Output;
				Port.PortIndex = OutputIndex;
				Port.Capacity = Definition->OutputInventory.SlotCapacity;
			}
		}
		return Facility;
	}

	FSRFacilityInstance MakeActiveLoadFacility(
		const TCHAR* OccupantId,
		int32 OperationalLoad,
		ESROperationalPriorityV2 Priority,
		bool bProcessing = true)
	{
		USRFacilityDataAsset* Definition = NewObject<USRFacilityDataAsset>(GetTransientPackage());
		Definition->FacilityDefinitionVersion = StarRovers::Facilities::CurrentFacilityDefinitionVersion;
		Definition->FacilityKind = ESRFacilityKind::Standard;
		Definition->OperationKind = ESRFacilityOperationKind::Process;
		Definition->BaseProcessSeconds = 100.0f;
		Definition->OperationalLoad = OperationalLoad;
		Definition->ResourceV2Process.ProcessArchetype = FName(TEXT("CapacityTest"));
		FSRFacilityInstance Facility = MakeRuntimeFacility(Definition, OccupantId);
		Facility.OperationalPriority = Priority;
		Facility.bProcessing = bProcessing;
		if (bProcessing)
		{
			FSRResourceInstance Card;
			Card.ResourceId = FName(TEXT("CapacityTestCard"));
			Card.ResourceClass = ESRResourceClass::Card;
			Card.Family = ESRResourceFamily::Metal;
			Card.Spectrum = ESRResourceSpectrum::Red;
			Card.Grade = 1;
			Card.StackCount = 1;
			Facility.ProcessingInventory.Add(Card);
		}
		return Facility;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSROperationalEconomyContentAndRecipeTest,
	"StarRovers.ResourceSystem.Phase7.OperationalEconomy.ContentAndRecipe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSROperationalEconomyContentAndRecipeTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::OperationalCapacityV2Tests;
	USRSimulationSettings* Settings = GetMutableDefault<USRSimulationSettings>();
	TGuardValue<ESRResourceRulesetVersion> RulesetGuard(
		Settings->ResourceRulesetVersion,
		ESRResourceRulesetVersion::ResourceV2);

	TArray<FSRUtilityResourceDefinitionV2> Utilities;
	TArray<FSRFacilityContentDefinitionV2> Facilities;
	FSRResourceSystemContent::GetAllUtilityResourceDefinitions(Utilities);
	FSRResourceSystemContent::GetAllFacilityDefinitions(Facilities);
	TestEqual(TEXT("Three Utility resources are registered outside the Card catalog"), Utilities.Num(), 3);
	TestEqual(TEXT("Twenty-one Resource V2 facilities include three infrastructure facilities"), Facilities.Num(), 21);

	USRResourceDataAsset* SupplyResource = NewObject<USRResourceDataAsset>(GetTransientPackage());
	TestTrue(TEXT("Industrial Supply preset applies"),
		FSRResourceSystemContent::ApplyResourcePreset(*SupplyResource, ESRResourceContentPresetV2::IndustrialSupply));
	TestEqual(TEXT("Industrial Supply is a Utility"), SupplyResource->ResourceClass, ESRResourceClass::Utility);
	TestEqual(TEXT("Utility resources have no Family"), SupplyResource->Family, ESRResourceFamily::None);

	USRFacilityDataAsset* SupplyDefinition = MakePresetFacility(ESRFacilityContentPresetV2::SupplyFabricator);
	FSRFacilityInstance SupplyFacility = MakeRuntimeFacility(SupplyDefinition, TEXT("SupplyFabricator_Test"));
	const TArray<FSRResourceInstance> CorrectInputs = {
		MakeUtility(ESRResourceContentPresetV2::CommonOre),
		MakeUtility(ESRResourceContentPresetV2::BiomassFeedstock),
	};
	const FSROperationalEconomyEvaluationV2 Evaluation =
		FSROperationalEconomyProcessor::Evaluate(SupplyFacility, CorrectInputs);
	TestTrue(TEXT("Supply recipe accepts one input of each feedstock"), Evaluation.IsSuccess());
	TestEqual(TEXT("Supply recipe creates one output stack"), Evaluation.OutputResources.Num(), 1);
	if (Evaluation.OutputResources.Num() == 1)
	{
		TestEqual(TEXT("Output is Industrial Supply"),
			Evaluation.OutputResources[0].ResourceId,
			FSRResourceSystemContent::GetUtilityResourceId(ESRResourceContentPresetV2::IndustrialSupply));
		TestEqual(TEXT("One cycle produces two Industrial Supply"), Evaluation.OutputResources[0].StackCount, 2);
		TestEqual(TEXT("Output remains Utility rather than Card"), Evaluation.OutputResources[0].ResourceClass, ESRResourceClass::Utility);
	}

	const TArray<FSRResourceInstance> DuplicateOre = {
		MakeUtility(ESRResourceContentPresetV2::CommonOre),
		MakeUtility(ESRResourceContentPresetV2::CommonOre),
	};
	TestFalse(TEXT("Duplicate feedstocks cannot substitute for Biomass"),
		FSROperationalEconomyProcessor::Evaluate(SupplyFacility, DuplicateOre).IsSuccess());
	TestEqual(TEXT("Supply Fabricator defaults to Critical recovery priority"),
		SupplyDefinition->DefaultOperationalPriority,
		ESROperationalPriorityV2::Critical);
	TestEqual(TEXT("Supply Fabricator has Operational Load 4"), SupplyDefinition->OperationalLoad, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSROperationalCapacityPriorityAllocationTest,
	"StarRovers.ResourceSystem.Phase7.OperationalCapacity.PriorityAllocation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSROperationalCapacityPriorityAllocationTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::OperationalCapacityV2Tests;
	USRSimulationSettings* Settings = GetMutableDefault<USRSimulationSettings>();
	TGuardValue<ESRResourceRulesetVersion> RulesetGuard(
		Settings->ResourceRulesetVersion,
		ESRResourceRulesetVersion::ResourceV2);

	FSRFacilityNetworkRuntimeState State;
	State.FacilityInstancesByOccupantId.Add(
		FName(TEXT("Critical")),
		MakeActiveLoadFacility(TEXT("Critical"), 20, ESROperationalPriorityV2::Critical));
	State.FacilityInstancesByOccupantId.Add(
		FName(TEXT("Normal")),
		MakeActiveLoadFacility(TEXT("Normal"), 30, ESROperationalPriorityV2::Normal));
	State.FacilityInstancesByOccupantId.Add(
		FName(TEXT("Background")),
		MakeActiveLoadFacility(TEXT("Background"), 20, ESROperationalPriorityV2::Background));
	State.FacilityInstancesByOccupantId.Add(
		FName(TEXT("Idle")),
		MakeActiveLoadFacility(TEXT("Idle"), 100, ESROperationalPriorityV2::Critical, false));

	const FSROperationalCapacityReportV2 BaseReport =
		FSROperationalCapacity::BuildReport(State, true, 45, 18);
	TestEqual(TEXT("Idle facilities reserve no capacity"), BaseReport.TotalDemand, 70);
	TestTrue(TEXT("Critical tier receives capacity first"), FMath::IsNearlyEqual(BaseReport.Critical.SpeedFactor, 1.0f));
	TestTrue(TEXT("Normal tier shares its remaining capacity proportionally"), FMath::IsNearlyEqual(BaseReport.Normal.SpeedFactor, 25.0f / 30.0f));
	TestTrue(TEXT("Background tier pauses when higher tiers consume all capacity"), FMath::IsNearlyZero(BaseReport.Background.SpeedFactor));

	USRFacilityDataAsset* CoreDefinition = MakePresetFacility(ESRFacilityContentPresetV2::ServiceCore);
	FSRFacilityInstance Core = MakeRuntimeFacility(CoreDefinition, TEXT("Core"));
	Core.InputPortInventories[0].Inventory.Add(MakeUtility(ESRResourceContentPresetV2::IndustrialSupply));
	State.FacilityInstancesByOccupantId.Add(Core.OccupantId, Core);
	const FSROperationalCapacityReportV2 CoreReport =
		FSROperationalCapacity::BuildReport(State, true, 45, 18);
	TestEqual(TEXT("A supplied Service Core is counted once"), CoreReport.ActiveServiceCoreCount, 1);
	TestEqual(TEXT("The Service Core adds eighteen capacity"), CoreReport.TotalCapacity, 63);
	TestTrue(TEXT("Normal tier reaches full speed after recovery"), FMath::IsNearlyEqual(CoreReport.Normal.SpeedFactor, 1.0f));
	TestTrue(TEXT("Background receives the final thirteen capacity"), FMath::IsNearlyEqual(CoreReport.Background.SpeedFactor, 13.0f / 20.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSROperationalCapacityTickThrottleTest,
	"StarRovers.ResourceSystem.Phase7.OperationalCapacity.TickThrottle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSROperationalCapacityTickThrottleTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::OperationalCapacityV2Tests;
	FSRFacilityNetworkRuntimeState State;
	FSRFacilityInstance Facility = MakeActiveLoadFacility(
		TEXT("Throttle"),
		4,
		ESROperationalPriorityV2::Normal);
	Facility.FacilityDataAsset->BaseProcessSeconds = 4.0f;
	State.FacilityInstancesByOccupantId.Add(Facility.OccupantId, Facility);

	int32 CompletionCount = 0;
	const int32 Processed = FSRFacilityProcessingTickRunner::ProcessFacilities(
		State,
		2.0f,
		64,
		[](FSRFacilityInstance&) { return false; },
		[]() {},
		[](const FSRFacilityInstance&) { return 0.5f; },
		[&CompletionCount](FSRFacilityInstance&) { ++CompletionCount; return true; });
	const FSRFacilityInstance& Updated = State.FacilityInstancesByOccupantId.FindChecked(FName(TEXT("Throttle")));
	TestEqual(TEXT("The active facility is still processed"), Processed, 1);
	TestTrue(TEXT("Two seconds at 50 percent speed advances one second"), FMath::IsNearlyEqual(Updated.ProcessProgressSeconds, 1.0f));
	TestTrue(TEXT("Throttling never discards or cancels in-flight work"), Updated.bProcessing);
	TestEqual(TEXT("Incomplete work does not complete early"), CompletionCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRFacilitySchedulerDeterministicRoundRobinTest,
	"StarRovers.ResourceSystem.Phase11.FacilityScheduler.DeterministicRoundRobin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRFacilitySchedulerDeterministicRoundRobinTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::OperationalCapacityV2Tests;
	FSRFacilityNetworkRuntimeState State;
	for (int32 FacilityIndex = 64; FacilityIndex >= 0; --FacilityIndex)
	{
		const FString OccupantIdString = FString::Printf(TEXT("Facility_%03d"), FacilityIndex);
		FSRFacilityInstance Facility = MakeActiveLoadFacility(
			*OccupantIdString,
			0,
			ESROperationalPriorityV2::Normal,
			false);
		State.FacilityInstancesByOccupantId.Add(Facility.OccupantId, MoveTemp(Facility));
	}

	TArray<FName> StartAttemptOrder;
	auto RunSchedulerTick = [&State, &StartAttemptOrder]()
	{
		return FSRFacilityProcessingTickRunner::ProcessFacilities(
			State,
			1.0f,
			64,
			[&StartAttemptOrder](FSRFacilityInstance& Facility)
			{
				StartAttemptOrder.Add(Facility.OccupantId);
				return false;
			},
			[]() {},
			[](const FSRFacilityInstance&) { return 1.0f; },
			[](FSRFacilityInstance&) { return false; });
	};

	TestEqual(TEXT("The first transition slice contains exactly the configured budget"),
		RunSchedulerTick(), 0);
	TestEqual(TEXT("Sixty-four idle facilities receive a start attempt on the first tick"),
		StartAttemptOrder.Num(), 64);
	if (!StartAttemptOrder.IsEmpty())
	{
		TestEqual(TEXT("Insertion order cannot change the deterministic first candidate"),
			StartAttemptOrder[0], FName(TEXT("Facility_000")));
	}

	RunSchedulerTick();
	TSet<FName> AttemptedOccupantIds;
	for (const FName AttemptedOccupantId : StartAttemptOrder)
	{
		AttemptedOccupantIds.Add(AttemptedOccupantId);
	}
	TestEqual(TEXT("The sixty-fifth facility receives a transition opportunity on the next tick"),
		AttemptedOccupantIds.Num(), 65);
	TestTrue(TEXT("The facility beyond the old fixed prefix cannot starve"),
		AttemptedOccupantIds.Contains(FName(TEXT("Facility_064"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRFacilitySchedulerActiveClockFairnessTest,
	"StarRovers.ResourceSystem.Phase11.FacilityScheduler.ActiveClockFairness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRFacilitySchedulerActiveClockFairnessTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::OperationalCapacityV2Tests;
	FSRFacilityNetworkRuntimeState State;
	for (int32 FacilityIndex = 0; FacilityIndex < 128; ++FacilityIndex)
	{
		const FString OccupantIdString = FString::Printf(TEXT("Clock_%03d"), FacilityIndex);
		FSRFacilityInstance Facility = MakeActiveLoadFacility(
			*OccupantIdString,
			0,
			ESROperationalPriorityV2::Normal);
		Facility.FacilityDataAsset->BaseProcessSeconds = 10.0f;
		State.FacilityInstancesByOccupantId.Add(Facility.OccupantId, MoveTemp(Facility));
	}

	TSet<FName> CompletedOccupantIds;
	auto RunSchedulerTick = [&State, &CompletedOccupantIds](float DeltaTime)
	{
		return FSRFacilityProcessingTickRunner::ProcessFacilities(
			State,
			DeltaTime,
			64,
			[](FSRFacilityInstance&) { return false; },
			[]() {},
			[](const FSRFacilityInstance&) { return 1.0f; },
			[&CompletedOccupantIds](FSRFacilityInstance& Facility)
			{
				CompletedOccupantIds.Add(Facility.OccupantId);
				Facility.ProcessProgressSeconds = 0.0f;
				Facility.bProcessing = false;
				return true;
			});
	};

	TestEqual(TEXT("All 128 active clocks advance despite the 64-transition budget"),
		RunSchedulerTick(1.0f), 128);
	for (const TPair<FName, FSRFacilityInstance>& FacilityPair : State.FacilityInstancesByOccupantId)
	{
		TestTrue(TEXT("Every active facility receives the full elapsed second"),
			FMath::IsNearlyEqual(FacilityPair.Value.ProcessProgressSeconds, 1.0f));
	}

	for (TPair<FName, FSRFacilityInstance>& FacilityPair : State.FacilityInstancesByOccupantId)
	{
		FacilityPair.Value.ProcessProgressSeconds = 9.0f;
	}
	RunSchedulerTick(1.0f);
	TestEqual(TEXT("Only one bounded transition slice completes on a tick"),
		CompletedOccupantIds.Num(), 64);
	RunSchedulerTick(1.0f);
	TestEqual(TEXT("The next slice completes every remaining due facility"),
		CompletedOccupantIds.Num(), 128);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRServiceCoreSupplyLifecycleTest,
	"StarRovers.ResourceSystem.Phase7.OperationalEconomy.ServiceCoreSupplyLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRServiceCoreSupplyLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::OperationalCapacityV2Tests;
	USRSimulationSettings* Settings = GetMutableDefault<USRSimulationSettings>();
	TGuardValue<ESRResourceRulesetVersion> RulesetGuard(
		Settings->ResourceRulesetVersion,
		ESRResourceRulesetVersion::ResourceV2);

	USRFacilityDataAsset* CoreDefinition = MakePresetFacility(ESRFacilityContentPresetV2::ServiceCore);
	FSRFacilityInstance Core = MakeRuntimeFacility(CoreDefinition, TEXT("LifecycleCore"));
	Core.InputPortInventories[0].Inventory.Add(
		MakeUtility(ESRResourceContentPresetV2::IndustrialSupply));
	TestTrue(TEXT("Buffered Industrial Supply activates the Service Core"),
		FSROperationalEconomyProcessor::IsServiceCoreSupplied(Core));
	TArray<FSRResourceInstance> PreviewOutputs;
	TArray<FString> PreviewTexts;
	FSRFacilityOutputResourceBuilder::BuildOutputResources(
		Core,
		TArray<FSRResourceInstance>({Core.InputPortInventories[0].Inventory[0]}),
		PreviewOutputs,
		nullptr,
		nullptr,
		&PreviewTexts);
	TestTrue(TEXT("Service Core exposes its consumption rule without a fake output"),
		PreviewOutputs.IsEmpty()
			&& PreviewTexts.Num() == 1
			&& PreviewTexts[0].Contains(TEXT("Service Core V2")));

	FSRFacilityProcessingStartResult StartResult;
	TestTrue(TEXT("Service Core starts a thirty-second consumption cycle"),
		FSRFacilityProcessingStepExecutor::TryStartProcessing(nullptr, Core, &StartResult));
	TestTrue(TEXT("Reserved in-flight Supply keeps capacity active"),
		FSROperationalEconomyProcessor::IsServiceCoreSupplied(Core));
	TestEqual(TEXT("Exactly one Supply is reserved"), Core.ProcessingInventory.Num(), 1);

	Core.ProcessProgressSeconds = CoreDefinition->BaseProcessSeconds;
	FSRFacilityProcessingCompletionResult CompletionResult;
	TestTrue(TEXT("Service Core consumes Supply without requiring an output slot"),
		FSRFacilityProcessingStepExecutor::TryCompleteProcessing(nullptr, Core, &CompletionResult));
	TestTrue(TEXT("Completion records the Operational Economy route"), CompletionResult.bUsedOperationalEconomyV2);
	TestEqual(TEXT("Service Core emits no fake output resource"), CompletionResult.OutputCount, 0);
	TestFalse(TEXT("Capacity turns off after the final buffered Supply is consumed"),
		FSROperationalEconomyProcessor::IsServiceCoreSupplied(Core));
	return true;
}

#endif
