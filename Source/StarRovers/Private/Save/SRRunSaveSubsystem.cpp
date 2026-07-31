#include "Save/SRRunSaveSubsystem.h"

#include "Automation/SRFacilityNetworkComponent.h"
#include "Celestial/SRCelestialBody.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Celestial/SRStar.h"
#include "Conveyor/SRConveyorNetworkComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Logistics/SRSpaceLogisticsSubsystem.h"
#include "Pattern/SRPatternRoutingFilter.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "Simulation/SROrbit.h"
#include "Simulation/SRRunModifierSubsystem.h"
#include "Simulation/SRSolarSystemGenerator.h"
#include "Simulation/SRTimeControlSubsystem.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"
#include "Utility/SRLog.h"

namespace
{
	FString BuildBodyKeyString(const FSRRunCelestialBodyKey& Key)
	{
		return FString::Printf(
			TEXT("%s|%s|%d|%d"),
			*Key.ActorName.ToString(),
			*Key.VariableName,
			static_cast<int32>(Key.BodyCategory),
			Key.GenerationSeed);
	}

	bool MatchesExternalBodyIdentity(
		const FSRRunCelestialBodyKey& Key,
		FName ActorName,
		const FString& VariableName)
	{
		return (!ActorName.IsNone() && Key.ActorName == ActorName)
			|| (!VariableName.IsEmpty() && Key.VariableName == VariableName);
	}

	const FSRRunCelestialBodySaveData* FindSavedBodyByExternalIdentity(
		const FSRRunSaveData& RunData,
		FName ActorName,
		const FString& VariableName)
	{
		if (!ActorName.IsNone())
		{
			for (const FSRRunCelestialBodySaveData& BodyState : RunData.CelestialBodies)
			{
				if (BodyState.BodyKey.ActorName == ActorName)
				{
					return &BodyState;
				}
			}
		}
		const FSRRunCelestialBodySaveData* Match = nullptr;
		if (!VariableName.IsEmpty())
		{
			for (const FSRRunCelestialBodySaveData& BodyState : RunData.CelestialBodies)
			{
				if (BodyState.BodyKey.VariableName != VariableName)
				{
					continue;
				}
				if (Match)
				{
					return nullptr;
				}
				Match = &BodyState;
			}
		}
		return Match;
	}

	const FSRPlacedStructureSaveData* FindSavedStructure(
		const FSRRunCelestialBodySaveData& BodyState,
		FName OccupantId)
	{
		for (const FSRPlacedStructureSaveData& Structure : BodyState.Structures.Structures)
		{
			if (Structure.OccupantId == OccupantId)
			{
				return &Structure;
			}
		}
		return nullptr;
	}

	const FSRFacilityInstance* FindSavedFacility(
		const FSRRunCelestialBodySaveData& BodyState,
		FName OccupantId)
	{
		for (const FSRFacilityInstance& Facility : BodyState.Facilities.Facilities)
		{
			if (Facility.OccupantId == OccupantId)
			{
				return &Facility;
			}
		}
		return nullptr;
	}

	bool ValidateEndpoint(
		const FSRRunSaveData& RunData,
		const FSRSpaceLogisticsHubEndpointSaveData& Endpoint,
		FString& OutFailureReason)
	{
		if (!Endpoint.IsValid())
		{
			OutFailureReason = TEXT("Space-logistics save contains an invalid Hub endpoint identity.");
			return false;
		}
		const FSRRunCelestialBodySaveData* BodyState = FindSavedBodyByExternalIdentity(
			RunData,
			Endpoint.BodyActorName,
			Endpoint.BodyVariableName);
		const FSRPlacedStructureSaveData* Structure = BodyState
			? FindSavedStructure(*BodyState, Endpoint.HubOccupantId)
			: nullptr;
		const FSRFacilityInstance* Facility = BodyState
			? FindSavedFacility(*BodyState, Endpoint.HubOccupantId)
			: nullptr;
		if (!BodyState
			|| !BodyState->bHasSurfaceState
			|| !Structure
			|| !Facility
			|| !IsValid(Facility->FacilityDataAsset.Get())
			|| Facility->FacilityDataAsset->FacilityKind != ESRFacilityKind::Hub)
		{
			OutFailureReason = TEXT("Space-logistics Hub endpoint does not resolve to a saved Hub facility.");
			return false;
		}
		const FName StructureId = Structure->StructureDataAsset->BuildData().StructureId;
		if (!Endpoint.StructureId.IsNone() && Endpoint.StructureId != StructureId)
		{
			OutFailureReason = TEXT("Space-logistics Hub endpoint Structure ID does not match the saved occupant.");
			return false;
		}
		return true;
	}

