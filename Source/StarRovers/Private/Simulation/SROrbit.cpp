#include "Simulation/SROrbit.h"

#include "Celestial/SRCelestialBody.h"
#include "Components/SceneComponent.h"
#include "Simulation/SRTimeControlSubsystem.h"
#include "Visual/SRCelestialRingMeshComponent.h"

namespace
{
	const FName SROrbitLineTag(TEXT("StarRovers.OrbitLine"));
	const FName SROrbitLineRootTag(TEXT("StarRovers.OrbitLineRoot"));
}

USROrbit::USROrbit()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void USROrbit::OnRegister()
{
	Super::OnRegister();

	EnsureOrbitRingVisual();
	RefreshDerivedState();
	UpdateTickDependency();
	RefreshOrbitLineVisual();
}

void USROrbit::BeginPlay()
{
	Super::BeginPlay();

	RefreshDerivedState();
	UpdateTickDependency();
	RefreshOrbitLineVisual();
	SetComponentTickEnabled(HasOrbit());
}

void USROrbit::OnUnregister()
{
	ReleaseOrbitRingVisual();

	if (AActor* Owner = GetOwner())
	{
		if (IsValid(OrbitTickDependencyActor))
		{
			Owner->RemoveTickPrerequisiteActor(OrbitTickDependencyActor);
		}
	}

	OrbitTickDependencyActor = nullptr;
	Super::OnUnregister();
}

void USROrbit::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RefreshDerivedState();

	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !HasOrbit())
	{
		RefreshOrbitLineVisual();
		return;
	}

	OrbitElapsedTimeSeconds = FMath::Max(0.0f, OrbitElapsedTimeSeconds + ResolveSimulationDeltaSeconds(DeltaTime));
	const FVector OrbitLocation = ComputeOrbitLocationAtCurrentPhase();
	if (!Owner->GetActorLocation().Equals(OrbitLocation, 0.1f))
	{
		Owner->SetActorLocation(OrbitLocation, false, nullptr, ETeleportType::TeleportPhysics);
		if (ASRCelestialBody* CelestialBody = Cast<ASRCelestialBody>(Owner))
		{
			CelestialBody->RefreshMaterialParameters();
			CelestialBody->RefreshRotationAxisLineVisual();
		}
	}

	RefreshOrbitLineVisual();
}

void USROrbit::ConfigureOrbit(AActor* NewParentBody, float NewOrbitRadius, float NewOrbitPeriod, float NewInitialAngleDegrees)
{
	ParentBody = NewParentBody;
	OrbitRadius = FMath::Max(0.0f, NewOrbitRadius);
	this->OrbitPeriod = FMath::Max(0.0f, NewOrbitPeriod);
	InitialAngleDegrees = NewInitialAngleDegrees;
	RefreshDerivedState();
	UpdateTickDependency();
	ResetOrbitSimulation();
	RefreshOrbitLineVisual();
	SetComponentTickEnabled(HasOrbit());
}

void USROrbit::ConfigureOrbitLineVisual(bool bNewShowOrbitLine, FLinearColor NewOrbitLineColor, float NewOrbitLineOpacity, int32 NewOrbitLineSegments, float NewOrbitLineThickness)
{
	ShowOrbitLine = bNewShowOrbitLine;
	OrbitLineColor = NewOrbitLineColor;
	OrbitLineOpacity = FMath::Clamp(NewOrbitLineOpacity, 0.0f, 1.0f);
	OrbitLineSegments = FMath::Max(3, NewOrbitLineSegments);
	OrbitLineThickness = FMath::Max(0.0f, NewOrbitLineThickness);
	RefreshOrbitLineVisual();
}

