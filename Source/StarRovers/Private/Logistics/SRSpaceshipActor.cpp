#include "Logistics/SRSpaceshipActor.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

namespace
{
	const FName TrailParamEnabled(TEXT("User.TrailEnabled"));
	const FName TrailParamMaterial(TEXT("User.TrailMaterial"));
	const FName TrailParamColor(TEXT("User.TrailColor"));
	const FName TrailParamWidth(TEXT("User.TrailWidth"));
	const FName TrailParamLifetime(TEXT("User.TrailLifetime"));
	const FName TrailParamSpawnRate(TEXT("User.TrailSpawnRate"));
	const FName TrailParamSourcePosition(TEXT("User.SourcePosition"));
	const FName TrailParamSourceVelocity(TEXT("User.SourceVelocity"));
	const FName TrailParamSourceSpeed(TEXT("User.SourceSpeed"));
	const FName TrailParamRouteProgress(TEXT("User.RouteProgress"));
}

ASRSpaceshipActor::ASRSpaceshipActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SpaceshipMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpaceshipMesh"));
	SetRootComponent(SpaceshipMesh);
	SpaceshipMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpaceshipMesh->SetGenerateOverlapEvents(false);
	SpaceshipMesh->SetCastShadow(false);
	SpaceshipMesh->SetMobility(EComponentMobility::Movable);

	FocusCollision = CreateDefaultSubobject<USphereComponent>(TEXT("FocusCollision"));
	FocusCollision->SetupAttachment(SpaceshipMesh);
	FocusCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	FocusCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	FocusCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	FocusCollision->SetGenerateOverlapEvents(false);
	FocusCollision->SetHiddenInGame(true);
	FocusCollision->ComponentTags.Add(TEXT("StarRovers.FocusCollision"));

	TrailAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("TrailAnchor"));
	TrailAnchor->SetupAttachment(SpaceshipMesh);
	TrailAnchor->SetMobility(EComponentMobility::Movable);
	TrailAnchor->SetRelativeLocation(FVector(0.0f, 0.0f, -450.0f));

	TrailNiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TrailNiagara"));
	TrailNiagaraComponent->SetupAttachment(TrailAnchor);
	TrailNiagaraComponent->SetMobility(EComponentMobility::Movable);
	TrailNiagaraComponent->SetAutoActivate(false);
	TrailNiagaraComponent->SetHiddenInGame(true);
	TrailNiagaraComponent->SetVisibility(false, true);
	TrailNiagaraComponent->ComponentTags.Add(TEXT("StarRovers.SpaceshipTrail"));

	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	ConfigureTrail();
}

void ASRSpaceshipActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (FocusCollision)
	{
		FocusCollision->SetSphereRadius(FMath::Max(1.0f, FocusTraceRadius));
	}

	ConfigureTrail();
}

void ASRSpaceshipActor::SetRouteId(FName NewRouteId)
{
	if (RouteId != NewRouteId)
	{
		ResetTrailState();
		ResetVisualMotionState();
	}
	RouteId = NewRouteId;
}

FName ASRSpaceshipActor::GetRouteId() const
{
	return RouteId;
}

float ASRSpaceshipActor::GetInitialSpeedUnitsPerSecond() const
{
	return FMath::Max(100.0f, InitialSpeedUnitsPerSecond);
}

float ASRSpaceshipActor::GetLaunchAccelerationUnitsPerSecondSquared() const
{
	return FMath::Max(1.0f, LaunchAccelerationUnitsPerSecondSquared);
}

void ASRSpaceshipActor::UpdateRouteVisual(const FVector& WorldLocation, const FVector& TargetWorldLocation, float TravelProgressRatio)
{
	UpdateRouteVisualWithDirection(
		WorldLocation,
		TargetWorldLocation,
		TargetWorldLocation - WorldLocation,
		TravelProgressRatio);
}

