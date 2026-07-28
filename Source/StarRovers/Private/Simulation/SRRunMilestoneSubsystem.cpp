#include "Simulation/SRRunMilestoneSubsystem.h"

#include "Assembly/SRStructureBuildCatalog.h"
#include "Automation/SRFacilityNetworkComponent.h"
#include "Automation/SRResourceSystemContent.h"
#include "Automation/SRResourceV2AuthoredContent.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Celestial/SRStar.h"
#include "EngineUtils.h"
#include "Logistics/SRSpaceLogisticsSubsystem.h"
#include "Simulation/SRAugmentSubsystem.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "Simulation/SRSimulationSettings.h"
#include "Simulation/SRSolarSystemGenerator.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"
#include "Utility/SRLog.h"

namespace
{
	bool IsCardResource(const FSRResourceInstance& ResourceInstance)
	{
		return ResourceInstance.ResourceClass == ESRResourceClass::Card
			|| (IsValid(ResourceInstance.ResourceDataAsset.Get())
				&& ResourceInstance.ResourceDataAsset->ResourceClass == ESRResourceClass::Card);
	}

	bool IsStellarFuelResource(const FSRResourceInstance& ResourceInstance)
	{
		return ResourceInstance.ResourceClass == ESRResourceClass::StellarFuel
			|| ResourceInstance.ResourceId == FName(TEXT("StellarFuel"));
	}

	ESRResourceFamily ResolveResourceFamily(const FSRResourceInstance& ResourceInstance)
	{
		if (ResourceInstance.Family != ESRResourceFamily::None)
		{
			return ResourceInstance.Family;
		}
		return IsValid(ResourceInstance.ResourceDataAsset.Get())
			? ResourceInstance.ResourceDataAsset->Family
			: ESRResourceFamily::None;
	}

	bool HasBeenProcessed(const FSRResourceInstance& ResourceInstance)
	{
		return ResourceInstance.ProcessingMemory.ProcessCount > 0
			|| ResourceInstance.ProcessCount > 0;
	}

	bool HasAdjacentBuildAccess(
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRStructureInstanceManagerComponent* StructureManager,
		const FSRPlanetSurfaceGridCellId& DepositCellId)
	{
		if (!IsValid(SurfaceGrid))
		{
			return false;
		}

		FSRPlanetSurfaceGridCellNeighbors Neighbors;
		if (!SurfaceGrid->GetCellNeighbors(DepositCellId, Neighbors))
		{
			return false;
		}
		const FSRPlanetSurfaceGridCellId NeighborIds[] = {
			Neighbors.NegativeU,
			Neighbors.PositiveU,
			Neighbors.NegativeV,
			Neighbors.PositiveV,
		};
		for (const FSRPlanetSurfaceGridCellId& NeighborId : NeighborIds)
		{
			FSRPlanetSurfaceGridCellInfo CellInfo;
			if (!SurfaceGrid->GetCellInfoById(NeighborId, CellInfo))
			{
				continue;
			}
			if ((!CellInfo.bOccupied && CellInfo.bCanConstruct)
				|| (IsValid(StructureManager)
					&& !CellInfo.OccupantId.IsNone()
					&& StructureManager->CanDestroyNaturalStructureForConstruction(
						CellInfo.OccupantId)))
			{
				return true;
			}
		}
		return false;
	}

	void BuildAvailableFamilySet(
		const USRAugmentSubsystem* AugmentSubsystem,
		TSet<ESRResourceFamily>& OutFamilies,
		bool& bOutGenericFamilyProcessorAvailable)
	{
		OutFamilies.Reset();
		bOutGenericFamilyProcessorAvailable = false;
		TArray<FSRFacilityContentDefinitionV2> FacilityDefinitions;
		FSRResourceSystemContent::GetAllFacilityDefinitions(FacilityDefinitions);
		for (const FSRFacilityContentDefinitionV2& Definition : FacilityDefinitions)
		{
			if (Definition.OperationKind != ESRFacilityOperationKind::Process
				|| Definition.ProcessRole != ESRFacilityProcessRoleV2::FamilyProcess
				|| (IsValid(AugmentSubsystem)
					&& !AugmentSubsystem->IsFacilityContentUnlockedV2(Definition.ContentId)))
			{
				continue;
			}
			if (Definition.AcceptedFamily == ESRResourceFamily::None)
			{
				bOutGenericFamilyProcessorAvailable = true;
			}
			else
			{
				OutFamilies.Add(Definition.AcceptedFamily);
			}
		}
	}

	double ResolveSeedEnergy(const USRResourceDataAsset& ResourceDataAsset)
	{
		return ResourceDataAsset.ResourceDefinitionVersion
			>= StarRovers::Resources::CurrentResourceDefinitionVersion
			? FMath::Max(0.0, ResourceDataAsset.SeedEnergy)
			: FMath::Max(0.0, ResourceDataAsset.BaseEnergyValue);
	}

	FText ResolveResourceDisplayName(const USRResourceDataAsset& ResourceDataAsset)
	{
		return ResourceDataAsset.DisplayName.IsEmpty()
			? FText::FromName(ResourceDataAsset.ResourceId)
			: ResourceDataAsset.DisplayName;
	}

	bool IsSystemGenerationInProgress(const UWorld& World)
	{
		for (TActorIterator<ASRSolarSystemGenerator> It(&World); It; ++It)
		{
			if (It->IsRuntimeSystemGenerationInProgress())
			{
				return true;
			}
		}
		return false;
	}

	template <typename VisitorType>
	void VisitFacilityResources(const FSRFacilityInstance& FacilityInstance, VisitorType&& Visitor)
	{
		for (const FSRFacilityPortInventory& Port : FacilityInstance.InputPortInventories)
		{
			for (const FSRResourceInstance& Resource : Port.Inventory)
			{
				Visitor(Resource);
			}
		}
		for (const FSRFacilityPortInventory& Port : FacilityInstance.OutputPortInventories)
		{
			for (const FSRResourceInstance& Resource : Port.Inventory)
			{
				Visitor(Resource);
			}
		}
		for (const FSRResourceInstance& Resource : FacilityInstance.ProcessingInventory)
		{
			Visitor(Resource);
		}
		for (const FSRResourceInstance& Resource : FacilityInstance.InputInventory)
		{
			Visitor(Resource);
		}
		for (const FSRResourceInstance& Resource : FacilityInstance.OutputInventory)
		{
			Visitor(Resource);
		}
	}
}

