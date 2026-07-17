#include "SRSpaceLogisticsStarFuelMissileProcessor.h"

#include "Automation/SRFacilityNetworkComponent.h"
#include "Celestial/SRCelestialBody.h"
#include "Celestial/SRStar.h"
#include "Components/SphereComponent.h"
#include "Logistics/SRSpaceLogisticsSubsystem.h"
#include "SRSpaceLogisticsRoutePathResolver.h"
#include "SRSpaceLogisticsRouteVisualController.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "Utility/SRLog.h"

namespace StarRovers::SpaceLogistics::StarFuelMissiles
{
	bool HasValidMissileCargo(const FSRResourceInstance& Cargo)
	{
		return !Cargo.ResourceId.IsNone() && Cargo.StackCount > 0;
	}

	double CalculateMissileFuelValue(const FSRResourceInstance& Cargo)
	{
		if (!HasValidMissileCargo(Cargo))
		{
			return 0.0;
		}

		return FMath::Max(0.0, Cargo.EnergyValue) * static_cast<double>(FMath::Max(1, Cargo.StackCount));
	}

	bool CanUseAsMissileFuelCargo(const FSRResourceInstance& Cargo)
	{
		return CalculateMissileFuelValue(Cargo) > UE_DOUBLE_SMALL_NUMBER;
	}

	bool IsClickSphereCollision(const USphereComponent& SphereComponent)
	{
		return SphereComponent.GetFName() == FName(TEXT("ClickSphereCollision"));
	}
}

bool FSRSpaceLogisticsStarFuelMissileProcessor::LaunchFromHub(
	USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	const FSRSpaceLogisticsHubEndpoint& SourceHub,
	FName& OutMissileId,
	float InitialSpeedUnitsPerSecond,
	float LaunchAccelerationUnitsPerSecondSquared,
	TArray<FSRSpaceLogisticsStarFuelMissile>& StarFuelMissiles,
	int32& NextStarFuelMissileSequence)
{
	OutMissileId = NAME_None;

	FSRSpaceLogisticsHubEndpoint ResolvedSourceHub;
	if (!SpaceLogisticsSubsystem.ResolveCurrentHubEndpoint(SourceHub, ResolvedSourceHub))
	{
		return false;
	}

	ASRStar* TargetStar = ResolvePrimaryStar(SpaceLogisticsSubsystem);
	if (!IsValid(TargetStar))
	{
		return false;
	}

	FSRSpaceLogisticsStarFuelMissile Missile;
	Missile.MissileId = MakeMissileId(ResolvedSourceHub, NextStarFuelMissileSequence);
	Missile.SourceHub = ResolvedSourceHub;
	Missile.TargetStarActor = TargetStar;
	Missile.bEnabled = true;
	ApplyMissileFlightSettings(SpaceLogisticsSubsystem, Missile, InitialSpeedUnitsPerSecond, LaunchAccelerationUnitsPerSecondSquared);
	if (!StartMissileTravel(SpaceLogisticsSubsystem, Missile))
	{
		return false;
	}

	FSRResourceInstance Cargo;
	if (!TryTakeFuelCargoFromHub(ResolvedSourceHub, Cargo))
	{
		return false;
	}

	Missile.Cargo = Cargo;
	OutMissileId = Missile.MissileId;
	StarFuelMissiles.Add(Missile);

	double FuelAmount = 0.0;
	FuelAmount = StarRovers::SpaceLogistics::StarFuelMissiles::CalculateMissileFuelValue(Cargo);
	SR_LOG(SpaceLogistics,
		LogTemp,
		Display,
		TEXT("[SpaceLogistics] Star fuel missile launched: MissileId=%s Source=%s/%s TargetStar=%s ResourceId=%s StackCount=%d FuelValue=%.3f Duration=%.2f"),
		*OutMissileId.ToString(),
		*GetNameSafe(ResolvedSourceHub.BodyActor.Get()),
		*ResolvedSourceHub.HubOccupantId.ToString(),
		*GetNameSafe(TargetStar),
		*Cargo.ResourceId.ToString(),
		Cargo.StackCount,
		FuelAmount,
		Missile.TravelDurationSeconds);
	return true;
}