	bool IsFiniteTravelState(
		float Duration,
		float InitialSpeed,
		float Acceleration,
		float ProgressSeconds,
		float ProgressRatio,
		const FVector& StartLocation,
		const FVector& LaunchVelocity)
	{
		return FMath::IsFinite(Duration) && Duration > 0.0f
			&& FMath::IsFinite(InitialSpeed) && InitialSpeed >= 0.0f
			&& FMath::IsFinite(Acceleration) && Acceleration >= 0.0f
			&& FMath::IsFinite(ProgressSeconds) && ProgressSeconds >= 0.0f
			&& FMath::IsFinite(ProgressRatio) && ProgressRatio >= 0.0f && ProgressRatio <= 1.0f
			&& !StartLocation.ContainsNaN()
			&& !LaunchVelocity.ContainsNaN();
	}

	bool ValidateFacilityStructureRelationship(
		const FSRRunCelestialBodySaveData& BodyState,
		FString& OutFailureReason)
	{
		TSet<FName> FacilityOccupants;
		for (const FSRFacilityInstance& Facility : BodyState.Facilities.Facilities)
		{
			FacilityOccupants.Add(Facility.OccupantId);
			const FSRPlacedStructureSaveData* Structure = FindSavedStructure(BodyState, Facility.OccupantId);
			if (!Structure || Structure->StructureDataAsset.Get() != Facility.StructureDataAsset.Get())
			{
				OutFailureReason = TEXT("Saved facility does not have a matching saved structure occupant.");
				return false;
			}
		}
		for (const FSRPlacedStructureSaveData& Structure : BodyState.Structures.Structures)
		{
			if (!IsValid(Structure.StructureDataAsset.Get()))
			{
				continue;
			}
			const bool bShouldHaveFacility = IsValid(Structure.StructureDataAsset->BuildData().FacilityDataAsset.Get());
			if (bShouldHaveFacility != FacilityOccupants.Contains(Structure.OccupantId))
			{
				OutFailureReason = TEXT("Saved structure/facility topology is incomplete.");
				return false;
			}
		}
		return true;
	}
}

void USRRunSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency(USRTimeControlSubsystem::StaticClass());
	Collection.InitializeDependency(USRRunModifierSubsystem::StaticClass());
	Collection.InitializeDependency(USRAugmentSubsystem::StaticClass());
	Collection.InitializeDependency(USRCelestialBodyRegistrySubsystem::StaticClass());
	Collection.InitializeDependency(USRSpaceLogisticsSubsystem::StaticClass());
}

ASRSolarSystemGenerator* USRRunSaveSubsystem::FindRuntimeGenerator(
	FName PreferredActorName,
	FString& OutFailureReason) const
{
	OutFailureReason.Reset();
	UWorld* World = GetWorld();
	if (!World)
	{
		OutFailureReason = TEXT("The run-save world is unavailable.");
		return nullptr;
	}

	ASRSolarSystemGenerator* OnlyGenerator = nullptr;
	int32 GeneratorCount = 0;
	for (TActorIterator<ASRSolarSystemGenerator> It(World); It; ++It)
	{
		ASRSolarSystemGenerator* Generator = *It;
		if (!IsValid(Generator))
		{
			continue;
		}
		++GeneratorCount;
		OnlyGenerator = Generator;
		if (!PreferredActorName.IsNone() && Generator->GetFName() == PreferredActorName)
		{
			return Generator;
		}
	}

	if (GeneratorCount == 1)
	{
		return OnlyGenerator;
	}
	OutFailureReason = GeneratorCount == 0
		? TEXT("The runtime SolarSystemGenerator is unavailable.")
		: TEXT("The runtime world contains multiple ambiguous SolarSystemGenerators.");
	return nullptr;
}

bool USRRunSaveSubsystem::RegenerateTopologyForRunData(
	const FSRRunSaveData& RunData,
	FString& OutFailureReason)
{
	OutFailureReason.Reset();
	if (!RunData.Generation.IsValid())
	{
		OutFailureReason = TEXT("The run save contains an invalid generation payload.");
		return false;
	}

	ASRSolarSystemGenerator* Generator = FindRuntimeGenerator(
		RunData.Generation.GeneratorActorName,
		OutFailureReason);
	if (!IsValid(Generator))
	{
		return false;
	}
	if (Generator->IsRuntimeGenerationInProgress())
	{
		OutFailureReason = TEXT("The SolarSystemGenerator is already generating a runtime topology.");
		return false;
	}

	int32 CurrentSeed = 0;
	if (Generator->GetLastCompletedGenerationSeed(CurrentSeed)
		&& CurrentSeed == RunData.Generation.RuntimeGenerationSeed)
	{
		return true;
	}
	if (!IsValid(Generator->GenerateRuntimeSystemWithSeed(RunData.Generation.RuntimeGenerationSeed)))
	{
		OutFailureReason = FString::Printf(
			TEXT("Solar-system regeneration failed for saved Seed %d."),
			RunData.Generation.RuntimeGenerationSeed);
		return false;
	}

	int32 CompletedSeed = 0;
	if (!Generator->GetLastCompletedGenerationSeed(CompletedSeed)
		|| CompletedSeed != RunData.Generation.RuntimeGenerationSeed)
	{
		OutFailureReason = TEXT("Solar-system regeneration completed without the requested Seed authority.");
		return false;
	}
	return true;
}