void FSRFirstFuelMilestoneModel::ApplyConsistency(FSRFirstFuelMilestoneFacts& Facts)
{
	if (Facts.bFirstStellarFuelDelivered)
	{
		Facts.bFirstStellarFuelLaunched = true;
	}
	if (Facts.bFirstStellarFuelLaunched)
	{
		Facts.bHubPlaced = true;
		Facts.bFirstStellarFuelFabricated = true;
	}
	if (Facts.bFirstStellarFuelFabricated)
	{
		Facts.bStellarFuelFabricatorPlaced = true;
		Facts.bFirstCardProcessed = true;
	}
	if (Facts.bFirstCardProcessed)
	{
		Facts.bFamilyProcessorPlaced = true;
		Facts.bFirstCardExtracted = true;
		Facts.bExtractorPlaced = true;
	}
	if (Facts.bFirstCardExtracted)
	{
		Facts.bExtractorPlaced = true;
	}
}

ESRFirstFuelMilestone FSRFirstFuelMilestoneModel::ResolveCurrentMilestone(
	const FSRFirstFuelMilestoneFacts& Facts)
{
	if (!Facts.bExtractorPlaced) return ESRFirstFuelMilestone::PlaceExtractor;
	if (!Facts.bFirstCardExtracted) return ESRFirstFuelMilestone::ExtractFirstCard;
	if (!Facts.bFamilyProcessorPlaced) return ESRFirstFuelMilestone::PlaceFamilyProcessor;
	if (!Facts.bFirstCardProcessed) return ESRFirstFuelMilestone::ProcessFirstCard;
	if (!Facts.bStellarFuelFabricatorPlaced) return ESRFirstFuelMilestone::PlaceStellarFuelFabricator;
	if (!Facts.bFirstStellarFuelFabricated) return ESRFirstFuelMilestone::FabricateFirstStellarFuel;
	if (!Facts.bHubPlaced) return ESRFirstFuelMilestone::PlaceHub;
	if (!Facts.bFirstStellarFuelLaunched) return ESRFirstFuelMilestone::LaunchFirstStellarFuel;
	if (!Facts.bFirstStellarFuelDelivered) return ESRFirstFuelMilestone::DeliverFirstStellarFuel;
	return ESRFirstFuelMilestone::Complete;
}

int32 FSRFirstFuelMilestoneModel::ResolveCompletedMilestoneCount(
	const FSRFirstFuelMilestoneFacts& Facts)
{
	return FMath::Clamp(
		static_cast<int32>(ResolveCurrentMilestone(Facts)),
		0,
		TotalMilestoneCount);
}

void USRRunMilestoneSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ResetFirstFuelMilestone();
}

void USRRunMilestoneSubsystem::Deinitialize()
{
	for (const TWeakObjectPtr<USRFacilityNetworkComponent>& Network : BoundFacilityNetworks)
	{
		if (Network.IsValid())
		{
			Network->OnResourceProduced().RemoveAll(this);
		}
	}
	BoundFacilityNetworks.Reset();
	Super::Deinitialize();
}

void USRRunMilestoneSubsystem::ResetFirstFuelMilestone()
{
	ObservedFacts = FSRFirstFuelMilestoneFacts();
	FirstResourceFamily = ESRResourceFamily::None;
	RecommendedDepositBody.Reset();
	FallbackConstructibleBody.Reset();
	FirstAutomationBody.Reset();
	PrimaryStarActor.Reset();
	InitialSystemScan = FSRSystemScanSnapshot();
	InitialProgressRecovery = FSRInitialProgressRecoverySnapshot();
	ExtractorTarget = FFacilityTarget();
	FamilyProcessorTarget = FFacilityTarget();
	StellarFuelFabricatorTarget = FFacilityTarget();
	HubTarget = FFacilityTarget();
}

void USRRunMilestoneSubsystem::RefreshFromWorld()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	for (auto It = BoundFacilityNetworks.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}

	USRCelestialBodyRegistrySubsystem* Registry =
		World->GetSubsystem<USRCelestialBodyRegistrySubsystem>();
	if (!IsValid(Registry))
	{
		return;
	}

	PrimaryStarActor = Registry->GetPrimaryStarActor();
	ValidateFacilityTargets();
	TArray<AActor*> CelestialBodies;
	Registry->GetCelestialBodies(CelestialBodies);
	for (AActor* BodyActor : CelestialBodies)
	{
		if (!IsValid(BodyActor))
		{
			continue;
		}

		if (USRCelestialBodyRuntimeLibrary::GetCelestialCanConstruct(BodyActor)
			&& !FallbackConstructibleBody.IsValid())
		{
			FallbackConstructibleBody = BodyActor;
		}

		if (USRStructureInstanceManagerComponent* StructureManager =
			BodyActor->FindComponentByClass<USRStructureInstanceManagerComponent>())
		{
			TArray<FSRPlacedStructureInstance> PlacedStructures;
			StructureManager->GetPlacedStructures(PlacedStructures);
			for (const FSRPlacedStructureInstance& PlacedStructure : PlacedStructures)
			{
				if (!PlacedStructure.bNaturalStructure
					|| !IsValid(PlacedStructure.StructureDataAsset.Get()))
				{
					continue;
				}

				const FSRStructureData StructureData = PlacedStructure.StructureDataAsset->BuildData();
				USRResourceDataAsset* DepositResource = StructureData.DepositResourceDataAsset.Get();
				if (!StructureData.bIsResourceDeposit
					|| !IsValid(DepositResource)
					|| DepositResource->ResourceClass != ESRResourceClass::Card)
				{
					continue;
				}

				if (!RecommendedDepositBody.IsValid())
				{
					RecommendedDepositBody = BodyActor;
				}
				if (FirstResourceFamily == ESRResourceFamily::None)
				{
					FirstResourceFamily = DepositResource->Family;
				}
				break;
			}
		}

		if (USRFacilityNetworkComponent* FacilityNetwork =
			BodyActor->FindComponentByClass<USRFacilityNetworkComponent>())
		{
			BindFacilityNetwork(FacilityNetwork);
			TArray<FName> OccupantIds;
			FacilityNetwork->GetRegisteredFacilityOccupantIds(OccupantIds);
			OccupantIds.Sort(
				[](const FName& Left, const FName& Right)
				{
					return Left.LexicalLess(Right);
				});
			for (const FName OccupantId : OccupantIds)
			{
				ObserveFacility(BodyActor, FacilityNetwork, OccupantId);
			}
		}
	}

	if (USRSpaceLogisticsSubsystem* Logistics =
		World->GetSubsystem<USRSpaceLogisticsSubsystem>())
	{
		TArray<FSRSpaceLogisticsStarFuelMissile> Missiles;
		Logistics->GetStarFuelMissiles(Missiles);
		ObservedFacts.bFirstStellarFuelLaunched |= !Missiles.IsEmpty();
	}

	if (const ASRStar* PrimaryStar = Cast<ASRStar>(PrimaryStarActor.Get()))
	{
		ObservedFacts.bFirstStellarFuelDelivered |=
			PrimaryStar->GetStellarFuelState().TotalDeliveredFuel > 0.0;
	}

	if (!ObservedFacts.bFirstCardExtracted
		&& !ExtractorTarget.IsValid()
		&& InitialSystemScan.HasRecommendation()
		&& !IsInitialSystemScanRecommendationStillViable())
	{
		InitialSystemScan = FSRSystemScanSnapshot();
		RecommendedDepositBody.Reset();
		FirstResourceFamily = ESRResourceFamily::None;
	}
	TryBuildInitialSystemScan();
	FSRFirstFuelMilestoneModel::ApplyConsistency(ObservedFacts);
}

