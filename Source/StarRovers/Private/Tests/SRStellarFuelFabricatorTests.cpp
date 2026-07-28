#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Automation/SRFacilityResourceV2Processor.h"
#include "Automation/SRResourceSystemContent.h"
#include "Automation/SRStellarFuelFabricator.h"
#include "Misc/AutomationTest.h"
#include "Simulation/SRSimulationSettings.h"
#include "../Automation/SRFacilityOutputResourceBuilder.h"
#include "../Automation/SRFacilityProcessingStepExecutor.h"
#include "../Logistics/SRSpaceLogisticsStarFuelMissileProcessor.h"

namespace StarRovers::StellarFuelTests
{
	USRFacilityDataAsset* MakeFabricatorDataAsset()
	{
		USRFacilityDataAsset* DataAsset = NewObject<USRFacilityDataAsset>(GetTransientPackage());
		return FSRResourceSystemContent::ApplyFacilityPreset(
			*DataAsset,
			ESRFacilityContentPresetV2::StellarFuelFabricator)
			? DataAsset
			: nullptr;
	}

	FSRFacilityInstance MakeFabricatorFacility(USRFacilityDataAsset* DataAsset, bool bAddRuntimePorts)
	{
		FSRFacilityInstance Facility;
		Facility.OccupantId = FName(TEXT("TestStellarFuelFabricator"));
		Facility.FacilityDataAsset = DataAsset;
		Facility.bProcessEnabled = true;
		if (!bAddRuntimePorts)
		{
			return Facility;
		}

		for (int32 InputIndex = 0; InputIndex < StarRovers::StellarFuel::RequiredCardCount; ++InputIndex)
		{
			FSRFacilityPortInventory& InputPort = Facility.InputPortInventories.AddDefaulted_GetRef();
			InputPort.PortId = FName(*FString::Printf(TEXT("Input_%d"), InputIndex));
			InputPort.PortKind = ESRFacilityPortKind::Input;
			InputPort.PortIndex = InputIndex;
			InputPort.Capacity = 8;
		}
		FSRFacilityPortInventory& OutputPort = Facility.OutputPortInventories.AddDefaulted_GetRef();
		OutputPort.PortId = FName(TEXT("Output_0"));
		OutputPort.PortKind = ESRFacilityPortKind::Output;
		OutputPort.PortIndex = 0;
		OutputPort.Capacity = 8;
		return Facility;
	}

	TArray<FSRResourceInstance> MakeBatch(
		ESRStellarFuelReferenceTopologyV2 Topology = ESRStellarFuelReferenceTopologyV2::DistributedConvergence)
	{
		TArray<FSRResourceInstance> Cards;
		FSRResourceSystemContent::MakeReferenceStellarFuelBatch(
			Topology,
			FName(TEXT("Concord")),
			Cards);
		return Cards;
	}

	void ClearFuelImprintsAndTopology(TArray<FSRResourceInstance>& Cards)
	{
		for (FSRResourceInstance& Card : Cards)
		{
			Card.FuelImprintSlot = FSRResourceFuelImprintSlot();
			Card.LogisticsMetadata = FSRResourceLogisticsMetadata();
		}
	}