bool USRRunSaveSubsystem::GatherCelestialBodies(
	TArray<AActor*>& OutBodies,
	FString& OutFailureReason)
{
	OutBodies.Reset();
	OutFailureReason.Reset();
	UWorld* World = GetWorld();
	USRCelestialBodyRegistrySubsystem* Registry = World
		? World->GetSubsystem<USRCelestialBodyRegistrySubsystem>()
		: nullptr;
	if (!IsValid(Registry))
	{
		OutFailureReason = TEXT("The celestial-body registry is unavailable.");
		return false;
	}
	Registry->RefreshCelestialBodies();
	Registry->GetCelestialBodies(OutBodies);
	if (OutBodies.IsEmpty())
	{
		OutFailureReason = TEXT("A run save requires at least one generated celestial body.");
		return false;
	}
	for (AActor* Body : OutBodies)
	{
		if (!IsValid(Body) || !Cast<ASRCelestialBody>(Body))
		{
			OutFailureReason = TEXT("Every saved celestial body must use the native Star Rovers runtime actor.");
			return false;
		}
	}
	return true;
}

FSRRunCelestialBodyKey USRRunSaveSubsystem::BuildBodyKey(const AActor* BodyActor) const
{
	FSRRunCelestialBodyKey Key;
	const ASRCelestialBody* CelestialBody = Cast<ASRCelestialBody>(BodyActor);
	if (!IsValid(CelestialBody))
	{
		return Key;
	}
	const FSRCelestialBodyData BodyData = CelestialBody->GetData();
	Key.ActorName = CelestialBody->GetFName();
	Key.VariableName = BodyData.VariableName.ToString();
	Key.BodyCategory = BodyData.BodyCategory;
	Key.GenerationSeed = BodyData.GenerationSeed;
	return Key;
}

AActor* USRRunSaveSubsystem::ResolveSavedBody(
	const FSRRunCelestialBodyKey& BodyKey,
	const TArray<AActor*>& CurrentBodies,
	FString& OutFailureReason) const
{
	OutFailureReason.Reset();
	AActor* ActorNameMatch = nullptr;
	for (AActor* Body : CurrentBodies)
	{
		const FSRRunCelestialBodyKey CurrentKey = BuildBodyKey(Body);
		if (!BodyKey.ActorName.IsNone() && CurrentKey.ActorName == BodyKey.ActorName)
		{
			ActorNameMatch = Body;
			break;
		}
	}
	if (ActorNameMatch)
	{
		const FSRRunCelestialBodyKey CurrentKey = BuildBodyKey(ActorNameMatch);
		if (CurrentKey.BodyCategory == BodyKey.BodyCategory
			&& CurrentKey.GenerationSeed == BodyKey.GenerationSeed)
		{
			return ActorNameMatch;
		}
		OutFailureReason = TEXT("Saved actor name resolved, but its category or generation seed changed.");
		return nullptr;
	}

	AActor* VariableNameMatch = nullptr;
	for (AActor* Body : CurrentBodies)
	{
		const FSRRunCelestialBodyKey CurrentKey = BuildBodyKey(Body);
		if (!BodyKey.VariableName.IsEmpty()
			&& CurrentKey.VariableName == BodyKey.VariableName
			&& CurrentKey.BodyCategory == BodyKey.BodyCategory
			&& CurrentKey.GenerationSeed == BodyKey.GenerationSeed)
		{
			if (VariableNameMatch)
			{
				OutFailureReason = TEXT("Saved celestial variable name is ambiguous in the current world.");
				return nullptr;
			}
			VariableNameMatch = Body;
		}
	}
	if (!VariableNameMatch)
	{
		OutFailureReason = FString::Printf(TEXT("Saved celestial body '%s' is absent from the current generated topology."), *BodyKey.VariableName);
	}
	return VariableNameMatch;
}