FSRFirstFuelMilestoneSnapshot USRRunMilestoneSubsystem::GetFirstFuelMilestoneSnapshot() const
{
	FSRFirstFuelMilestoneSnapshot Snapshot;
	Snapshot.bIsTracking = true;
	Snapshot.Facts = ObservedFacts;
	Snapshot.CurrentMilestone = FSRFirstFuelMilestoneModel::ResolveCurrentMilestone(ObservedFacts);
	Snapshot.CompletedMilestoneCount =
		FSRFirstFuelMilestoneModel::ResolveCompletedMilestoneCount(ObservedFacts);
	Snapshot.TotalMilestoneCount = FSRFirstFuelMilestoneModel::TotalMilestoneCount;
	Snapshot.FirstResourceFamily = FirstResourceFamily;
	Snapshot.PrimaryStarActor = PrimaryStarActor.Get();
	Snapshot.InitialSystemScan = InitialSystemScan;
	Snapshot.InitialProgressRecovery = InitialProgressRecovery;
	const USRSimulationSettings* Settings = GetDefault<USRSimulationSettings>();
	Snapshot.InitialProgressRecovery.bEnabled = IsValid(Settings)
		&& Settings->bEnableEmergencyProspectingRecoveryV2;
	Snapshot.InitialProgressRecovery.bAvailable =
		Snapshot.InitialProgressRecovery.bEnabled
		&& !Snapshot.InitialProgressRecovery.bAttempted
		&& !ObservedFacts.bFirstCardExtracted
		&& InitialSystemScan.bScanComplete
		&& !InitialSystemScan.HasRecommendation();

	const FFacilityTarget* TargetFacility = nullptr;
	TWeakObjectPtr<AActor> PreferredBody;
	switch (Snapshot.CurrentMilestone)
	{
	case ESRFirstFuelMilestone::PlaceExtractor:
		if (const FSRSystemScanCandidate* RecommendedCandidate =
			InitialSystemScan.GetRecommendedCandidate())
		{
			PreferredBody = RecommendedCandidate->BodyActor;
		}
		break;
	case ESRFirstFuelMilestone::ExtractFirstCard:
		TargetFacility = &ExtractorTarget;
		PreferredBody = ExtractorTarget.BodyActor;
		break;
	case ESRFirstFuelMilestone::PlaceFamilyProcessor:
		PreferredBody = ExtractorTarget.BodyActor;
		break;
	case ESRFirstFuelMilestone::ProcessFirstCard:
		TargetFacility = &FamilyProcessorTarget;
		PreferredBody = FamilyProcessorTarget.BodyActor;
		break;
	case ESRFirstFuelMilestone::PlaceStellarFuelFabricator:
		PreferredBody = FamilyProcessorTarget.BodyActor;
		break;
	case ESRFirstFuelMilestone::FabricateFirstStellarFuel:
		TargetFacility = &StellarFuelFabricatorTarget;
		PreferredBody = StellarFuelFabricatorTarget.BodyActor;
		break;
	case ESRFirstFuelMilestone::PlaceHub:
		PreferredBody = StellarFuelFabricatorTarget.BodyActor;
		break;
	case ESRFirstFuelMilestone::LaunchFirstStellarFuel:
		TargetFacility = &HubTarget;
		PreferredBody = HubTarget.BodyActor;
		break;
	case ESRFirstFuelMilestone::DeliverFirstStellarFuel:
		PreferredBody = PrimaryStarActor;
		break;
	case ESRFirstFuelMilestone::Complete:
		break;
	}

	if (TargetFacility && TargetFacility->IsValid())
	{
		Snapshot.TargetFacilityBodyActor = TargetFacility->BodyActor.Get();
		Snapshot.TargetFacilityOccupantId = TargetFacility->OccupantId;
	}
	if (Snapshot.CurrentMilestone != ESRFirstFuelMilestone::PlaceExtractor)
	{
		if (!PreferredBody.IsValid()) PreferredBody = FirstAutomationBody;
		if (!PreferredBody.IsValid()) PreferredBody = RecommendedDepositBody;
		if (!PreferredBody.IsValid()) PreferredBody = FallbackConstructibleBody;
	}
	Snapshot.RecommendedBodyActor = PreferredBody.Get();
	return Snapshot;
}

FSRFirstFuelMilestoneFacts USRRunMilestoneSubsystem::GetFirstFuelMilestoneFacts() const
{
	return ObservedFacts;
}

FSRSystemScanSnapshot USRRunMilestoneSubsystem::GetInitialSystemScanSnapshot() const
{
	return InitialSystemScan;
}

bool USRRunMilestoneSubsystem::IsInitialSystemScanRecommendationActive() const
{
	return !ObservedFacts.bFirstCardExtracted
		&& !ExtractorTarget.IsValid()
		&& InitialSystemScan.HasRecommendation();
}

