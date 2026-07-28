#include "Automation/SRResourceSystemValidationActor.h"

#include "Automation/SRFacilityNetworkComponent.h"
#include "Automation/SRFacilityResourceV2Processor.h"
#include "Automation/SROperationalCapacity.h"
#include "Automation/SROperationalEconomyProcessor.h"
#include "Automation/SRRefinementResistanceV2.h"
#include "Automation/SRResourceDataAsset.h"
#include "Automation/SRResourceInstanceOperations.h"
#include "Automation/SRResourceProcessingKernel.h"
#include "Automation/SRResourceSystemContent.h"
#include "Automation/SRResourceV2AuthoredContent.h"
#include "Automation/SRStellarFuelFabricator.h"
#include "Camera/SRPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Logistics/SRConditionedTransitV2.h"
#include "Logistics/SRFleetCapacityV2.h"
#include "Logistics/SRSpaceLogisticsTypes.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "Simulation/SRAugmentPackageContent.h"
#include "Simulation/SRResourceV2RunSaveSubsystem.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "UObject/UObjectIterator.h"
#include "SRFacilityProcessingStepExecutor.h"

DEFINE_LOG_CATEGORY_STATIC(LogSRResourceSystemValidation, Log, All);

namespace StarRovers::ResourceSystemValidation
{
	void AddCheck(
		FSRResourceSystemValidationReport& Report,
		bool bCondition,
		const FString& FailureMessage)
	{
		++Report.CheckCount;
		if (!bCondition)
		{
			++Report.FailureCount;
			Report.FailureMessages.Add(FailureMessage);
		}
	}

	UWorld* FindRuntimeWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}

		UWorld* GameWorld = nullptr;
		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			UWorld* CandidateWorld = WorldContext.World();
			if (!IsValid(CandidateWorld))
			{
				continue;
			}

			if (WorldContext.WorldType == EWorldType::PIE)
			{
				return CandidateWorld;
			}
			if (WorldContext.WorldType == EWorldType::Game)
			{
				GameWorld = CandidateWorld;
			}
		}

		return GameWorld;
	}

	void RunConsoleValidation()
	{
		FSRResourceSystemValidationReport Report;
		ASRResourceSystemValidationActor::ValidateWorld(
			FindRuntimeWorld(),
			true,
			true,
			Report);
		ASRResourceSystemValidationActor::LogReport(Report);
	}

	FAutoConsoleCommand ValidateBaselineCommand(
		TEXT("sr.ResourceSystem.ValidateBaseline"),
		TEXT("Validates the current Resource System baseline in the active PIE or game world."),
		FConsoleCommandDelegate::CreateStatic(&RunConsoleValidation));
}

ASRResourceSystemValidationActor::ASRResourceSystemValidationActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);
}

void ASRResourceSystemValidationActor::BeginPlay()
{
	Super::BeginPlay();

	if (bRunOnBeginPlay)
	{
		RunValidation();
	}
}

bool ASRResourceSystemValidationActor::RunValidation()
{
	ValidateWorld(
		GetWorld(),
		bRequirePrimaryStar,
		bRequireFacilityNetwork,
		LastValidationReport);
	LogReport(LastValidationReport);
	return LastValidationReport.bPassed;
}

FSRResourceSystemValidationReport ASRResourceSystemValidationActor::GetLastValidationReport() const
{
	return LastValidationReport;
}