bool USRRunSaveSubsystem::CaptureRunData(FSRRunSaveData& OutRunData)
{
	OutRunData = FSRRunSaveData();
	OutRunData.SaveId = FGuid::NewGuid();
	LastSaveError.Reset();
	UWorld* World = GetWorld();
	USRTimeControlSubsystem* TimeControl = World ? World->GetSubsystem<USRTimeControlSubsystem>() : nullptr;
	USRRunModifierSubsystem* RunModifiers = World ? World->GetSubsystem<USRRunModifierSubsystem>() : nullptr;
	USRAugmentSubsystem* Augments = World ? World->GetSubsystem<USRAugmentSubsystem>() : nullptr;
	USRSpaceLogisticsSubsystem* SpaceLogistics = World ? World->GetSubsystem<USRSpaceLogisticsSubsystem>() : nullptr;
	if (!IsValid(TimeControl) || !IsValid(RunModifiers) || !IsValid(Augments) || !IsValid(SpaceLogistics))
	{
		LastSaveError = TEXT("One or more run subsystems are unavailable.");
		return false;
	}
	ASRSolarSystemGenerator* Generator = FindRuntimeGenerator(NAME_None, LastSaveError);
	int32 RuntimeGenerationSeed = 0;
	if (!IsValid(Generator)
		|| Generator->IsRuntimeGenerationInProgress()
		|| !Generator->GetLastCompletedGenerationSeed(RuntimeGenerationSeed))
	{
		if (LastSaveError.IsEmpty())
		{
			LastSaveError = TEXT("A run save requires a completed SolarSystemGenerator topology.");
		}
		return false;
	}
	OutRunData.Generation.GeneratorActorName = Generator->GetFName();
	OutRunData.Generation.RuntimeGenerationSeed = RuntimeGenerationSeed;

	TimeControl->ExportSaveData(OutRunData.TimeControl);
	RunModifiers->ExportSaveData(OutRunData.RunModifiers);
	Augments->ExportSaveData(OutRunData.AugmentOffer);
	SpaceLogistics->ExportSaveData(OutRunData.SpaceLogistics);

	TArray<AActor*> Bodies;
	if (!GatherCelestialBodies(Bodies, LastSaveError))
	{
		return false;
	}
	Bodies.Sort([this](const AActor& Left, const AActor& Right)
	{
		return BuildBodyKey(&Left).ActorName.LexicalLess(BuildBodyKey(&Right).ActorName);
	});
	for (AActor* Body : Bodies)
	{
		FSRRunCelestialBodySaveData& BodyState = OutRunData.CelestialBodies.AddDefaulted_GetRef();
		BodyState.BodyKey = BuildBodyKey(Body);
		BodyState.ActorTransform = Body->GetActorTransform();
		ASRCelestialBody* CelestialBody = CastChecked<ASRCelestialBody>(Body);
		if (USROrbit* Orbit = CelestialBody->GetOrbit())
		{
			BodyState.bHasOrbitState = true;
			Orbit->ExportSaveData(BodyState.OrbitState);
		}

		USRPlanetSurfaceGrid* SurfaceGrid = CelestialBody->GetSurfaceGrid();
		if (IsValid(SurfaceGrid))
		{
			USRStructureInstanceManagerComponent* Structures = Body->FindComponentByClass<USRStructureInstanceManagerComponent>();
			USRFacilityNetworkComponent* Facilities = Body->FindComponentByClass<USRFacilityNetworkComponent>();
			USRConveyorNetworkComponent* Conveyors = Body->FindComponentByClass<USRConveyorNetworkComponent>();
			if (!IsValid(Structures) || !IsValid(Facilities) || !IsValid(Conveyors))
			{
				LastSaveError = FString::Printf(TEXT("Surface body '%s' is missing a persistence component."), *Body->GetName());
				return false;
			}
			BodyState.bHasSurfaceState = true;
			Structures->ExportSaveData(BodyState.Structures);
			Facilities->ExportSaveData(BodyState.Facilities);
			Conveyors->ExportSaveData(BodyState.Conveyors);
		}
		if (ASRStar* Star = Cast<ASRStar>(Body))
		{
			BodyState.bHasStarState = true;
			Star->ExportSaveData(BodyState.StarState);
		}
	}

	FString ValidationFailure;
	if (!CanRestoreRunData(OutRunData, ValidationFailure))
	{
		LastSaveError = FString::Printf(TEXT("Captured run state failed self-validation: %s"), *ValidationFailure);
		OutRunData = FSRRunSaveData();
		return false;
	}
	return true;
}