bool USRRunMilestoneSubsystem::TryActivateEmergencyProspectingRecovery()
{
	RefreshFromWorld();
	const FSRFirstFuelMilestoneSnapshot CurrentSnapshot =
		GetFirstFuelMilestoneSnapshot();
	if (!CurrentSnapshot.InitialProgressRecovery.bAvailable)
	{
		return false;
	}

	InitialProgressRecovery.bAttempted = true;
	InitialProgressRecovery.bAvailable = false;
	const USRSimulationSettings* Settings = GetDefault<USRSimulationSettings>();
	const int32 EmergencyAmount = IsValid(Settings)
		? FMath::Max(5, Settings->EmergencyProspectingCardAmountV2)
		: 25;
	UWorld* World = GetWorld();
	USRCelestialBodyRegistrySubsystem* Registry = IsValid(World)
		? World->GetSubsystem<USRCelestialBodyRegistrySubsystem>()
		: nullptr;
	if (!IsValid(World) || !IsValid(Registry))
	{
		return false;
	}

	TSet<ESRResourceFamily> AvailableFamilies;
	bool bGenericFamilyProcessorAvailable = false;
	BuildAvailableFamilySet(
		World->GetSubsystem<USRAugmentSubsystem>(),
		AvailableFamilies,
		bGenericFamilyProcessorAvailable);

	TArray<AActor*> Bodies;
	Registry->GetCelestialBodies(Bodies);
	Bodies.RemoveAll(
		[](const AActor* BodyActor)
		{
			return !IsValid(BodyActor)
				|| !USRCelestialBodyRuntimeLibrary::GetCelestialCanConstruct(BodyActor);
		});
	AActor* PrimaryStar = Registry->GetPrimaryStarActor();
	Bodies.Sort(
		[PrimaryStar](const AActor& Left, const AActor& Right)
		{
			const float LeftDistance = IsValid(PrimaryStar)
				? FVector::DistSquared(
					Left.GetActorLocation(),
					PrimaryStar->GetActorLocation())
				: 0.0f;
			const float RightDistance = IsValid(PrimaryStar)
				? FVector::DistSquared(
					Right.GetActorLocation(),
					PrimaryStar->GetActorLocation())
				: 0.0f;
			if (!FMath::IsNearlyEqual(LeftDistance, RightDistance))
			{
				return LeftDistance < RightDistance;
			}
			return Left.GetPathName() < Right.GetPathName();
		});

	struct FRecoverableDeposit
	{
		TWeakObjectPtr<AActor> BodyActor;
		TWeakObjectPtr<USRStructureInstanceManagerComponent> StructureManager;
		FName OccupantId = NAME_None;
		TObjectPtr<USRResourceDataAsset> ResourceDataAsset = nullptr;
		double SeedEnergy = 0.0;
		FName StableId = NAME_None;
	};
	TArray<FRecoverableDeposit> DepletedDeposits;
	TArray<TObjectPtr<USRStructureDataAsset>> DepositTemplates;
	for (AActor* BodyActor : Bodies)
	{
		USRPlanetSurfaceGrid* SurfaceGrid =
			BodyActor->FindComponentByClass<USRPlanetSurfaceGrid>();
		USRStructureInstanceManagerComponent* StructureManager =
			BodyActor->FindComponentByClass<USRStructureInstanceManagerComponent>();
		if (!IsValid(SurfaceGrid) || !IsValid(StructureManager))
		{
			continue;
		}

		TArray<FSRPlacedStructureInstance> PlacedStructures;
		StructureManager->GetPlacedStructures(PlacedStructures);
		for (const FSRPlacedStructureInstance& PlacedStructure : PlacedStructures)
		{
			if (!IsValid(PlacedStructure.StructureDataAsset.Get()))
			{
				continue;
			}
			const FSRStructureData StructureData =
				PlacedStructure.StructureDataAsset->BuildData();
			USRResourceDataAsset* ResourceDataAsset =
				StructureData.DepositResourceDataAsset.Get();
			if (!StructureData.bIsResourceDeposit
				|| !IsValid(ResourceDataAsset)
				|| ResourceDataAsset->ResourceClass != ESRResourceClass::Card
				|| ResourceDataAsset->Family == ESRResourceFamily::None
				|| (!bGenericFamilyProcessorAvailable
					&& !AvailableFamilies.Contains(ResourceDataAsset->Family)))
			{
				continue;
			}
			DepositTemplates.AddUnique(PlacedStructure.StructureDataAsset);

			FSRResourceDepositInstance Deposit;
			if (StructureManager->GetResourceDepositInstance(
					PlacedStructure.OccupantId,
					Deposit)
				&& Deposit.RemainingAmount <= 0
				&& HasAdjacentBuildAccess(
					SurfaceGrid,
					StructureManager,
					PlacedStructure.OriginCellId))
			{
				FRecoverableDeposit& Recoverable =
					DepletedDeposits.AddDefaulted_GetRef();
				Recoverable.BodyActor = BodyActor;
				Recoverable.StructureManager = StructureManager;
				Recoverable.OccupantId = PlacedStructure.OccupantId;
				Recoverable.ResourceDataAsset = ResourceDataAsset;
				Recoverable.SeedEnergy = ResolveSeedEnergy(*ResourceDataAsset);
				Recoverable.StableId = FName(*FString::Printf(
					TEXT("%s|%s"),
					*BodyActor->GetPathName(),
					*PlacedStructure.OccupantId.ToString()));
			}
		}
	}

	DepletedDeposits.Sort(
		[](const FRecoverableDeposit& Left, const FRecoverableDeposit& Right)
		{
			if (!FMath::IsNearlyEqual(Left.SeedEnergy, Right.SeedEnergy))
			{
				return Left.SeedEnergy > Right.SeedEnergy;
			}
			return Left.StableId.LexicalLess(Right.StableId);
		});
	if (!DepletedDeposits.IsEmpty())
	{
		const FRecoverableDeposit& Target = DepletedDeposits[0];
		FSRResourceDepositInstance UpdatedDeposit;
		if (Target.StructureManager.IsValid()
			&& Target.StructureManager->TryConfigureResourceDepositAmount(
				Target.OccupantId,
				EmergencyAmount,
				EmergencyAmount,
				UpdatedDeposit))
		{
			InitialProgressRecovery.bApplied = true;
			InitialProgressRecovery.BodyActor = Target.BodyActor.Get();
			InitialProgressRecovery.DepositOccupantId = Target.OccupantId;
			InitialProgressRecovery.ResourceId = UpdatedDeposit.ResourceId;
			InitialProgressRecovery.GrantedCardAmount = EmergencyAmount;
		}
	}

	if (!InitialProgressRecovery.bApplied)
	{
		DepositTemplates.Sort(
			[](const USRStructureDataAsset& Left, const USRStructureDataAsset& Right)
			{
				return Left.GetPathName() < Right.GetPathName();
			});
		if (DepositTemplates.IsEmpty())
		{
			const FString FallbackPath =
				FSRResourceV2AuthoredContent::GetDepositObjectPath(
					ESRResourceContentPresetV2::HeliosIron);
			if (USRStructureDataAsset* FallbackTemplate =
				Cast<USRStructureDataAsset>(FSoftObjectPath(FallbackPath).TryLoad()))
			{
				DepositTemplates.Add(FallbackTemplate);
			}
		}

		USRStructureDataAsset* DepositTemplate = DepositTemplates.IsEmpty()
			? nullptr
			: DepositTemplates[0].Get();
		if (IsValid(DepositTemplate))
		{
			for (AActor* BodyActor : Bodies)
			{
				USRPlanetSurfaceGrid* SurfaceGrid =
					BodyActor->FindComponentByClass<USRPlanetSurfaceGrid>();
				USRStructureInstanceManagerComponent* StructureManager =
					BodyActor->FindComponentByClass<USRStructureInstanceManagerComponent>();
				if (!IsValid(SurfaceGrid) || !IsValid(StructureManager))
				{
					continue;
				}

				for (const FSRPlanetSurfaceGridCell& Cell : SurfaceGrid->GetCellsRef())
				{
					if (Cell.bOccupied
						|| !HasAdjacentBuildAccess(
							SurfaceGrid,
							StructureManager,
							Cell.CellId))
					{
						continue;
					}

					FName NewOccupantId = NAME_None;
					if (!StructureManager->TryPlaceStructureOnSurfaceGrid(
							SurfaceGrid,
							Cell.CellId,
							DepositTemplate,
							NewOccupantId,
							true,
							true))
					{
						continue;
					}

					FSRResourceDepositInstance UpdatedDeposit;
					if (!StructureManager->TryConfigureResourceDepositAmount(
							NewOccupantId,
							EmergencyAmount,
							EmergencyAmount,
							UpdatedDeposit))
					{
						FSRPlacedStructureInstance RemovedStructure;
						StructureManager->TryRemoveStructureByOccupantId(
							SurfaceGrid,
							NewOccupantId,
							RemovedStructure);
						continue;
					}

					InitialProgressRecovery.bApplied = true;
					InitialProgressRecovery.BodyActor = BodyActor;
					InitialProgressRecovery.DepositOccupantId = NewOccupantId;
					InitialProgressRecovery.ResourceId = UpdatedDeposit.ResourceId;
					InitialProgressRecovery.GrantedCardAmount = EmergencyAmount;
					break;
				}
				if (InitialProgressRecovery.bApplied)
				{
					break;
				}
			}
		}
	}

	if (!InitialProgressRecovery.bApplied)
	{
		SR_LOG(SolarSystem, LogTemp, Error,
			TEXT("Emergency prospecting could not find a recoverable deposit or an accessible placement cell."));
		return false;
	}

	SR_LOG(SolarSystem, LogTemp, Display,
		TEXT("Emergency prospecting restored %d finite Card(s): Body=%s Resource=%s Deposit=%s"),
		EmergencyAmount,
		*GetNameSafe(InitialProgressRecovery.BodyActor.Get()),
		*InitialProgressRecovery.ResourceId.ToString(),
		*InitialProgressRecovery.DepositOccupantId.ToString());
	InitialSystemScan = FSRSystemScanSnapshot();
	RecommendedDepositBody.Reset();
	FirstResourceFamily = ESRResourceFamily::None;
	TryBuildInitialSystemScan();
	return InitialSystemScan.HasRecommendation();
}

