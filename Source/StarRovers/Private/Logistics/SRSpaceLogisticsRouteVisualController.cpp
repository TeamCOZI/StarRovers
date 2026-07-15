#include "SRSpaceLogisticsRouteVisualController.h"

#include "Camera/SRGameMode.h"
#include "GameFramework/WorldSettings.h"
#include "Logistics/SRSpaceshipActor.h"
#include "Logistics/SRSpaceLogisticsSubsystem.h"
#include "SRSpaceLogisticsRoutePathResolver.h"
#include "Utility/SRLog.h"

void FSRSpaceLogisticsRouteVisualController::Refresh(
	const USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	UWorld* World,
	TArray<FSRSpaceLogisticsHubRoute>& HubRoutes,
	TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId)
{
	TSet<FName> ActiveRouteIds;
	ActiveRouteIds.Reserve(HubRoutes.Num());
	for (FSRSpaceLogisticsHubRoute& HubRoute : HubRoutes)
	{
		if (!IsRouteTraveling(HubRoute))
		{
			DestroyRouteActor(HubRoute.RouteId, SpaceshipActorsByRouteId);
			continue;
		}

		FVector VisualWorldLocation = FVector::ZeroVector;
		FVector TargetWorldLocation = FVector::ZeroVector;
		FVector TravelDirection = FVector::ZeroVector;
		if (!FSRSpaceLogisticsRoutePathResolver::ResolveVisualWorldLocation(
			SpaceLogisticsSubsystem,
			HubRoute,
			VisualWorldLocation,
			TargetWorldLocation,
			TravelDirection))
		{
			DestroyRouteActor(HubRoute.RouteId, SpaceshipActorsByRouteId);
			continue;
		}

		ASRSpaceshipActor* SpaceshipActor = FindOrSpawn(World, HubRoute.RouteId, SpaceshipActorsByRouteId);
		if (!IsValid(SpaceshipActor))
		{
			continue;
		}

		SpaceshipActor->UpdateRouteVisualWithDirection(
			VisualWorldLocation,
			TargetWorldLocation,
			TravelDirection,
			HubRoute.TravelProgressRatio);
		ActiveRouteIds.Add(HubRoute.RouteId);
	}

	TArray<FName> InactiveRouteIds;
	InactiveRouteIds.Reserve(SpaceshipActorsByRouteId.Num());
	for (const TPair<FName, TObjectPtr<ASRSpaceshipActor>>& Pair : SpaceshipActorsByRouteId)
	{
		if (!ActiveRouteIds.Contains(Pair.Key))
		{
			InactiveRouteIds.Add(Pair.Key);
		}
	}

	for (const FName InactiveRouteId : InactiveRouteIds)
	{
		DestroyRouteActor(InactiveRouteId, SpaceshipActorsByRouteId);
	}
}

void FSRSpaceLogisticsRouteVisualController::RefreshStarFuelMissiles(
	const USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	UWorld* World,
	TArray<FSRSpaceLogisticsStarFuelMissile>& StarFuelMissiles,
	TMap<FName, TObjectPtr<ASRSpaceshipActor>>& MissileActorsByMissileId)
{
	TSet<FName> ActiveMissileIds;
	ActiveMissileIds.Reserve(StarFuelMissiles.Num());
	for (FSRSpaceLogisticsStarFuelMissile& Missile : StarFuelMissiles)
	{
		if (!Missile.bEnabled || !Missile.IsValid())
		{
			DestroyRouteActor(Missile.MissileId, MissileActorsByMissileId);
			continue;
		}

		FVector VisualWorldLocation = FVector::ZeroVector;
		FVector TargetWorldLocation = FVector::ZeroVector;
		FVector TravelDirection = FVector::ZeroVector;
		if (!FSRSpaceLogisticsRoutePathResolver::ResolveStarFuelMissileVisualWorldLocation(
			SpaceLogisticsSubsystem,
			Missile,
			VisualWorldLocation,
			TargetWorldLocation,
			TravelDirection))
		{
			DestroyRouteActor(Missile.MissileId, MissileActorsByMissileId);
			continue;
		}

		ASRSpaceshipActor* MissileActor = FindOrSpawn(World, Missile.MissileId, MissileActorsByMissileId);
		if (!IsValid(MissileActor))
		{
			continue;
		}

		MissileActor->UpdateRouteVisualWithDirection(
			VisualWorldLocation,
			TargetWorldLocation,
			TravelDirection,
			Missile.TravelProgressRatio);
		ActiveMissileIds.Add(Missile.MissileId);
	}

	TArray<FName> InactiveMissileIds;
	InactiveMissileIds.Reserve(MissileActorsByMissileId.Num());
	for (const TPair<FName, TObjectPtr<ASRSpaceshipActor>>& Pair : MissileActorsByMissileId)
	{
		if (!ActiveMissileIds.Contains(Pair.Key))
		{
			InactiveMissileIds.Add(Pair.Key);
		}
	}

	for (const FName InactiveMissileId : InactiveMissileIds)
	{
		DestroyRouteActor(InactiveMissileId, MissileActorsByMissileId);
	}
}