void USROrbit::ResetOrbitSimulation()
{
	AActor* Owner = GetOwner();
	OrbitElapsedTimeSeconds = 0.0f;
	OrbitAnchorLocation = IsValid(Owner) ? Owner->GetActorLocation() : FVector::ZeroVector;

	if (IsValid(Owner) && HasOrbit())
	{
		const FVector OrbitLocation = ComputeOrbitLocationAtCurrentPhase();
		if (!Owner->GetActorLocation().Equals(OrbitLocation, 0.1f))
		{
			Owner->SetActorLocation(OrbitLocation, false, nullptr, ETeleportType::TeleportPhysics);
			if (ASRCelestialBody* CelestialBody = Cast<ASRCelestialBody>(Owner))
			{
				CelestialBody->RefreshMaterialParameters();
				CelestialBody->RefreshRotationAxisLineVisual();
			}
		}
	}
}

bool USROrbit::ShouldShowOrbitLine() const
{
	return ShowOrbitLine && HasOrbit() && OrbitRadius > KINDA_SMALL_NUMBER;
}

FLinearColor USROrbit::GetOrbitLineColor() const
{
	return OrbitLineColor;
}

float USROrbit::GetOrbitLineOpacity() const
{
	return FMath::Clamp(OrbitLineOpacity, 0.0f, 1.0f);
}

int32 USROrbit::GetOrbitLineSegments() const
{
	return FMath::Max(3, OrbitLineSegments);
}

float USROrbit::GetOrbitLineThickness() const
{
	return FMath::Max(0.0f, OrbitLineThickness);
}

void USROrbit::RefreshOrbitLineVisual()
{
	EnsureOrbitRingVisual();
	if (!IsValid(OrbitRingVisual))
	{
		const AActor* Owner = GetOwner();
		if (IsValid(Owner) && Owner->HasActorBegunPlay() && ShouldShowOrbitLine())
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("USROrbit cannot draw orbit line for owner '%s': OrbitRingVisual is null after lookup, ShowOrbitLine=true, OrbitRadius=%.2f, OrbitPeriodSeconds=%.2f, and no registered USRCelestialRingMeshComponent named 'OrbitRingVisual' was available."),
				*Owner->GetName(),
				OrbitRadius,
				OrbitPeriodSeconds);
		}
		return;
	}

	if (!ShouldShowOrbitLine())
	{
		OrbitRingVisual->ClearRingVisual();
		return;
	}

	const FLinearColor LineColor(OrbitLineColor.R, OrbitLineColor.G, OrbitLineColor.B, GetOrbitLineOpacity());
	OrbitRingVisual->UpdateRingVisual(
		ComputeOrbitCenterLocation(),
		OrbitRadius,
		LineColor,
		GetOrbitLineThickness(),
		GetOrbitLineSegments());
}

AActor* USROrbit::GetParentBody() const
{
	return ParentBody.Get();
}

float USROrbit::GetOrbitRadius() const
{
	return OrbitRadius;
}

float USROrbit::GetOrbitPeriod() const
{
	return OrbitPeriod;
}

float USROrbit::GetOrbitPeriodSeconds() const
{
	return OrbitPeriodSeconds;
}

float USROrbit::GetInitialAngleDegrees() const
{
	return InitialAngleDegrees;
}

bool USROrbit::HasOrbit() const
{
	return IsValid(ParentBody) && OrbitRadius > KINDA_SMALL_NUMBER && OrbitPeriodSeconds > KINDA_SMALL_NUMBER;
}

FVector USROrbit::ComputeOrbitCenterLocation() const
{
	if (!IsValid(ParentBody))
	{
		return OrbitAnchorLocation;
	}

	const FVector ParentLocation = ParentBody->GetActorLocation();
	return FVector(OrbitAnchorLocation.X, ParentLocation.Y, ParentLocation.Z);
}

FVector USROrbit::ComputeOrbitLocationAtCurrentPhase() const
{
	return ComputeOrbitLocationAtAngle(ComputeOrbitAngleRadians());
}

USRTimeControlSubsystem* USROrbit::FindTimeControlSubsystem() const
{
	if (CachedTimeControlSubsystem.IsValid())
	{
		return CachedTimeControlSubsystem.Get();
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		CachedTimeControlSubsystem = nullptr;
		return nullptr;
	}

	USRTimeControlSubsystem* TimeControlSubsystem = World->GetSubsystem<USRTimeControlSubsystem>();
	CachedTimeControlSubsystem = TimeControlSubsystem;
	return TimeControlSubsystem;
}