bool USRRunMilestoneSubsystem::IsFacilityTargetPresent(
	const FFacilityTarget& Target) const
{
	if (!Target.IsValid())
	{
		return false;
	}
	USRFacilityNetworkComponent* FacilityNetwork =
		Target.BodyActor->FindComponentByClass<USRFacilityNetworkComponent>();
	FSRFacilityInstance FacilityInstance;
	return IsValid(FacilityNetwork)
		&& FacilityNetwork->GetFacilityInstance(
			Target.OccupantId,
			FacilityInstance);
}

bool USRRunMilestoneSubsystem::IsExtractorTargetOperational(
	const FFacilityTarget& Target) const
{
	if (!IsFacilityTargetPresent(Target))
	{
		return false;
	}
	USRFacilityNetworkComponent* FacilityNetwork =
		Target.BodyActor->FindComponentByClass<USRFacilityNetworkComponent>();
	FSRResourceDepositInstance Deposit;
	return IsValid(FacilityNetwork)
		&& FacilityNetwork->GetFacilityMiningTarget(Target.OccupantId, Deposit)
		&& Deposit.RemainingAmount > 0
		&& IsValid(Deposit.ResourceDataAsset.Get())
		&& Deposit.ResourceDataAsset->ResourceClass == ESRResourceClass::Card;
}

bool USRRunMilestoneSubsystem::IsFamilyProcessorTargetCompatible(
	const FFacilityTarget& Target) const
{
	if (!IsFacilityTargetPresent(Target))
	{
		return false;
	}
	USRFacilityNetworkComponent* FacilityNetwork =
		Target.BodyActor->FindComponentByClass<USRFacilityNetworkComponent>();
	FSRFacilityInstance FacilityInstance;
	if (!IsValid(FacilityNetwork)
		|| !FacilityNetwork->GetFacilityInstance(
			Target.OccupantId,
			FacilityInstance)
		|| !IsValid(FacilityInstance.StructureDataAsset.Get()))
	{
		return false;
	}
	const ESRResourceFamily FacilityFamily =
		FSRStructureBuildCatalogBuilder::ResolveResourceFamily(
			FacilityInstance.StructureDataAsset->BuildData());
	return FirstResourceFamily == ESRResourceFamily::None
		|| FacilityFamily == ESRResourceFamily::None
		|| FacilityFamily == FirstResourceFamily;
}

