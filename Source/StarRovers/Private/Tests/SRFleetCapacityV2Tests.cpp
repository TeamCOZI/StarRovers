#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Automation/SROperationalEconomyProcessor.h"
#include "Automation/SRResourceSystemContent.h"
#include "Logistics/SRFleetCapacityV2.h"
#include "Misc/AutomationTest.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Simulation/SRSimulationSettings.h"
#include "../Automation/SRFacilityProcessingStepExecutor.h"

namespace StarRovers::FleetCapacityV2Tests
{
	FSRSpaceLogisticsHubEndpoint MakeHub(const TCHAR* HubOccupantId)
	{
		FSRSpaceLogisticsHubEndpoint Hub;
		Hub.HubOccupantId = FName(HubOccupantId);
		return Hub;
	}

	FSRSpaceLogisticsHubRoute MakeRoute(
		const TCHAR* RouteId,
		const FSRSpaceLogisticsHubEndpoint& Source,
		const FSRSpaceLogisticsHubEndpoint& Destination,
		ESRSpaceLogisticsRouteProfileV2 Profile,
		ESRSpaceLogisticsHubRoutePhase Phase)
	{
		FSRSpaceLogisticsHubRoute Route;
		Route.RouteId = FName(RouteId);
		Route.SourceHub = Source;
		Route.DestinationHub = Destination;
		Route.RouteProfile = Profile;
		Route.MaxCargoStackCount = FSRFleetCapacityV2::GetRouteProfileRules(Profile).CargoCapacity;
		Route.Phase = Phase;
		return Route;
	}

	FSRResourceInstance MakeResource(ESRResourceClass ResourceClass)
	{
		FSRResourceInstance Resource;
		Resource.ResourceId = ResourceClass == ESRResourceClass::Card
			? FName(TEXT("FleetTestCard"))
			: FName(TEXT("FleetTestUtility"));
		Resource.ResourceClass = ResourceClass;
		Resource.StackCount = 1;
		return Resource;
	}

	FSRResourceInstance MakeIndustrialSupply()
	{
		FSRResourceInstance Supply;
		FSRResourceSystemContent::MakeReferenceResourceInstance(
			ESRResourceContentPresetV2::IndustrialSupply,
			NAME_None,
			Supply);
		Supply.ResourceInstanceId = FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		Supply.StackCount = 1;
		return Supply;
	}