float USROrbit::ResolveSecondsPerPeriod() const
{
	if (const USRTimeControlSubsystem* TimeControlSubsystem = FindTimeControlSubsystem())
	{
		return FMath::Max(0.0f, TimeControlSubsystem->GetSecondsPerPeriod());
	}

	return 20.0f;
}

void USROrbit::RefreshDerivedState()
{
	OrbitPeriodSeconds = FMath::Max(0.0f, OrbitPeriod) * ResolveSecondsPerPeriod();
}

void USROrbit::UpdateTickDependency()
{
	AActor* Owner = GetOwner();
	AActor* DesiredDependencyActor = IsValid(ParentBody) ? ParentBody.Get() : nullptr;
	if (!IsValid(Owner) || OrbitTickDependencyActor == DesiredDependencyActor)
	{
		return;
	}

	if (IsValid(OrbitTickDependencyActor))
	{
		Owner->RemoveTickPrerequisiteActor(OrbitTickDependencyActor);
	}

	OrbitTickDependencyActor = DesiredDependencyActor;
	if (IsValid(OrbitTickDependencyActor))
	{
		Owner->AddTickPrerequisiteActor(OrbitTickDependencyActor);
	}
}

float USROrbit::ResolveSimulationDeltaSeconds(float DeltaSeconds) const
{
	const float ClampedDeltaSeconds = FMath::Max(0.0f, DeltaSeconds);
	if (const USRTimeControlSubsystem* TimeControlSubsystem = FindTimeControlSubsystem())
	{
		return ClampedDeltaSeconds * FMath::Max(0.0f, TimeControlSubsystem->GetEffectiveTimeScale());
	}

	return ClampedDeltaSeconds;
}

float USROrbit::ComputeOrbitAngleRadians() const
{
	if (OrbitPeriodSeconds <= KINDA_SMALL_NUMBER)
	{
		return FMath::DegreesToRadians(InitialAngleDegrees);
	}

	return FMath::DegreesToRadians(InitialAngleDegrees) + ((OrbitElapsedTimeSeconds / OrbitPeriodSeconds) * UE_TWO_PI);
}

FVector USROrbit::ComputeOrbitLocationAtAngle(float AngleRadians) const
{
	const FVector OrbitCenter = ComputeOrbitCenterLocation();
	return FVector(
		OrbitCenter.X,
		OrbitCenter.Y + (FMath::Cos(AngleRadians) * OrbitRadius),
		OrbitCenter.Z + (FMath::Sin(AngleRadians) * OrbitRadius));
}

void USROrbit::EnsureOrbitRingVisual()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || IsValid(OrbitRingVisual))
	{
		return;
	}

	TInlineComponentArray<USRCelestialRingMeshComponent*> RingComponents(Owner);
	Owner->GetComponents(RingComponents);
	for (USRCelestialRingMeshComponent* RingComponent : RingComponents)
	{
		if (IsValid(RingComponent) && RingComponent->GetFName() == TEXT("OrbitRingVisual"))
		{
			OrbitRingVisual = RingComponent;
			break;
		}
	}

	if (IsValid(OrbitRingVisual))
	{
		OrbitRingVisual->SetMobility(EComponentMobility::Movable);
		OrbitRingVisual->SetUsingAbsoluteLocation(true);
		OrbitRingVisual->SetUsingAbsoluteRotation(true);
		OrbitRingVisual->SetUsingAbsoluteScale(true);
		OrbitRingVisual->ComponentTags.AddUnique(SROrbitLineTag);
		OrbitRingVisual->ComponentTags.AddUnique(SROrbitLineRootTag);
		return;
	}

	return;
}

void USROrbit::ReleaseOrbitRingVisual()
{
	if (!IsValid(OrbitRingVisual))
	{
		return;
	}

	OrbitRingVisual->ClearRingVisual();
	OrbitRingVisual = nullptr;
}