	void FillInputPorts(FSRFacilityInstance& Facility, const TArray<FSRResourceInstance>& Cards)
	{
		for (int32 CardIndex = 0;
			CardIndex < Cards.Num() && Facility.InputPortInventories.IsValidIndex(CardIndex);
			++CardIndex)
		{
			Facility.InputPortInventories[CardIndex].Inventory.Add(Cards[CardIndex]);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStellarFuelHandEvaluationTest,
	"StarRovers.ResourceSystem.Phase5.StellarFuel.HandAndDuplicateKeys",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStellarFuelHandEvaluationTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::StellarFuelTests;

	TArray<FSRResourceInstance> Cards = MakeBatch();
	ClearFuelImprintsAndTopology(Cards);
	const FSRStellarFuelFabricationRulesV2 Rules;
	const FSRStellarFuelFabricationResultV2 FullHouse = FSRStellarFuelFabricator::EvaluateCards(
		Cards,
		Rules,
		FName(TEXT("Concord")));
	TestTrue(TEXT("The five unique reference Card Keys evaluate successfully"), FullHouse.IsSuccess());
	TestEqual(TEXT("R2, B2, G4, Y4, R4 is a Full House"), FullHouse.Hand, ESRStellarFuelHandV2::FullHouse);
	TestEqual(TEXT("All five Card Keys are unique"), FullHouse.UniqueCardKeyCount, 5);
	TestTrue(TEXT("Every Card Energy contributes to B"), FMath::IsNearlyEqual(FullHouse.InputEnergySum, 176.0));
	TestTrue(TEXT("Without Imprints B is 176 + 30"), FMath::IsNearlyEqual(FullHouse.FormulaB, 206.0));
	TestTrue(TEXT("Without a Catalyst C is 1 + 3"), FMath::IsNearlyEqual(FullHouse.FormulaC, 4.0));
	TestTrue(TEXT("The Fabricator is the only multiplication point"), FMath::IsNearlyEqual(FullHouse.FuelEnergy, 824.0));

	Cards[4].Spectrum = Cards[0].Spectrum;
	Cards[4].Grade = Cards[0].Grade;
	const FSRStellarFuelFabricationResultV2 DuplicateKey = FSRStellarFuelFabricator::EvaluateCards(
		Cards,
		Rules,
		FName(TEXT("Concord")));
	TestTrue(TEXT("An exact duplicate Card Key remains a valid Energy input"), DuplicateKey.IsSuccess());
	TestEqual(TEXT("An exact duplicate is counted once for the hand"), DuplicateKey.UniqueCardKeyCount, 4);
	TestEqual(TEXT("The remaining unique keys form Two Pair"), DuplicateKey.Hand, ESRStellarFuelHandV2::TwoPair);
	TestTrue(TEXT("Duplicate Card Energy still contributes in full"), FMath::IsNearlyEqual(DuplicateKey.InputEnergySum, 176.0));

	TArray<FSRResourceInstance> FourCards = Cards;
	FourCards.Pop();
	TestEqual(
		TEXT("A non-five-card batch is rejected"),
		FSRStellarFuelFabricator::EvaluateCards(FourCards, Rules).Outcome,
		ESRStellarFuelFabricationOutcomeV2::WrongCardCount);
	Cards[0].ResourceClass = ESRResourceClass::StellarFuel;
	TestEqual(
		TEXT("Fabricated fuel cannot be fed back as a Card"),
		FSRStellarFuelFabricator::EvaluateCards(Cards, Rules).Outcome,
		ESRStellarFuelFabricationOutcomeV2::InvalidCard);
	Cards[0].ResourceClass = ESRResourceClass::Card;
	Cards[0].Spectrum = static_cast<ESRResourceSpectrum>(255);
	TestEqual(
		TEXT("An unknown Spectrum enum value is rejected"),
		FSRStellarFuelFabricator::EvaluateCards(Cards, Rules).Outcome,
		ESRStellarFuelFabricationOutcomeV2::InvalidCard);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStellarFuelReferenceTopologyTest,
	"StarRovers.ResourceSystem.Phase5.StellarFuel.ReferenceTopologies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStellarFuelReferenceTopologyTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::StellarFuelTests;

	const FSRStellarFuelFabricationRulesV2 Rules;
	const FName FabricatorBodyId(TEXT("Concord"));
	const FSRStellarFuelFabricationResultV2 Distributed = FSRStellarFuelFabricator::EvaluateCards(
		MakeBatch(ESRStellarFuelReferenceTopologyV2::DistributedConvergence),
		Rules,
		FabricatorBodyId);
	TestTrue(TEXT("Distributed reference batch succeeds"), Distributed.IsSuccess());
	TestEqual(TEXT("Distributed batch uses Convergence Seal"), Distributed.AppliedTopologySealId,
		FSRResourceSystemContent::GetFuelImprintId(ESRFuelImprintContentV2::ConvergenceSeal));
	TestEqual(TEXT("Three valid unique Card Keys contribute Twin Seal"), Distributed.EffectiveTwinSealCount, 3);
	TestEqual(TEXT("Only one Prismatic Catalyst contributes"), Distributed.EffectivePrismaticCatalystCount, 1);
	TestTrue(TEXT("Distributed B is 176 + 30 + 18 + 12"), FMath::IsNearlyEqual(Distributed.FormulaB, 236.0));
	TestTrue(TEXT("Distributed C is 1 + 3 + 1"), FMath::IsNearlyEqual(Distributed.FormulaC, 5.0));
	TestTrue(TEXT("Distributed reference fuel is 1180"), FMath::IsNearlyEqual(Distributed.FuelEnergy, 1180.0));

	const FSRStellarFuelFabricationResultV2 Central = FSRStellarFuelFabricator::EvaluateCards(
		MakeBatch(ESRStellarFuelReferenceTopologyV2::CentralFoundry),
		Rules,
		FabricatorBodyId);
	TestTrue(TEXT("Central reference batch succeeds"), Central.IsSuccess());
	TestEqual(TEXT("Central batch uses Foundry Seal"), Central.AppliedTopologySealId,
		FSRResourceSystemContent::GetFuelImprintId(ESRFuelImprintContentV2::FoundrySeal));
	TestTrue(TEXT("Central reference fuel is 1180"), FMath::IsNearlyEqual(Central.FuelEnergy, 1180.0));

	const FSRStellarFuelFabricationResultV2 Pilgrim = FSRStellarFuelFabricator::EvaluateCards(
		MakeBatch(ESRStellarFuelReferenceTopologyV2::PilgrimCircuit),
		Rules,
		FabricatorBodyId);
	TestTrue(TEXT("Pilgrim reference batch succeeds"), Pilgrim.IsSuccess());
	TestEqual(TEXT("Pilgrim batch uses Pilgrim Seal"), Pilgrim.AppliedTopologySealId,
		FSRResourceSystemContent::GetFuelImprintId(ESRFuelImprintContentV2::PilgrimSeal));
	TestTrue(TEXT("Pilgrim input Energy includes its +5 route gain"), FMath::IsNearlyEqual(Pilgrim.InputEnergySum, 181.0));
	TestTrue(TEXT("Pilgrim reference fuel is 1205"), FMath::IsNearlyEqual(Pilgrim.FuelEnergy, 1205.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStellarFuelFacilityExecutorTest,
	"StarRovers.ResourceSystem.Phase5.StellarFuel.FacilityExecutorAndTerminalOutput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStellarFuelFacilityExecutorTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::StellarFuelTests;

	USRSimulationSettings* Settings = GetMutableDefault<USRSimulationSettings>();
	if (!TestNotNull(TEXT("Simulation settings exist"), Settings))
	{
		return false;
	}
	TGuardValue<ESRResourceRulesetVersion> RulesetGuard(
		Settings->ResourceRulesetVersion,
		ESRResourceRulesetVersion::ResourceV2);

	USRFacilityDataAsset* FabricatorDataAsset = MakeFabricatorDataAsset();
	if (!TestNotNull(TEXT("Stellar Fuel Fabricator preset exists"), FabricatorDataAsset))
	{
		return false;
	}
	FSRFacilityInstance Fabricator = MakeFabricatorFacility(FabricatorDataAsset, true);
	const TArray<FSRResourceInstance> Cards = MakeBatch();
	FillInputPorts(Fabricator, Cards);
	TestTrue(TEXT("The complete five-port batch matches the Fabricator operation"),
		FSRFacilityOutputResourceBuilder::DoesInputSetMatchOperation(
			FabricatorDataAsset,
			Cards,
			Fabricator.TemperatureState));

	FSRFacilityProcessingStartResult StartResult;
	FSRFacilityProcessingCompletionResult CompletionResult;
	TestTrue(TEXT("Fabricator starts through the normal inventory executor"),
		FSRFacilityProcessingStepExecutor::TryStartProcessing(nullptr, Fabricator, &StartResult));
	TestEqual(TEXT("Exactly five Cards move into the processing batch"), StartResult.ProcessingInputCount, 5);
	TestTrue(TEXT("Fabricator completes through the normal inventory executor"),
		FSRFacilityProcessingStepExecutor::TryCompleteProcessing(nullptr, Fabricator, &CompletionResult));
	TestTrue(TEXT("Completion reports the dedicated Fabricator V2 route"),
		CompletionResult.bUsedStellarFuelFabricatorV2 && CompletionResult.bUsedResourceV2);
	TestFalse(TEXT("Fabrication is not reported as a normal V2 process"), CompletionResult.bUsedResourceV2Process);
	TestEqual(TEXT("Fabricator emits one terminal output"), CompletionResult.OutputCount, 1);
	TestEqual(TEXT("Output class is Stellar Fuel"), CompletionResult.PrimaryOutputResource.ResourceClass,
		ESRResourceClass::StellarFuel);
	TestTrue(TEXT("Inventory execution preserves the 1180 formula result"),
		FMath::IsNearlyEqual(CompletionResult.PrimaryOutputResource.CurrentEnergy, 1180.0));
	TestTrue(TEXT("All five processing inputs are consumed atomically"), Fabricator.ProcessingInventory.IsEmpty());

	USRFacilityDataAsset* PulseDataAsset = NewObject<USRFacilityDataAsset>(GetTransientPackage());
	FSRResourceSystemContent::ApplyFacilityPreset(*PulseDataAsset, ESRFacilityContentPresetV2::PulseProcessor);
	TestFalse(TEXT("Terminal Stellar Fuel cannot enter a normal processing facility"),
		FSRFacilityOutputResourceBuilder::DoesInputSetMatchOperation(
			PulseDataAsset,
			TArray<FSRResourceInstance>({CompletionResult.PrimaryOutputResource}),
			ESRFacilityTemperatureState::Normal));

	FSRFacilityInstance IncompleteFabricator = MakeFabricatorFacility(FabricatorDataAsset, true);
	TArray<FSRResourceInstance> FourCards = Cards;
	FourCards.Pop();
	FillInputPorts(IncompleteFabricator, FourCards);
	TestFalse(TEXT("An incomplete batch cannot start and consume inputs"),
		FSRFacilityProcessingStepExecutor::TryStartProcessing(nullptr, IncompleteFabricator));
	int32 RemainingCardCount = 0;
	for (const FSRFacilityPortInventory& InputPort : IncompleteFabricator.InputPortInventories)
	{
		RemainingCardCount += InputPort.Inventory.Num();
	}
	TestEqual(TEXT("Rejected incomplete batch leaves all four Cards in their ports"), RemainingCardCount, 4);

	FabricatorDataAsset->FacilityDefinitionVersion = StarRovers::Facilities::LegacyFacilityDefinitionVersion;
	TestTrue(TEXT("An explicitly marked Fabricator remains on the guarded V2 route"),
		FSRStellarFuelFabricator::ShouldRouteThroughResourceV2(FabricatorDataAsset));
	TestFalse(TEXT("An outdated Fabricator definition is blocked instead of falling through to Legacy synthesis"),
		FSRFacilityOutputResourceBuilder::DoesInputSetMatchOperation(
			FabricatorDataAsset,
			Cards,
			ESRFacilityTemperatureState::Normal));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStellarFuelLaunchGateTest,
	"StarRovers.ResourceSystem.Phase5.StellarFuel.LaunchGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStellarFuelLaunchGateTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::StellarFuelTests;

	USRSimulationSettings* Settings = GetMutableDefault<USRSimulationSettings>();
	if (!TestNotNull(TEXT("Simulation settings exist"), Settings))
	{
		return false;
	}

	FSRResourceInstance LegacyCargo;
	LegacyCargo.ResourceId = FName(TEXT("LegacyFuel"));
	LegacyCargo.EnergyValue = 10.0;
	LegacyCargo.StackCount = 2;
	{
		TGuardValue<ESRResourceRulesetVersion> LegacyGuard(
			Settings->ResourceRulesetVersion,
			ESRResourceRulesetVersion::Legacy);
		TestTrue(TEXT("Legacy ruleset retains positive-Energy missile cargo behavior"),
			StarRovers::SpaceLogistics::StarFuelMissiles::CanUseAsMissileFuelCargo(LegacyCargo));
		TestTrue(TEXT("Legacy missile fuel still uses EnergyValue times stack"),
			FMath::IsNearlyEqual(
				StarRovers::SpaceLogistics::StarFuelMissiles::CalculateMissileFuelValue(LegacyCargo),
				20.0));
	}

	TGuardValue<ESRResourceRulesetVersion> ResourceV2Guard(
		Settings->ResourceRulesetVersion,
		ESRResourceRulesetVersion::ResourceV2);
	FSRResourceInstance Card = MakeBatch()[0];
	TestFalse(TEXT("A positive-Energy Card cannot be launched under Resource V2"),
		StarRovers::SpaceLogistics::StarFuelMissiles::CanUseAsMissileFuelCargo(Card));
	TestTrue(TEXT("Rejected Card cargo contributes zero fuel"),
		FMath::IsNearlyZero(
			StarRovers::SpaceLogistics::StarFuelMissiles::CalculateMissileFuelValue(Card)));

	const FSRStellarFuelFabricationResultV2 Fabrication = FSRStellarFuelFabricator::EvaluateCards(
		MakeBatch(),
		FSRStellarFuelFabricationRulesV2(),
		FName(TEXT("Concord")));
	FSRResourceInstance StellarFuel = Fabrication.OutputFuel;
	StellarFuel.StackCount = 2;
	TestTrue(TEXT("A valid terminal Stellar Fuel resource can be launched"),
		StarRovers::SpaceLogistics::StarFuelMissiles::CanUseAsMissileFuelCargo(StellarFuel));
	TestTrue(TEXT("Resource V2 missile delivery uses Current Energy times stack"),
		FMath::IsNearlyEqual(
			StarRovers::SpaceLogistics::StarFuelMissiles::CalculateMissileFuelValue(StellarFuel),
			2360.0));
	StellarFuel.ResourceSchemaVersion = StarRovers::Resources::LegacyResourceSchemaVersion;
	TestFalse(TEXT("Outdated Stellar Fuel schema is rejected at launch"),
		StarRovers::SpaceLogistics::StarFuelMissiles::CanUseAsMissileFuelCargo(StellarFuel));
	return true;
}

#endif
