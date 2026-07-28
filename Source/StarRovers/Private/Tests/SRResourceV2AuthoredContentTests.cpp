#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Automation/SRResourceInstanceOperations.h"
#include "Automation/SRResourceV2AuthoredContent.h"
#include "Misc/AutomationTest.h"
#include "Simulation/SRSimulationSettings.h"
#include "Structure/SRStructureDataAsset.h"
#include "../Automation/SRFacilityPortInventoryBuilder.h"
#include "../Automation/SRFacilityProcessingStepExecutor.h"

namespace StarRovers::ResourceV2AuthoredContentTests
{
	USRFacilityDataAsset* LoadFacility(ESRFacilityContentPresetV2 Preset)
	{
		return LoadObject<USRFacilityDataAsset>(
			nullptr,
			*FSRResourceV2AuthoredContent::GetFacilityObjectPath(Preset));
	}

	USRStructureDataAsset* LoadFacilityStructure(ESRFacilityContentPresetV2 Preset)
	{
		return LoadObject<USRStructureDataAsset>(
			nullptr,
			*FSRResourceV2AuthoredContent::GetFacilityStructureObjectPath(Preset));
	}

	USRResourceDataAsset* LoadResource(ESRResourceContentPresetV2 Preset)
	{
		return LoadObject<USRResourceDataAsset>(
			nullptr,
			*FSRResourceV2AuthoredContent::GetResourceObjectPath(Preset));
	}

	FSRFacilityInstance MakeFacilityInstance(
		ESRFacilityContentPresetV2 Preset,
		ESRFacilityTemperatureState Temperature)
	{
		FSRFacilityInstance Facility;
		Facility.OccupantId = FName(*FString::Printf(TEXT("Authored_%d"), static_cast<int32>(Preset)));
		Facility.FacilityDataAsset = LoadFacility(Preset);
		Facility.StructureDataAsset = LoadFacilityStructure(Preset);
		Facility.TemperatureState = Temperature;
		Facility.bProcessEnabled = true;
		FSRFacilityPortInventoryBuilder::Initialize(Facility);
		return Facility;
	}