void FSRSpaceLogisticsRouteVisualController::Clear(TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId)
{
	for (const TPair<FName, TObjectPtr<ASRSpaceshipActor>>& Pair : SpaceshipActorsByRouteId)
	{
		if (IsValid(Pair.Value.Get()))
		{
			Pair.Value->Destroy();
		}
	}

	SpaceshipActorsByRouteId.Reset();
}

void FSRSpaceLogisticsRouteVisualController::DestroyRouteActor(
	FName RouteId,
	TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId)
{
	if (RouteId.IsNone())
	{
		return;
	}

	TObjectPtr<ASRSpaceshipActor> RemovedSpaceshipActor;
	if (SpaceshipActorsByRouteId.RemoveAndCopyValue(RouteId, RemovedSpaceshipActor) && IsValid(RemovedSpaceshipActor.Get()))
	{
		RemovedSpaceshipActor->Destroy();
	}
}

void FSRSpaceLogisticsRouteVisualController::ResolveDefaultFlightSettings(
	UWorld* World,
	float& OutInitialSpeedUnitsPerSecond,
	float& OutLaunchAccelerationUnitsPerSecondSquared)
{
	OutInitialSpeedUnitsPerSecond = FSRSpaceLogisticsRoutePathResolver::GetDefaultInitialSpeedUnitsPerSecond();
	OutLaunchAccelerationUnitsPerSecondSquared =
		FSRSpaceLogisticsRoutePathResolver::GetDefaultLaunchAccelerationUnitsPerSecondSquared();

	const TSubclassOf<ASRSpaceshipActor> SpaceshipActorClass = ResolveActorClass(World);
	const ASRSpaceshipActor* SpaceshipDefaultObject = SpaceshipActorClass
		? Cast<ASRSpaceshipActor>(SpaceshipActorClass->GetDefaultObject())
		: nullptr;
	if (!IsValid(SpaceshipDefaultObject))
	{
		return;
	}

	OutInitialSpeedUnitsPerSecond = SpaceshipDefaultObject->GetInitialSpeedUnitsPerSecond();
	OutLaunchAccelerationUnitsPerSecondSquared = SpaceshipDefaultObject->GetLaunchAccelerationUnitsPerSecondSquared();
}

ASRSpaceshipActor* FSRSpaceLogisticsRouteVisualController::FindOrSpawn(
	UWorld* World,
	FName VisualActorId,
	TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId)
{
	if (VisualActorId.IsNone())
	{
		return nullptr;
	}

	if (TObjectPtr<ASRSpaceshipActor>* ExistingSpaceshipActor = SpaceshipActorsByRouteId.Find(VisualActorId))
	{
		if (IsValid(ExistingSpaceshipActor->Get()))
		{
			return ExistingSpaceshipActor->Get();
		}
	}

	if (!IsValid(World))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	TSubclassOf<ASRSpaceshipActor> SpaceshipActorClass = ResolveActorClass(World);
	ASRSpaceshipActor* SpaceshipActor = World->SpawnActor<ASRSpaceshipActor>(
		SpaceshipActorClass.Get(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParameters);
	if (!IsValid(SpaceshipActor))
	{
		return nullptr;
	}

	SpaceshipActor->SetRouteId(VisualActorId);
	SpaceshipActorsByRouteId.Add(VisualActorId, SpaceshipActor);
	return SpaceshipActor;
}

TSubclassOf<ASRSpaceshipActor> FSRSpaceLogisticsRouteVisualController::ResolveActorClass(UWorld* World)
{
	if (IsValid(World))
	{
		if (const ASRGameMode* GameMode = World->GetAuthGameMode<ASRGameMode>())
		{
			if (TSubclassOf<ASRSpaceshipActor> ConfiguredClass = GameMode->ResolveSpaceLogisticsSpaceshipActorClass())
			{
				return ConfiguredClass;
			}
		}

		const AWorldSettings* WorldSettings = World->GetWorldSettings();
		const UClass* DefaultGameModeClass = WorldSettings ? WorldSettings->DefaultGameMode : nullptr;
		const ASRGameMode* DefaultGameMode = DefaultGameModeClass
			? Cast<ASRGameMode>(DefaultGameModeClass->GetDefaultObject())
			: nullptr;
		if (DefaultGameMode)
		{
			if (TSubclassOf<ASRSpaceshipActor> ConfiguredClass = DefaultGameMode->ResolveSpaceLogisticsSpaceshipActorClass())
			{
				return ConfiguredClass;
			}
		}
	}

	SR_LOG(SpaceLogistics,
		LogTemp,
		Warning,
		TEXT("[SpaceLogistics] No Spaceship Actor Class configured on ASRGameMode. Falling back to native ASRSpaceshipActor."));
	return ASRSpaceshipActor::StaticClass();
}

bool FSRSpaceLogisticsRouteVisualController::IsRouteTraveling(const FSRSpaceLogisticsHubRoute& HubRoute)
{
	return HubRoute.Phase == ESRSpaceLogisticsHubRoutePhase::TravelingToDestination
		|| HubRoute.Phase == ESRSpaceLogisticsHubRoutePhase::TravelingToSource;
}