void ASRSpaceshipActor::UpdateRouteVisualWithDirection(
	const FVector& WorldLocation,
	const FVector& TargetWorldLocation,
	const FVector& TravelDirection,
	float TravelProgressRatio)
{
	const FVector EffectiveTravelDirection = ResolveEffectiveTravelDirection(
		WorldLocation,
		TargetWorldLocation,
		TravelDirection);
	const FQuat VisualRotation = ResolveVisualRotation(EffectiveTravelDirection);
	SetActorLocationAndRotation(WorldLocation, VisualRotation, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorHiddenInGame(false);
	UpdateTrailParameters(WorldLocation, TravelProgressRatio);
}

void ASRSpaceshipActor::ConfigureTrail()
{
	if (!TrailNiagaraComponent)
	{
		return;
	}

	TrailNiagaraComponent->SetAsset(TrailNiagaraSystem);
	TrailNiagaraComponent->SetRelativeLocation(FVector::ZeroVector);
	TrailNiagaraComponent->SetRelativeRotation(FRotator::ZeroRotator);
	TrailNiagaraComponent->SetRelativeScale3D(FVector::OneVector);

	const bool bShouldShowTrail = bTrailEnabled && IsValid(TrailNiagaraSystem);
	TrailNiagaraComponent->SetVisibility(bShouldShowTrail, true);
	TrailNiagaraComponent->SetHiddenInGame(!bShouldShowTrail);
	ApplyTrailUserParameters();

	if (!bShouldShowTrail)
	{
		TrailNiagaraComponent->DeactivateImmediate();
	}
}

void ASRSpaceshipActor::ApplyTrailUserParameters()
{
	if (!TrailNiagaraComponent)
	{
		return;
	}

	const bool bShouldShowTrail = bTrailEnabled && IsValid(TrailNiagaraSystem);
	TrailNiagaraComponent->SetVariableBool(TrailParamEnabled, bShouldShowTrail);
	TrailNiagaraComponent->SetVariableMaterial(TrailParamMaterial, TrailMaterial);
	TrailNiagaraComponent->SetVariableLinearColor(TrailParamColor, TrailColor);
	TrailNiagaraComponent->SetVariableFloat(TrailParamWidth, FMath::Max(0.0f, TrailWidth));
	TrailNiagaraComponent->SetVariableFloat(TrailParamLifetime, FMath::Max(0.01f, TrailLifetime));
	TrailNiagaraComponent->SetVariableFloat(TrailParamSpawnRate, FMath::Max(0.0f, TrailSpawnRate));
}

void ASRSpaceshipActor::ResetTrailState()
{
	bHasLastTrailWorldLocation = false;
	LastTrailWorldLocation = FVector::ZeroVector;
	LastTrailUpdateTimeSeconds = 0.0;
	if (TrailNiagaraComponent)
	{
		TrailNiagaraComponent->SetVariableVec3(TrailParamSourceVelocity, FVector::ZeroVector);
		TrailNiagaraComponent->SetVariableFloat(TrailParamSourceSpeed, 0.0f);
		TrailNiagaraComponent->ResetSystem();
	}
}

void ASRSpaceshipActor::ResetVisualMotionState()
{
	LastVisualWorldLocation = FVector::ZeroVector;
	bHasLastVisualWorldLocation = false;
}

FVector ASRSpaceshipActor::ResolveEffectiveTravelDirection(
	const FVector& WorldLocation,
	const FVector& TargetWorldLocation,
	const FVector& FallbackTravelDirection)
{
	FVector EffectiveTravelDirection = FallbackTravelDirection.IsNearlyZero()
		? TargetWorldLocation - WorldLocation
		: FallbackTravelDirection;

	if (bHasLastVisualWorldLocation)
	{
		const FVector ActualWorldDelta = WorldLocation - LastVisualWorldLocation;
		if (!ActualWorldDelta.IsNearlyZero())
		{
			EffectiveTravelDirection = ActualWorldDelta;
		}
	}

	LastVisualWorldLocation = WorldLocation;
	bHasLastVisualWorldLocation = true;
	return EffectiveTravelDirection;
}

FVector ASRSpaceshipActor::ResolveVisualForwardLocalAxis() const
{
	if (bOrientVisualFromTrailAnchor && TrailAnchor)
	{
		const FVector TrailAnchorLocalDirection = TrailAnchor->GetRelativeLocation().GetSafeNormal();
		if (!TrailAnchorLocalDirection.IsNearlyZero())
		{
			return -TrailAnchorLocalDirection;
		}
	}

	FVector ForwardLocalAxis = VisualForwardLocalAxis.GetSafeNormal();
	if (ForwardLocalAxis.IsNearlyZero())
	{
		ForwardLocalAxis = FVector::UpVector;
	}
	return ForwardLocalAxis;
}

FQuat ASRSpaceshipActor::ResolveVisualRotation(const FVector& TravelDirection) const
{
	const FVector Direction = TravelDirection.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return GetActorQuat();
	}

	return FQuat::FindBetweenNormals(ResolveVisualForwardLocalAxis(), Direction);
}

