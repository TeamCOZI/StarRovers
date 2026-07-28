#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Automation/SRResourceInstanceOperations.h"
#include "Automation/SRResourceSystemContent.h"
#include "Logistics/SRConditionedTransitV2.h"
#include "Misc/AutomationTest.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Simulation/SRAugmentPackageContent.h"
#include "../Logistics/SRSpaceLogisticsRouteRegistry.h"

namespace StarRovers::ConditionedTransitV2Tests
{
	FSRResourceInstance MakeCard(ESRResourceContentPresetV2 Preset, const TCHAR* OriginBodyId)
	{
		FSRResourceInstance Resource;
		FSRResourceSystemContent::MakeReferenceResourceInstance(
			Preset,
			FName(OriginBodyId),
			Resource);
		return Resource;
	}

	bool HasState(const FSRResourceInstance& Resource, ESRResourceFamilyState State)
	{
		return (Resource.ActiveFamilyStateFlags & StarRovers::Resources::GetFamilyStateBit(State)) != 0;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRConditionedTransitCatalogContractTest,
	"StarRovers.ResourceSystem.Phase9.ConditionedTransit.ModuleCatalogAndAugments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRConditionedTransitCatalogContractTest::RunTest(const FString& Parameters)
{
	TArray<ESRConditionedTransitModuleV2> Modules;
	FSRConditionedTransitV2::GetConditionedModules(Modules);
	TestEqual(TEXT("The vertical slice has three concrete conditioned Modules"), Modules.Num(), 3);

	const FSRConditionedTransitModuleRulesV2 Cryogenic =
		FSRConditionedTransitV2::GetModuleRules(ESRConditionedTransitModuleV2::CryogenicHold);
	const FSRConditionedTransitModuleRulesV2 BioCulture =
		FSRConditionedTransitV2::GetModuleRules(ESRConditionedTransitModuleV2::BioCultureHold);
	const FSRConditionedTransitModuleRulesV2 Grounding =
		FSRConditionedTransitV2::GetModuleRules(ESRConditionedTransitModuleV2::GroundingHold);
	TestTrue(TEXT("Cryogenic is the Metal Cold +3 contract"),
		Cryogenic.CompatibleFamily == ESRResourceFamily::Metal
			&& Cryogenic.Temperature == ESRResourceProcessTemperatureState::Cold
			&& Cryogenic.FamilyAction == ESRResourceFamilyAction::None
			&& FMath::IsNearlyEqual(Cryogenic.BaseEnergyDelta, 3.0)
			&& FMath::IsNearlyEqual(Cryogenic.BaseConditioningSeconds, 6.0f));
	TestTrue(TEXT("Bio-Culture is one Organic Growth contract"),
		BioCulture.CompatibleFamily == ESRResourceFamily::Organic
			&& BioCulture.FamilyAction == ESRResourceFamilyAction::Growth
			&& FMath::IsNearlyZero(BioCulture.BaseEnergyDelta)
			&& FMath::IsNearlyEqual(BioCulture.BaseConditioningSeconds, 8.0f));
	TestTrue(TEXT("Grounding is the Plasma Discharge +1 contract"),
		Grounding.CompatibleFamily == ESRResourceFamily::Plasma
			&& Grounding.FamilyAction == ESRResourceFamilyAction::Discharge
			&& FMath::IsNearlyEqual(Grounding.BaseEnergyDelta, 1.0)
			&& FMath::IsNearlyEqual(Grounding.BaseConditioningSeconds, 4.0f));

	const TArray<FName> NoPackages;
	TestFalse(TEXT("A Hold is not available before its Augment"),
		FSRAugmentPackageContentV2::IsLogisticsModuleUnlocked(Cryogenic.UnlockModuleId, NoPackages));
	const TArray<FName> MetalPackage = { FName(TEXT("DeepSpaceTempering")) };
	TestTrue(TEXT("Deep-Space Tempering unlocks only its Route Module"),
		FSRAugmentPackageContentV2::IsLogisticsModuleUnlocked(Cryogenic.UnlockModuleId, MetalPackage)
			&& !FSRAugmentPackageContentV2::IsLogisticsModuleUnlocked(BioCulture.UnlockModuleId, MetalPackage));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRConditionedTransitStateNeutralContractTest,
	"StarRovers.ResourceSystem.Phase9.ConditionedTransit.StateNeutralDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRConditionedTransitStateNeutralContractTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::ConditionedTransitV2Tests;
	FSRResourceInstance Input = MakeCard(ESRResourceContentPresetV2::HeliosIron, TEXT("Cinder"));
	Input.ProcessingMemory.LastProcessArchetype = FName(TEXT("InductionForge"));
	Input.ProcessingMemory.LastTemperature = ESRResourceProcessTemperatureState::Hot;
	Input.ProcessingMemory.ProcessCount = 7;
	Input.ActiveFamilyStateFlags = StarRovers::Resources::GetFamilyStateBit(ESRResourceFamilyState::Tempered);
	const FSRResourceProcessingMemory InputMemory = Input.ProcessingMemory;
	const double InputEnergy = Input.CurrentEnergy;
	const int32 InputStates = Input.ActiveFamilyStateFlags;

	const FSRConditionedTransitResultV2 Result = FSRConditionedTransitV2::EvaluateArrival(
		Input,
		ESRSpaceLogisticsRouteProfileV2::NeutralShuttle,
		ESRConditionedTransitModuleV2::None,
		FName(TEXT("Cinder")),
		FName(TEXT("Prism")),
		true);
	TestEqual(TEXT("Ordinary transport is explicitly state-neutral"),
		Result.Outcome,
		ESRConditionedTransitOutcomeV2::StateNeutral);
	TestTrue(TEXT("Ordinary transport records exactly one completed leg"),
		Result.bTransitRecorded
			&& Result.OutputResource.LogisticsMetadata.TransitCount == 1
			&& Result.OutputResource.LogisticsMetadata.LastTransitDestinationBodyId == FName(TEXT("Prism")));
	TestTrue(TEXT("Ordinary transport cannot become a hidden process"),
		!Result.bProcessApplied
			&& FMath::IsNearlyEqual(Result.OutputResource.CurrentEnergy, InputEnergy)
			&& Result.OutputResource.ActiveFamilyStateFlags == InputStates
			&& Result.OutputResource.ProcessingMemory.LastProcessArchetype == InputMemory.LastProcessArchetype
			&& Result.OutputResource.ProcessingMemory.LastTemperature == InputMemory.LastTemperature
			&& Result.OutputResource.ProcessingMemory.ProcessCount == InputMemory.ProcessCount);
	TestTrue(TEXT("Preview evaluation never mutates its input"),
		Input.LogisticsMetadata.TransitCount == 0
			&& Input.ProcessingMemory.ProcessCount == 7
			&& FMath::IsNearlyEqual(Input.CurrentEnergy, InputEnergy));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRConditionedTransitFamilyActionsTest,
	"StarRovers.ResourceSystem.Phase9.ConditionedTransit.FamilyActions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRConditionedTransitFamilyActionsTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::ConditionedTransitV2Tests;
	FSRResourceInstance Metal = MakeCard(ESRResourceContentPresetV2::HeliosIron, TEXT("Cinder"));
	Metal.ProcessingMemory.LastProcessArchetype = FName(TEXT("InductionForge"));
	Metal.ProcessingMemory.LastTemperature = ESRResourceProcessTemperatureState::Hot;
	const FSRConditionedTransitResultV2 Cryogenic = FSRConditionedTransitV2::EvaluateArrival(
		Metal,
		ESRSpaceLogisticsRouteProfileV2::ConditionedHold,
		ESRConditionedTransitModuleV2::CryogenicHold,
		FName(TEXT("Cinder")),
		FName(TEXT("Prism")),
		true);
	TestTrue(TEXT("Cryogenic arrival is one visible Metal process"),
		Cryogenic.bProcessApplied
			&& Cryogenic.OutputResource.ProcessingMemory.ProcessCount == 1
			&& Cryogenic.OutputResource.LogisticsMetadata.TransitCount == 1
			&& Cryogenic.OutputResource.ProcessingMemory.LastProcessArchetype == FName(TEXT("CryogenicTransit"))
			&& Cryogenic.OutputResource.ProcessingMemory.LastTemperature == ESRResourceProcessTemperatureState::Cold);
	TestTrue(TEXT("Hot Metal becomes Tempered and receives +3 plus the +5 State payoff"),
		HasState(Cryogenic.OutputResource, ESRResourceFamilyState::Tempered)
			&& FMath::IsNearlyEqual(Cryogenic.ProcessResult.AppliedEnergyDelta, 8.0));

	FSRResourceInstance Organic = MakeCard(ESRResourceContentPresetV2::VerdantSpore, TEXT("Viridia"));
	Organic.ActiveFamilyStateFlags = StarRovers::Resources::GetFamilyStateBit(ESRResourceFamilyState::Depleted);
	Organic.ProcessingMemory.GeneralProcessesSinceReset = 2;
	const FSRConditionedTransitResultV2 BioCulture = FSRConditionedTransitV2::EvaluateArrival(
		Organic,
		ESRSpaceLogisticsRouteProfileV2::ConditionedHold,
		ESRConditionedTransitModuleV2::BioCultureHold,
		FName(TEXT("Viridia")),
		FName(TEXT("Concord")),
		true);
	TestTrue(TEXT("Bio-Culture completes exactly one Growth cycle without free base Energy"),
		BioCulture.bProcessApplied
			&& BioCulture.OutputResource.ProcessingMemory.ProcessCount == 1
			&& BioCulture.OutputResource.ProcessingMemory.LastFamilyAction == ESRResourceFamilyAction::Growth
			&& HasState(BioCulture.OutputResource, ESRResourceFamilyState::Matured)
			&& !HasState(BioCulture.OutputResource, ESRResourceFamilyState::Depleted)
			&& FMath::IsNearlyEqual(BioCulture.ProcessResult.AppliedEnergyDelta, 0.0));

	FSRResourceInstance Plasma = MakeCard(ESRResourceContentPresetV2::AuroraPlasma, TEXT("Tempest"));
	Plasma.ActiveFamilyStateFlags = StarRovers::Resources::GetFamilyStateBit(ESRResourceFamilyState::Energized)
		| StarRovers::Resources::GetFamilyStateBit(ESRResourceFamilyState::Overloaded);
	Plasma.ProcessingMemory.LastFamilyAction = ESRResourceFamilyAction::Amplification;
	Plasma.ProcessingMemory.ConsecutiveSameFamilyActionCount = 3;
	const FSRConditionedTransitResultV2 Grounding = FSRConditionedTransitV2::EvaluateArrival(
		Plasma,
		ESRSpaceLogisticsRouteProfileV2::ConditionedHold,
		ESRConditionedTransitModuleV2::GroundingHold,
		FName(TEXT("Tempest")),
		FName(TEXT("Concord")),
		true);
	TestTrue(TEXT("Grounding performs one +1 Discharge and clears both Plasma States"),
		Grounding.bProcessApplied
			&& Grounding.OutputResource.ProcessingMemory.ProcessCount == 1
			&& Grounding.OutputResource.ProcessingMemory.LastFamilyAction == ESRResourceFamilyAction::Discharge
			&& !HasState(Grounding.OutputResource, ESRResourceFamilyState::Energized)
			&& !HasState(Grounding.OutputResource, ESRResourceFamilyState::Overloaded)
			&& FMath::IsNearlyEqual(Grounding.ProcessResult.AppliedEnergyDelta, 1.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRConditionedTransitSafetyAndSaveTest,
	"StarRovers.ResourceSystem.Phase9.ConditionedTransit.SafetyAndSaveSchema",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRConditionedTransitSafetyAndSaveTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::ConditionedTransitV2Tests;
	FSRResourceInstance Metal = MakeCard(ESRResourceContentPresetV2::HeliosIron, TEXT("Cinder"));
	Metal.ProcessingMemory.LastTemperature = ESRResourceProcessTemperatureState::Hot;
	const FSRConditionedTransitResultV2 Locked = FSRConditionedTransitV2::EvaluateArrival(
		Metal,
		ESRSpaceLogisticsRouteProfileV2::ConditionedHold,
		ESRConditionedTransitModuleV2::CryogenicHold,
		FName(TEXT("Cinder")),
		FName(TEXT("Prism")),
		false);
	TestTrue(TEXT("A locked Module degrades safely to transport without processing"),
		Locked.Outcome == ESRConditionedTransitOutcomeV2::LockedModule
			&& Locked.bTransitRecorded
			&& !Locked.bProcessApplied
			&& Locked.OutputResource.ProcessingMemory.ProcessCount == 0
			&& FMath::IsNearlyEqual(Locked.OutputResource.CurrentEnergy, Metal.CurrentEnergy));

	const FSRResourceInstance Crystal = MakeCard(ESRResourceContentPresetV2::EchoQuartz, TEXT("Prism"));
	const FSRConditionedTransitResultV2 Incompatible = FSRConditionedTransitV2::EvaluateArrival(
		Crystal,
		ESRSpaceLogisticsRouteProfileV2::ConditionedHold,
		ESRConditionedTransitModuleV2::CryogenicHold,
		FName(TEXT("Prism")),
		FName(TEXT("Concord")),
		true);
	TestTrue(TEXT("A mismatched Family is never transformed"),
		Incompatible.Outcome == ESRConditionedTransitOutcomeV2::IncompatibleCargo
			&& !Incompatible.bProcessApplied
			&& Incompatible.OutputResource.ProcessingMemory.ProcessCount == 0
			&& FMath::IsNearlyEqual(Incompatible.OutputResource.CurrentEnergy, Crystal.CurrentEnergy));

	TArray<FSRSpaceLogisticsHubRoute> ConfigurationRoutes;
	FSRSpaceLogisticsHubRoute& ConfigurationRoute = ConfigurationRoutes.AddDefaulted_GetRef();
	ConfigurationRoute.RouteId = FName(TEXT("ConditionedConfigurationRoute"));
	ConfigurationRoute.RouteProfile = ESRSpaceLogisticsRouteProfileV2::ConditionedHold;
	ConfigurationRoute.Phase = ESRSpaceLogisticsHubRoutePhase::WaitingForCargo;
	TestTrue(TEXT("An empty docked Conditioned Hold accepts a concrete Module"),
		FSRSpaceLogisticsRouteRegistry::SetHubRouteConditionedTransitModule(
			ConfigurationRoute.RouteId,
			ESRConditionedTransitModuleV2::CryogenicHold,
			ConfigurationRoutes)
			&& ConfigurationRoute.ConditionedTransitModule == ESRConditionedTransitModuleV2::CryogenicHold);
	TestTrue(TEXT("Leaving the Conditioned profile clears the Module"),
		FSRSpaceLogisticsRouteRegistry::SetHubRouteProfile(
			ConfigurationRoute.RouteId,
			ESRSpaceLogisticsRouteProfileV2::CardCourier,
			ConfigurationRoutes)
			&& ConfigurationRoute.ConditionedTransitModule == ESRConditionedTransitModuleV2::None);

	FSRSpaceLogisticsSaveData Source;
	FSRSpaceLogisticsHubRouteSaveData& Route = Source.HubRoutes.AddDefaulted_GetRef();
	Route.RouteId = FName(TEXT("ConditionedSaveRoute"));
	Route.RouteProfile = ESRSpaceLogisticsRouteProfileV2::ConditionedHold;
	Route.ConditionedTransitModule = ESRConditionedTransitModuleV2::GroundingHold;
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
	TestTrue(TEXT("Schema five persists the concrete Route Module"),
		Loaded.Version == FSRSpaceLogisticsSaveData::ConditioningDwellVersion
			&& Loaded.HubRoutes.Num() == 1
			&& Loaded.HubRoutes[0].ConditionedTransitModule == ESRConditionedTransitModuleV2::GroundingHold);
	FSRSpaceLogisticsSaveData PreviousSchema;
	PreviousSchema.Version = FSRSpaceLogisticsSaveData::ConditionedTransitVersion;
	TestTrue(TEXT("Schema four remains accepted and defaults new dwell fields to zero"),
		PreviousSchema.IsSupportedVersion()
			&& FMath::IsNearlyZero(FSRSpaceLogisticsHubRouteSaveData().ConditioningDurationSeconds)
			&& FMath::IsNearlyZero(FSRSpaceLogisticsHubRouteSaveData().ConditioningProgressSeconds));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRConditionedTransitDwellTest,
	"StarRovers.ResourceSystem.Phase15.ConditionedTransit.ConditioningDwell",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRConditionedTransitDwellTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::ConditionedTransitV2Tests;
	FSRResourceInstance Metal = MakeCard(ESRResourceContentPresetV2::HeliosIron, TEXT("Cinder"));
	const double SeedEnergy = Metal.SeedEnergySnapshot;
	Metal.CurrentEnergy = SeedEnergy + 40.0;
	const FSRConditioningDwellResultV2 Evaluation = FSRConditionedTransitV2::EvaluateConditioningDwell(
		ESRSpaceLogisticsRouteProfileV2::ConditionedHold,
		ESRConditionedTransitModuleV2::CryogenicHold,
		Metal,
		40.0);
	TestTrue(TEXT("Cryogenic dwell uses the shared Refinement Resistance curve"),
		Evaluation.bRequired
			&& Evaluation.RefinementResistance.bSeedEnergyResolved
			&& FMath::IsNearlyEqual(Evaluation.RefinementResistance.RefinementEnergy, 40.0)
			&& FMath::IsNearlyEqual(Evaluation.RefinementResistance.CycleMultiplier, 2.0)
			&& FMath::IsNearlyEqual(Evaluation.RefinementResistance.EffectiveProcessSeconds, 12.0f));

	FSRSpaceLogisticsHubRoute Route;
	Route.RouteId = FName(TEXT("ConditioningDwellRoute"));
	Route.RouteProfile = ESRSpaceLogisticsRouteProfileV2::ConditionedHold;
	Route.ConditionedTransitModule = ESRConditionedTransitModuleV2::CryogenicHold;
	Route.Phase = ESRSpaceLogisticsHubRoutePhase::TravelingToDestination;
	Route.Cargo = Metal;
	const double EnergyBeforeDwell = Route.Cargo.CurrentEnergy;
	TestTrue(TEXT("Arrival starts an immutable destination-side dwell"),
		FSRConditionedTransitV2::TryBeginConditioningDwell(
			Route,
			ESRSpaceLogisticsHubRouteDockSide::Destination,
			true,
			40.0)
			&& Route.Phase == ESRSpaceLogisticsHubRoutePhase::ConditioningAtDestination
			&& Route.CurrentDockSide == ESRSpaceLogisticsHubRouteDockSide::Destination
			&& FMath::IsNearlyEqual(Route.ConditioningDurationSeconds, 12.0f)
			&& FMath::IsNearlyZero(Route.ConditioningProgressSeconds));
	TestFalse(TEXT("Dwell does not finish early"),
		FSRConditionedTransitV2::AdvanceConditioningDwell(Route, 11.5f));
	TestTrue(TEXT("Waiting alone never applies Energy or a hidden process"),
		FMath::IsNearlyEqual(Route.Cargo.CurrentEnergy, EnergyBeforeDwell)
			&& Route.Cargo.ProcessingMemory.ProcessCount == Metal.ProcessingMemory.ProcessCount);

	FSRSpaceLogisticsSaveData Source;
	FSRSpaceLogisticsHubRouteSaveData& SavedRoute = Source.HubRoutes.AddDefaulted_GetRef();
	SavedRoute.RouteId = Route.RouteId;
	SavedRoute.RouteProfile = Route.RouteProfile;
	SavedRoute.ConditionedTransitModule = Route.ConditionedTransitModule;
	SavedRoute.Phase = Route.Phase;
	SavedRoute.CurrentDockSide = Route.CurrentDockSide;
	SavedRoute.ConditioningDurationSeconds = Route.ConditioningDurationSeconds;
	SavedRoute.ConditioningProgressSeconds = Route.ConditioningProgressSeconds;
	SavedRoute.Cargo = Route.Cargo;
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
	TestTrue(TEXT("Schema five resumes a captured mid-dwell cycle"),
		Loaded.Version == FSRSpaceLogisticsSaveData::ConditioningDwellVersion
			&& Loaded.HubRoutes.Num() == 1
			&& Loaded.HubRoutes[0].Phase == ESRSpaceLogisticsHubRoutePhase::ConditioningAtDestination
			&& FMath::IsNearlyEqual(Loaded.HubRoutes[0].ConditioningDurationSeconds, 12.0f)
			&& FMath::IsNearlyEqual(Loaded.HubRoutes[0].ConditioningProgressSeconds, 11.5f));
	TestTrue(TEXT("Only the remaining dwell completes the cycle"),
		FSRConditionedTransitV2::AdvanceConditioningDwell(Route, 0.5f));
	FSRConditionedTransitV2::ClearConditioningDwell(Route);
	TestTrue(TEXT("Completion cleanup removes the per-leg snapshot"),
		FMath::IsNearlyZero(Route.ConditioningDurationSeconds)
			&& FMath::IsNearlyZero(Route.ConditioningProgressSeconds));

	FSRSpaceLogisticsHubRoute NeutralRoute;
	NeutralRoute.RouteProfile = ESRSpaceLogisticsRouteProfileV2::NeutralShuttle;
	NeutralRoute.ConditionedTransitModule = ESRConditionedTransitModuleV2::None;
	NeutralRoute.Cargo = Metal;
	TestFalse(TEXT("Neutral transport keeps its immediate state-neutral arrival"),
		FSRConditionedTransitV2::TryBeginConditioningDwell(
			NeutralRoute,
			ESRSpaceLogisticsHubRouteDockSide::Destination,
			true,
			40.0));
	return true;
}

#endif