bool FSRSpaceLogisticsStarFuelMissileProcessor::LaunchFromHubInputPort(
	USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	const FSRSpaceLogisticsHubEndpoint& SourceHub,
	int32 InputPortIndex,
	FName& OutMissileId,
	float InitialSpeedUnitsPerSecond,
	float LaunchAccelerationUnitsPerSecondSquared,
	TArray<FSRSpaceLogisticsStarFuelMissile>& StarFuelMissiles,
	int32& NextStarFuelMissileSequence)
{
	OutMissileId = NAME_None;

	FSRSpaceLogisticsHubEndpoint ResolvedSourceHub;
	if (!SpaceLogisticsSubsystem.ResolveCurrentHubEndpoint(SourceHub, ResolvedSourceHub))
	{
		return false;
	}

	ASRStar* TargetStar = ResolvePrimaryStar(SpaceLogisticsSubsystem);
	if (!IsValid(TargetStar))
	{
		return false;
	}

	FSRSpaceLogisticsStarFuelMissile Missile;
	Missile.MissileId = MakeMissileId(ResolvedSourceHub, NextStarFuelMissileSequence);
	Missile.SourceHub = ResolvedSourceHub;
	Missile.TargetStarActor = TargetStar;
	Missile.bEnabled = true;
	ApplyMissileFlightSettings(SpaceLogisticsSubsystem, Missile, InitialSpeedUnitsPerSecond, LaunchAccelerationUnitsPerSecondSquared);
	if (!StartMissileTravel(SpaceLogisticsSubsystem, Missile))
	{
		return false;
	}

	FSRResourceInstance Cargo;
	if (!TryTakeFuelCargoFromHubInputPort(ResolvedSourceHub, InputPortIndex, Cargo))
	{
		return false;
	}

	Missile.Cargo = Cargo;
	OutMissileId = Missile.MissileId;
	StarFuelMissiles.Add(Missile);

	const double FuelAmount = StarRovers::SpaceLogistics::StarFuelMissiles::CalculateMissileFuelValue(Cargo);
	SR_LOG(SpaceLogistics,
		LogTemp,
		Display,
		TEXT("[SpaceLogistics] Star fuel missile launched from input port: MissileId=%s Source=%s/%s InputPortIndex=%d TargetStar=%s ResourceId=%s StackCount=%d FuelValue=%.3f Duration=%.2f"),
		*OutMissileId.ToString(),
		*GetNameSafe(ResolvedSourceHub.BodyActor.Get()),
		*ResolvedSourceHub.HubOccupantId.ToString(),
		InputPortIndex,
		*GetNameSafe(TargetStar),
		*Cargo.ResourceId.ToString(),
		Cargo.StackCount,
		FuelAmount,
		Missile.TravelDurationSeconds);
	return true;
}

void FSRSpaceLogisticsStarFuelMissileProcessor::ProcessMissiles(
	USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	float DeltaTime,
	TArray<FSRSpaceLogisticsStarFuelMissile>& StarFuelMissiles,
	TMap<FName, TObjectPtr<ASRSpaceshipActor>>& MissileActorsByMissileId)
{
	TArray<int32> ImpactedMissileIndices;
	for (int32 MissileIndex = 0; MissileIndex < StarFuelMissiles.Num(); ++MissileIndex)
	{
		FSRSpaceLogisticsStarFuelMissile& Missile = StarFuelMissiles[MissileIndex];
		if (!Missile.bEnabled || !Missile.IsValid())
		{
			ImpactedMissileIndices.Add(MissileIndex);
			FSRSpaceLogisticsRouteVisualController::DestroyRouteActor(Missile.MissileId, MissileActorsByMissileId);
			continue;
		}

		FSRSpaceLogisticsHubEndpoint ResolvedSourceHub;
		if (SpaceLogisticsSubsystem.ResolveCurrentHubEndpoint(Missile.SourceHub, ResolvedSourceHub))
		{
			Missile.SourceHub = ResolvedSourceHub;
		}

		AdvanceMissile(
			SpaceLogisticsSubsystem,
			Missile,
			DeltaTime,
			ImpactedMissileIndices,
			MissileIndex,
			MissileActorsByMissileId);
	}

	ImpactedMissileIndices.Sort(TGreater<int32>());
	for (const int32 MissileIndex : ImpactedMissileIndices)
	{
		if (StarFuelMissiles.IsValidIndex(MissileIndex))
		{
			StarFuelMissiles.RemoveAt(MissileIndex);
		}
	}
}

