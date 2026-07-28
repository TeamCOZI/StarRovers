#include "SRSpaceLogisticsSaveAdapter.h"

#include "Automation/SRResourceInstanceOperations.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Logistics/SRFleetCapacityV2.h"
#include "Logistics/SRConditionedTransitV2.h"
#include "SRSpaceLogisticsRoutePathResolver.h"
#include "SRSpaceLogisticsRouteRegistry.h"
#include "SRSpaceLogisticsRouteVisualController.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "Utility/SRLog.h"

namespace
{
	bool IsConditioningRoutePhase(ESRSpaceLogisticsHubRoutePhase Phase)
	{
		return Phase == ESRSpaceLogisticsHubRoutePhase::ConditioningAtDestination
			|| Phase == ESRSpaceLogisticsHubRoutePhase::ConditioningAtSource;
	}

	bool IsKnownRoutePhase(ESRSpaceLogisticsHubRoutePhase Phase)
	{
		switch (Phase)
		{
		case ESRSpaceLogisticsHubRoutePhase::Idle:
		case ESRSpaceLogisticsHubRoutePhase::WaitingForCargo:
		case ESRSpaceLogisticsHubRoutePhase::TravelingToDestination:
		case ESRSpaceLogisticsHubRoutePhase::UnloadingAtDestination:
		case ESRSpaceLogisticsHubRoutePhase::TravelingToSource:
		case ESRSpaceLogisticsHubRoutePhase::UnloadingAtSource:
		case ESRSpaceLogisticsHubRoutePhase::Blocked:
		case ESRSpaceLogisticsHubRoutePhase::WaitingForFleetCapacity:
		case ESRSpaceLogisticsHubRoutePhase::ConditioningAtDestination:
		case ESRSpaceLogisticsHubRoutePhase::ConditioningAtSource:
			return true;
		default:
			return false;
		}
	}

	bool IsValidConditioningSavePayload(
		const FSRSpaceLogisticsHubRouteSaveData& RouteSaveData,
		int32 SourceSaveVersion)
	{
		if (!IsKnownRoutePhase(RouteSaveData.Phase))
		{
			return false;
		}
		if (!IsConditioningRoutePhase(RouteSaveData.Phase))
		{
			return true;
		}
		if (SourceSaveVersion < FSRSpaceLogisticsSaveData::ConditioningDwellVersion
			|| RouteSaveData.RouteProfile != ESRSpaceLogisticsRouteProfileV2::ConditionedHold
			|| !FSRConditionedTransitV2::GetModuleRules(RouteSaveData.ConditionedTransitModule).IsConditionedModule()
			|| RouteSaveData.Cargo.ResourceId.IsNone()
			|| RouteSaveData.Cargo.StackCount <= 0
			|| !FSRConditionedTransitV2::IsCargoCompatible(RouteSaveData.ConditionedTransitModule, RouteSaveData.Cargo)
			|| !FMath::IsFinite(RouteSaveData.ConditioningDurationSeconds)
			|| !FMath::IsFinite(RouteSaveData.ConditioningProgressSeconds)
			|| RouteSaveData.ConditioningDurationSeconds <= 0.0f
			|| RouteSaveData.ConditioningProgressSeconds < 0.0f
			|| RouteSaveData.ConditioningProgressSeconds > RouteSaveData.ConditioningDurationSeconds)
		{
			return false;
		}
		return (RouteSaveData.Phase == ESRSpaceLogisticsHubRoutePhase::ConditioningAtDestination
				&& RouteSaveData.CurrentDockSide == ESRSpaceLogisticsHubRouteDockSide::Destination)
			|| (RouteSaveData.Phase == ESRSpaceLogisticsHubRoutePhase::ConditioningAtSource
				&& RouteSaveData.CurrentDockSide == ESRSpaceLogisticsHubRouteDockSide::Source);
	}
}