bool ASRResourceSystemValidationActor::ValidateWorld(
	UWorld* World,
	bool bInRequirePrimaryStar,
	bool bInRequireFacilityNetwork,
	FSRResourceSystemValidationReport& OutReport)
{
	using namespace StarRovers::ResourceSystemValidation;

	OutReport = FSRResourceSystemValidationReport();
	AddCheck(OutReport, IsValid(World), TEXT("No active PIE or game world was found."));
	if (!IsValid(World))
	{
		OutReport.Summary = TEXT("Resource System baseline validation failed: no runtime world.");
		return false;
	}

	AddCheck(OutReport, World->IsGameWorld(), TEXT("Validation must run in PIE or a game world."));

	const USRSimulationSettings* SimulationSettings = GetDefault<USRSimulationSettings>();
	AddCheck(OutReport, IsValid(SimulationSettings), TEXT("SRSimulationSettings could not be resolved."));
	if (IsValid(SimulationSettings))
	{
		OutReport.ActiveRuleset = SimulationSettings->ResourceRulesetVersion;
		const UEnum* RulesetEnum = StaticEnum<ESRResourceRulesetVersion>();
		AddCheck(
			OutReport,
			RulesetEnum && RulesetEnum->IsValidEnumValue(static_cast<int64>(OutReport.ActiveRuleset)),
			TEXT("The configured Resource Ruleset Version is invalid."));
	}

	const FSRSpaceLogisticsSaveData DefaultSaveData;
	OutReport.LogisticsSaveVersion = DefaultSaveData.Version;
	AddCheck(
		OutReport,
		DefaultSaveData.IsSupportedVersion()
			&& OutReport.LogisticsSaveVersion == FSRSpaceLogisticsSaveData::CurrentVersion,
		TEXT("Space Logistics must emit the current supported save schema."));

	const FSRResourceV2RunSaveData DefaultRunSaveData;
	const FSRStructureInstanceManagerSaveData DefaultStructureSaveData;
	OutReport.RunSaveVersion = DefaultRunSaveData.Version;
	OutReport.StructureSaveVersion = DefaultStructureSaveData.Version;
	AddCheck(
		OutReport,
		DefaultRunSaveData.IsSupportedVersion()
			&& OutReport.RunSaveVersion == FSRResourceV2RunSaveData::CurrentVersion
			&& DefaultStructureSaveData.IsSupportedVersion()
			&& OutReport.StructureSaveVersion
				== FSRStructureInstanceManagerSaveData::CurrentVersion,
		TEXT("Run and Structure persistence must emit their current finite-resource schemas."));

	const FSRResourceInstance DefaultResourceInstance;
	AddCheck(
		OutReport,
		DefaultResourceInstance.ResourceSchemaVersion == StarRovers::Resources::CurrentResourceSchemaVersion,
		TEXT("A newly constructed resource must use the current Resource schema."));
	AddCheck(
		OutReport,
		DefaultResourceInstance.StackCount == 1,
		TEXT("A default resource instance must represent one stack item."));

	TArray<FSRProcessTagDefinitionV2> ContentProcessTags;
	TArray<FSRReferenceResourceDefinitionV2> ContentResources;
	TArray<FSRUtilityResourceDefinitionV2> ContentUtilityResources;
	TArray<FSRFacilityContentDefinitionV2> ContentFacilities;
	FSRResourceSystemContent::GetAllProcessTagDefinitions(ContentProcessTags);
	FSRResourceSystemContent::GetAllReferenceResourceDefinitions(ContentResources);
	FSRResourceSystemContent::GetAllUtilityResourceDefinitions(ContentUtilityResources);
	FSRResourceSystemContent::GetAllFacilityDefinitions(ContentFacilities);
	AddCheck(
		OutReport,
		ContentProcessTags.Num() == 5
			&& ContentResources.Num() == 5
			&& ContentUtilityResources.Num() == 3
			&& ContentFacilities.Num() == 21,
		TEXT("Resource V2 content catalog is incomplete."));

	FSRResourceV2AuthoredContentValidation AuthoredContentValidation;
	const bool bAuthoredContentValid =
		FSRResourceV2AuthoredContent::ValidateAuthoredContent(AuthoredContentValidation);
	OutReport.AuthoredResourceV2ResourceCount = AuthoredContentValidation.ResourceAssetCount;
	OutReport.AuthoredResourceV2FacilityCount = AuthoredContentValidation.FacilityAssetCount;
	OutReport.AuthoredResourceV2StructureCount = AuthoredContentValidation.StructureAssetCount;
	OutReport.AuthoredResourceV2DepositCount = AuthoredContentValidation.DepositAssetCount;
	AddCheck(
		OutReport,
		bAuthoredContentValid,
		AuthoredContentValidation.Errors.IsEmpty()
			? TEXT("Resource V2 authored content is incomplete.")
			: FString::Join(AuthoredContentValidation.Errors, TEXT(" | ")));

	if (OutReport.ActiveRuleset == ESRResourceRulesetVersion::ResourceV2)
	{
		const ASRPlayerController* PlayerController = Cast<ASRPlayerController>(
			World->GetFirstPlayerController());
		TArray<USRStructureDataAsset*> BuildableStructures;
		if (IsValid(PlayerController))
		{
			PlayerController->GetBuildableStructureDataAssets(BuildableStructures);
		}
		bool bContainsLegacyProcessingStructure = false;
		bool bHasBuildableMiner = false;
		TSet<FName> BuildableResourceV2StructureIds;
		for (const USRStructureDataAsset* StructureDataAsset : BuildableStructures)
		{
			bContainsLegacyProcessingStructure |=
				FSRResourceV2AuthoredContent::IsLegacyProcessingStructure(StructureDataAsset);
			if (FSRResourceV2AuthoredContent::IsResourceV2FacilityStructure(StructureDataAsset))
			{
				BuildableResourceV2StructureIds.Add(StructureDataAsset->BuildData().StructureId);
			}
			if (IsValid(StructureDataAsset))
			{
				const USRFacilityDataAsset* FacilityDataAsset =
					StructureDataAsset->BuildData().FacilityDataAsset.Get();
				bHasBuildableMiner |= IsValid(FacilityDataAsset)
					&& FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Mine;
			}
		}
		OutReport.BuildableResourceV2StructureCount = BuildableResourceV2StructureIds.Num();
		AddCheck(
			OutReport,
			IsValid(PlayerController)
				&& OutReport.BuildableResourceV2StructureCount == AuthoredContentValidation.StructureAssetCount
				&& bHasBuildableMiner
				&& !bContainsLegacyProcessingStructure,
			TEXT("The active Resource V2 PlayerController build catalog must expose every authored V2 facility exactly once, retain a Miner, and suppress Legacy processors/synthesizers."));
	}

	FSRFacilityContentDefinitionV2 AnnealingDefinition;
	FSRResourceInstance RefinementSmokeInput;
	const bool bRefinementSmokeInputBuilt = FSRResourceSystemContent::MakeReferenceResourceInstance(
		ESRResourceContentPresetV2::HeliosIron,
		FName(TEXT("Cinder")),
		RefinementSmokeInput);
	RefinementSmokeInput.CurrentEnergy = 17.0;
	RefinementSmokeInput.ActiveFamilyStateFlags =
		StarRovers::Resources::GetFamilyStateBit(ESRResourceFamilyState::Tempered)
		| StarRovers::Resources::GetFamilyStateBit(ESRResourceFamilyState::Fatigued);
	RefinementSmokeInput.ProcessingMemory.GeneralProcessesSinceReset = 4;
	const FSRRefinementResistanceResultV2 RefinementSmokeResult =
		FSRRefinementResistanceV2::Evaluate(RefinementSmokeInput, 4.0f, 40.0);
	FSRResourceProcessSpec AnnealSmokeSpec;
	AnnealSmokeSpec.ProcessArchetype = FName(TEXT("Anneal"));
	AnnealSmokeSpec.Temperature = ESRResourceProcessTemperatureState::Normal;
	AnnealSmokeSpec.FamilyAction = ESRResourceFamilyAction::Anneal;
	AnnealSmokeSpec.FacilityEnergyDelta = 0.0;
	const FSRResourceProcessResult AnnealSmokeResult =
		FSRResourceProcessingKernel::Evaluate(RefinementSmokeInput, AnnealSmokeSpec);
	AddCheck(
		OutReport,
		bRefinementSmokeInputBuilt
			&& FSRResourceSystemContent::TryGetFacilityDefinition(
				ESRFacilityContentPresetV2::AnnealingChamber,
				AnnealingDefinition)
			&& AnnealingDefinition.FamilyAction == ESRResourceFamilyAction::Anneal
			&& AnnealingDefinition.OperationalLoad == 2
			&& FMath::IsNearlyEqual(AnnealingDefinition.CycleSeconds, 6.0f)
			&& RefinementSmokeResult.bApplied
			&& FMath::IsNearlyEqual(RefinementSmokeResult.EffectiveProcessSeconds, 5.2f)
			&& AnnealSmokeResult.IsSuccess()
			&& FMath::IsNearlyEqual(AnnealSmokeResult.OutputEnergy, 17.0)
			&& AnnealSmokeResult.OutputResource.ActiveFamilyStateFlags == 0
			&& AnnealSmokeResult.OutputResource.ProcessingMemory.GeneralProcessesSinceReset == 0,
		TEXT("Refinement Resistance or the explicit Metal Annealing recovery contract failed its deterministic smoke check."));

	USRFacilityDataAsset* SupplyFabricatorDataAsset = NewObject<USRFacilityDataAsset>(GetTransientPackage());
	const bool bSupplyPresetApplied = FSRResourceSystemContent::ApplyFacilityPreset(
		*SupplyFabricatorDataAsset,
		ESRFacilityContentPresetV2::SupplyFabricator);
	FSRFacilityInstance SupplyFabricatorFacility;
	SupplyFabricatorFacility.FacilityDataAsset = SupplyFabricatorDataAsset;
	FSRResourceInstance CommonOre;
	FSRResourceInstance Biomass;
	const bool bUtilityInputsBuilt = FSRResourceSystemContent::MakeReferenceResourceInstance(
		ESRResourceContentPresetV2::CommonOre,
		NAME_None,
		CommonOre)
		&& FSRResourceSystemContent::MakeReferenceResourceInstance(
			ESRResourceContentPresetV2::BiomassFeedstock,
			NAME_None,
			Biomass);
	const FSROperationalEconomyEvaluationV2 SupplyEvaluation =
		bSupplyPresetApplied && bUtilityInputsBuilt
			? FSROperationalEconomyProcessor::Evaluate(
				SupplyFabricatorFacility,
				TArray<FSRResourceInstance>({CommonOre, Biomass}))
			: FSROperationalEconomyEvaluationV2();
	AddCheck(
		OutReport,
		SupplyEvaluation.IsSuccess()
			&& SupplyEvaluation.OutputResources.Num() == 1
			&& SupplyEvaluation.OutputResources[0].ResourceClass == ESRResourceClass::Utility
			&& SupplyEvaluation.OutputResources[0].ResourceId
				== FSRResourceSystemContent::GetUtilityResourceId(ESRResourceContentPresetV2::IndustrialSupply)
			&& SupplyEvaluation.OutputResources[0].StackCount == 2,
		TEXT("Operational Economy Supply Fabricator failed its Utility resource recipe smoke check."));

	FSRFacilityNetworkRuntimeState CapacitySmokeState;
	auto AddCapacitySmokeFacility = [&CapacitySmokeState](
		const TCHAR* OccupantId,
		int32 Load,
		ESROperationalPriorityV2 Priority)
	{
		USRFacilityDataAsset* Definition = NewObject<USRFacilityDataAsset>(GetTransientPackage());
		Definition->FacilityKind = ESRFacilityKind::Standard;
		Definition->OperationKind = ESRFacilityOperationKind::Process;
		Definition->BaseProcessSeconds = 100.0f;
		Definition->OperationalLoad = Load;
		FSRFacilityInstance Facility;
		Facility.OccupantId = FName(OccupantId);
		Facility.FacilityDataAsset = Definition;
		Facility.bProcessEnabled = true;
		Facility.bProcessing = true;
		Facility.OperationalPriority = Priority;
		FSRResourceInstance ProcessingResource;
		ProcessingResource.ResourceId = FName(TEXT("CapacitySmokeResource"));
		ProcessingResource.StackCount = 1;
		ProcessingResource.RemainingProcessLimit = 1;
		Facility.ProcessingInventory.Add(ProcessingResource);
		CapacitySmokeState.FacilityInstancesByOccupantId.Add(Facility.OccupantId, Facility);
	};
	AddCapacitySmokeFacility(TEXT("CriticalSmoke"), 20, ESROperationalPriorityV2::Critical);
	AddCapacitySmokeFacility(TEXT("NormalSmoke"), 30, ESROperationalPriorityV2::Normal);
	AddCapacitySmokeFacility(TEXT("BackgroundSmoke"), 20, ESROperationalPriorityV2::Background);
	const FSROperationalCapacityReportV2 CapacitySmokeReport =
		FSROperationalCapacity::BuildReport(CapacitySmokeState, true, 45, 18);
	AddCheck(
		OutReport,
		CapacitySmokeReport.TotalDemand == 70
			&& FMath::IsNearlyEqual(CapacitySmokeReport.Critical.SpeedFactor, 1.0f)
			&& FMath::IsNearlyEqual(CapacitySmokeReport.Normal.SpeedFactor, 25.0f / 30.0f)
			&& FMath::IsNearlyZero(CapacitySmokeReport.Background.SpeedFactor),
		TEXT("Operational Capacity priority allocation failed its non-destructive throttle smoke check."));

	FSRSpaceLogisticsHubEndpoint FleetSmokeHub;
	FleetSmokeHub.BodyActor = World->GetWorldSettings();
	FleetSmokeHub.HubOccupantId = FName(TEXT("FleetSmokeHub"));
	FSRSpaceLogisticsHubEndpoint FleetSmokeDestination = FleetSmokeHub;
	FleetSmokeDestination.HubOccupantId = FName(TEXT("FleetSmokeDestination"));
	TArray<FSRSpaceLogisticsHubRoute> FleetSmokeRoutes;
	for (int32 RouteIndex = 0; RouteIndex < 4; ++RouteIndex)
	{
		FSRSpaceLogisticsHubRoute& Route = FleetSmokeRoutes.AddDefaulted_GetRef();
		Route.RouteId = FName(*FString::Printf(TEXT("FleetSmokeTravel_%d"), RouteIndex));
		Route.SourceHub = FleetSmokeHub;
		Route.DestinationHub = FleetSmokeDestination;
		Route.RouteProfile = ESRSpaceLogisticsRouteProfileV2::NeutralShuttle;
		Route.Phase = ESRSpaceLogisticsHubRoutePhase::TravelingToDestination;
	}
	FSRSpaceLogisticsHubRoute& QueuedFleetRoute = FleetSmokeRoutes.AddDefaulted_GetRef();
	QueuedFleetRoute.RouteId = FName(TEXT("FleetSmokeQueued"));
	QueuedFleetRoute.SourceHub = FleetSmokeHub;
	QueuedFleetRoute.DestinationHub = FleetSmokeDestination;
	QueuedFleetRoute.RouteProfile = ESRSpaceLogisticsRouteProfileV2::BulkRawHold;
	QueuedFleetRoute.Phase = ESRSpaceLogisticsHubRoutePhase::WaitingForFleetCapacity;
	QueuedFleetRoute.CurrentDockSide = ESRSpaceLogisticsHubRouteDockSide::Source;
	QueuedFleetRoute.FleetDepartureQueueSequence = 1;
	FSRFleetCapacityV2::RefreshQueuePositions(FleetSmokeRoutes);
	const FSRFleetCapacityReportV2 BaseFleetSmokeReport = FSRFleetCapacityV2::BuildReport(
		FleetSmokeHub,
		FleetSmokeRoutes,
		true,
		0,
		8,
		8);
	const FSRFleetCapacityReportV2 BerthFleetSmokeReport = FSRFleetCapacityV2::BuildReport(
		FleetSmokeHub,
		FleetSmokeRoutes,
		true,
		1,
		8,
		8);
	AddCheck(
		OutReport,
		BaseFleetSmokeReport.ReservedLoad == 8
			&& BaseFleetSmokeReport.QueuedDepartureCount == 1
			&& QueuedFleetRoute.FleetQueuePosition == 1
			&& !FSRFleetCapacityV2::CanGrantDeparture(
				QueuedFleetRoute,
				FleetSmokeHub,
				FleetSmokeRoutes,
				BaseFleetSmokeReport)
			&& BerthFleetSmokeReport.TotalCapacity == 16
			&& FSRFleetCapacityV2::CanGrantDeparture(
				QueuedFleetRoute,
				FleetSmokeHub,
				FleetSmokeRoutes,
				BerthFleetSmokeReport),
		TEXT("Fleet Capacity reservation, fair queue, or supplied Fleet Berth expansion failed its smoke check."));

	FString AugmentCatalogFailure;
	FSRAugmentBuildContextV2 AugmentContext;
	FSRAugmentPackageContentV2::GetTechnologyFacilityContentIds(
		AugmentContext.AvailableFacilityContentIds);
	for (const FSRReferenceResourceDefinitionV2& ResourceDefinition : ContentResources)
	{
		AugmentContext.AccessibleFamilies.AddUnique(ResourceDefinition.Family);
		AugmentContext.AccessibleSpectra.AddUnique(ResourceDefinition.Spectrum);
		AugmentContext.AccessibleGrades.AddUnique(ResourceDefinition.Grade);
	}
	AugmentContext.HubEndpointCount = 3;
	FSRAugmentOfferGenerationRulesV2 AugmentOfferRules;
	AugmentOfferRules.RandomSeed = 9137;
	TArray<FSRAugmentPackageOfferV2> AugmentOffers;
	FSRAugmentPackageContentV2::GenerateOffer(
		AugmentContext,
		AugmentOfferRules,
		AugmentOffers);
	AddCheck(
		OutReport,
		FSRAugmentPackageContentV2::ValidateCatalog(AugmentCatalogFailure)
			&& AugmentOffers.Num() == 3
			&& AugmentOffers[0].OfferRole == ESRAugmentOfferRoleV2::Immediate
			&& FSRAugmentPackageContentV2::IsProcessTagRecipeUnlocked(
				FSRResourceSystemContent::GetProcessTagId(ESRProcessTagContentV2::Crosslink),
				TArray<FName>())
			&& !FSRAugmentPackageContentV2::IsProcessTagRecipeUnlocked(
				FSRResourceSystemContent::GetProcessTagId(ESRProcessTagContentV2::Overtone),
				TArray<FName>())
			&& !FSRAugmentPackageContentV2::IsLogisticsModuleUnlocked(
				FName(TEXT("CryogenicHold")),
				TArray<FName>())
			&& FSRAugmentPackageContentV2::IsLogisticsModuleUnlocked(
				FName(TEXT("CryogenicHold")),
				TArray<FName>({ FName(TEXT("DeepSpaceTempering")) })),
		TEXT("Resource V2 Augment Package catalog, safe Offer, or concrete Recipe access policy failed."));

	FSRResourceInstance KernelSmokeInput;
	KernelSmokeInput.ResourceId = FName(TEXT("PIEKernelSmokeMetal"));
	KernelSmokeInput.ResourceClass = ESRResourceClass::Card;
	KernelSmokeInput.Family = ESRResourceFamily::Metal;
	KernelSmokeInput.CurrentEnergy = 12.0;
	KernelSmokeInput.EnergyValue = 12.0;
	KernelSmokeInput.ProcessingMemory.LastProcessArchetype = FName(TEXT("Forge"));
	KernelSmokeInput.ProcessingMemory.LastTemperature = ESRResourceProcessTemperatureState::Hot;
	KernelSmokeInput.ProcessingMemory.ConsecutiveSameArchetypeCount = 1;
	FSRResourceProcessSpec KernelSmokeSpec;
	KernelSmokeSpec.ProcessArchetype = FName(TEXT("CryoPress"));
	KernelSmokeSpec.Temperature = ESRResourceProcessTemperatureState::Cold;
	KernelSmokeSpec.FacilityEnergyDelta = 3.0;
	const FSRResourceProcessResult KernelSmokeResult = FSRResourceProcessingKernel::Evaluate(
		KernelSmokeInput,
		KernelSmokeSpec);
	AddCheck(
		OutReport,
		KernelSmokeResult.IsSuccess()
			&& FMath::IsNearlyEqual(KernelSmokeResult.OutputEnergy, 20.0)
			&& (KernelSmokeResult.OutputResource.ActiveFamilyStateFlags
				& StarRovers::Resources::GetFamilyStateBit(ESRResourceFamilyState::Tempered)) != 0,
		TEXT("Resource V2 processing Kernel failed its Hot-to-Cold Tempered runtime smoke check."));

	const FSRConditionedTransitResultV2 NeutralTransitSmoke = FSRConditionedTransitV2::EvaluateArrival(
		KernelSmokeInput,
		ESRSpaceLogisticsRouteProfileV2::NeutralShuttle,
		ESRConditionedTransitModuleV2::None,
		FName(TEXT("Cinder")),
		FName(TEXT("Prism")),
		true);
	const FSRConditionedTransitResultV2 CryogenicTransitSmoke = FSRConditionedTransitV2::EvaluateArrival(
		KernelSmokeInput,
		ESRSpaceLogisticsRouteProfileV2::ConditionedHold,
		ESRConditionedTransitModuleV2::CryogenicHold,
		FName(TEXT("Cinder")),
		FName(TEXT("Prism")),
		true);
	AddCheck(
		OutReport,
		NeutralTransitSmoke.Outcome == ESRConditionedTransitOutcomeV2::StateNeutral
			&& NeutralTransitSmoke.OutputResource.LogisticsMetadata.TransitCount == 1
			&& NeutralTransitSmoke.OutputResource.ProcessingMemory.ProcessCount == 0
			&& FMath::IsNearlyEqual(NeutralTransitSmoke.OutputResource.CurrentEnergy, 12.0)
			&& CryogenicTransitSmoke.bProcessApplied
			&& CryogenicTransitSmoke.OutputResource.LogisticsMetadata.TransitCount == 1
			&& CryogenicTransitSmoke.OutputResource.ProcessingMemory.ProcessCount == 1
			&& CryogenicTransitSmoke.OutputResource.ProcessingMemory.LastProcessArchetype
				== FName(TEXT("CryogenicTransit"))
			&& FMath::IsNearlyEqual(CryogenicTransitSmoke.OutputResource.CurrentEnergy, 20.0),
		TEXT("Conditioned Transit must preserve neutral transport and apply exactly one explicit Cryogenic arrival process."));

	USRFacilityDataAsset* ForgeDataAsset = NewObject<USRFacilityDataAsset>(GetTransientPackage());
	FSRResourceSystemContent::ApplyFacilityPreset(
		*ForgeDataAsset,
		ESRFacilityContentPresetV2::InductionForge);
	FSRFacilityInstance ForgeFacility;
	ForgeFacility.FacilityDataAsset = ForgeDataAsset;
	ForgeFacility.TemperatureState = ESRFacilityTemperatureState::Hot;
	FSRResourceInstance FacilitySmokeInput;
	FacilitySmokeInput.ResourceId = FName(TEXT("PIEFacilitySmokeMetal"));
	FacilitySmokeInput.ResourceClass = ESRResourceClass::Card;
	FacilitySmokeInput.Family = ESRResourceFamily::Metal;
	FacilitySmokeInput.CurrentEnergy = 5.0;
	FacilitySmokeInput.EnergyValue = 5.0;
	FacilitySmokeInput.Spectrum = ESRResourceSpectrum::Red;
	FacilitySmokeInput.Grade = 2;
	FacilitySmokeInput.RemainingProcessLimit = 0;
	const FSRFacilityResourceV2Evaluation ForgeEvaluation = FSRFacilityResourceV2Processor::Evaluate(
		ForgeFacility,
		FacilitySmokeInput);
	AddCheck(
		OutReport,
		ForgeEvaluation.IsSuccess()
			&& FMath::IsNearlyEqual(ForgeEvaluation.ResourceProcessResult.OutputEnergy, 9.0)
			&& ForgeEvaluation.ResourceProcessResult.OutputResource.RemainingProcessLimit == 0
			&& ForgeEvaluation.ResourceProcessResult.OutputResource.ProcessingMemory.LastTemperature
				== ESRResourceProcessTemperatureState::Hot,
		TEXT("Resource V2 Induction Forge facility contract failed its Hot additive process smoke check."));

	USRFacilityDataAsset* PressDataAsset = NewObject<USRFacilityDataAsset>(GetTransientPackage());
	FSRResourceSystemContent::ApplyFacilityPreset(
		*PressDataAsset,
		ESRFacilityContentPresetV2::CryoPress);
	FSRFacilityInstance PressFacility;
	PressFacility.FacilityDataAsset = PressDataAsset;
	PressFacility.TemperatureState = ESRFacilityTemperatureState::Cold;
	const FSRFacilityResourceV2Evaluation PressEvaluation = FSRFacilityResourceV2Processor::Evaluate(
		PressFacility,
		ForgeEvaluation.ResourceProcessResult.OutputResource);
	const FString PressPreview = FSRFacilityResourceV2Processor::BuildPreviewSummary(PressEvaluation);
	AddCheck(
		OutReport,
		PressEvaluation.IsSuccess()
			&& FMath::IsNearlyEqual(PressEvaluation.ResourceProcessResult.OutputEnergy, 17.0)
			&& (PressEvaluation.ResourceProcessResult.OutputResource.ActiveFamilyStateFlags
				& StarRovers::Resources::GetFamilyStateBit(ESRResourceFamilyState::Tempered)) != 0
			&& PressPreview.Contains(TEXT("Resource V2 Additive Process"))
			&& PressPreview.Contains(TEXT("Tempered")),
		TEXT("Resource V2 Cryo Press facility contract failed to activate and preview Tempered after the Hot Forge step."));

	TArray<FSRResourceInstance> ReferenceFuelCards;
	USRFacilityDataAsset* FabricatorDataAsset = NewObject<USRFacilityDataAsset>(GetTransientPackage());
	const FName ReferenceFabricatorBodyId(TEXT("Concord"));
	const bool bReferenceBatchBuilt = FSRResourceSystemContent::MakeReferenceStellarFuelBatch(
		ESRStellarFuelReferenceTopologyV2::DistributedConvergence,
		ReferenceFabricatorBodyId,
		ReferenceFuelCards);
	const bool bFabricatorPresetApplied = FSRResourceSystemContent::ApplyFacilityPreset(
		*FabricatorDataAsset,
		ESRFacilityContentPresetV2::StellarFuelFabricator);
	FSRFacilityInstance FabricatorFacility;
	FabricatorFacility.FacilityDataAsset = FabricatorDataAsset;
	const FSRStellarFuelFabricationResultV2 FabricationResult =
		bReferenceBatchBuilt && bFabricatorPresetApplied
			? FSRStellarFuelFabricator::Evaluate(
				FabricatorFacility,
				ReferenceFuelCards,
				ReferenceFabricatorBodyId)
			: FSRStellarFuelFabricationResultV2();
	const FString FabricationPreview = FSRStellarFuelFabricator::BuildPreviewSummary(FabricationResult);
	AddCheck(
		OutReport,
		FabricationResult.IsSuccess()
			&& FabricationResult.Hand == ESRStellarFuelHandV2::FullHouse
			&& FMath::IsNearlyEqual(FabricationResult.InputEnergySum, 176.0)
			&& FMath::IsNearlyEqual(FabricationResult.FormulaB, 236.0)
			&& FMath::IsNearlyEqual(FabricationResult.FormulaC, 5.0)
			&& FMath::IsNearlyEqual(FabricationResult.FuelEnergy, 1180.0)
			&& FabricationResult.OutputFuel.ResourceClass == ESRResourceClass::StellarFuel
			&& FabricationPreview.Contains(TEXT("Full House"))
			&& FabricationPreview.Contains(TEXT("Final")),
		TEXT("Resource V2 Stellar Fuel Fabricator failed the distributed Full House reference batch (A + B * C = 1180)."));

	bool bFacilityExecutorSmokePassed = false;
	bool bAugmentRecipeRuntimeGatePassed = false;
	if (USRSimulationSettings* MutableSettings = GetMutableDefault<USRSimulationSettings>())
	{
		TGuardValue<ESRResourceRulesetVersion> RulesetGuard(
			MutableSettings->ResourceRulesetVersion,
			ESRResourceRulesetVersion::ResourceV2);
		auto AddRuntimePorts = [](FSRFacilityInstance& Facility)
		{
			FSRFacilityPortInventory& InputPort = Facility.InputPortInventories.AddDefaulted_GetRef();
			InputPort.PortId = FName(TEXT("Input_0"));
			InputPort.PortKind = ESRFacilityPortKind::Input;
			InputPort.PortIndex = 0;
			InputPort.Capacity = 1;
			FSRFacilityPortInventory& OutputPort = Facility.OutputPortInventories.AddDefaulted_GetRef();
			OutputPort.PortId = FName(TEXT("Output_0"));
			OutputPort.PortKind = ESRFacilityPortKind::Output;
			OutputPort.PortIndex = 0;
			OutputPort.Capacity = 1;
		};

		USRFacilityDataAsset* TagDataAsset = NewObject<USRFacilityDataAsset>(GetTransientPackage());
		FSRResourceSystemContent::ApplyFacilityPreset(
			*TagDataAsset,
			ESRFacilityContentPresetV2::TagImprinter);
		FSRFacilityInstance RuntimeTagImprinter;
		RuntimeTagImprinter.FacilityDataAsset = TagDataAsset;
		RuntimeTagImprinter.TemperatureState = ESRFacilityTemperatureState::Normal;
		AddRuntimePorts(RuntimeTagImprinter);
		RuntimeTagImprinter.InputPortInventories[0].Inventory.Add(FacilitySmokeInput);

		const UActorComponent* RuntimeGateOwnerComponent = nullptr;
		for (TObjectIterator<USRFacilityNetworkComponent> It; It; ++It)
		{
			USRFacilityNetworkComponent* Candidate = *It;
			if (IsValid(Candidate) && Candidate->GetWorld() == World)
			{
				RuntimeGateOwnerComponent = Candidate;
				break;
			}
		}

		FSRFacilityInstance LockedRecipeImprinter = RuntimeTagImprinter;
		LockedRecipeImprinter.SelectedProcessTagRecipeId = FName(TEXT("UnknownRecipe"));
		FSRFacilityProcessingStartResult LockedRecipeStart;
		const bool bLockedRecipeBlockedBeforeConsume = IsValid(RuntimeGateOwnerComponent)
			&& !FSRFacilityProcessingStepExecutor::TryStartProcessing(
				RuntimeGateOwnerComponent,
				LockedRecipeImprinter,
				&LockedRecipeStart)
			&& LockedRecipeImprinter.InputPortInventories[0].Inventory.Num() == 1
			&& LockedRecipeImprinter.ProcessingInventory.IsEmpty();

		FSRFacilityInstance TechnologyRecipeImprinter = RuntimeTagImprinter;
		TechnologyRecipeImprinter.SelectedProcessTagRecipeId =
			FSRResourceSystemContent::GetProcessTagId(ESRProcessTagContentV2::Crosslink);
		FSRFacilityProcessingStartResult TechnologyRecipeStart;
		const bool bTechnologyRecipeStarted = IsValid(RuntimeGateOwnerComponent)
			&& FSRFacilityProcessingStepExecutor::TryStartProcessing(
				RuntimeGateOwnerComponent,
				TechnologyRecipeImprinter,
				&TechnologyRecipeStart)
			&& TechnologyRecipeImprinter.InputPortInventories[0].Inventory.IsEmpty()
			&& TechnologyRecipeImprinter.ProcessingInventory.Num() == 1;
		bAugmentRecipeRuntimeGatePassed =
			bLockedRecipeBlockedBeforeConsume && bTechnologyRecipeStarted;

		FSRFacilityProcessingStartResult TagStart;
		FSRFacilityProcessingCompletionResult TagCompletion;
		const bool bTagCompleted = FSRFacilityProcessingStepExecutor::TryStartProcessing(
			nullptr,
			RuntimeTagImprinter,
			&TagStart)
			&& FSRFacilityProcessingStepExecutor::TryCompleteProcessing(
				nullptr,
				RuntimeTagImprinter,
				&TagCompletion);

		FSRFacilityInstance RuntimeForge = ForgeFacility;
		AddRuntimePorts(RuntimeForge);
		if (bTagCompleted)
		{
			RuntimeForge.InputPortInventories[0].Inventory.Add(TagCompletion.PrimaryOutputResource);
		}
		FSRFacilityProcessingStartResult ForgeStart;
		FSRFacilityProcessingCompletionResult ForgeCompletion;
		const bool bForgeCompleted = bTagCompleted
			&& FSRFacilityProcessingStepExecutor::TryStartProcessing(
			nullptr,
			RuntimeForge,
			&ForgeStart)
			&& FSRFacilityProcessingStepExecutor::TryCompleteProcessing(
				nullptr,
				RuntimeForge,
				&ForgeCompletion);

		FSRFacilityInstance RuntimePress = PressFacility;
		AddRuntimePorts(RuntimePress);
		if (bForgeCompleted)
		{
			RuntimePress.InputPortInventories[0].Inventory.Add(ForgeCompletion.PrimaryOutputResource);
		}
		FSRFacilityProcessingStartResult PressStart;
		FSRFacilityProcessingCompletionResult PressCompletion;
		const bool bPressCompleted = bForgeCompleted
			&& FSRFacilityProcessingStepExecutor::TryStartProcessing(nullptr, RuntimePress, &PressStart)
			&& FSRFacilityProcessingStepExecutor::TryCompleteProcessing(nullptr, RuntimePress, &PressCompletion);
		bFacilityExecutorSmokePassed = bTagCompleted
			&& bPressCompleted
			&& TagCompletion.bUsedResourceV2
			&& ForgeCompletion.bUsedResourceV2
			&& PressCompletion.bUsedResourceV2
			&& FMath::IsNearlyEqual(TagCompletion.PrimaryOutputResource.CurrentEnergy, 5.0)
			&& TagCompletion.PrimaryOutputResource.ProcessingMemory.ProcessCount == 0
			&& FMath::IsNearlyEqual(PressCompletion.PrimaryOutputResource.CurrentEnergy, 22.0)
			&& PressCompletion.PrimaryOutputResource.ProcessTagSlot.Lifecycle
				== ESRResourceSlotLifecycle::Spent
			&& (PressCompletion.PrimaryOutputResource.ActiveFamilyStateFlags
				& StarRovers::Resources::GetFamilyStateBit(ESRResourceFamilyState::Tempered)) != 0;
	}
	AddCheck(
		OutReport,
		bFacilityExecutorSmokePassed,
		TEXT("Resource V2 Tag Imprinter and Metal facility line failed through the gated runtime inventory executor in PIE."));
	AddCheck(
		OutReport,
		bAugmentRecipeRuntimeGatePassed,
		TEXT("Resource V2 Augment Recipe gate did not block a locked recipe before consumption or allow the Technology recipe in PIE."));

	USRCelestialBodyRegistrySubsystem* CelestialRegistry = World->GetSubsystem<USRCelestialBodyRegistrySubsystem>();
	AddCheck(OutReport, IsValid(CelestialRegistry), TEXT("Celestial Body Registry Subsystem is unavailable."));
	if (IsValid(CelestialRegistry))
	{
		TArray<AActor*> CelestialBodies;
		CelestialRegistry->GetCelestialBodies(CelestialBodies);
		OutReport.CelestialBodyCount = CelestialBodies.Num();
		OutReport.bHasPrimaryStar = IsValid(CelestialRegistry->GetPrimaryStarActor());
	}

	if (bInRequirePrimaryStar)
	{
		AddCheck(OutReport, OutReport.bHasPrimaryStar, TEXT("The active world has no registered primary star."));
	}

	FSRResourceV2RunSaveData CapturedRunSave;
	FString RunSaveFailureReason;
	TArray<uint8> RunSavePayload;
	uint32 RunSaveChecksum = 0;
	FSRResourceV2RunSaveData DecodedRunSave;
	const USRResourceV2RunSaveSubsystem* RunSaveSubsystem =
		World->GetSubsystem<USRResourceV2RunSaveSubsystem>();
	const bool bRunSaveCaptured = IsValid(RunSaveSubsystem)
		&& RunSaveSubsystem->CaptureRunState(
			CapturedRunSave,
			RunSaveFailureReason);
	const bool bRunSaveEncoded = bRunSaveCaptured
		&& FSRResourceV2RunSaveCodec::Encode(
			CapturedRunSave,
			RunSavePayload,
			RunSaveChecksum,
			RunSaveFailureReason);
	const bool bRunSaveDecoded = bRunSaveEncoded
		&& FSRResourceV2RunSaveCodec::Decode(
			RunSavePayload,
			RunSaveChecksum,
			DecodedRunSave,
			RunSaveFailureReason);
	OutReport.RunSavePayloadBytes = RunSavePayload.Num();
	AddCheck(
		OutReport,
		bRunSaveDecoded
			&& DecodedRunSave.Version == FSRResourceV2RunSaveData::CurrentVersion
			&& DecodedRunSave.Bodies.Num() == OutReport.CelestialBodyCount
			&& DecodedRunSave.PrimaryStarBodyId
				== CapturedRunSave.PrimaryStarBodyId
			&& OutReport.RunSavePayloadBytes > 0,
		RunSaveFailureReason.IsEmpty()
			? TEXT("The active World failed its checked Run save capture/codec round trip.")
			: RunSaveFailureReason);

	if (OutReport.ActiveRuleset == ESRResourceRulesetVersion::ResourceV2)
	{
		TSet<FName> RuntimeDepositResourceIds;
		for (TObjectIterator<USRStructureInstanceManagerComponent> It; It; ++It)
		{
			USRStructureInstanceManagerComponent* StructureManager = *It;
			if (!IsValid(StructureManager)
				|| StructureManager->GetWorld() != World
				|| StructureManager->HasAnyFlags(RF_ClassDefaultObject))
			{
				continue;
			}
			TArray<FSRPlacedStructureInstance> PlacedStructures;
			StructureManager->GetPlacedStructures(PlacedStructures);
			for (const FSRPlacedStructureInstance& PlacedStructure : PlacedStructures)
			{
				const USRStructureDataAsset* StructureDataAsset = PlacedStructure.StructureDataAsset.Get();
				if (!IsValid(StructureDataAsset))
				{
					continue;
				}
				const FSRStructureData StructureData = StructureDataAsset->BuildData();
				const USRResourceDataAsset* DepositResource =
					StructureData.DepositResourceDataAsset.Get();
				if (!PlacedStructure.bNaturalStructure
					|| !StructureData.bIsResourceDeposit
					|| !IsValid(DepositResource)
					|| DepositResource->ResourceDefinitionVersion
						< StarRovers::Resources::CurrentResourceDefinitionVersion)
				{
					continue;
				}
				++OutReport.RuntimeResourceV2DepositCount;
				RuntimeDepositResourceIds.Add(DepositResource->ResourceId);
			}
		}
		OutReport.RuntimeResourceV2DepositTypeCount = RuntimeDepositResourceIds.Num();
		AddCheck(
			OutReport,
			OutReport.RuntimeResourceV2DepositCount > 0
				&& OutReport.RuntimeResourceV2DepositTypeCount == 7,
			TEXT("SolarSystem natural generation must register all seven Resource V2 deposit types before the PIE baseline is ready."));
	}

	for (TObjectIterator<USRFacilityNetworkComponent> It; It; ++It)
	{
		USRFacilityNetworkComponent* FacilityNetwork = *It;
		if (!IsValid(FacilityNetwork)
			|| FacilityNetwork->GetWorld() != World
			|| FacilityNetwork->HasAnyFlags(RF_ClassDefaultObject))
		{
			continue;
		}

		++OutReport.FacilityNetworkCount;
		TArray<FName> OccupantIds;
		FacilityNetwork->GetRegisteredFacilityOccupantIds(OccupantIds);
		OutReport.RegisteredFacilityCount += OccupantIds.Num();
	}

	if (bInRequireFacilityNetwork)
	{
		AddCheck(
			OutReport,
			OutReport.FacilityNetworkCount > 0,
			TEXT("The active world has no Facility Network Component."));
	}

	OutReport.bPassed = OutReport.FailureCount == 0;
	OutReport.Summary = FString::Printf(
		TEXT("Resource System baseline validation %s: Checks=%d Failures=%d Ruleset=%s LogisticsSave=%d RunSave=%d StructureSave=%d Payload=%dB Bodies=%d PrimaryStar=%s FacilityNetworks=%d RegisteredFacilities=%d AuthoredV2=%dR/%dF/%dS/%dD BuildableV2=%d RuntimeDepositsV2=%d/%dTypes"),
		OutReport.bPassed ? TEXT("passed") : TEXT("failed"),
		OutReport.CheckCount,
		OutReport.FailureCount,
		*StaticEnum<ESRResourceRulesetVersion>()->GetNameStringByValue(static_cast<int64>(OutReport.ActiveRuleset)),
		OutReport.LogisticsSaveVersion,
		OutReport.RunSaveVersion,
		OutReport.StructureSaveVersion,
		OutReport.RunSavePayloadBytes,
		OutReport.CelestialBodyCount,
		OutReport.bHasPrimaryStar ? TEXT("true") : TEXT("false"),
		OutReport.FacilityNetworkCount,
		OutReport.RegisteredFacilityCount,
		OutReport.AuthoredResourceV2ResourceCount,
		OutReport.AuthoredResourceV2FacilityCount,
		OutReport.AuthoredResourceV2StructureCount,
		OutReport.AuthoredResourceV2DepositCount,
		OutReport.BuildableResourceV2StructureCount,
		OutReport.RuntimeResourceV2DepositCount,
		OutReport.RuntimeResourceV2DepositTypeCount);
	return OutReport.bPassed;
}

void ASRResourceSystemValidationActor::LogReport(const FSRResourceSystemValidationReport& Report)
{
	if (Report.bPassed)
	{
		UE_LOG(LogSRResourceSystemValidation, Display, TEXT("%s"), *Report.Summary);
		return;
	}

	UE_LOG(LogSRResourceSystemValidation, Error, TEXT("%s"), *Report.Summary);
	for (const FString& FailureMessage : Report.FailureMessages)
	{
		UE_LOG(LogSRResourceSystemValidation, Error, TEXT("  - %s"), *FailureMessage);
	}
}