ASRStar* FSRSpaceLogisticsStarFuelMissileProcessor::ResolvePrimaryStar(
	USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem)
{
	UWorld* World = SpaceLogisticsSubsystem.GetWorld();
	USRCelestialBodyRegistrySubsystem* CelestialRegistry = IsValid(World)
		? World->GetSubsystem<USRCelestialBodyRegistrySubsystem>()
		: nullptr;
	if (!IsValid(CelestialRegistry))
	{
		return nullptr;
	}

	AActor* PrimaryStarActor = CelestialRegistry->GetPrimaryStarActor();
	if (!IsValid(PrimaryStarActor))
	{
		CelestialRegistry->RefreshCelestialBodies();
		PrimaryStarActor = CelestialRegistry->GetPrimaryStarActor();
	}

	return Cast<ASRStar>(PrimaryStarActor);
}

bool FSRSpaceLogisticsStarFuelMissileProcessor::TryTakeFuelCargoFromHub(
	const FSRSpaceLogisticsHubEndpoint& SourceHub,
	FSRResourceInstance& OutCargo)
{
	OutCargo = FSRResourceInstance();
	AActor* BodyActor = SourceHub.BodyActor.Get();
	USRFacilityNetworkComponent* FacilityNetwork = IsValid(BodyActor)
		? BodyActor->FindComponentByClass<USRFacilityNetworkComponent>()
		: nullptr;
	if (!IsValid(FacilityNetwork))
	{
		return false;
	}

	return FacilityNetwork->TryTakeHubOutboundCargoMatching(
		SourceHub.HubOccupantId,
		1,
		[](const FSRResourceInstance& CandidateCargo)
		{
			return StarRovers::SpaceLogistics::StarFuelMissiles::CanUseAsMissileFuelCargo(CandidateCargo);
		},
		OutCargo);
}

bool FSRSpaceLogisticsStarFuelMissileProcessor::TryTakeFuelCargoFromHubInputPort(
	const FSRSpaceLogisticsHubEndpoint& SourceHub,
	int32 InputPortIndex,
	FSRResourceInstance& OutCargo)
{
	OutCargo = FSRResourceInstance();
	AActor* BodyActor = SourceHub.BodyActor.Get();
	USRFacilityNetworkComponent* FacilityNetwork = IsValid(BodyActor)
		? BodyActor->FindComponentByClass<USRFacilityNetworkComponent>()
		: nullptr;
	if (!IsValid(FacilityNetwork))
	{
		return false;
	}

	return FacilityNetwork->TryTakeHubOutboundCargoMatchingFromInputPort(
		SourceHub.HubOccupantId,
		InputPortIndex,
		1,
		[](const FSRResourceInstance& CandidateCargo)
		{
			return StarRovers::SpaceLogistics::StarFuelMissiles::CanUseAsMissileFuelCargo(CandidateCargo);
		},
		OutCargo);
}