void FSRSpaceLogisticsSaveAdapter::ExportSaveData(
	const USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	const TArray<FSRSpaceLogisticsHubRoute>& HubRoutes,
	int32 NextHubRouteSequence,
	int64 NextFleetDepartureQueueSequence,
	const TArray<FSRSpaceLogisticsStarFuelMissile>& StarFuelMissiles,
	int32 NextStarFuelMissileSequence,
	FSRSpaceLogisticsSaveData& OutSaveData)
{
	OutSaveData = FSRSpaceLogisticsSaveData();
	OutSaveData.NextHubRouteSequence = FMath::Max(1, NextHubRouteSequence);
	OutSaveData.NextFleetDepartureQueueSequence = FMath::Max<int64>(1, NextFleetDepartureQueueSequence);
	OutSaveData.NextStarFuelMissileSequence = FMath::Max(1, NextStarFuelMissileSequence);
	OutSaveData.HubRoutes.Reserve(HubRoutes.Num());
	OutSaveData.StarFuelMissiles.Reserve(StarFuelMissiles.Num());

	for (const FSRSpaceLogisticsHubRoute& HubRoute : HubRoutes)
	{
		FSRSpaceLogisticsHubRouteSaveData RouteSaveData;
		if (BuildRouteSaveData(SpaceLogisticsSubsystem, HubRoute, RouteSaveData))
		{
			OutSaveData.HubRoutes.Add(RouteSaveData);
		}
	}

	for (const FSRSpaceLogisticsStarFuelMissile& Missile : StarFuelMissiles)
	{
		FSRSpaceLogisticsStarFuelMissileSaveData MissileSaveData;
		if (BuildMissileSaveData(SpaceLogisticsSubsystem, Missile, MissileSaveData))
		{
			OutSaveData.StarFuelMissiles.Add(MissileSaveData);
		}
	}
}

bool FSRSpaceLogisticsSaveAdapter::ImportSaveData(
	USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	const FSRSpaceLogisticsSaveData& SaveData,
	TArray<FSRSpaceLogisticsHubRoute>& HubRoutes,
	int32& NextHubRouteSequence,
	int64& NextFleetDepartureQueueSequence,
	TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId,
	TArray<FSRSpaceLogisticsStarFuelMissile>& StarFuelMissiles,
	int32& NextStarFuelMissileSequence,
	TMap<FName, TObjectPtr<ASRSpaceshipActor>>& StarFuelMissileActorsByMissileId,
	TMap<FString, FSRSpaceLogisticsHubEndpointMotionSample>& HubEndpointMotionSamples)
{
	if (!SaveData.IsSupportedVersion())
	{
		SR_LOG(SpaceLogistics,
			LogTemp,
			Warning,
			TEXT("[SpaceLogistics] Save data import rejected: UnsupportedVersion=%d SupportedRange=%d-%d"),
			SaveData.Version,
			FSRSpaceLogisticsSaveData::InitialVersion,
			FSRSpaceLogisticsSaveData::CurrentVersion);
		return false;
	}

	FSRSpaceLogisticsRouteVisualController::Clear(SpaceshipActorsByRouteId);
	FSRSpaceLogisticsRouteVisualController::Clear(StarFuelMissileActorsByMissileId);
	HubRoutes.Reset();
	StarFuelMissiles.Reset();
	HubEndpointMotionSamples.Reset();
	NextHubRouteSequence = FMath::Max(1, SaveData.NextHubRouteSequence);
	NextFleetDepartureQueueSequence = SaveData.Version >= FSRSpaceLogisticsSaveData::FleetCapacityVersion
		? FMath::Max<int64>(1, SaveData.NextFleetDepartureQueueSequence)
		: 1;
	NextStarFuelMissileSequence = FMath::Max(1, SaveData.NextStarFuelMissileSequence);

	int32 ImportedRouteCount = 0;
	for (const FSRSpaceLogisticsHubRouteSaveData& RouteSaveData : SaveData.HubRoutes)
	{
		if (ImportRoute(SpaceLogisticsSubsystem, RouteSaveData, SaveData.Version, HubRoutes))
		{
			++ImportedRouteCount;
		}
	}
	for (const FSRSpaceLogisticsHubRoute& HubRoute : HubRoutes)
	{
		NextFleetDepartureQueueSequence = FMath::Max(
			NextFleetDepartureQueueSequence,
			HubRoute.FleetDepartureQueueSequence + 1);
	}

	int32 ImportedMissileCount = 0;
	for (const FSRSpaceLogisticsStarFuelMissileSaveData& MissileSaveData : SaveData.StarFuelMissiles)
	{
		if (ImportMissile(SpaceLogisticsSubsystem, MissileSaveData, SaveData.Version, StarFuelMissiles))
		{
			++ImportedMissileCount;
		}
	}

	SR_LOG(SpaceLogistics,
		LogTemp,
		Display,
		TEXT("[SpaceLogistics] Save data imported: Version=%d ImportedRoutes=%d SavedRoutes=%d ImportedMissiles=%d SavedMissiles=%d NextRouteSequence=%d NextFleetQueueSequence=%lld NextMissileSequence=%d"),
		SaveData.Version,
		ImportedRouteCount,
		SaveData.HubRoutes.Num(),
		ImportedMissileCount,
		SaveData.StarFuelMissiles.Num(),
		NextHubRouteSequence,
		NextFleetDepartureQueueSequence,
		NextStarFuelMissileSequence);
	FSRSpaceLogisticsRouteVisualController::Refresh(
		SpaceLogisticsSubsystem,
		SpaceLogisticsSubsystem.GetWorld(),
		HubRoutes,
		SpaceshipActorsByRouteId);
	FSRSpaceLogisticsRouteVisualController::RefreshStarFuelMissiles(
		SpaceLogisticsSubsystem,
		SpaceLogisticsSubsystem.GetWorld(),
		StarFuelMissiles,
		StarFuelMissileActorsByMissileId);
	return (ImportedRouteCount > 0 || SaveData.HubRoutes.IsEmpty())
		&& (ImportedMissileCount > 0 || SaveData.StarFuelMissiles.IsEmpty());
}

