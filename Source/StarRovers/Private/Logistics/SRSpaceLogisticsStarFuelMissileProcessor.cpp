#include "SRSpaceLogisticsStarFuelMissileProcessor.h"

#include "Automation/SRFacilityNetworkComponent.h"
#include "Celestial/SRCelestialBody.h"
#include "Celestial/SRStar.h"
#include "Components/SphereComponent.h"
#include "Logistics/SRSpaceLogisticsSubsystem.h"
#include "SRSpaceLogisticsRoutePathResolver.h"
#include "Simulation/SRRunModifierSubsystem.h"
#include "SRSpaceLogisticsRouteVisualController.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "Utility/SRLog.h"

namespace StarRovers::SpaceLogistics::StarFuelMissiles
{
	bool HasValidMissileCargo(const FSRResourceInstance& Cargo)
	{
		return StarRovers::PatternRouting::IsValidPatternPayload(Cargo);
	}

	bool CanUseAsMissileFuelCargo(const ASRStar& TargetStar, const FSRResourceInstance& Cargo)
	{
		return HasValidMissileCargo(Cargo) && TargetStar.CanAcceptStellarFuelResource(Cargo);
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
	if (!TryTakeFuelCargoFromHub(ResolvedSourceHub, *TargetStar, Cargo))
	{
		return false;
	}

	Missile.Cargo = Cargo;
	OutMissileId = Missile.MissileId;
	StarFuelMissiles.Add(Missile);

	const FSRStellarPatternScoreResult ScorePreview = TargetStar->PreviewStellarPatternSubmission(Cargo);
	SR_LOG(SpaceLogistics,
		LogTemp,
		Display,
		TEXT("[SpaceLogistics] Stellar Pattern missile launched: MissileId=%s Source=%s/%s TargetStar=%s ResourceId=%s StackCount=%d Contract=%s Score=%lld Bonus=%lld Duration=%.2f"),
		*OutMissileId.ToString(),
		*GetNameSafe(ResolvedSourceHub.BodyActor.Get()),
		*ResolvedSourceHub.HubOccupantId.ToString(),
		*GetNameSafe(TargetStar),
		*Cargo.ResourceId.ToString(),
		Cargo.StackCount,
		*TargetStar->GetStellarPatternContract().ContractId.ToString(),
		static_cast<long long>(ScorePreview.TotalScore),
		static_cast<long long>(ScorePreview.BonusScorePerPattern * static_cast<int64>(Cargo.StackCount)),
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
	if (!TryTakeFuelCargoFromHubInputPort(ResolvedSourceHub, InputPortIndex, *TargetStar, Cargo))
	{
		return false;
	}

	Missile.Cargo = Cargo;
	OutMissileId = Missile.MissileId;
	StarFuelMissiles.Add(Missile);

	const FSRStellarPatternScoreResult ScorePreview = TargetStar->PreviewStellarPatternSubmission(Cargo);
	SR_LOG(SpaceLogistics,
		LogTemp,
		Display,
		TEXT("[SpaceLogistics] Stellar Pattern missile launched from input port: MissileId=%s Source=%s/%s InputPortIndex=%d TargetStar=%s ResourceId=%s StackCount=%d Contract=%s Score=%lld Bonus=%lld Duration=%.2f"),
		*OutMissileId.ToString(),
		*GetNameSafe(ResolvedSourceHub.BodyActor.Get()),
		*ResolvedSourceHub.HubOccupantId.ToString(),
		InputPortIndex,
		*GetNameSafe(TargetStar),
		*Cargo.ResourceId.ToString(),
		Cargo.StackCount,
		*TargetStar->GetStellarPatternContract().ContractId.ToString(),
		static_cast<long long>(ScorePreview.TotalScore),
		static_cast<long long>(ScorePreview.BonusScorePerPattern * static_cast<int64>(Cargo.StackCount)),
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
	const ASRStar& TargetStar,
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
		[&TargetStar](const FSRResourceInstance& CandidateCargo)
		{
			return StarRovers::SpaceLogistics::StarFuelMissiles::CanUseAsMissileFuelCargo(TargetStar, CandidateCargo);
		},
		OutCargo);
}

bool FSRSpaceLogisticsStarFuelMissileProcessor::TryTakeFuelCargoFromHubInputPort(
	const FSRSpaceLogisticsHubEndpoint& SourceHub,
	int32 InputPortIndex,
	const ASRStar& TargetStar,
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
		[&TargetStar](const FSRResourceInstance& CandidateCargo)
		{
			return StarRovers::SpaceLogistics::StarFuelMissiles::CanUseAsMissileFuelCargo(TargetStar, CandidateCargo);
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

	const float BaseTravelDurationSeconds = FSRSpaceLogisticsRoutePathResolver::ResolveStarFuelMissileTravelDurationSeconds(
		SpaceLogisticsSubsystem,
		Missile);
	const FSRResolvedRunModifiers RunModifiers = USRRunModifierSubsystem::ResolveForObject(&SpaceLogisticsSubsystem);
	Missile.TravelDurationSeconds = FMath::Max(
		FSRSpaceLogisticsRoutePathResolver::GetMinimumTravelDurationSeconds(),
		static_cast<float>(static_cast<double>(BaseTravelDurationSeconds) * RunModifiers.LogisticsTravelTimeMultiplier));
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
	FSRStellarPatternScoreResult ScoreResult;
	const bool bSubmittedPattern = IsValid(TargetStar)
		&& TargetStar->SubmitStellarPatternResource(Missile.Cargo, ScoreResult);
	if (!IsValid(TargetStar))
	{
		ScoreResult.FailureReason = TEXT("The target star no longer exists.");
	}

	SR_LOG(SpaceLogistics,
		LogTemp,
		Display,
		TEXT("[SpaceLogistics] Stellar Pattern missile impacted: MissileId=%s TargetStar=%s ResourceId=%s StackCount=%d DemandMatch=%s Score=%lld Hands=%d Success=%s Failure=%s"),
		*Missile.MissileId.ToString(),
		*GetNameSafe(TargetStar),
		*Missile.Cargo.ResourceId.ToString(),
		Missile.Cargo.StackCount,
		ScoreResult.bMatchesDemand ? TEXT("true") : TEXT("false"),
		static_cast<long long>(ScoreResult.TotalScore),
		ScoreResult.HandMatches.Num(),
		bSubmittedPattern ? TEXT("true") : TEXT("false"),
		*ScoreResult.FailureReason);

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