FVector ASRSpaceshipActor::ResolveTrailSourceWorldLocation(const FVector& FallbackWorldLocation) const
{
	if (TrailAnchor)
	{
		return TrailAnchor->GetComponentLocation();
	}

	if (TrailNiagaraComponent)
	{
		return TrailNiagaraComponent->GetComponentLocation();
	}

	return FallbackWorldLocation;
}

void ASRSpaceshipActor::UpdateTrailParameters(const FVector& WorldLocation, float TravelProgressRatio)
{
	if (!TrailNiagaraComponent || !bTrailEnabled || !IsValid(TrailNiagaraSystem))
	{
		ResetTrailState();
		return;
	}

	TrailNiagaraComponent->SetVisibility(true, true);
	TrailNiagaraComponent->SetHiddenInGame(false);
	ApplyTrailUserParameters();
	if (!TrailNiagaraComponent->IsActive())
	{
		TrailNiagaraComponent->Activate(true);
	}

	UWorld* World = GetWorld();
	const double CurrentTimeSeconds = World ? World->GetTimeSeconds() : 0.0;
	const FVector TrailSourceWorldLocation = ResolveTrailSourceWorldLocation(WorldLocation);
	FVector VisualVelocity = FVector::ZeroVector;
	if (!bHasLastTrailWorldLocation || CurrentTimeSeconds <= LastTrailUpdateTimeSeconds + UE_DOUBLE_SMALL_NUMBER)
	{
		LastTrailWorldLocation = TrailSourceWorldLocation;
		LastTrailUpdateTimeSeconds = CurrentTimeSeconds;
		bHasLastTrailWorldLocation = true;
	}
	else
	{
		const double DeltaTimeSeconds = CurrentTimeSeconds - LastTrailUpdateTimeSeconds;
		VisualVelocity = (TrailSourceWorldLocation - LastTrailWorldLocation) / FMath::Max(static_cast<float>(DeltaTimeSeconds), UE_SMALL_NUMBER);
		LastTrailWorldLocation = TrailSourceWorldLocation;
		LastTrailUpdateTimeSeconds = CurrentTimeSeconds;
		bHasLastTrailWorldLocation = true;
	}

	TrailNiagaraComponent->SetVariablePosition(TrailParamSourcePosition, TrailSourceWorldLocation);
	TrailNiagaraComponent->SetVariableVec3(TrailParamSourceVelocity, VisualVelocity);
	TrailNiagaraComponent->SetVariableFloat(TrailParamSourceSpeed, VisualVelocity.Size());
	TrailNiagaraComponent->SetVariableFloat(TrailParamRouteProgress, FMath::Clamp(TravelProgressRatio, 0.0f, 1.0f));
}