bool USRRunSaveSubsystem::CanRestoreRunData(
	const FSRRunSaveData& RunData,
	FString& OutFailureReason)
{
	OutFailureReason.Reset();
	if (!StarRovers::Save::Run::IsSupportedVersion(RunData.Version))
	{
		OutFailureReason = FString::Printf(TEXT("Unsupported unified run-save version %d."), RunData.Version);
		return false;
	}
	if (!RunData.SaveId.IsValid())
	{
		OutFailureReason = TEXT("Unified run save has no valid save identity.");
		return false;
	}
	if (!RunData.Generation.IsValid())
	{
		OutFailureReason = TEXT("Unified run save has an invalid or incompatible generation payload.");
		return false;
	}
	ASRSolarSystemGenerator* Generator = FindRuntimeGenerator(
		RunData.Generation.GeneratorActorName,
		OutFailureReason);
	int32 CurrentRuntimeSeed = 0;
	if (!IsValid(Generator)
		|| Generator->IsRuntimeGenerationInProgress()
		|| !Generator->GetLastCompletedGenerationSeed(CurrentRuntimeSeed)
		|| CurrentRuntimeSeed != RunData.Generation.RuntimeGenerationSeed)
	{
		if (OutFailureReason.IsEmpty())
		{
			OutFailureReason = TEXT("The current celestial topology was not generated from the saved runtime Seed.");
		}
		return false;
	}
	UWorld* World = GetWorld();
	USRTimeControlSubsystem* TimeControl = World ? World->GetSubsystem<USRTimeControlSubsystem>() : nullptr;
	USRRunModifierSubsystem* RunModifiers = World ? World->GetSubsystem<USRRunModifierSubsystem>() : nullptr;
	USRAugmentSubsystem* Augments = World ? World->GetSubsystem<USRAugmentSubsystem>() : nullptr;
	if (!IsValid(TimeControl) || !IsValid(RunModifiers) || !IsValid(Augments))
	{
		OutFailureReason = TEXT("Run-state validation requires time, modifier, and Augment subsystems.");
		return false;
	}
	if (!TimeControl->CanImportSaveData(RunData.TimeControl, OutFailureReason)
		|| !RunModifiers->CanImportSaveData(RunData.RunModifiers, OutFailureReason)
		|| !Augments->CanImportSaveData(RunData.AugmentOffer, OutFailureReason))
	{
		return false;
	}
	for (const FSRActiveTrialState& Trial : RunData.RunModifiers.ActiveTrials)
	{
		if (RunData.TimeControl.CurrentCycleIndex < Trial.StartCycleIndex
			|| RunData.TimeControl.CurrentCycleIndex >= Trial.EndCycleIndexExclusive)
		{
			OutFailureReason = FString::Printf(TEXT("Trial '%s' is not active at the saved Cycle."), *Trial.TrialId.ToString());
			return false;
		}
	}
	TMap<FName, int32> SavedAugmentStacks;
	for (const FSRRunModifierAugmentStackSaveData& Stack : RunData.RunModifiers.AugmentStacks)
	{
		SavedAugmentStacks.Add(Stack.AugmentId, Stack.StackCount);
	}
	for (const FName OfferedAugmentId : RunData.AugmentOffer.OfferedAugmentIds)
	{
		const USRRunAugmentDataAsset* Augment = RunModifiers->FindAugmentDataAsset(OfferedAugmentId);
		if (!IsValid(Augment) || SavedAugmentStacks.FindRef(OfferedAugmentId) >= Augment->MaximumStacks)
		{
			OutFailureReason = FString::Printf(TEXT("Pending Augment '%s' is already at its saved stack cap."), *OfferedAugmentId.ToString());
			return false;
		}
	}

	TArray<AActor*> CurrentBodies;
	if (!GatherCelestialBodies(CurrentBodies, OutFailureReason))
	{
		return false;
	}
	if (CurrentBodies.Num() != RunData.CelestialBodies.Num())
	{
		OutFailureReason = TEXT("Saved and current celestial-body counts differ.");
		return false;
	}
	TSet<FString> SavedKeys;
	TSet<AActor*> ResolvedBodies;
	for (const FSRRunCelestialBodySaveData& BodyState : RunData.CelestialBodies)
	{
		bool bDuplicateKey = false;
		SavedKeys.Add(BuildBodyKeyString(BodyState.BodyKey), &bDuplicateKey);
		if (!BodyState.BodyKey.IsValid() || bDuplicateKey || BodyState.ActorTransform.ContainsNaN())
		{
			OutFailureReason = TEXT("Unified run save contains an invalid or duplicate celestial-body record.");
			return false;
		}
		AActor* Body = ResolveSavedBody(BodyState.BodyKey, CurrentBodies, OutFailureReason);
		bool bDuplicateActor = false;
		ResolvedBodies.Add(Body, &bDuplicateActor);
		if (!IsValid(Body) || bDuplicateActor)
		{
			if (OutFailureReason.IsEmpty())
			{
				OutFailureReason = TEXT("Multiple saved celestial records resolve to one actor.");
			}
			return false;
		}

		ASRCelestialBody* CelestialBody = CastChecked<ASRCelestialBody>(Body);
		USROrbit* Orbit = CelestialBody->GetOrbit();
		if (BodyState.bHasOrbitState != IsValid(Orbit)
			|| (BodyState.bHasOrbitState && !Orbit->CanImportSaveData(BodyState.OrbitState, OutFailureReason)))
		{
			if (OutFailureReason.IsEmpty())
			{
				OutFailureReason = TEXT("Saved orbit topology differs from the current body.");
			}
			return false;
		}

		USRPlanetSurfaceGrid* SurfaceGrid = CelestialBody->GetSurfaceGrid();
		if (BodyState.bHasSurfaceState != IsValid(SurfaceGrid))
		{
			OutFailureReason = TEXT("Saved surface topology differs from the current body.");
			return false;
		}
		if (BodyState.bHasSurfaceState)
		{
			USRStructureInstanceManagerComponent* Structures = Body->FindComponentByClass<USRStructureInstanceManagerComponent>();
			USRFacilityNetworkComponent* Facilities = Body->FindComponentByClass<USRFacilityNetworkComponent>();
			USRConveyorNetworkComponent* Conveyors = Body->FindComponentByClass<USRConveyorNetworkComponent>();
			if (!IsValid(Structures)
				|| !IsValid(Facilities)
				|| !IsValid(Conveyors)
				|| !Structures->CanImportSaveData(SurfaceGrid, BodyState.Structures, OutFailureReason)
				|| !Facilities->CanImportSaveData(BodyState.Facilities, OutFailureReason)
				|| !Conveyors->CanImportSaveData(SurfaceGrid, BodyState.Conveyors, OutFailureReason)
				|| !ValidateFacilityStructureRelationship(BodyState, OutFailureReason))
			{
				if (OutFailureReason.IsEmpty())
				{
					OutFailureReason = TEXT("Saved surface state failed component validation.");
				}
				return false;
			}
		}

		ASRStar* Star = Cast<ASRStar>(Body);
		if (BodyState.bHasStarState != IsValid(Star)
			|| (BodyState.bHasStarState && !Star->CanImportSaveData(BodyState.StarState, OutFailureReason)))
		{
			if (OutFailureReason.IsEmpty())
			{
				OutFailureReason = TEXT("Saved star topology or contract state is invalid.");
			}
			return false;
		}
	}

	return ValidateSpaceLogisticsAgainstRun(RunData, OutFailureReason);
}

