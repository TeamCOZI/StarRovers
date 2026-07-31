#include "SRSpaceLogisticsSaveAdapter.h"

#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "SRSpaceLogisticsRoutePathResolver.h"
#include "SRSpaceLogisticsRouteRegistry.h"
#include "SRSpaceLogisticsRouteVisualController.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "Utility/SRLog.h"

void FSRSpaceLogisticsSaveAdapter::ExportSaveData(
	const USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	const TArray<FSRSpaceLogisticsHubRoute>& HubRoutes,
	int32 NextHubRouteSequence,
	const TArray<FSRSpaceLogisticsStarFuelMissile>& StarFuelMissiles,
	int32 NextStarFuelMissileSequence,
	FSRSpaceLogisticsSaveData& OutSaveData)
{
	OutSaveData = FSRSpaceLogisticsSaveData();
	OutSaveData.NextHubRouteSequence = FMath::Max(1, NextHubRouteSequence);
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
		else
		{
			SR_LOG(SpaceLogistics,
				LogTemp,
				Error,
				TEXT("[SpaceLogistics] Pattern route omitted from save because its endpoint, filter, or cargo payload is invalid: RouteId=%s"),
				*HubRoute.RouteId.ToString());
		}
	}

	for (const FSRSpaceLogisticsStarFuelMissile& Missile : StarFuelMissiles)
	{
		FSRSpaceLogisticsStarFuelMissileSaveData MissileSaveData;
		if (BuildMissileSaveData(SpaceLogisticsSubsystem, Missile, MissileSaveData))
		{
			OutSaveData.StarFuelMissiles.Add(MissileSaveData);
		}
		else
		{
			SR_LOG(SpaceLogistics,
				LogTemp,
				Error,
				TEXT("[SpaceLogistics] Pattern missile omitted from save because its endpoint or cargo payload is invalid: MissileId=%s"),
				*Missile.MissileId.ToString());
		}
	}
}

