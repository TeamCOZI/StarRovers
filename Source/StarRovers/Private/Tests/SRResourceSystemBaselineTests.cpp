#include "Automation/SRResourceDataAsset.h"
#include "Logistics/SRSpaceLogisticsTypes.h"
#include "Misc/AutomationTest.h"
#include "Simulation/SRSimulationSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourceRulesetDefaultsToResourceV2Test,
	"StarRovers.ResourceSystem.Baseline.RulesetDefaultsToResourceV2",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourceRulesetDefaultsToResourceV2Test::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const USRSimulationSettings* Settings = GetDefault<USRSimulationSettings>();
	TestNotNull(TEXT("Simulation settings are available"), Settings);
	if (Settings)
	{
		TestTrue(
			TEXT("The verified project baseline activates Resource V2 by default"),
			Settings->ResourceRulesetVersion == ESRResourceRulesetVersion::ResourceV2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourceDefaultInstanceBaselineTest,
	"StarRovers.ResourceSystem.Baseline.DefaultResourceInstance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourceDefaultInstanceBaselineTest::RunTest(const FString& Parameters)
{
	USRResourceDataAsset* ResourceDataAsset = NewObject<USRResourceDataAsset>(GetTransientPackage());
	TestNotNull(TEXT("Transient Resource Data Asset was created"), ResourceDataAsset);
	if (!ResourceDataAsset)
	{
		return false;
	}

	ResourceDataAsset->ResourceId = FName(TEXT("BaselineResource"));
	ResourceDataAsset->BaseEnergyValue = 17.5;
	ResourceDataAsset->BaseProcessLimit = 7;
	FSRResourceTagStack DefaultTag;
	DefaultTag.Tag = ESRResourceProcessTag::Responsive;
	DefaultTag.StackCount = 2;
	DefaultTag.RemainingCycles = 3;
	ResourceDataAsset->DefaultTags.Add(DefaultTag);

	const FSRResourceInstance ResourceInstance = ResourceDataAsset->BuildDefaultInstance();
	TestEqual(TEXT("Resource Id is copied"), ResourceInstance.ResourceId, ResourceDataAsset->ResourceId);
	TestEqual(TEXT("Resource Data Asset is retained"), ResourceInstance.ResourceDataAsset.Get(), ResourceDataAsset);
	TestEqual(TEXT("Base Energy becomes runtime Energy"), ResourceInstance.EnergyValue, 17.5);
	TestEqual(TEXT("Legacy definitions bridge Energy into Current Energy"), ResourceInstance.CurrentEnergy, 17.5);
	TestEqual(
		TEXT("Every newly built instance uses the current resource schema"),
		ResourceInstance.ResourceSchemaVersion,
		StarRovers::Resources::CurrentResourceSchemaVersion);
	TestEqual(TEXT("Legacy definitions remain unclassified"), ResourceInstance.ResourceClass, ESRResourceClass::Unknown);
	TestEqual(TEXT("Base Process Limit becomes runtime Remaining Process Limit"), ResourceInstance.RemainingProcessLimit, 7);
	TestEqual(TEXT("A default instance starts at process count zero"), ResourceInstance.ProcessCount, 0);
	TestEqual(TEXT("A default instance starts at energy-change count zero"), ResourceInstance.EnergyChangeCount, 0);
	TestEqual(TEXT("A default instance represents one stack item"), ResourceInstance.StackCount, 1);
	TestEqual(TEXT("Default tags are copied"), ResourceInstance.Tags.Num(), 1);
	if (ResourceInstance.Tags.Num() == 1)
	{
		TestEqual(TEXT("Default tag stack count is preserved"), ResourceInstance.Tags[0].StackCount, 2);
		TestEqual(TEXT("Default tag cycle count is preserved"), ResourceInstance.Tags[0].RemainingCycles, 3);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourceLogisticsSchemaBaselineTest,
	"StarRovers.ResourceSystem.Baseline.LogisticsSchemaVersion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourceLogisticsSchemaBaselineTest::RunTest(const FString& Parameters)
{
	const FSRSpaceLogisticsSaveData SaveData;
	TestEqual(
		TEXT("New Logistics saves use the Resource V2-aware schema"),
		SaveData.Version,
		FSRSpaceLogisticsSaveData::CurrentVersion);
	TestTrue(TEXT("The current Logistics schema is supported"), SaveData.IsSupportedVersion());
	return true;
}

#endif