	FSRFacilityInstance MakeFleetBerth(USRFacilityDataAsset* Definition)
	{
		FSRFacilityInstance Facility;
		Facility.OccupantId = FName(TEXT("FleetBerth_Test"));
		Facility.FacilityDataAsset = Definition;
		Facility.bProcessEnabled = true;
		Facility.OperationalPriority = Definition->DefaultOperationalPriority;
		FSRFacilityPortInventory& InputPort = Facility.InputPortInventories.AddDefaulted_GetRef();
		InputPort.PortId = FName(TEXT("Input_0"));
		InputPort.PortKind = ESRFacilityPortKind::Input;
		InputPort.PortIndex = 0;
		InputPort.Capacity = Definition->InputInventory.SlotCapacity;
		return Facility;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRFleetCapacityRouteProfileContractTest,
	"StarRovers.ResourceSystem.Phase8.FleetCapacity.RouteProfiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRFleetCapacityRouteProfileContractTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::FleetCapacityV2Tests;
	const FSRSpaceLogisticsRouteProfileRulesV2 Courier =
		FSRFleetCapacityV2::GetRouteProfileRules(ESRSpaceLogisticsRouteProfileV2::CardCourier);
	const FSRSpaceLogisticsRouteProfileRulesV2 Shuttle =
		FSRFleetCapacityV2::GetRouteProfileRules(ESRSpaceLogisticsRouteProfileV2::NeutralShuttle);
	const FSRSpaceLogisticsRouteProfileRulesV2 Bulk =
		FSRFleetCapacityV2::GetRouteProfileRules(ESRSpaceLogisticsRouteProfileV2::BulkRawHold);
	const FSRSpaceLogisticsRouteProfileRulesV2 Conditioned =
		FSRFleetCapacityV2::GetRouteProfileRules(ESRSpaceLogisticsRouteProfileV2::ConditionedHold);
	TestTrue(TEXT("Card Courier specialization is Cargo 12 / Fleet Load 2"), Courier.CargoCapacity == 12 && Courier.FleetLoad == 2);
	TestTrue(TEXT("Neutral Shuttle baseline is Cargo 8 / Fleet Load 2"), Shuttle.CargoCapacity == 8 && Shuttle.FleetLoad == 2);
	TestTrue(TEXT("Bulk baseline is Cargo 16 / Fleet Load 3"), Bulk.CargoCapacity == 16 && Bulk.FleetLoad == 3);
	TestTrue(TEXT("Conditioned baseline is Cargo 4 / Fleet Load 3"),
		Conditioned.CargoCapacity == 4 && Conditioned.FleetLoad == 3 && Conditioned.bSupportsConditionedTransit);

	const FSRResourceInstance Card = MakeResource(ESRResourceClass::Card);
	FSRResourceInstance ProcessedCard = Card;
	ProcessedCard.ProcessingMemory.ProcessCount = 1;
	FSRResourceInstance TaggedCard = Card;
	TaggedCard.ProcessTagSlot.TagId = TEXT("FleetTestTag");
	TaggedCard.ProcessTagSlot.Lifecycle = ESRResourceSlotLifecycle::Primed;
	TaggedCard.ProcessTagSlot.RemainingTriggers = 1;
	FSRResourceInstance TransitedCard = Card;
	TransitedCard.LogisticsMetadata.TransitCount = 2;
	const FSRResourceInstance Utility = MakeResource(ESRResourceClass::Utility);
	TestTrue(TEXT("Card Courier accepts cards"), FSRFleetCapacityV2::IsCargoEligible(Courier.Profile, Card));
	TestFalse(TEXT("Card Courier rejects utility freight"), FSRFleetCapacityV2::IsCargoEligible(Courier.Profile, Utility));
	TestTrue(TEXT("Bulk Hold accepts utility/raw freight"), FSRFleetCapacityV2::IsCargoEligible(Bulk.Profile, Utility));
	TestTrue(TEXT("Bulk Hold accepts an untouched raw Card"), FSRFleetCapacityV2::IsCargoEligible(Bulk.Profile, Card));
	TestFalse(TEXT("Bulk Hold rejects a processed Card"), FSRFleetCapacityV2::IsCargoEligible(Bulk.Profile, ProcessedCard));
	TestFalse(TEXT("Bulk Hold rejects a tagged Card"), FSRFleetCapacityV2::IsCargoEligible(Bulk.Profile, TaggedCard));
	TestTrue(TEXT("Transit history alone does not consume raw Card eligibility"),
		FSRFleetCapacityV2::IsCargoEligible(Bulk.Profile, TransitedCard));
	TestTrue(TEXT("Neutral Shuttle remains the safe compatibility profile"),
		FSRFleetCapacityV2::IsCargoEligible(Shuttle.Profile, Card)
			&& FSRFleetCapacityV2::IsCargoEligible(Shuttle.Profile, Utility));

	FSRSpaceLogisticsHubRoute ClampedRoute;
	ClampedRoute.RouteProfile = ESRSpaceLogisticsRouteProfileV2::ConditionedHold;
	ClampedRoute.MaxCargoStackCount = 999;
	TestEqual(TEXT("A profile hard-caps effective cargo without deleting overflow"),
		FSRFleetCapacityV2::ResolveEffectiveCargoCapacity(ClampedRoute), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRInterplanetaryLogisticsProfileDifferentiationTest,
	"StarRovers.ResourceSystem.Phase18.Logistics.ProfileDifferentiation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRInterplanetaryLogisticsProfileDifferentiationTest::RunTest(const FString& Parameters)
{
	TArray<ESRSpaceLogisticsRouteProfileV2> Profiles;
	FSRFleetCapacityV2::GetRouteProfiles(Profiles);
	TestEqual(TEXT("The Route deck exposes four stable hull profiles"), Profiles.Num(), 4);

	TSet<FName> ProfileIds;
	for (const ESRSpaceLogisticsRouteProfileV2 Profile : Profiles)
	{
		const FName ProfileId = FSRFleetCapacityV2::GetRouteProfileId(Profile);
		TestFalse(TEXT("Every Route Profile has a unique stable id"),
			ProfileId.IsNone() || ProfileIds.Contains(ProfileId));
		ProfileIds.Add(ProfileId);
		ESRSpaceLogisticsRouteProfileV2 ResolvedProfile;
		TestTrue(TEXT("Every authored Route Profile id resolves"),
			FSRFleetCapacityV2::TryResolveRouteProfileId(ProfileId, ResolvedProfile));
		TestEqual(TEXT("Route Profile ids round-trip without changing the enum"),
			ResolvedProfile,
			Profile);
	}

	TestTrue(TEXT("Neutral Shuttle is guaranteed Technology"),
		FSRFleetCapacityV2::IsTechnologyRouteProfile(ESRSpaceLogisticsRouteProfileV2::NeutralShuttle));
	TestTrue(TEXT("Card Courier is guaranteed Technology"),
		FSRFleetCapacityV2::IsTechnologyRouteProfile(ESRSpaceLogisticsRouteProfileV2::CardCourier));
	TestFalse(TEXT("Bulk Raw Hold requires a strategic unlock"),
		FSRFleetCapacityV2::IsTechnologyRouteProfile(ESRSpaceLogisticsRouteProfileV2::BulkRawHold));
	TestFalse(TEXT("Conditioned Hold requires a strategic unlock"),
		FSRFleetCapacityV2::IsTechnologyRouteProfile(ESRSpaceLogisticsRouteProfileV2::ConditionedHold));
	TestTrue(TEXT("Card Courier is materially more efficient than the compatibility Shuttle"),
		FSRFleetCapacityV2::ResolveMaximumCargoPerFleetLoad(ESRSpaceLogisticsRouteProfileV2::CardCourier)
			> FSRFleetCapacityV2::ResolveMaximumCargoPerFleetLoad(ESRSpaceLogisticsRouteProfileV2::NeutralShuttle));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRFleetCapacityReservationAndBerthTest,
	"StarRovers.ResourceSystem.Phase8.FleetCapacity.ReservationAndBerth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRFleetCapacityReservationAndBerthTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::FleetCapacityV2Tests;
	const FSRSpaceLogisticsHubEndpoint Source = MakeHub(TEXT("SourceHub"));
	const FSRSpaceLogisticsHubEndpoint Destination = MakeHub(TEXT("DestinationHub"));
	TArray<FSRSpaceLogisticsHubRoute> Routes;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Routes.Add(MakeRoute(
			*FString::Printf(TEXT("Travel_%d"), Index),
			Source,
			Destination,
			ESRSpaceLogisticsRouteProfileV2::NeutralShuttle,
			ESRSpaceLogisticsHubRoutePhase::TravelingToDestination));
	}
	FSRSpaceLogisticsHubRoute& Queued = Routes.Add_GetRef(MakeRoute(
		TEXT("Queued"),
		Source,
		Destination,
		ESRSpaceLogisticsRouteProfileV2::BulkRawHold,
		ESRSpaceLogisticsHubRoutePhase::WaitingForFleetCapacity));
	Queued.CurrentDockSide = ESRSpaceLogisticsHubRouteDockSide::Source;
	Queued.FleetDepartureQueueSequence = 10;

	const FSRFleetCapacityReportV2 BaseReport =
		FSRFleetCapacityV2::BuildReport(Source, Routes, true, 0, 8, 8);
	TestEqual(TEXT("Four Shuttle departures reserve all eight base capacity"), BaseReport.ReservedLoad, 8);
	TestEqual(TEXT("No base capacity remains"), BaseReport.AvailableCapacity, 0);
	TestEqual(TEXT("The later departure is visible in the queue"), BaseReport.QueuedDepartureCount, 1);
	TestFalse(TEXT("The queued Bulk Hold cannot depart while capacity is full"),
		FSRFleetCapacityV2::CanGrantDeparture(Queued, Source, Routes, BaseReport));

	const FSRFleetCapacityReportV2 BerthReport =
		FSRFleetCapacityV2::BuildReport(Source, Routes, true, 1, 8, 8);
	TestEqual(TEXT("One supplied Fleet Berth expands total capacity to sixteen"), BerthReport.TotalCapacity, 16);
	TestTrue(TEXT("The same queued Bulk Hold can depart after expansion"),
		FSRFleetCapacityV2::CanGrantDeparture(Queued, Source, Routes, BerthReport));

	const FSRFleetCapacityReportV2 LegacyReport =
		FSRFleetCapacityV2::BuildReport(Source, Routes, false, 99, 8, 8);
	TestFalse(TEXT("Fleet rules remain off under the Legacy ruleset"), LegacyReport.bRulesActive);
	TestEqual(TEXT("Legacy ignores supplied Berths"), LegacyReport.ActiveFleetBerthCount, 0);
	TestTrue(TEXT("Legacy departures are never blocked by the V2 budget"),
		FSRFleetCapacityV2::CanGrantDeparture(Queued, Source, Routes, LegacyReport));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRFleetCapacityFairQueueTest,
	"StarRovers.ResourceSystem.Phase8.FleetCapacity.FairQueue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRFleetCapacityFairQueueTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::FleetCapacityV2Tests;
	const FSRSpaceLogisticsHubEndpoint Source = MakeHub(TEXT("FairSource"));
	const FSRSpaceLogisticsHubEndpoint Destination = MakeHub(TEXT("FairDestination"));
	TArray<FSRSpaceLogisticsHubRoute> Routes;
	Routes.Add(MakeRoute(
		TEXT("Newer"), Source, Destination,
		ESRSpaceLogisticsRouteProfileV2::NeutralShuttle,
		ESRSpaceLogisticsHubRoutePhase::WaitingForFleetCapacity));
	Routes.Add(MakeRoute(
		TEXT("Older"), Source, Destination,
		ESRSpaceLogisticsRouteProfileV2::NeutralShuttle,
		ESRSpaceLogisticsHubRoutePhase::WaitingForFleetCapacity));
	FSRSpaceLogisticsHubRoute& Newer = Routes[0];
	FSRSpaceLogisticsHubRoute& Older = Routes[1];
	Newer.FleetDepartureQueueSequence = 20;
	Newer.CurrentDockSide = ESRSpaceLogisticsHubRouteDockSide::Source;
	Older.FleetDepartureQueueSequence = 10;
	Older.CurrentDockSide = ESRSpaceLogisticsHubRouteDockSide::Source;

	FSRFleetCapacityV2::RefreshQueuePositions(Routes);
	TestEqual(TEXT("Older ticket is first even when stored later"), Older.FleetQueuePosition, 1);
	TestEqual(TEXT("Newer ticket is second"), Newer.FleetQueuePosition, 2);
	const FSRFleetCapacityReportV2 EmptyReport =
		FSRFleetCapacityV2::BuildReport(Source, Routes, true, 0, 8, 8);
	TestTrue(TEXT("Oldest waiter receives the available departure"),
		FSRFleetCapacityV2::CanGrantDeparture(Older, Source, Routes, EmptyReport));
	TestFalse(TEXT("A newer waiter cannot jump the queue"),
		FSRFleetCapacityV2::CanGrantDeparture(Newer, Source, Routes, EmptyReport));

	Older.Phase = ESRSpaceLogisticsHubRoutePhase::TravelingToDestination;
	Older.FleetDepartureQueueSequence = 0;
	// Runtime refreshes the remaining positions at the end of the processing
	// tick, so the next waiter becomes eligible on the following tick.
	FSRFleetCapacityV2::RefreshQueuePositions(Routes);
	const FSRFleetCapacityReportV2 AfterDeparture =
		FSRFleetCapacityV2::BuildReport(Source, Routes, true, 0, 8, 8);
	TestEqual(TEXT("An active Shuttle reserves two capacity"), AfterDeparture.ReservedLoad, 2);
	TestTrue(TEXT("The next waiter departs when its turn and load both fit"),
		FSRFleetCapacityV2::CanGrantDeparture(Newer, Source, Routes, AfterDeparture));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRFleetBerthSupplyLifecycleTest,
	"StarRovers.ResourceSystem.Phase8.FleetCapacity.FleetBerthSupplyLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRFleetBerthSupplyLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::FleetCapacityV2Tests;
	USRSimulationSettings* Settings = GetMutableDefault<USRSimulationSettings>();
	TGuardValue<ESRResourceRulesetVersion> RulesetGuard(
		Settings->ResourceRulesetVersion,
		ESRResourceRulesetVersion::ResourceV2);

	USRFacilityDataAsset* Definition = NewObject<USRFacilityDataAsset>(GetTransientPackage());
	TestTrue(TEXT("Fleet Berth content preset applies"),
		FSRResourceSystemContent::ApplyFacilityPreset(*Definition, ESRFacilityContentPresetV2::FleetBerth));
	TestTrue(TEXT("Fleet Berth is a sixty-second, zero-load infrastructure consumer"),
		Definition->ResourceV2Synthesis.SynthesisRole == ESRFacilitySynthesisRoleV2::FleetBerth
			&& FMath::IsNearlyEqual(Definition->BaseProcessSeconds, 60.0f)
			&& Definition->OperationalLoad == 0
			&& Definition->InputInventory.SlotCapacity == 4
			&& Definition->OutputInventory.SlotCount == 0);

	FSRFacilityInstance Berth = MakeFleetBerth(Definition);
	Berth.InputPortInventories[0].Inventory.Add(MakeIndustrialSupply());
	TestTrue(TEXT("Buffered Industrial Supply activates Fleet Capacity"),
		FSROperationalEconomyProcessor::IsFleetBerthSupplied(Berth));
	FSRFacilityProcessingStartResult StartResult;
	TestTrue(TEXT("Fleet Berth reserves exactly one Supply for its cycle"),
		FSRFacilityProcessingStepExecutor::TryStartProcessing(nullptr, Berth, &StartResult));
	TestTrue(TEXT("Reserved in-flight Supply keeps the Berth active"),
		Berth.ProcessingInventory.Num() == 1
			&& FSROperationalEconomyProcessor::IsFleetBerthSupplied(Berth));

	Berth.ProcessProgressSeconds = Definition->BaseProcessSeconds;
	FSRFacilityProcessingCompletionResult CompletionResult;
	TestTrue(TEXT("Fleet Berth consumes Supply without manufacturing fake cargo"),
		FSRFacilityProcessingStepExecutor::TryCompleteProcessing(nullptr, Berth, &CompletionResult));
	TestTrue(TEXT("The final consumed Supply removes the capacity bonus"),
		CompletionResult.bUsedOperationalEconomyV2
			&& CompletionResult.OutputCount == 0
			&& !FSROperationalEconomyProcessor::IsFleetBerthSupplied(Berth));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRFleetCapacitySaveSchemaTest,
	"StarRovers.ResourceSystem.Phase8.FleetCapacity.SaveSchema",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRFleetCapacitySaveSchemaTest::RunTest(const FString& Parameters)
{
	FSRSpaceLogisticsSaveData Source;
	Source.NextFleetDepartureQueueSequence = 100;
	FSRSpaceLogisticsHubRouteSaveData& Route = Source.HubRoutes.AddDefaulted_GetRef();
	Route.RouteId = FName(TEXT("FleetSaveRoute"));
	Route.RouteProfile = ESRSpaceLogisticsRouteProfileV2::BulkRawHold;
	Route.Phase = ESRSpaceLogisticsHubRoutePhase::WaitingForFleetCapacity;
	Route.FleetDepartureQueueSequence = 42;

	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes, true);
	FObjectAndNameAsStringProxyArchive WriterArchive(Writer, false);
	FSRSpaceLogisticsSaveData::StaticStruct()->SerializeItem(WriterArchive, &Source, nullptr);
	WriterArchive.Close();

	FSRSpaceLogisticsSaveData Loaded;
	FMemoryReader Reader(Bytes, true);
	FObjectAndNameAsStringProxyArchive ReaderArchive(Reader, true);
	FSRSpaceLogisticsSaveData::StaticStruct()->SerializeItem(ReaderArchive, &Loaded, nullptr);
	ReaderArchive.Close();
	TestEqual(TEXT("Fleet Capacity payload upgrades with Logistics schema five"), Loaded.Version, 5);
	TestEqual(TEXT("Queue allocator sequence survives"), Loaded.NextFleetDepartureQueueSequence, static_cast<int64>(100));
	TestEqual(TEXT("One route survives"), Loaded.HubRoutes.Num(), 1);
	if (Loaded.HubRoutes.Num() == 1)
	{
		TestTrue(TEXT("Profile, queue phase, and fair ticket all survive"),
			Loaded.HubRoutes[0].RouteProfile == ESRSpaceLogisticsRouteProfileV2::BulkRawHold
				&& Loaded.HubRoutes[0].Phase == ESRSpaceLogisticsHubRoutePhase::WaitingForFleetCapacity
				&& Loaded.HubRoutes[0].FleetDepartureQueueSequence == 42);
	}
	FSRSpaceLogisticsSaveData LegacyCompatible;
	LegacyCompatible.Version = 2;
	TestTrue(TEXT("Schema two remains accepted for migration"), LegacyCompatible.IsSupportedVersion());
	return true;
}

#endif