void USRRunMilestoneSubsystem::ValidateFacilityTargets()
{
	if (!IsFacilityTargetPresent(ExtractorTarget)
		|| (!ObservedFacts.bFirstCardExtracted
			&& !IsExtractorTargetOperational(ExtractorTarget)))
	{
		ExtractorTarget = FFacilityTarget();
	}
	if (!IsFacilityTargetPresent(FamilyProcessorTarget)
		|| (!ObservedFacts.bFirstCardProcessed
			&& !IsFamilyProcessorTargetCompatible(FamilyProcessorTarget)))
	{
		FamilyProcessorTarget = FFacilityTarget();
	}
	if (!IsFacilityTargetPresent(StellarFuelFabricatorTarget))
	{
		StellarFuelFabricatorTarget = FFacilityTarget();
	}
	if (!IsFacilityTargetPresent(HubTarget))
	{
		HubTarget = FFacilityTarget();
	}
}

bool USRRunMilestoneSubsystem::IsInitialSystemScanRecommendationStillViable() const
{
	const FSRSystemScanCandidate* Candidate =
		InitialSystemScan.GetRecommendedCandidate();
	if (!Candidate || !IsValid(Candidate->BodyActor.Get()))
	{
		return false;
	}
	USRPlanetSurfaceGrid* SurfaceGrid =
		Candidate->BodyActor->FindComponentByClass<USRPlanetSurfaceGrid>();
	USRStructureInstanceManagerComponent* StructureManager =
		Candidate->BodyActor->FindComponentByClass<USRStructureInstanceManagerComponent>();
	FSRResourceDepositInstance Deposit;
	return IsValid(SurfaceGrid)
		&& IsValid(StructureManager)
		&& StructureManager->GetResourceDepositInstance(
			Candidate->DepositOccupantId,
			Deposit)
		&& Deposit.RemainingAmount > 0
		&& HasAdjacentBuildAccess(
			SurfaceGrid,
			StructureManager,
			Candidate->DepositCellId);
}