bool USRRunSaveSubsystem::ValidateSpaceLogisticsAgainstRun(
	const FSRRunSaveData& RunData,
	FString& OutFailureReason) const
{
	const FSRSpaceLogisticsSaveData& Logistics = RunData.SpaceLogistics;
	if (!StarRovers::SpaceLogistics::PatternSave::IsSupportedVersion(Logistics.Version)
		|| Logistics.NextHubRouteSequence < 1
		|| Logistics.NextStarFuelMissileSequence < 1)
	{
		OutFailureReason = TEXT("Unified run save contains an unsupported space-logistics payload.");
		return false;
	}

	TSet<FName> RouteIds;
	TSet<FString> RouteEndpointPairs;
	for (const FSRSpaceLogisticsHubRouteSaveData& Route : Logistics.HubRoutes)
	{
		bool bDuplicateId = false;
		RouteIds.Add(Route.RouteId, &bDuplicateId);
		const FSRPatternRoutingFilter CargoFilter = StarRovers::SpaceLogistics::PatternSave::ResolveRouteCargoFilter(
			Logistics.Version,
			Route.CargoFilter,
			Route.CargoResourceId);
		if (bDuplicateId
			|| !Route.IsValid()
			|| !CargoFilter.IsCanonical()
			|| !StarRovers::PatternRouting::IsValidOrEmptyPatternPayload(Route.Cargo)
			|| Route.MaxCargoStackCount < 1
			|| !StaticEnum<ESRSpaceLogisticsHubRoutePhase>()->IsValidEnumValue(static_cast<int64>(Route.Phase))
			|| !StaticEnum<ESRSpaceLogisticsHubRouteDockSide>()->IsValidEnumValue(static_cast<int64>(Route.CurrentDockSide))
			|| !IsFiniteTravelState(
				Route.TravelDurationSeconds,
				Route.InitialSpeedUnitsPerSecond,
				Route.LaunchAccelerationUnitsPerSecondSquared,
				Route.TravelProgressSeconds,
				Route.TravelProgressRatio,
				Route.TravelStartWorldLocation,
				Route.LaunchWorldVelocity)
			|| !ValidateEndpoint(RunData, Route.SourceHub, OutFailureReason)
			|| !ValidateEndpoint(RunData, Route.DestinationHub, OutFailureReason))
		{
			if (OutFailureReason.IsEmpty())
			{
				OutFailureReason = TEXT("Unified run save contains an invalid space-logistics route.");
			}
			return false;
		}
		const FString EndpointPair = FString::Printf(
			TEXT("%s|%s>%s|%s"),
			*Route.SourceHub.BodyActorName.ToString(),
			*Route.SourceHub.HubOccupantId.ToString(),
			*Route.DestinationHub.BodyActorName.ToString(),
			*Route.DestinationHub.HubOccupantId.ToString());
		bool bDuplicatePair = false;
		RouteEndpointPairs.Add(EndpointPair, &bDuplicatePair);
		if (bDuplicatePair && !Route.bDebugLocalOrbit)
		{
			OutFailureReason = TEXT("Unified run save contains duplicate Hub routes.");
			return false;
		}
	}

	TSet<FName> MissileIds;
	for (const FSRSpaceLogisticsStarFuelMissileSaveData& Missile : Logistics.StarFuelMissiles)
	{
		bool bDuplicateId = false;
		MissileIds.Add(Missile.MissileId, &bDuplicateId);
		const FSRRunCelestialBodySaveData* TargetStar = FindSavedBodyByExternalIdentity(
			RunData,
			Missile.TargetStarActorName,
			Missile.TargetStarVariableName);
		if (bDuplicateId
			|| !Missile.IsValid()
			|| !TargetStar
			|| !TargetStar->bHasStarState
			|| !ValidateEndpoint(RunData, Missile.SourceHub, OutFailureReason)
			|| !IsFiniteTravelState(
				Missile.TravelDurationSeconds,
				Missile.InitialSpeedUnitsPerSecond,
				Missile.LaunchAccelerationUnitsPerSecondSquared,
				Missile.TravelProgressSeconds,
				Missile.TravelProgressRatio,
				Missile.TravelStartWorldLocation,
				Missile.LaunchWorldVelocity))
		{
			if (OutFailureReason.IsEmpty())
			{
				OutFailureReason = TEXT("Unified run save contains an invalid star-fuel missile.");
			}
			return false;
		}
	}
	return true;
}