bool FSRSpaceLogisticsSaveAdapter::ImportSaveData(
	USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	const FSRSpaceLogisticsSaveData& SaveData,
	TArray<FSRSpaceLogisticsHubRoute>& HubRoutes,
	int32& NextHubRouteSequence,
	TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId,
	TArray<FSRSpaceLogisticsStarFuelMissile>& StarFuelMissiles,
	int32& NextStarFuelMissileSequence,
	TMap<FName, TObjectPtr<ASRSpaceshipActor>>& StarFuelMissileActorsByMissileId,
	TMap<FString, FSRSpaceLogisticsHubEndpointMotionSample>& HubEndpointMotionSamples)
{
	if (!StarRovers::SpaceLogistics::PatternSave::IsSupportedVersion(SaveData.Version))
	{
		SR_LOG(SpaceLogistics,
			LogTemp,
			Error,
			TEXT("[SpaceLogistics] Save import rejected: unsupported Pattern logistics version %d"),
			SaveData.Version);
		return false;
	}

	TArray<FSRSpaceLogisticsHubRoute> ImportedHubRoutes;
	ImportedHubRoutes.Reserve(SaveData.HubRoutes.Num());
	TArray<FSRSpaceLogisticsStarFuelMissile> ImportedStarFuelMissiles;
	ImportedStarFuelMissiles.Reserve(SaveData.StarFuelMissiles.Num());

	int32 ImportedRouteCount = 0;
	for (const FSRSpaceLogisticsHubRouteSaveData& RouteSaveData : SaveData.HubRoutes)
	{
		if (!ImportRoute(SpaceLogisticsSubsystem, SaveData.Version, RouteSaveData, ImportedHubRoutes))
		{
			return false;
		}
		++ImportedRouteCount;
	}

	int32 ImportedMissileCount = 0;
	for (const FSRSpaceLogisticsStarFuelMissileSaveData& MissileSaveData : SaveData.StarFuelMissiles)
	{
		if (!ImportMissile(SpaceLogisticsSubsystem, MissileSaveData, ImportedStarFuelMissiles))
		{
			return false;
		}
		++ImportedMissileCount;
	}

	FSRSpaceLogisticsRouteVisualController::Clear(SpaceshipActorsByRouteId);
	FSRSpaceLogisticsRouteVisualController::Clear(StarFuelMissileActorsByMissileId);
	HubRoutes = MoveTemp(ImportedHubRoutes);
	StarFuelMissiles = MoveTemp(ImportedStarFuelMissiles);
	HubEndpointMotionSamples.Reset();
	NextHubRouteSequence = FMath::Max(1, SaveData.NextHubRouteSequence);
	NextStarFuelMissileSequence = FMath::Max(1, SaveData.NextStarFuelMissileSequence);

	SR_LOG(SpaceLogistics,
		LogTemp,
		Display,
		TEXT("[SpaceLogistics] Pattern save data imported atomically: Version=%d ImportedRoutes=%d SavedRoutes=%d ImportedMissiles=%d SavedMissiles=%d NextRouteSequence=%d NextMissileSequence=%d"),
		SaveData.Version,
		ImportedRouteCount,
		SaveData.HubRoutes.Num(),
		ImportedMissileCount,
		SaveData.StarFuelMissiles.Num(),
		NextHubRouteSequence,
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
	return true;
}

bool FSRSpaceLogisticsSaveAdapter::CanImportSaveData(
	USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	const FSRSpaceLogisticsSaveData& SaveData)
{
	if (!StarRovers::SpaceLogistics::PatternSave::IsSupportedVersion(SaveData.Version)
		|| SaveData.NextHubRouteSequence < 1
		|| SaveData.NextStarFuelMissileSequence < 1)
	{
		return false;
	}

	TArray<FSRSpaceLogisticsHubRoute> ImportedHubRoutes;
	for (const FSRSpaceLogisticsHubRouteSaveData& RouteSaveData : SaveData.HubRoutes)
	{
		if (!ImportRoute(SpaceLogisticsSubsystem, SaveData.Version, RouteSaveData, ImportedHubRoutes))
		{
			return false;
		}
	}
	TArray<FSRSpaceLogisticsStarFuelMissile> ImportedMissiles;
	for (const FSRSpaceLogisticsStarFuelMissileSaveData& MissileSaveData : SaveData.StarFuelMissiles)
	{
		if (!ImportMissile(SpaceLogisticsSubsystem, MissileSaveData, ImportedMissiles))
		{
			return false;
		}
	}
	return true;
}

bool FSRSpaceLogisticsSaveAdapter::BuildRouteSaveData(
	const USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	const FSRSpaceLogisticsHubRoute& HubRoute,
	FSRSpaceLogisticsHubRouteSaveData& OutRouteSaveData)
{
	OutRouteSaveData = FSRSpaceLogisticsHubRouteSaveData();
	OutRouteSaveData.RouteId = HubRoute.RouteId;
	if (!HubRoute.IsValid()
		|| !SpaceLogisticsSubsystem.BuildHubEndpointSaveData(HubRoute.SourceHub, OutRouteSaveData.SourceHub)
		|| !SpaceLogisticsSubsystem.BuildHubEndpointSaveData(HubRoute.DestinationHub, OutRouteSaveData.DestinationHub))
	{
		return false;
	}

	OutRouteSaveData.bEnabled = HubRoute.bEnabled;
	OutRouteSaveData.bReturnEmptyWhenNoCargo = HubRoute.bReturnEmptyWhenNoCargo;
	OutRouteSaveData.MaxCargoStackCount = HubRoute.MaxCargoStackCount;
	OutRouteSaveData.CargoFilter = HubRoute.CargoFilter;
	OutRouteSaveData.CargoResourceId = HubRoute.CargoFilter.ResourceId;
	OutRouteSaveData.bDebugLocalOrbit = HubRoute.bDebugLocalOrbit;
	OutRouteSaveData.Phase = HubRoute.Phase;
	OutRouteSaveData.CurrentDockSide = HubRoute.CurrentDockSide;
	OutRouteSaveData.TravelDurationSeconds = HubRoute.TravelDurationSeconds;
	OutRouteSaveData.InitialSpeedUnitsPerSecond = HubRoute.InitialSpeedUnitsPerSecond;
	OutRouteSaveData.LaunchAccelerationUnitsPerSecondSquared = HubRoute.LaunchAccelerationUnitsPerSecondSquared;
	OutRouteSaveData.TravelProgressSeconds = HubRoute.TravelProgressSeconds;
	OutRouteSaveData.TravelProgressRatio = HubRoute.TravelProgressRatio;
	OutRouteSaveData.TravelStartWorldLocation = HubRoute.TravelStartWorldLocation;
	OutRouteSaveData.bHasTravelStartWorldLocation = HubRoute.bHasTravelStartWorldLocation;
	OutRouteSaveData.LaunchWorldVelocity = HubRoute.LaunchWorldVelocity;
	OutRouteSaveData.bHasLaunchWorldVelocity = HubRoute.bHasLaunchWorldVelocity;
	OutRouteSaveData.Cargo = HubRoute.Cargo;
	return true;
}

bool FSRSpaceLogisticsSaveAdapter::ImportRoute(
	USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	int32 SaveVersion,
	const FSRSpaceLogisticsHubRouteSaveData& RouteSaveData,
	TArray<FSRSpaceLogisticsHubRoute>& HubRoutes)
{
	if (!RouteSaveData.IsValid())
	{
		return false;
	}
	const FSRPatternRoutingFilter CargoFilter =
		StarRovers::SpaceLogistics::PatternSave::ResolveRouteCargoFilter(
			SaveVersion,
			RouteSaveData.CargoFilter,
			RouteSaveData.CargoResourceId);
	if (!CargoFilter.IsCanonical()
		|| !StarRovers::PatternRouting::IsValidOrEmptyPatternPayload(RouteSaveData.Cargo))
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
	HubRoute.MaxCargoStackCount = FMath::Max(1, RouteSaveData.MaxCargoStackCount);
	HubRoute.CargoFilter = CargoFilter;
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
	HubRoute.TravelStartWorldLocation = RouteSaveData.TravelStartWorldLocation;
	HubRoute.bHasTravelStartWorldLocation = RouteSaveData.bHasTravelStartWorldLocation;
	HubRoute.LaunchWorldVelocity = RouteSaveData.LaunchWorldVelocity;
	HubRoute.bHasLaunchWorldVelocity = RouteSaveData.bHasLaunchWorldVelocity;
	HubRoute.Cargo = RouteSaveData.Cargo;
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
		|| !StarRovers::PatternRouting::IsValidPatternPayload(Missile.Cargo))
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
	return OutMissileSaveData.IsValid();
}

bool FSRSpaceLogisticsSaveAdapter::ImportMissile(
	USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	const FSRSpaceLogisticsStarFuelMissileSaveData& MissileSaveData,
	TArray<FSRSpaceLogisticsStarFuelMissile>& StarFuelMissiles)
{
	if (!MissileSaveData.IsValid())
	{
		return false;
	}
	for (const FSRSpaceLogisticsStarFuelMissile& ExistingMissile : StarFuelMissiles)
	{
		if (ExistingMissile.MissileId == MissileSaveData.MissileId)
		{
			return false;
		}
	}

	FSRSpaceLogisticsHubEndpoint SourceHub;
	AActor* TargetStarActor = ResolveSavedStarActor(
		SpaceLogisticsSubsystem,
		MissileSaveData.TargetStarActorName,
		MissileSaveData.TargetStarVariableName);
	if (!SpaceLogisticsSubsystem.ResolveSavedHubEndpoint(MissileSaveData.SourceHub, SourceHub)
		|| !IsValid(TargetStarActor)
		|| !USRCelestialBodyRuntimeLibrary::IsCelestialStarActor(TargetStarActor)
		|| !StarRovers::PatternRouting::IsValidPatternPayload(MissileSaveData.Cargo))
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