void FSRSpaceLogisticsStarFuelMissileProcessor::ApplyMissileFlightSettings(
	USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	FSRSpaceLogisticsStarFuelMissile& Missile,
	float InitialSpeedUnitsPerSecond,
	float LaunchAccelerationUnitsPerSecondSquared)
{
	float DefaultInitialSpeedUnitsPerSecond = FSRSpaceLogisticsRoutePathResolver::GetDefaultInitialSpeedUnitsPerSecond();
	float DefaultLaunchAccelerationUnitsPerSecondSquared =
		FSRSpaceLogisticsRoutePathResolver::GetDefaultLaunchAccelerationUnitsPerSecondSquared();
	FSRSpaceLogisticsRouteVisualController::ResolveDefaultFlightSettings(
		SpaceLogisticsSubsystem.GetWorld(),
		DefaultInitialSpeedUnitsPerSecond,
		DefaultLaunchAccelerationUnitsPerSecondSquared);

	Missile.InitialSpeedUnitsPerSecond = FSRSpaceLogisticsRoutePathResolver::ClampInitialSpeed(
		InitialSpeedUnitsPerSecond > 0.0f
			? InitialSpeedUnitsPerSecond
			: DefaultInitialSpeedUnitsPerSecond);
	Missile.LaunchAccelerationUnitsPerSecondSquared = FSRSpaceLogisticsRoutePathResolver::ClampLaunchAcceleration(
		LaunchAccelerationUnitsPerSecondSquared > 0.0f
			? LaunchAccelerationUnitsPerSecondSquared
			: DefaultLaunchAccelerationUnitsPerSecondSquared);
}

bool FSRSpaceLogisticsStarFuelMissileProcessor::StartMissileTravel(
	USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	FSRSpaceLogisticsStarFuelMissile& Missile)
{
	Missile.TravelProgressSeconds = 0.0f;
	Missile.TravelProgressRatio = 0.0f;

	if (!SpaceLogisticsSubsystem.ResolveHubEndpointSurfaceWorldLocation(Missile.SourceHub, Missile.TravelStartWorldLocation))
	{
		Missile.bHasTravelStartWorldLocation = false;
		Missile.LaunchWorldVelocity = FVector::ZeroVector;
		Missile.bHasLaunchWorldVelocity = false;
		SR_LOG(SpaceLogistics,
			LogTemp,
			Warning,
			TEXT("[SpaceLogistics] Star fuel missile launch failed because start location could not be resolved: MissileId=%s"),
			*Missile.MissileId.ToString());
		return false;
	}

	FVector LaunchWorldVelocity = FVector::ZeroVector;
	Missile.bHasLaunchWorldVelocity = SpaceLogisticsSubsystem.ResolveHubEndpointWorldVelocity(Missile.SourceHub, LaunchWorldVelocity);
	Missile.LaunchWorldVelocity = Missile.bHasLaunchWorldVelocity
		? LaunchWorldVelocity
		: FVector::ZeroVector;
	Missile.bHasTravelStartWorldLocation = true;

	FVector InitialWorldLocation = FVector::ZeroVector;
	FVector TargetWorldLocation = FVector::ZeroVector;
	FVector TravelDirection = FVector::ZeroVector;
	if (!FSRSpaceLogisticsRoutePathResolver::ResolveStarFuelMissileVisualWorldLocation(
		SpaceLogisticsSubsystem,
		Missile,
		InitialWorldLocation,
		TargetWorldLocation,
		TravelDirection))
	{
		return false;
	}

	Missile.TravelDurationSeconds = FSRSpaceLogisticsRoutePathResolver::ResolveStarFuelMissileTravelDurationSeconds(
		SpaceLogisticsSubsystem,
		Missile);
	return true;
}

