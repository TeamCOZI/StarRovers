#include "SRSpaceLogisticsRouteVisualController.h"

#include "Logistics/SRSpaceshipActor.h"
#include "Logistics/SRSpaceLogisticsSubsystem.h"
#include "SRSpaceLogisticsRoutePathResolver.h"

namespace
{
	const TCHAR* DefaultSpaceshipActorClassPath = TEXT("/Game/StarRovers/Logistics/Blueprints/BP_SpaceshipActor.BP_SpaceshipActor_C");
}

void FSRSpaceLogisticsRouteVisualController::Refresh(
	const USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	UWorld* World,
	TArray<FSRSpaceLogisticsHubRoute>& HubRoutes,
	TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId)
{
	TSet<FName> ActiveRouteIds;
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

		ASRSpaceshipActor* SpaceshipActor = FindOrSpawn(World, HubRoute, SpaceshipActorsByRouteId);
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
	float& OutInitialSpeedUnitsPerSecond,
	float& OutLaunchAccelerationUnitsPerSecondSquared)
{
	OutInitialSpeedUnitsPerSecond = FSRSpaceLogisticsRoutePathResolver::GetDefaultInitialSpeedUnitsPerSecond();
	OutLaunchAccelerationUnitsPerSecondSquared =
		FSRSpaceLogisticsRoutePathResolver::GetDefaultLaunchAccelerationUnitsPerSecondSquared();

	const TSubclassOf<ASRSpaceshipActor> SpaceshipActorClass = ResolveActorClass();
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
	FSRSpaceLogisticsHubRoute& HubRoute,
	TMap<FName, TObjectPtr<ASRSpaceshipActor>>& SpaceshipActorsByRouteId)
{
	if (HubRoute.RouteId.IsNone())
	{
		return nullptr;
	}

	if (TObjectPtr<ASRSpaceshipActor>* ExistingSpaceshipActor = SpaceshipActorsByRouteId.Find(HubRoute.RouteId))
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

	TSubclassOf<ASRSpaceshipActor> SpaceshipActorClass = ResolveActorClass();
	ASRSpaceshipActor* SpaceshipActor = World->SpawnActor<ASRSpaceshipActor>(
		SpaceshipActorClass.Get(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParameters);
	if (!IsValid(SpaceshipActor))
	{
		return nullptr;
	}

	SpaceshipActor->SetRouteId(HubRoute.RouteId);
	SpaceshipActorsByRouteId.Add(HubRoute.RouteId, SpaceshipActor);
	return SpaceshipActor;
}

TSubclassOf<ASRSpaceshipActor> FSRSpaceLogisticsRouteVisualController::ResolveActorClass()
{
	static TWeakObjectPtr<UClass> CachedSpaceshipActorClass;
	if (CachedSpaceshipActorClass.IsValid())
	{
		return CachedSpaceshipActorClass.Get();
	}

	UClass* LoadedClass = LoadClass<ASRSpaceshipActor>(nullptr, DefaultSpaceshipActorClassPath);
	if (IsValid(LoadedClass) && LoadedClass->IsChildOf(ASRSpaceshipActor::StaticClass()))
	{
		CachedSpaceshipActorClass = LoadedClass;
		return LoadedClass;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[SpaceLogistics] Failed to load BP spaceship actor class at '%s'. Falling back to ASRSpaceshipActor."),
		DefaultSpaceshipActorClassPath);
	return ASRSpaceshipActor::StaticClass();
}

bool FSRSpaceLogisticsRouteVisualController::IsRouteTraveling(const FSRSpaceLogisticsHubRoute& HubRoute)
{
	return HubRoute.Phase == ESRSpaceLogisticsHubRoutePhase::TravelingToDestination
		|| HubRoute.Phase == ESRSpaceLogisticsHubRoutePhase::TravelingToSource;
}
