#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Automation/SRResourceSystemContent.h"
#include "Automation/SRStellarFuelBatchPlanner.h"
#include "Misc/AutomationTest.h"
#include "Simulation/SRSimulationSettings.h"
#include "../Automation/SRFacilityDirectInventoryRouter.h"
#include "../Automation/SRFacilityInputAcceptancePolicy.h"
#include "../Automation/SRFacilityProcessingInventoryRouter.h"

namespace StarRovers::StellarFuelBatchPlannerTests
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

	FSRFacilityInstance MakeFabricator(USRFacilityDataAsset* DataAsset)
	{
		FSRFacilityInstance Facility;
		Facility.OccupantId = FName(TEXT("Phase19Fabricator"));
		Facility.FacilityDataAsset = DataAsset;
		Facility.bProcessEnabled = true;
		for (int32 LaneIndex = 0; LaneIndex < StarRovers::StellarFuel::RequiredCardCount; ++LaneIndex)
		{
			FSRFacilityPortInventory& Port = Facility.InputPortInventories.AddDefaulted_GetRef();
			Port.PortId = FName(*FString::Printf(TEXT("Input_%d"), LaneIndex));
			Port.PortKind = ESRFacilityPortKind::Input;
			Port.PortIndex = LaneIndex;
			Port.Capacity = 8;
		}
		FSRFacilityPortInventory& OutputPort = Facility.OutputPortInventories.AddDefaulted_GetRef();
		OutputPort.PortId = FName(TEXT("Output_0"));
		OutputPort.PortKind = ESRFacilityPortKind::Output;
		OutputPort.PortIndex = 0;
		OutputPort.Capacity = 8;
		return Facility;
	}

	TArray<FSRResourceInstance> MakeBatch()
	{
		TArray<FSRResourceInstance> Cards;
		FSRResourceSystemContent::MakeReferenceStellarFuelBatch(
			ESRStellarFuelReferenceTopologyV2::DistributedConvergence,
			FName(TEXT("Concord")),
			Cards);
		return Cards;
	}

	void FillLanes(
		FSRFacilityInstance& Facility,
		const TArray<FSRResourceInstance>& Cards,
		int32 CardCount = StarRovers::StellarFuel::RequiredCardCount)
	{
		for (int32 CardIndex = 0;
			CardIndex < FMath::Min(CardCount, Cards.Num())
				&& Facility.InputPortInventories.IsValidIndex(CardIndex);
			++CardIndex)
		{
			Facility.InputPortInventories[CardIndex].Inventory.Add(Cards[CardIndex]);
		}
	}

	int32 CountInputStacks(const FSRFacilityInstance& Facility)
	{
		int32 Count = 0;
		for (const FSRFacilityPortInventory& Port : Facility.InputPortInventories)
		{
			Count += Port.Inventory.Num();
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStellarFuelBatchPlannerStateTest,
	"StarRovers.ResourceSystem.Phase19.StellarFuelBatch.PlannerStates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStellarFuelBatchPlannerStateTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::StellarFuelBatchPlannerTests;
	USRSimulationSettings* Settings = GetMutableDefault<USRSimulationSettings>();
	if (!TestNotNull(TEXT("Simulation settings exist"), Settings))
	{
		return false;
	}
	TGuardValue<ESRResourceRulesetVersion> RulesetGuard(
		Settings->ResourceRulesetVersion,
		ESRResourceRulesetVersion::ResourceV2);
	USRFacilityDataAsset* DataAsset = MakeFabricatorDataAsset();
	if (!TestNotNull(TEXT("Fabricator preset exists"), DataAsset))
	{
		return false;
	}
	const TArray<FSRResourceInstance> Cards = MakeBatch();

	FSRFacilityInstance Partial = MakeFabricator(DataAsset);
	FillLanes(Partial, Cards, 3);
	FSRStellarFuelBatchStatusV2 Status;
	TestTrue(TEXT("A Fabricator produces a batch status"),
		FSRStellarFuelBatchPlanner::TryBuildStatus(Partial, FName(TEXT("Concord")), Status));
	TestEqual(TEXT("Three Cards are collecting, not a Recipe mismatch"),
		Status.State,
		ESRStellarFuelBatchStateV2::Collecting);
	TestEqual(TEXT("The planner counts three valid Cards"), Status.ValidCardCount, 3);
	TestEqual(TEXT("Two precise lanes remain empty"), Status.EmptyLaneIndices.Num(), 2);
	TestEqual(TEXT("The partial R2 B2 G4 pattern exposes One Pair"),
		Status.Hand,
		ESRStellarFuelHandV2::OnePair);
	TestTrue(TEXT("The concise summary exposes 3/5 immediately"), Status.Summary.Contains(TEXT("3/5")));

	FSRFacilityInstance Ready = MakeFabricator(DataAsset);
	FillLanes(Ready, Cards);
	TestTrue(TEXT("A complete batch produces a ready status"),
		FSRStellarFuelBatchPlanner::TryBuildStatus(Ready, FName(TEXT("Concord")), Status));
	TestEqual(TEXT("The complete batch is ready before reservation"),
		Status.State,
		ESRStellarFuelBatchStateV2::Ready);
	TestEqual(TEXT("The reference batch forecasts Full House"),
		Status.Hand,
		ESRStellarFuelHandV2::FullHouse);
	TestTrue(TEXT("The exact final Energy is available before committing the cycle"),
		Status.bHasFinalPreview && FMath::IsNearlyEqual(Status.FinalPreview.FuelEnergy, 1180.0));

	FSRFacilityInstance Duplicate = MakeFabricator(DataAsset);
	TArray<FSRResourceInstance> DuplicateCards = Cards;
	DuplicateCards[4].Spectrum = DuplicateCards[0].Spectrum;
	DuplicateCards[4].Grade = DuplicateCards[0].Grade;
	FillLanes(Duplicate, DuplicateCards);
	TestTrue(TEXT("A duplicate-key batch remains analyzable"),
		FSRStellarFuelBatchPlanner::TryBuildStatus(Duplicate, FName(TEXT("Concord")), Status));
	TestEqual(TEXT("Exactly one repeated Card beyond the first is reported"),
		Status.DuplicateCardKeyCount,
		1);
	TestTrue(TEXT("The duplicate warning explains Energy versus scoring"),
		Status.Detail.Contains(TEXT("add Energy but score once")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStellarFuelBatchAdmissionTest,
	"StarRovers.ResourceSystem.Phase19.StellarFuelBatch.NonDestructiveAdmission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStellarFuelBatchAdmissionTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::StellarFuelBatchPlannerTests;
	USRSimulationSettings* Settings = GetMutableDefault<USRSimulationSettings>();
	if (!TestNotNull(TEXT("Simulation settings exist"), Settings))
	{
		return false;
	}
	TGuardValue<ESRResourceRulesetVersion> RulesetGuard(
		Settings->ResourceRulesetVersion,
		ESRResourceRulesetVersion::ResourceV2);
	USRFacilityDataAsset* DataAsset = MakeFabricatorDataAsset();
	if (!TestNotNull(TEXT("Fabricator preset exists"), DataAsset))
	{
		return false;
	}
	const TArray<FSRResourceInstance> Cards = MakeBatch();
	FSRFacilityInstance Facility = MakeFabricator(DataAsset);

	FSRResourceInstance Utility = Cards[0];
	Utility.ResourceId = FName(TEXT("IndustrialSupply"));
	Utility.ResourceClass = ESRResourceClass::Utility;
	Utility.Family = ESRResourceFamily::None;
	Utility.Spectrum = ESRResourceSpectrum::None;
	FString FailureReason;
	TestFalse(TEXT("The shared admission policy rejects Utility cargo"),
		FSRFacilityInputAcceptancePolicy::CanAcceptResource(Facility, Utility, &FailureReason));
	TestTrue(TEXT("The rejection explains that a Family Card is required"),
		FailureReason.Contains(TEXT("Family Card")));
	TestFalse(TEXT("Direct transfer rejects Utility before mutating lane 1"),
		FSRFacilityDirectInventoryRouter::TryAddInputResourceToPort(Facility, 0, Utility));
	TestEqual(TEXT("Rejected Utility leaves the Fabricator empty"), CountInputStacks(Facility), 0);

	FSRResourceInstance UnknownImprintCard = Cards[0];
	UnknownImprintCard.FuelImprintSlot.ImprintId = FName(TEXT("UnknownPhase19Imprint"));
	TestFalse(TEXT("A Card with an unknown Imprint is rejected at admission"),
		FSRFacilityDirectInventoryRouter::TryAddInputResourceToPort(Facility, 0, UnknownImprintCard));
	TestEqual(TEXT("Rejected Imprint also leaves the endpoint untouched"), CountInputStacks(Facility), 0);
	TestTrue(TEXT("A valid Card is admitted normally"),
		FSRFacilityDirectInventoryRouter::TryAddInputResourceToPort(Facility, 0, Cards[0]));
	TestEqual(TEXT("Only the accepted Card occupies the lane"), CountInputStacks(Facility), 1);

	FSRFacilityInstance LoadedLegacySave = MakeFabricator(DataAsset);
	LoadedLegacySave.InputPortInventories[1].Inventory.Add(Utility);
	FSRStellarFuelBatchStatusV2 Status;
	TestTrue(TEXT("Pre-existing contamination remains inspectable"),
		FSRStellarFuelBatchPlanner::TryBuildStatus(
			LoadedLegacySave,
			FName(TEXT("Concord")),
			Status));
	TestEqual(TEXT("Pre-existing invalid cargo becomes a precise contaminated state"),
		Status.State,
		ESRStellarFuelBatchStateV2::Contaminated);
	TestTrue(TEXT("The contaminated lane is identified as lane 2"),
		Status.Summary.Contains(TEXT("Lane 2")));
	TestEqual(TEXT("Analysis never consumes or relocates the blocked cargo"),
		CountInputStacks(LoadedLegacySave),
		1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStellarFuelBatchReservationTest,
	"StarRovers.ResourceSystem.Phase19.StellarFuelBatch.AtomicReservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStellarFuelBatchReservationTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::StellarFuelBatchPlannerTests;
	USRSimulationSettings* Settings = GetMutableDefault<USRSimulationSettings>();
	if (!TestNotNull(TEXT("Simulation settings exist"), Settings))
	{
		return false;
	}
	TGuardValue<ESRResourceRulesetVersion> RulesetGuard(
		Settings->ResourceRulesetVersion,
		ESRResourceRulesetVersion::ResourceV2);
	USRFacilityDataAsset* DataAsset = MakeFabricatorDataAsset();
	if (!TestNotNull(TEXT("Fabricator preset exists"), DataAsset))
	{
		return false;
	}
	const TArray<FSRResourceInstance> Cards = MakeBatch();

	FSRFacilityInstance Incomplete = MakeFabricator(DataAsset);
	FillLanes(Incomplete, Cards, 4);
	TestFalse(TEXT("Four lanes cannot create a reservation"),
		FSRFacilityProcessingInventoryRouter::TryMoveInputsToProcessingInventory(Incomplete));
	TestEqual(TEXT("A failed reservation preserves all four source Cards"),
		CountInputStacks(Incomplete),
		4);
	TestTrue(TEXT("A failed reservation leaves no partial processing batch"),
		Incomplete.ProcessingInventory.IsEmpty());

	FSRFacilityInstance Complete = MakeFabricator(DataAsset);
	FillLanes(Complete, Cards);
	TestTrue(TEXT("Five lanes commit one atomic reservation"),
		FSRFacilityProcessingInventoryRouter::TryMoveInputsToProcessingInventory(Complete));
	Complete.bProcessing = true;
	TestEqual(TEXT("All five source lanes commit together"), CountInputStacks(Complete), 0);
	TestEqual(TEXT("The reserved inventory owns exactly five Cards"),
		Complete.ProcessingInventory.Num(),
		StarRovers::StellarFuel::RequiredCardCount);
	FSRStellarFuelBatchStatusV2 Status;
	TestTrue(TEXT("The reserved batch remains previewable while processing"),
		FSRStellarFuelBatchPlanner::TryBuildStatus(Complete, FName(TEXT("Concord")), Status));
	TestEqual(TEXT("Inspector-facing state distinguishes reserved from merely ready"),
		Status.State,
		ESRStellarFuelBatchStateV2::Reserved);
	TestTrue(TEXT("The reservation keeps the exact Full House fuel forecast"),
		Status.bHasFinalPreview && FMath::IsNearlyEqual(Status.FinalPreview.FuelEnergy, 1180.0));
	TestFalse(TEXT("A second reservation cannot overwrite the running batch"),
		FSRFacilityProcessingInventoryRouter::TryMoveInputsToProcessingInventory(Complete));
	TestEqual(TEXT("The rejected second reservation preserves the original five Cards"),
		Complete.ProcessingInventory.Num(),
		StarRovers::StellarFuel::RequiredCardCount);
	return true;
}

#endif