void FSRSpaceLogisticsStarFuelMissileProcessor::AdvanceMissile(
	USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	FSRSpaceLogisticsStarFuelMissile& Missile,
	float DeltaTime,
	TArray<int32>& OutImpactedMissileIndices,
	int32 MissileIndex,
	TMap<FName, TObjectPtr<ASRSpaceshipActor>>& MissileActorsByMissileId)
{
	Missile.TravelProgressSeconds = FMath::Max(0.0f, Missile.TravelProgressSeconds + FMath::Max(0.0f, DeltaTime));
	Missile.TravelProgressRatio = FSRSpaceLogisticsRoutePathResolver::ResolveStarFuelMissileMotionProgressRatio(
		SpaceLogisticsSubsystem,
		Missile,
		Missile.TravelProgressSeconds);

	if (!HasMissileImpactedTargetStar(SpaceLogisticsSubsystem, Missile))
	{
		return;
	}

	ASRStar* TargetStar = Cast<ASRStar>(Missile.TargetStarActor.Get());
	const double DeliveredFuelAmount = StarRovers::SpaceLogistics::StarFuelMissiles::CalculateMissileFuelValue(Missile.Cargo);
	const bool bDeliveredFuel = IsValid(TargetStar)
		&& DeliveredFuelAmount > UE_DOUBLE_SMALL_NUMBER;
	if (bDeliveredFuel)
	{
		TargetStar->AddStellarFuel(DeliveredFuelAmount);
	}

	SR_LOG(SpaceLogistics,
		LogTemp,
		Display,
		TEXT("[SpaceLogistics] Star fuel missile impacted: MissileId=%s TargetStar=%s ResourceId=%s StackCount=%d DeliveredFuel=%.3f Success=%s"),
		*Missile.MissileId.ToString(),
		*GetNameSafe(TargetStar),
		*Missile.Cargo.ResourceId.ToString(),
		Missile.Cargo.StackCount,
		DeliveredFuelAmount,
		bDeliveredFuel ? TEXT("true") : TEXT("false"));

	FSRSpaceLogisticsRouteVisualController::DestroyRouteActor(Missile.MissileId, MissileActorsByMissileId);
	OutImpactedMissileIndices.Add(MissileIndex);
}

bool FSRSpaceLogisticsStarFuelMissileProcessor::HasMissileImpactedTargetStar(
	const USRSpaceLogisticsSubsystem& SpaceLogisticsSubsystem,
	const FSRSpaceLogisticsStarFuelMissile& Missile)
{
	AActor* TargetStarActor = Missile.TargetStarActor.Get();
	if (!IsValid(TargetStarActor))
	{
		return true;
	}

	FVector MissileWorldLocation = FVector::ZeroVector;
	FVector TargetWorldLocation = FVector::ZeroVector;
	FVector TravelDirection = FVector::ZeroVector;
	if (!FSRSpaceLogisticsRoutePathResolver::ResolveStarFuelMissileVisualWorldLocation(
		SpaceLogisticsSubsystem,
		Missile,
		MissileWorldLocation,
		TargetWorldLocation,
		TravelDirection))
	{
		return Missile.TravelProgressRatio >= 1.0f;
	}

	const float ImpactRadius = ResolveTargetStarImpactRadius(*TargetStarActor);
	return Missile.TravelProgressRatio >= 1.0f
		|| FVector::DistSquared(MissileWorldLocation, TargetStarActor->GetActorLocation()) <= FMath::Square(ImpactRadius);
}

float FSRSpaceLogisticsStarFuelMissileProcessor::ResolveTargetStarImpactRadius(const AActor& TargetStarActor)
{
	TArray<USphereComponent*> SphereComponents;
	TargetStarActor.GetComponents(SphereComponents);
	for (const USphereComponent* SphereComponent : SphereComponents)
	{
		if (IsValid(SphereComponent) && StarRovers::SpaceLogistics::StarFuelMissiles::IsClickSphereCollision(*SphereComponent))
		{
			return FMath::Max(1.0f, SphereComponent->GetScaledSphereRadius());
		}
	}

	if (const ASRCelestialBody* CelestialBody = Cast<ASRCelestialBody>(&TargetStarActor))
	{
		return FMath::Max(1.0f, CelestialBody->GetData().Scale);
	}

	return 1.0f;
}

FName FSRSpaceLogisticsStarFuelMissileProcessor::MakeMissileId(
	const FSRSpaceLogisticsHubEndpoint& SourceHub,
	int32& NextStarFuelMissileSequence)
{
	return FName(*FString::Printf(
		TEXT("StarFuelMissile_%s_%d"),
		*SourceHub.HubOccupantId.ToString(),
		NextStarFuelMissileSequence++));
}