bool FSRSpaceLogisticsSaveAdapter::BuildRouteSaveData(
	const USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	const FSRSpaceLogisticsHubRoute& HubRoute,
	FSRSpaceLogisticsHubRouteSaveData& OutRouteSaveData)
{
	OutRouteSaveData = FSRSpaceLogisticsHubRouteSaveData();
	OutRouteSaveData.RouteId = HubRoute.RouteId;
	if (!SpaceLogisticsSubsystem.BuildHubEndpointSaveData(HubRoute.SourceHub, OutRouteSaveData.SourceHub)
		|| !SpaceLogisticsSubsystem.BuildHubEndpointSaveData(HubRoute.DestinationHub, OutRouteSaveData.DestinationHub))
	{
		return false;
	}

	OutRouteSaveData.bEnabled = HubRoute.bEnabled;
	OutRouteSaveData.bReturnEmptyWhenNoCargo = HubRoute.bReturnEmptyWhenNoCargo;
	OutRouteSaveData.MaxCargoStackCount = HubRoute.MaxCargoStackCount;
	OutRouteSaveData.CargoResourceId = HubRoute.CargoResourceId;
	OutRouteSaveData.RouteProfile = HubRoute.RouteProfile;
	OutRouteSaveData.ConditionedTransitModule = HubRoute.RouteProfile
		== ESRSpaceLogisticsRouteProfileV2::ConditionedHold
		? FSRConditionedTransitV2::GetModuleRules(HubRoute.ConditionedTransitModule).Module
		: ESRConditionedTransitModuleV2::None;
	OutRouteSaveData.FleetDepartureQueueSequence = HubRoute.FleetDepartureQueueSequence;
	OutRouteSaveData.bDebugLocalOrbit = HubRoute.bDebugLocalOrbit;
	OutRouteSaveData.Phase = HubRoute.Phase;
	OutRouteSaveData.CurrentDockSide = HubRoute.CurrentDockSide;
	OutRouteSaveData.TravelDurationSeconds = HubRoute.TravelDurationSeconds;
	OutRouteSaveData.InitialSpeedUnitsPerSecond = HubRoute.InitialSpeedUnitsPerSecond;
	OutRouteSaveData.LaunchAccelerationUnitsPerSecondSquared = HubRoute.LaunchAccelerationUnitsPerSecondSquared;
	OutRouteSaveData.TravelProgressSeconds = HubRoute.TravelProgressSeconds;
	OutRouteSaveData.TravelProgressRatio = HubRoute.TravelProgressRatio;
	OutRouteSaveData.ConditioningDurationSeconds = IsConditioningRoutePhase(HubRoute.Phase)
		? HubRoute.ConditioningDurationSeconds
		: 0.0f;
	OutRouteSaveData.ConditioningProgressSeconds = IsConditioningRoutePhase(HubRoute.Phase)
		? HubRoute.ConditioningProgressSeconds
		: 0.0f;
	OutRouteSaveData.TravelStartWorldLocation = HubRoute.TravelStartWorldLocation;
	OutRouteSaveData.bHasTravelStartWorldLocation = HubRoute.bHasTravelStartWorldLocation;
	OutRouteSaveData.LaunchWorldVelocity = HubRoute.LaunchWorldVelocity;
	OutRouteSaveData.bHasLaunchWorldVelocity = HubRoute.bHasLaunchWorldVelocity;
	OutRouteSaveData.Cargo = HubRoute.Cargo;
	StarRovers::Resources::PrepareResourceInstanceForSave(OutRouteSaveData.Cargo);
	return true;
}