bool USRRunSaveSubsystem::ApplyRunDataUnchecked(
	const FSRRunSaveData& RunData,
	FString& OutFailureReason)
{
	OutFailureReason.Reset();
	UWorld* World = GetWorld();
	USRTimeControlSubsystem* TimeControl = World ? World->GetSubsystem<USRTimeControlSubsystem>() : nullptr;
	USRRunModifierSubsystem* RunModifiers = World ? World->GetSubsystem<USRRunModifierSubsystem>() : nullptr;
	USRAugmentSubsystem* Augments = World ? World->GetSubsystem<USRAugmentSubsystem>() : nullptr;
	USRSpaceLogisticsSubsystem* SpaceLogistics = World ? World->GetSubsystem<USRSpaceLogisticsSubsystem>() : nullptr;
	TArray<AActor*> CurrentBodies;
	if (!IsValid(TimeControl)
		|| !IsValid(RunModifiers)
		|| !IsValid(Augments)
		|| !IsValid(SpaceLogistics)
		|| !GatherCelestialBodies(CurrentBodies, OutFailureReason))
	{
		if (OutFailureReason.IsEmpty())
		{
			OutFailureReason = TEXT("Run subsystems disappeared during restore.");
		}
		return false;
	}

	FSRTimeControlSaveData PausedTime = RunData.TimeControl;
	PausedTime.bSimulationPaused = true;
	if (!TimeControl->ImportSaveData(PausedTime) || !RunModifiers->ImportSaveData(RunData.RunModifiers))
	{
		OutFailureReason = TEXT("Failed to restore time or run-modifier authority.");
		return false;
	}

	for (const FSRRunCelestialBodySaveData& BodyState : RunData.CelestialBodies)
	{
		AActor* Body = ResolveSavedBody(BodyState.BodyKey, CurrentBodies, OutFailureReason);
		if (BodyState.bHasSurfaceState)
		{
			Body->FindComponentByClass<USRConveyorNetworkComponent>()->ClearConveyors();
		}
	}
	for (const FSRRunCelestialBodySaveData& BodyState : RunData.CelestialBodies)
	{
		AActor* Body = ResolveSavedBody(BodyState.BodyKey, CurrentBodies, OutFailureReason);
		if (!BodyState.bHasSurfaceState)
		{
			continue;
		}
		USRPlanetSurfaceGrid* SurfaceGrid = CastChecked<ASRCelestialBody>(Body)->GetSurfaceGrid();
		if (!Body->FindComponentByClass<USRStructureInstanceManagerComponent>()->ImportSaveData(SurfaceGrid, BodyState.Structures))
		{
			OutFailureReason = FString::Printf(TEXT("Failed to restore structures on '%s'."), *Body->GetName());
			return false;
		}
	}
	for (const FSRRunCelestialBodySaveData& BodyState : RunData.CelestialBodies)
	{
		AActor* Body = ResolveSavedBody(BodyState.BodyKey, CurrentBodies, OutFailureReason);
		if (BodyState.bHasSurfaceState
			&& !Body->FindComponentByClass<USRFacilityNetworkComponent>()->ImportSaveData(BodyState.Facilities))
		{
			OutFailureReason = FString::Printf(TEXT("Failed to restore facilities on '%s'."), *Body->GetName());
			return false;
		}
	}
	for (const FSRRunCelestialBodySaveData& BodyState : RunData.CelestialBodies)
	{
		AActor* Body = ResolveSavedBody(BodyState.BodyKey, CurrentBodies, OutFailureReason);
		if (BodyState.bHasSurfaceState)
		{
			USRPlanetSurfaceGrid* SurfaceGrid = CastChecked<ASRCelestialBody>(Body)->GetSurfaceGrid();
			if (!Body->FindComponentByClass<USRConveyorNetworkComponent>()->ImportSaveData(SurfaceGrid, BodyState.Conveyors))
			{
				OutFailureReason = FString::Printf(TEXT("Failed to restore conveyors on '%s'."), *Body->GetName());
				return false;
			}
		}
		if (BodyState.bHasStarState && !CastChecked<ASRStar>(Body)->ImportSaveData(BodyState.StarState))
		{
			OutFailureReason = FString::Printf(TEXT("Failed to restore stellar contract on '%s'."), *Body->GetName());
			return false;
		}
	}

	for (const FSRRunCelestialBodySaveData& BodyState : RunData.CelestialBodies)
	{
		AActor* Body = ResolveSavedBody(BodyState.BodyKey, CurrentBodies, OutFailureReason);
		Body->SetActorTransform(BodyState.ActorTransform, false, nullptr, ETeleportType::TeleportPhysics);
		if (BodyState.bHasOrbitState)
		{
			CastChecked<ASRCelestialBody>(Body)->GetOrbit()->ImportSaveData(BodyState.OrbitState);
		}
	}
	// Parent orbits may have moved after a child restored, so the exact saved transforms
	// are the authoritative frame until the next deterministic orbit tick.
	for (const FSRRunCelestialBodySaveData& BodyState : RunData.CelestialBodies)
	{
		AActor* Body = ResolveSavedBody(BodyState.BodyKey, CurrentBodies, OutFailureReason);
		Body->SetActorTransform(BodyState.ActorTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}

	SpaceLogistics->RefreshHubEndpoints();
	if (!SpaceLogistics->ImportSaveData(RunData.SpaceLogistics))
	{
		OutFailureReason = TEXT("Failed to restore space-logistics routes after surface topology commit.");
		return false;
	}
	if (!Augments->ImportSaveData(RunData.AugmentOffer))
	{
		OutFailureReason = TEXT("Failed to restore the pending Augment offer.");
		return false;
	}
	if (!TimeControl->ImportSaveData(RunData.TimeControl))
	{
		OutFailureReason = TEXT("Failed to restore the final simulation clock state.");
		return false;
	}
	return true;
}

bool USRRunSaveSubsystem::RestoreRunData(const FSRRunSaveData& RunData)
{
	LastSaveError.Reset();
	if (!StarRovers::Save::Run::IsSupportedVersion(RunData.Version)
		|| !RunData.SaveId.IsValid()
		|| !RunData.Generation.IsValid())
	{
		LastSaveError = TEXT("The run save envelope or generation payload is invalid or unsupported.");
		return false;
	}
	FString GeneratorFailure;
	ASRSolarSystemGenerator* Generator = FindRuntimeGenerator(
		RunData.Generation.GeneratorActorName,
		GeneratorFailure);
	if (!IsValid(Generator) || Generator->IsRuntimeGenerationInProgress())
	{
		LastSaveError = GeneratorFailure.IsEmpty()
			? TEXT("The SolarSystemGenerator is busy and cannot restore a run.")
			: GeneratorFailure;
		return false;
	}

	FSRRunSaveData RollbackData;
	if (!CaptureRunData(RollbackData))
	{
		if (LastSaveError.IsEmpty())
		{
			LastSaveError = TEXT("Could not capture rollback state before run restore.");
		}
		return false;
	}

	FString CommitFailure;
	bool bCommitted = RegenerateTopologyForRunData(RunData, CommitFailure);
	if (bCommitted)
	{
		bCommitted = CanRestoreRunData(RunData, CommitFailure);
	}
	if (bCommitted)
	{
		bCommitted = ApplyRunDataUnchecked(RunData, CommitFailure);
	}
	if (bCommitted)
	{
		LastSaveError.Reset();
		return true;
	}

	FString RollbackFailure;
	bool bRolledBack = RegenerateTopologyForRunData(RollbackData, RollbackFailure);
	if (bRolledBack)
	{
		bRolledBack = CanRestoreRunData(RollbackData, RollbackFailure);
	}
	if (bRolledBack)
	{
		bRolledBack = ApplyRunDataUnchecked(RollbackData, RollbackFailure);
	}
	LastSaveError = bRolledBack
		? FString::Printf(TEXT("Run restore failed and was rolled back: %s"), *CommitFailure)
		: FString::Printf(TEXT("Run restore failed (%s), and rollback also failed (%s)."), *CommitFailure, *RollbackFailure);
	return false;
}

bool USRRunSaveSubsystem::SaveRunToSlot(const FString& SlotName, int32 UserIndex)
{
	LastSaveError.Reset();
	if (SlotName.IsEmpty() || UserIndex < 0)
	{
		LastSaveError = TEXT("Save slot name must be non-empty and user index non-negative.");
		return false;
	}
	FSRRunSaveData RunData;
	if (!CaptureRunData(RunData))
	{
		return false;
	}
	USRRunSaveGame* SaveGame = NewObject<USRRunSaveGame>(this);
	SaveGame->RunData = MoveTemp(RunData);
	if (!UGameplayStatics::SaveGameToSlot(SaveGame, SlotName, UserIndex))
	{
		LastSaveError = FString::Printf(TEXT("Engine failed to write run slot '%s'."), *SlotName);
		return false;
	}
	return true;
}

bool USRRunSaveSubsystem::LoadRunFromSlot(const FString& SlotName, int32 UserIndex)
{
	LastSaveError.Reset();
	if (SlotName.IsEmpty() || UserIndex < 0)
	{
		LastSaveError = TEXT("Save slot name must be non-empty and user index non-negative.");
		return false;
	}
	USRRunSaveGame* SaveGame = Cast<USRRunSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
	if (!IsValid(SaveGame))
	{
		LastSaveError = FString::Printf(TEXT("Run slot '%s' is missing or has the wrong save class."), *SlotName);
		return false;
	}
	return RestoreRunData(SaveGame->RunData);
}

FString USRRunSaveSubsystem::GetLastSaveError() const
{
	return LastSaveError;
}