void USRRunMilestoneSubsystem::TryBuildInitialSystemScan()
{
	if (InitialSystemScan.bScanComplete)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World) || IsSystemGenerationInProgress(*World))
	{
		return;
	}
	USRCelestialBodyRegistrySubsystem* Registry =
		World->GetSubsystem<USRCelestialBodyRegistrySubsystem>();
	AActor* PrimaryStar = IsValid(Registry) ? Registry->GetPrimaryStarActor() : nullptr;
	if (!IsValid(PrimaryStar))
	{
		return;
	}

	const USRAugmentSubsystem* AugmentSubsystem =
		World->GetSubsystem<USRAugmentSubsystem>();
	TSet<ESRResourceFamily> AvailableFamilies;
	bool bGenericFamilyProcessorAvailable = false;
	BuildAvailableFamilySet(
		AugmentSubsystem,
		AvailableFamilies,
		bGenericFamilyProcessorAvailable);

	const USRSimulationSettings* Settings = GetDefault<USRSimulationSettings>();
	const int32 DefaultCapacity = IsValid(Settings)
		? FMath::Max(0, Settings->BaseOperationalCapacityV2)
		: 30;
	TArray<AActor*> Bodies;
	Registry->GetCelestialBodies(Bodies);
	Bodies.Sort(
		[](const AActor& Left, const AActor& Right)
		{
			return Left.GetPathName() < Right.GetPathName();
		});
	TArray<FSRReferenceResourceDefinitionV2> RequiredCardDefinitions;
	FSRResourceSystemContent::GetAllReferenceResourceDefinitions(
		RequiredCardDefinitions);
	TSet<FName> RequiredCardResourceIds;
	for (const FSRReferenceResourceDefinitionV2& Definition : RequiredCardDefinitions)
	{
		if (!Definition.ResourceId.IsNone())
		{
			RequiredCardResourceIds.Add(Definition.ResourceId);
		}
	}
	TSet<FName> AvailableRequiredCardResourceIds;
	TArray<FSRSystemScanCandidate> Candidates;
	int32 ConstructibleBodyCount = 0;
	int32 CardDepositCount = 0;
	int32 MineableCardDepositCount = 0;
	int32 DepletedCardDepositCount = 0;
	int32 InaccessibleCardDepositCount = 0;
	bool bHasReadyConstructibleSurface = false;

	for (AActor* BodyActor : Bodies)
	{
		if (!IsValid(BodyActor)
			|| !USRCelestialBodyRuntimeLibrary::GetCelestialCanConstruct(BodyActor))
		{
			continue;
		}
		USRPlanetSurfaceGrid* SurfaceGrid =
			BodyActor->FindComponentByClass<USRPlanetSurfaceGrid>();
		USRStructureInstanceManagerComponent* StructureManager =
			BodyActor->FindComponentByClass<USRStructureInstanceManagerComponent>();
		if (!IsValid(SurfaceGrid) || SurfaceGrid->GetCellCount() <= 0)
		{
			continue;
		}

		bHasReadyConstructibleSurface = true;
		++ConstructibleBodyCount;
		int32 OperationalLoad = 0;
		int32 OperationalCapacity = DefaultCapacity;
		if (USRFacilityNetworkComponent* FacilityNetwork =
			BodyActor->FindComponentByClass<USRFacilityNetworkComponent>())
		{
			const FSROperationalCapacityReportV2 CapacityReport =
				FacilityNetwork->RefreshOperationalCapacity();
			OperationalLoad = FMath::Max(0, CapacityReport.TotalDemand);
			OperationalCapacity = FMath::Max(0, CapacityReport.TotalCapacity);
		}

		if (!IsValid(StructureManager))
		{
			continue;
		}
		TArray<FSRPlacedStructureInstance> PlacedStructures;
		StructureManager->GetPlacedStructures(PlacedStructures);
		PlacedStructures.Sort(
			[](const FSRPlacedStructureInstance& Left, const FSRPlacedStructureInstance& Right)
			{
				return Left.OccupantId.LexicalLess(Right.OccupantId);
			});
		for (const FSRPlacedStructureInstance& PlacedStructure : PlacedStructures)
		{
			if (!PlacedStructure.bNaturalStructure
				|| PlacedStructure.OccupantId.IsNone()
				|| !IsValid(PlacedStructure.StructureDataAsset.Get()))
			{
				continue;
			}
			const FSRStructureData StructureData =
				PlacedStructure.StructureDataAsset->BuildData();
			USRResourceDataAsset* ResourceDataAsset =
				StructureData.DepositResourceDataAsset.Get();
			if (!StructureData.bIsResourceDeposit
				|| !IsValid(ResourceDataAsset)
				|| ResourceDataAsset->ResourceClass != ESRResourceClass::Card
				|| ResourceDataAsset->Family == ESRResourceFamily::None)
			{
				continue;
			}

			++CardDepositCount;
			FSRResourceDepositInstance ResourceDeposit;
			const bool bHasDepositInstance =
				StructureManager->GetResourceDepositInstance(
					PlacedStructure.OccupantId,
					ResourceDeposit);
			FSRSystemScanCandidate& Candidate = Candidates.AddDefaulted_GetRef();
			Candidate.BodyActor = BodyActor;
			Candidate.BodyDisplayName =
				USRCelestialBodyRuntimeLibrary::GetCelestialVariableName(BodyActor);
			if (Candidate.BodyDisplayName.IsEmpty())
			{
				Candidate.BodyDisplayName = FText::FromString(BodyActor->GetName());
			}
			Candidate.DepositOccupantId = PlacedStructure.OccupantId;
			Candidate.DepositCellId = PlacedStructure.OriginCellId;
			Candidate.ResourceDataAsset = ResourceDataAsset;
			Candidate.ResourceId = ResourceDataAsset->ResourceId;
			Candidate.ResourceDisplayName = ResolveResourceDisplayName(*ResourceDataAsset);
			Candidate.Family = ResourceDataAsset->Family;
			Candidate.Spectrum = ResourceDataAsset->NativeSpectrum;
			Candidate.Grade = FMath::Clamp(
				ResourceDataAsset->NativeGrade,
				StarRovers::Resources::MinimumGrade,
				StarRovers::Resources::MaximumGrade);
			Candidate.SeedEnergy = ResolveSeedEnergy(*ResourceDataAsset);
			Candidate.DistanceToPrimaryStar = FVector::Distance(
				BodyActor->GetActorLocation(),
				PrimaryStar->GetActorLocation());
			Candidate.OperationalLoad = OperationalLoad;
			Candidate.OperationalCapacity = OperationalCapacity;
			Candidate.OperationalHeadroom = FMath::Max(
				0,
				OperationalCapacity - OperationalLoad);
			Candidate.DepositTotalAmount = bHasDepositInstance
				? ResourceDeposit.TotalAmount
				: 0;
			Candidate.DepositRemainingAmount = bHasDepositInstance
				? ResourceDeposit.RemainingAmount
				: 0;
			Candidate.bHasFamilyProcessorAccess =
				bGenericFamilyProcessorAvailable
				|| AvailableFamilies.Contains(Candidate.Family);
			Candidate.bHasAdjacentBuildAccess = HasAdjacentBuildAccess(
				SurfaceGrid,
				StructureManager,
				Candidate.DepositCellId);
			if (Candidate.DepositRemainingAmount > 0)
			{
				++MineableCardDepositCount;
			}
			else
			{
				++DepletedCardDepositCount;
			}
			if (!Candidate.bHasAdjacentBuildAccess)
			{
				++InaccessibleCardDepositCount;
			}
			if (Candidate.IsViable()
				&& RequiredCardResourceIds.Contains(Candidate.ResourceId))
			{
				AvailableRequiredCardResourceIds.Add(Candidate.ResourceId);
			}
			Candidate.CapacityHeadroomNormalized = OperationalCapacity > 0
				? FMath::Clamp(
					static_cast<float>(Candidate.OperationalHeadroom)
						/ static_cast<float>(OperationalCapacity),
					0.0f,
					1.0f)
				: 0.0f;
			Candidate.StableCandidateId = FName(*FString::Printf(
				TEXT("%s|%s|%s"),
				*BodyActor->GetPathName(),
				*Candidate.ResourceId.ToString(),
				*Candidate.DepositOccupantId.ToString()));
		}
	}

	if (!bHasReadyConstructibleSurface)
	{
		return;
	}

	float MinimumEnergy = TNumericLimits<float>::Max();
	float MaximumEnergy = -TNumericLimits<float>::Max();
	float MinimumDistance = TNumericLimits<float>::Max();
	float MaximumDistance = -TNumericLimits<float>::Max();
	for (const FSRSystemScanCandidate& Candidate : Candidates)
	{
		if (!Candidate.IsViable())
		{
			continue;
		}
		const float SeedEnergy = static_cast<float>(Candidate.SeedEnergy);
		MinimumEnergy = FMath::Min(MinimumEnergy, SeedEnergy);
		MaximumEnergy = FMath::Max(MaximumEnergy, SeedEnergy);
		MinimumDistance = FMath::Min(MinimumDistance, Candidate.DistanceToPrimaryStar);
		MaximumDistance = FMath::Max(MaximumDistance, Candidate.DistanceToPrimaryStar);
	}

	int32 ViableCandidateCount = 0;
	for (FSRSystemScanCandidate& Candidate : Candidates)
	{
		if (Candidate.IsViable())
		{
			++ViableCandidateCount;
			Candidate.ResourceQualityNormalized =
				FSRSystemScanModel::NormalizeHigherIsBetter(
					static_cast<float>(Candidate.SeedEnergy),
					MinimumEnergy,
					MaximumEnergy);
			Candidate.StarProximityNormalized =
				FSRSystemScanModel::NormalizeLowerIsBetter(
					Candidate.DistanceToPrimaryStar,
					MinimumDistance,
					MaximumDistance);
		}
		Candidate.Score = FSRSystemScanModel::ScoreCandidate(
			Candidate.ResourceQualityNormalized,
			Candidate.StarProximityNormalized,
			Candidate.CapacityHeadroomNormalized,
			Candidate.bHasFamilyProcessorAccess,
			Candidate.bHasAdjacentBuildAccess);
	}
	FSRSystemScanModel::SortCandidates(Candidates);

	FSRSystemScanSnapshot CompletedScan;
	CompletedScan.bScanComplete = true;
	CompletedScan.ScannedConstructibleBodyCount = ConstructibleBodyCount;
	CompletedScan.ScannedCardDepositCount = CardDepositCount;
	CompletedScan.MineableCardDepositCount = MineableCardDepositCount;
	CompletedScan.DepletedCardDepositCount = DepletedCardDepositCount;
	CompletedScan.InaccessibleCardDepositCount = InaccessibleCardDepositCount;
	CompletedScan.ViableCandidateCount = ViableCandidateCount;
	CompletedScan.RequiredCardResourceCount = RequiredCardResourceIds.Num();
	CompletedScan.AvailableRequiredCardResourceCount =
		AvailableRequiredCardResourceIds.Num();
	for (const FName RequiredResourceId : RequiredCardResourceIds)
	{
		if (!AvailableRequiredCardResourceIds.Contains(RequiredResourceId))
		{
			CompletedScan.MissingRequiredCardResourceIds.Add(
				RequiredResourceId);
		}
	}
	CompletedScan.MissingRequiredCardResourceIds.Sort(
		[](const FName& Left, const FName& Right)
		{
			return Left.LexicalLess(Right);
		});
	TSet<FString> AddedBodyResourceKeys;
	for (const FSRSystemScanCandidate& Candidate : Candidates)
	{
		if (!Candidate.IsViable())
		{
			continue;
		}
		const FString BodyResourceKey = FString::Printf(
			TEXT("%s|%s"),
			*GetPathNameSafe(Candidate.BodyActor.Get()),
			*Candidate.ResourceId.ToString());
		if (AddedBodyResourceKeys.Contains(BodyResourceKey))
		{
			continue;
		}
		AddedBodyResourceKeys.Add(BodyResourceKey);
		CompletedScan.RankedCandidates.Add(Candidate);
		if (CompletedScan.RankedCandidates.Num() >= 3)
		{
			break;
		}
	}
	InitialSystemScan = MoveTemp(CompletedScan);
	if (const FSRSystemScanCandidate* RecommendedCandidate =
		InitialSystemScan.GetRecommendedCandidate())
	{
		RecommendedDepositBody = RecommendedCandidate->BodyActor;
		FirstResourceFamily = RecommendedCandidate->Family;
	}
}