bool FSRSpaceLogisticsSaveAdapter::ImportRoute(
	USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	const FSRSpaceLogisticsHubRouteSaveData& RouteSaveData,
	int32 SourceSaveVersion,
	TArray<FSRSpaceLogisticsHubRoute>& HubRoutes)
{
	if (!RouteSaveData.IsValid()
		|| !IsValidConditioningSavePayload(RouteSaveData, SourceSaveVersion))
	{
		return false;
	}

	FSRSpaceLogisticsHubEndpoint SourceHub;
	FSRSpaceLogisticsHubEndpoint DestinationHub;
	if (!SpaceLogisticsSubsystem.ResolveSavedHubEndpoint(RouteSaveData.SourceHub, SourceHub)
		|| !SpaceLogisticsSubsystem.ResolveSavedHubEndpoint(RouteSaveData.DestinationHub, DestinationHub)
		|| (!RouteSaveData.bDebugLocalOrbit && FSRSpaceLogisticsRouteRegistry::AreHubEndpointKeysEqual(SourceHub, DestinationHub))
		|| FSRSpaceLogisticsRouteRegistry::DoesRouteAlreadyExist(HubRoutes, SourceHub, DestinationHub))
	{
		return false;
	}

	FSRSpaceLogisticsHubRoute& HubRoute = HubRoutes.AddDefaulted_GetRef();
	HubRoute.RouteId = RouteSaveData.RouteId;
	HubRoute.SourceHub = SourceHub;
	HubRoute.DestinationHub = RouteSaveData.bDebugLocalOrbit ? SourceHub : DestinationHub;
	HubRoute.bEnabled = RouteSaveData.bEnabled;
	HubRoute.bReturnEmptyWhenNoCargo = RouteSaveData.bReturnEmptyWhenNoCargo;
	HubRoute.RouteProfile = SourceSaveVersion >= FSRSpaceLogisticsSaveData::FleetCapacityVersion
		? FSRFleetCapacityV2::GetRouteProfileRules(RouteSaveData.RouteProfile).Profile
		: ESRSpaceLogisticsRouteProfileV2::NeutralShuttle;
	HubRoute.ConditionedTransitModule = SourceSaveVersion >= FSRSpaceLogisticsSaveData::ConditionedTransitVersion
		&& HubRoute.RouteProfile == ESRSpaceLogisticsRouteProfileV2::ConditionedHold
		? FSRConditionedTransitV2::GetModuleRules(RouteSaveData.ConditionedTransitModule).Module
		: ESRConditionedTransitModuleV2::None;
	HubRoute.MaxCargoStackCount = SourceSaveVersion >= FSRSpaceLogisticsSaveData::FleetCapacityVersion
		? FMath::Min(
			FMath::Max(1, RouteSaveData.MaxCargoStackCount),
			FSRFleetCapacityV2::GetRouteProfileRules(HubRoute.RouteProfile).CargoCapacity)
		: FMath::Max(1, RouteSaveData.MaxCargoStackCount);
	HubRoute.CargoResourceId = RouteSaveData.CargoResourceId;
	HubRoute.FleetDepartureQueueSequence = SourceSaveVersion >= FSRSpaceLogisticsSaveData::FleetCapacityVersion
		&& RouteSaveData.Phase == ESRSpaceLogisticsHubRoutePhase::WaitingForFleetCapacity
		? FMath::Max<int64>(0, RouteSaveData.FleetDepartureQueueSequence)
		: 0;
	HubRoute.FleetQueuePosition = 0;
	HubRoute.bDebugLocalOrbit = RouteSaveData.bDebugLocalOrbit;
	HubRoute.Phase = RouteSaveData.Phase;
	HubRoute.CurrentDockSide = RouteSaveData.CurrentDockSide;
	HubRoute.TravelDurationSeconds = FMath::Max(0.01f, RouteSaveData.TravelDurationSeconds);
	HubRoute.InitialSpeedUnitsPerSecond =
		FSRSpaceLogisticsRoutePathResolver::ClampInitialSpeed(RouteSaveData.InitialSpeedUnitsPerSecond);
	HubRoute.LaunchAccelerationUnitsPerSecondSquared =
		FSRSpaceLogisticsRoutePathResolver::ClampLaunchAcceleration(RouteSaveData.LaunchAccelerationUnitsPerSecondSquared);
	HubRoute.TravelProgressSeconds = FMath::Max(0.0f, RouteSaveData.TravelProgressSeconds);
	HubRoute.TravelProgressRatio = FMath::Clamp(RouteSaveData.TravelProgressRatio, 0.0f, 1.0f);
	HubRoute.ConditioningDurationSeconds = SourceSaveVersion >= FSRSpaceLogisticsSaveData::ConditioningDwellVersion
		&& IsConditioningRoutePhase(HubRoute.Phase)
		? RouteSaveData.ConditioningDurationSeconds
		: 0.0f;
	HubRoute.ConditioningProgressSeconds = SourceSaveVersion >= FSRSpaceLogisticsSaveData::ConditioningDwellVersion
		&& IsConditioningRoutePhase(HubRoute.Phase)
		? RouteSaveData.ConditioningProgressSeconds
		: 0.0f;
	HubRoute.TravelStartWorldLocation = RouteSaveData.TravelStartWorldLocation;
	HubRoute.bHasTravelStartWorldLocation = RouteSaveData.bHasTravelStartWorldLocation;
	HubRoute.LaunchWorldVelocity = RouteSaveData.LaunchWorldVelocity;
	HubRoute.bHasLaunchWorldVelocity = RouteSaveData.bHasLaunchWorldVelocity;
	HubRoute.Cargo = RouteSaveData.Cargo;
	StarRovers::Resources::UpgradeResourceInstanceToCurrentSchema(
		HubRoute.Cargo,
		SourceSaveVersion <= FSRSpaceLogisticsSaveData::InitialVersion);
	return true;
}