	bool RunSingleInputFacility(
		FSRFacilityInstance& Facility,
		const FSRResourceInstance& Input,
		FSRFacilityProcessingCompletionResult& OutCompletion)
	{
		if (Facility.InputPortInventories.IsEmpty())
		{
			return false;
		}
		Facility.InputPortInventories[0].Inventory.Add(Input);
		return FSRFacilityProcessingStepExecutor::TryStartProcessing(nullptr, Facility)
			&& FSRFacilityProcessingStepExecutor::TryCompleteProcessing(nullptr, Facility, &OutCompletion);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourceV2AuthoredCatalogTest,
	"StarRovers.ResourceSystem.Phase16.AuthoredContent.Catalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourceV2AuthoredCatalogTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FSRResourceV2AuthoredContentValidation Validation;
	const bool bValid = FSRResourceV2AuthoredContent::ValidateAuthoredContent(Validation);
	for (const FString& Error : Validation.Errors)
	{
		AddError(Error);
	}
	TestTrue(TEXT("Every authored Resource V2 asset and terrain rule validates"), bValid);
	TestEqual(TEXT("Eight persistent Resource V2 assets exist"), Validation.ResourceAssetCount, 8);
	TestEqual(TEXT("Twenty-one persistent Facility V2 assets exist"), Validation.FacilityAssetCount, 21);
	TestEqual(TEXT("Twenty-one buildable Facility V2 structures exist"), Validation.StructureAssetCount, 21);
	TestEqual(TEXT("Seven mineable Resource V2 deposits exist"), Validation.DepositAssetCount, 7);
	TestEqual(TEXT("Seven Resource V2 deposit rules are attached to the Earth profile"), Validation.TerrainProfileRuleCount, 7);

	USRStructureDataAsset* LegacyProcessor = LoadObject<USRStructureDataAsset>(
		nullptr,
		TEXT("/Game/StarRovers/Structure/DataAssets/Artificial/Starter/DA_Structure_SP1.DA_Structure_SP1"));
	USRStructureDataAsset* V2Processor =
		StarRovers::ResourceV2AuthoredContentTests::LoadFacilityStructure(
			ESRFacilityContentPresetV2::PulseProcessor);
	USRStructureDataAsset* LegacyDeposit = LoadObject<USRStructureDataAsset>(
		nullptr,
		TEXT("/Game/StarRovers/Structure/DataAssets/Natural/DA_Structure_TerriteLode.DA_Structure_TerriteLode"));
	USRStructureDataAsset* V2Deposit = LoadObject<USRStructureDataAsset>(
		nullptr,
		*FSRResourceV2AuthoredContent::GetDepositObjectPath(ESRResourceContentPresetV2::HeliosIron));
	TestTrue(TEXT("The legacy processor is identified for V2 build-menu replacement"),
		FSRResourceV2AuthoredContent::IsLegacyProcessingStructure(LegacyProcessor));
	TestTrue(TEXT("The authored processor is a buildable V2 facility"),
		FSRResourceV2AuthoredContent::IsResourceV2FacilityStructure(V2Processor));

	USRFacilityDataAsset* AuthoredBridge =
		StarRovers::ResourceV2AuthoredContentTests::LoadFacility(
			ESRFacilityContentPresetV2::PulseProcessor);
	USRFacilityDataAsset* AuthoredOrganicYield =
		StarRovers::ResourceV2AuthoredContentTests::LoadFacility(
			ESRFacilityContentPresetV2::SporePress);
	USRFacilityDataAsset* AuthoredPlasmaBurst =
		StarRovers::ResourceV2AuthoredContentTests::LoadFacility(
			ESRFacilityContentPresetV2::ArcAmplifier);
	USRFacilityDataAsset* AuthoredVoidPayoff =
		StarRovers::ResourceV2AuthoredContentTests::LoadFacility(
			ESRFacilityContentPresetV2::EchoChamber);
	TestTrue(TEXT("Authored Pulse Processor carries the low-cost Bridge contract"),
		IsValid(AuthoredBridge)
			&& AuthoredBridge->ResourceV2Process.LineRole == ESRFacilityLineRoleV2::UniversalBridge
			&& FMath::IsNearlyEqual(AuthoredBridge->ResourceV2Process.FacilityEnergyDelta, 1.0)
			&& FMath::IsNearlyEqual(AuthoredBridge->BaseProcessSeconds, 2.0f)
			&& AuthoredBridge->OperationalLoad == 1);
	TestTrue(TEXT("Authored Organic payoff carries the slow, low-Load yield contract"),
		IsValid(AuthoredOrganicYield)
			&& AuthoredOrganicYield->ResourceV2Process.LineRole == ESRFacilityLineRoleV2::Payoff
			&& FMath::IsNearlyEqual(AuthoredOrganicYield->ResourceV2Process.FacilityEnergyDelta, 5.0)
			&& FMath::IsNearlyEqual(AuthoredOrganicYield->BaseProcessSeconds, 5.0f)
			&& AuthoredOrganicYield->OperationalLoad == 1);
	TestTrue(TEXT("Authored Plasma burst carries the fast, high-Load contract"),
		IsValid(AuthoredPlasmaBurst)
			&& AuthoredPlasmaBurst->ResourceV2Process.LineRole == ESRFacilityLineRoleV2::Burst
			&& FMath::IsNearlyEqual(AuthoredPlasmaBurst->BaseProcessSeconds, 2.0f)
			&& AuthoredPlasmaBurst->OperationalLoad == 5);
	TestTrue(TEXT("Authored Void payoff carries the deliberate Echo contract"),
		IsValid(AuthoredVoidPayoff)
			&& AuthoredVoidPayoff->ResourceV2Process.LineRole == ESRFacilityLineRoleV2::Payoff
			&& FMath::IsNearlyEqual(AuthoredVoidPayoff->ResourceV2Process.FacilityEnergyDelta, 5.0)
			&& FMath::IsNearlyEqual(AuthoredVoidPayoff->BaseProcessSeconds, 5.0f));
	TestTrue(TEXT("Legacy rules generate legacy deposits"),
		FSRResourceV2AuthoredContent::ShouldGenerateNaturalStructure(
			LegacyDeposit,
			ESRResourceRulesetVersion::Legacy));
	TestFalse(TEXT("Resource V2 rules suppress legacy deposits"),
		FSRResourceV2AuthoredContent::ShouldGenerateNaturalStructure(
			LegacyDeposit,
			ESRResourceRulesetVersion::ResourceV2));
	TestFalse(TEXT("Legacy rules suppress Resource V2 deposits"),
		FSRResourceV2AuthoredContent::ShouldGenerateNaturalStructure(
			V2Deposit,
			ESRResourceRulesetVersion::Legacy));
	TestTrue(TEXT("Resource V2 rules generate Resource V2 deposits"),
		FSRResourceV2AuthoredContent::ShouldGenerateNaturalStructure(
			V2Deposit,
			ESRResourceRulesetVersion::ResourceV2));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourceV2AuthoredVerticalSliceTest,
	"StarRovers.ResourceSystem.Phase16.AuthoredContent.VerticalSlice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourceV2AuthoredVerticalSliceTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace StarRovers::ResourceV2AuthoredContentTests;

	USRSimulationSettings* Settings = GetMutableDefault<USRSimulationSettings>();
	TestNotNull(TEXT("Simulation settings exist"), Settings);
	if (!Settings)
	{
		return false;
	}
	TGuardValue<ESRResourceRulesetVersion> RulesetGuard(
		Settings->ResourceRulesetVersion,
		ESRResourceRulesetVersion::ResourceV2);

	const TArray<ESRResourceContentPresetV2> CardPresets = {
		ESRResourceContentPresetV2::HeliosIron,
		ESRResourceContentPresetV2::EchoQuartz,
		ESRResourceContentPresetV2::VerdantSpore,
		ESRResourceContentPresetV2::AuroraPlasma,
		ESRResourceContentPresetV2::NullPearl,
	};
	TArray<FSRResourceInstance> Cards;
	for (const ESRResourceContentPresetV2 Preset : CardPresets)
	{
		USRResourceDataAsset* Resource = LoadResource(Preset);
		TestNotNull(TEXT("The authored Card resource loads"), Resource);
		if (Resource)
		{
			FSRResourceInstance Card = Resource->BuildDefaultInstance();
			StarRovers::Resources::InitializeResourceOrigin(Card, FName(TEXT("Cinder")));
			Cards.Add(Card);
		}
	}
	if (Cards.Num() != CardPresets.Num())
	{
		return false;
	}

	FSRFacilityInstance Forge = MakeFacilityInstance(
		ESRFacilityContentPresetV2::InductionForge,
		ESRFacilityTemperatureState::Hot);
	FSRFacilityProcessingCompletionResult ForgeCompletion;
	const bool bForgeCompleted = RunSingleInputFacility(Forge, Cards[0], ForgeCompletion);
	TestTrue(TEXT("A mined Helios Iron default instance runs through the authored Induction Forge"), bForgeCompleted);

	FSRFacilityInstance Press = MakeFacilityInstance(
		ESRFacilityContentPresetV2::CryoPress,
		ESRFacilityTemperatureState::Cold);
	FSRFacilityProcessingCompletionResult PressCompletion;
	const bool bPressCompleted = bForgeCompleted
		&& RunSingleInputFacility(Press, ForgeCompletion.PrimaryOutputResource, PressCompletion);
	TestTrue(TEXT("The forged card runs through the authored Cryo Press"), bPressCompleted);
	if (bPressCompleted)
	{
		Cards[0] = PressCompletion.PrimaryOutputResource;
		TestTrue(TEXT("Hot-to-Cold authored processing activates Tempered"),
			(Cards[0].ActiveFamilyStateFlags
				& StarRovers::Resources::GetFamilyStateBit(ESRResourceFamilyState::Tempered)) != 0);
		TestTrue(TEXT("Authored processing increases Energy additively"), Cards[0].CurrentEnergy > 5.0);
	}

	FSRFacilityInstance Fabricator = MakeFacilityInstance(
		ESRFacilityContentPresetV2::StellarFuelFabricator,
		ESRFacilityTemperatureState::Normal);
	TestEqual(TEXT("The authored fuel fabricator exposes five connected inputs"), Fabricator.InputPortInventories.Num(), 5);
	for (int32 CardIndex = 0; CardIndex < Cards.Num() && Fabricator.InputPortInventories.IsValidIndex(CardIndex); ++CardIndex)
	{
		Fabricator.InputPortInventories[CardIndex].Inventory.Add(Cards[CardIndex]);
	}
	FSRFacilityProcessingCompletionResult FuelCompletion;
	const bool bFuelCompleted = bPressCompleted
		&& FSRFacilityProcessingStepExecutor::TryStartProcessing(nullptr, Fabricator)
		&& FSRFacilityProcessingStepExecutor::TryCompleteProcessing(nullptr, Fabricator, &FuelCompletion);
	TestTrue(TEXT("Five authored Cards complete in the authored Stellar Fuel Fabricator"), bFuelCompleted);
	if (bFuelCompleted)
	{
		TestTrue(TEXT("The vertical slice uses the Resource V2 fuel route"), FuelCompletion.bUsedStellarFuelFabricatorV2);
		TestEqual(TEXT("The reference grades form a Full House"),
			FuelCompletion.StellarFuelFabricationResult.Hand,
			ESRStellarFuelHandV2::FullHouse);
		TestEqual(TEXT("The authored output is Stellar Fuel"),
			FuelCompletion.PrimaryOutputResource.ResourceClass,
			ESRResourceClass::StellarFuel);
		TestTrue(TEXT("The final multiplier produces more Energy than the additive inputs"),
			FuelCompletion.PrimaryOutputResource.CurrentEnergy
				> FuelCompletion.StellarFuelFabricationResult.InputEnergySum);
	}
	return !HasAnyErrors();
}

#endif