void USRRunMilestoneSubsystem::BindFacilityNetwork(USRFacilityNetworkComponent* FacilityNetwork)
{
	if (!IsValid(FacilityNetwork))
	{
		return;
	}
	const TWeakObjectPtr<USRFacilityNetworkComponent> NetworkKey(FacilityNetwork);
	if (BoundFacilityNetworks.Contains(NetworkKey))
	{
		return;
	}
	FacilityNetwork->OnResourceProduced().AddUObject(
		this,
		&USRRunMilestoneSubsystem::HandleResourceProduced);
	BoundFacilityNetworks.Add(NetworkKey);
}

void USRRunMilestoneSubsystem::HandleResourceProduced(
	USRFacilityNetworkComponent* FacilityNetwork,
	FName OccupantId,
	const FSRResourceInstance& ResourceInstance)
{
	if (!IsValid(FacilityNetwork))
	{
		return;
	}
	ObserveFacility(FacilityNetwork->GetOwner(), FacilityNetwork, OccupantId);
	ObserveResource(ResourceInstance);
	FSRFirstFuelMilestoneModel::ApplyConsistency(ObservedFacts);
}

void USRRunMilestoneSubsystem::ObserveResource(const FSRResourceInstance& ResourceInstance)
{
	if (IsCardResource(ResourceInstance))
	{
		const bool bWasFirstCardExtracted = ObservedFacts.bFirstCardExtracted;
		ObservedFacts.bFirstCardExtracted = true;
		ObservedFacts.bFirstCardProcessed |= HasBeenProcessed(ResourceInstance);
		if (!bWasFirstCardExtracted
			|| FirstResourceFamily == ESRResourceFamily::None)
		{
			FirstResourceFamily = ResolveResourceFamily(ResourceInstance);
		}
	}
	ObservedFacts.bFirstStellarFuelFabricated |= IsStellarFuelResource(ResourceInstance);
}

void USRRunMilestoneSubsystem::ObserveFacility(
	AActor* BodyActor,
	USRFacilityNetworkComponent* FacilityNetwork,
	FName OccupantId)
{
	if (!IsValid(BodyActor) || !IsValid(FacilityNetwork) || OccupantId.IsNone())
	{
		return;
	}

	FSRFacilityInstance FacilityInstance;
	if (!FacilityNetwork->GetFacilityInstance(OccupantId, FacilityInstance)
		|| !IsValid(FacilityInstance.StructureDataAsset.Get()))
	{
		return;
	}
	if (!FirstAutomationBody.IsValid())
	{
		FirstAutomationBody = BodyActor;
	}

	const FSRStructureData StructureData = FacilityInstance.StructureDataAsset->BuildData();
	const ESRStructureBuildRole Role = FSRStructureBuildCatalogBuilder::ResolveRole(StructureData);
	switch (Role)
	{
	case ESRStructureBuildRole::Extraction:
	{
		FSRResourceDepositInstance MiningTarget;
		if (FacilityNetwork->GetFacilityMiningTarget(OccupantId, MiningTarget)
			&& MiningTarget.RemainingAmount > 0
			&& IsValid(MiningTarget.ResourceDataAsset.Get())
			&& MiningTarget.ResourceDataAsset->ResourceClass == ESRResourceClass::Card)
		{
			ObservedFacts.bExtractorPlaced = true;
			SetTargetIfUnset(ExtractorTarget, BodyActor, OccupantId);
		}
		break;
	}
	case ESRStructureBuildRole::FamilyProcessing:
	{
		const ESRResourceFamily FacilityFamily =
			FSRStructureBuildCatalogBuilder::ResolveResourceFamily(StructureData);
		const bool bMatchesFirstCard = FirstResourceFamily != ESRResourceFamily::None
			&& (FacilityFamily == ESRResourceFamily::None
				|| FacilityFamily == FirstResourceFamily);
		if (ObservedFacts.bFirstCardExtracted && bMatchesFirstCard)
		{
			ObservedFacts.bFamilyProcessorPlaced = true;
			SetTargetIfUnset(FamilyProcessorTarget, BodyActor, OccupantId);
		}
		break;
	}
	case ESRStructureBuildRole::StellarFuelFabrication:
		ObservedFacts.bStellarFuelFabricatorPlaced = true;
		SetTargetIfUnset(StellarFuelFabricatorTarget, BodyActor, OccupantId);
		break;
	case ESRStructureBuildRole::Hub:
		ObservedFacts.bHubPlaced = true;
		SetTargetIfUnset(HubTarget, BodyActor, OccupantId);
		break;
	default:
		break;
	}

	VisitFacilityResources(
		FacilityInstance,
		[this](const FSRResourceInstance& ResourceInstance)
		{
			ObserveResource(ResourceInstance);
		});
}

void USRRunMilestoneSubsystem::SetTargetIfUnset(
	FFacilityTarget& Target,
	AActor* BodyActor,
	FName OccupantId)
{
	if (!Target.IsValid() && IsValid(BodyActor) && !OccupantId.IsNone())
	{
		Target.BodyActor = BodyActor;
		Target.OccupantId = OccupantId;
	}
}