bool FSRSpaceLogisticsSaveAdapter::BuildMissileSaveData(
	const USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	const FSRSpaceLogisticsStarFuelMissile& Missile,
	FSRSpaceLogisticsStarFuelMissileSaveData& OutMissileSaveData)
{
	OutMissileSaveData = FSRSpaceLogisticsStarFuelMissileSaveData();
	OutMissileSaveData.MissileId = Missile.MissileId;
	AActor* TargetStarActor = Missile.TargetStarActor.Get();
	if (!SpaceLogisticsSubsystem.BuildHubEndpointSaveData(Missile.SourceHub, OutMissileSaveData.SourceHub)
		|| !IsValid(TargetStarActor)
		|| !USRCelestialBodyRuntimeLibrary::IsCelestialStarActor(TargetStarActor)
		|| Missile.Cargo.ResourceId.IsNone()
		|| Missile.Cargo.StackCount <= 0)
	{
		return false;
	}

	OutMissileSaveData.TargetStarActorName = TargetStarActor->GetFName();
	OutMissileSaveData.TargetStarVariableName =
		USRCelestialBodyRuntimeLibrary::GetCelestialVariableName(TargetStarActor).ToString();
	OutMissileSaveData.bEnabled = Missile.bEnabled;
	OutMissileSaveData.TravelDurationSeconds = Missile.TravelDurationSeconds;
	OutMissileSaveData.InitialSpeedUnitsPerSecond = Missile.InitialSpeedUnitsPerSecond;
	OutMissileSaveData.LaunchAccelerationUnitsPerSecondSquared = Missile.LaunchAccelerationUnitsPerSecondSquared;
	OutMissileSaveData.TravelProgressSeconds = Missile.TravelProgressSeconds;
	OutMissileSaveData.TravelProgressRatio = Missile.TravelProgressRatio;
	OutMissileSaveData.TravelStartWorldLocation = Missile.TravelStartWorldLocation;
	OutMissileSaveData.bHasTravelStartWorldLocation = Missile.bHasTravelStartWorldLocation;
	OutMissileSaveData.LaunchWorldVelocity = Missile.LaunchWorldVelocity;
	OutMissileSaveData.bHasLaunchWorldVelocity = Missile.bHasLaunchWorldVelocity;
	OutMissileSaveData.Cargo = Missile.Cargo;
	StarRovers::Resources::PrepareResourceInstanceForSave(OutMissileSaveData.Cargo);
	return OutMissileSaveData.IsValid();
}

bool FSRSpaceLogisticsSaveAdapter::ImportMissile(
	USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	const FSRSpaceLogisticsStarFuelMissileSaveData& MissileSaveData,
	int32 SourceSaveVersion,
	TArray<FSRSpaceLogisticsStarFuelMissile>& StarFuelMissiles)
{
	if (!MissileSaveData.IsValid())
	{
		return false;
	}

	FSRSpaceLogisticsHubEndpoint SourceHub;
	AActor* TargetStarActor = ResolveSavedStarActor(
		SpaceLogisticsSubsystem,
		MissileSaveData.TargetStarActorName,
		MissileSaveData.TargetStarVariableName);
	if (!SpaceLogisticsSubsystem.ResolveSavedHubEndpoint(MissileSaveData.SourceHub, SourceHub)
		|| !IsValid(TargetStarActor)
		|| !USRCelestialBodyRuntimeLibrary::IsCelestialStarActor(TargetStarActor)
		|| MissileSaveData.Cargo.ResourceId.IsNone()
		|| MissileSaveData.Cargo.StackCount <= 0)
	{
		return false;
	}

	FSRSpaceLogisticsStarFuelMissile& Missile = StarFuelMissiles.AddDefaulted_GetRef();
	Missile.MissileId = MissileSaveData.MissileId;
	Missile.SourceHub = SourceHub;
	Missile.TargetStarActor = TargetStarActor;
	Missile.bEnabled = MissileSaveData.bEnabled;
	Missile.TravelDurationSeconds = FMath::Max(0.01f, MissileSaveData.TravelDurationSeconds);
	Missile.InitialSpeedUnitsPerSecond =
		FSRSpaceLogisticsRoutePathResolver::ClampInitialSpeed(MissileSaveData.InitialSpeedUnitsPerSecond);
	Missile.LaunchAccelerationUnitsPerSecondSquared =
		FSRSpaceLogisticsRoutePathResolver::ClampLaunchAcceleration(MissileSaveData.LaunchAccelerationUnitsPerSecondSquared);
	Missile.TravelProgressSeconds = FMath::Max(0.0f, MissileSaveData.TravelProgressSeconds);
	Missile.TravelProgressRatio = FMath::Clamp(MissileSaveData.TravelProgressRatio, 0.0f, 1.0f);
	Missile.TravelStartWorldLocation = MissileSaveData.TravelStartWorldLocation;
	Missile.bHasTravelStartWorldLocation = MissileSaveData.bHasTravelStartWorldLocation;
	Missile.LaunchWorldVelocity = MissileSaveData.LaunchWorldVelocity;
	Missile.bHasLaunchWorldVelocity = MissileSaveData.bHasLaunchWorldVelocity;
	Missile.Cargo = MissileSaveData.Cargo;
	StarRovers::Resources::UpgradeResourceInstanceToCurrentSchema(
		Missile.Cargo,
		SourceSaveVersion <= FSRSpaceLogisticsSaveData::InitialVersion);
	return true;
}

AActor* FSRSpaceLogisticsSaveAdapter::ResolveSavedStarActor(
	USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	FName TargetStarActorName,
	const FString& TargetStarVariableName)
{
	UWorld* World = SpaceLogisticsSubsystem.GetWorld();
	USRCelestialBodyRegistrySubsystem* CelestialRegistry = IsValid(World)
		? World->GetSubsystem<USRCelestialBodyRegistrySubsystem>()
		: nullptr;
	if (!IsValid(CelestialRegistry))
	{
		return nullptr;
	}

	TArray<AActor*> BodyActors;
	CelestialRegistry->GetCelestialBodies(BodyActors);
	if (BodyActors.IsEmpty())
	{
		CelestialRegistry->RefreshCelestialBodies();
		CelestialRegistry->GetCelestialBodies(BodyActors);
	}

	for (AActor* BodyActor : BodyActors)
	{
		if (!IsValid(BodyActor) || !USRCelestialBodyRuntimeLibrary::IsCelestialStarActor(BodyActor))
		{
			continue;
		}

		const bool bMatchesActorName = !TargetStarActorName.IsNone() && BodyActor->GetFName() == TargetStarActorName;
		const FString BodyVariableName = USRCelestialBodyRuntimeLibrary::GetCelestialVariableName(BodyActor).ToString();
		const bool bMatchesVariableName = !TargetStarVariableName.IsEmpty() && BodyVariableName == TargetStarVariableName;
		if (bMatchesActorName || bMatchesVariableName)
		{
			return BodyActor;
		}
	}

	return CelestialRegistry->GetPrimaryStarActor();
}
