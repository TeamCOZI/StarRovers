#include "Simulation/SROrbit.h"

#include "Utility/SRLog.h"
#include "Celestial/SRCelestialBody.h"
#include "Components/LineBatchComponent.h"
#include "Components/SceneComponent.h"
#include "Rendering/SRScreenSpaceLineThickness.h"
#include "Simulation/SRTimeControlSubsystem.h"
#include "Rendering/SRCelestialRingMeshComponent.h"
#include "SceneManagement.h"

namespace
{
	const FName SROrbitLineTag(TEXT("StarRovers.OrbitLine"));
	const FName SROrbitLineRootTag(TEXT("StarRovers.OrbitLineRoot"));
	constexpr uint8 SROrbitLineDepthPriority = SDPG_Foreground;
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
	EnsureOrbitLineBatchVisual();

	if (!ShouldShowOrbitLine())
	{
		if (IsValid(OrbitRingVisual))
		{
			OrbitRingVisual->ClearRingVisual();
		}
		ClearOrbitLineBatchVisual();
		return;
	}

	const FLinearColor LineColor(OrbitLineColor.R, OrbitLineColor.G, OrbitLineColor.B, GetOrbitLineOpacity());
	const FVector OrbitCenter = ComputeOrbitCenterLocation();
	if (DrawOrbitLineBatchVisual(OrbitCenter, OrbitRadius, LineColor, GetOrbitLineThickness(), GetOrbitLineSegments()))
	{
		if (IsValid(OrbitRingVisual))
		{
			OrbitRingVisual->ClearRingVisual();
		}
		return;
	}

	if (IsValid(OrbitRingVisual))
	{
		OrbitRingVisual->UpdateRingVisual(
			OrbitCenter,
			OrbitRadius,
			LineColor,
			GetOrbitLineThickness(),
			GetOrbitLineSegments());
		return;
	}

	const AActor* Owner = GetOwner();
	if (IsValid(Owner) && Owner->HasActorBegunPlay())
	{
		SR_LOG(Gravity, LogTemp,
			Error,
			TEXT("USROrbit cannot draw orbit line for owner '%s': no OrbitLineBatch or OrbitRingVisual was available."),
			*Owner->GetName());
	}
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

FVector USROrbit::ComputeOrbitPlaneNormal() const
{
	return FVector::XAxisVector;
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

	TInlineComponentArray<USRCelestialRingMeshComponent*> RingComponents;
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

void USROrbit::EnsureOrbitLineBatchVisual()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || IsValid(OrbitLineBatch))
	{
		return;
	}

	TInlineComponentArray<ULineBatchComponent*> LineBatchComponents;
	Owner->GetComponents(LineBatchComponents);
	for (ULineBatchComponent* LineBatchComponent : LineBatchComponents)
	{
		if (IsValid(LineBatchComponent) && LineBatchComponent->GetFName() == TEXT("OrbitLineBatch"))
		{
			OrbitLineBatch = LineBatchComponent;
			break;
		}
	}

	if (!IsValid(OrbitLineBatch))
	{
		return;
	}

	OrbitLineBatch->SetMobility(EComponentMobility::Movable);
	OrbitLineBatch->SetUsingAbsoluteLocation(true);
	OrbitLineBatch->SetUsingAbsoluteRotation(true);
	OrbitLineBatch->SetUsingAbsoluteScale(true);
	OrbitLineBatch->ComponentTags.AddUnique(SROrbitLineTag);
	OrbitLineBatch->ComponentTags.AddUnique(SROrbitLineRootTag);
}

void USROrbit::ClearOrbitLineBatchVisual() const
{
	if (!IsValid(OrbitLineBatch))
	{
		return;
	}

	OrbitLineBatch->Flush();
	OrbitLineBatch->SetVisibility(false);
	OrbitLineBatch->SetHiddenInGame(true);
}

bool USROrbit::DrawOrbitLineBatchVisual(
	const FVector& WorldCenter,
	float Radius,
	const FLinearColor& Color,
	float LineThickness,
	int32 SegmentCount) const
{
	if (!IsValid(OrbitLineBatch))
	{
		return false;
	}

	const float SafeRadius = FMath::Max(0.0f, Radius);
	const float SafeThickness = FMath::Max(0.0f, LineThickness);
	const int32 SafeSegmentCount = FMath::Max(3, SegmentCount);
	if (SafeRadius <= KINDA_SMALL_NUMBER || SafeThickness <= KINDA_SMALL_NUMBER || Color.A <= KINDA_SMALL_NUMBER)
	{
		ClearOrbitLineBatchVisual();
		return true;
	}

	OrbitLineBatch->Flush();
	OrbitLineBatch->SetWorldLocation(FVector::ZeroVector);
	OrbitLineBatch->SetWorldRotation(FRotator::ZeroRotator);
	OrbitLineBatch->SetWorldScale3D(FVector::OneVector);
	OrbitLineBatch->SetVisibility(true);
	OrbitLineBatch->SetHiddenInGame(false);

	UWorld* World = GetWorld();
	FSRScreenSpaceLineViewInfo CameraInfo;
	FSRScreenSpaceLineThickness::TryBuildPrimaryCameraViewInfo(World, CameraInfo);

	float ReferenceViewDepth = FSRScreenSpaceLineThickness::DefaultReferenceViewDepth;
	float ReferenceFieldOfViewDegrees = FSRScreenSpaceLineThickness::DefaultReferenceFieldOfViewDegrees;
	FSRScreenSpaceLineThickness::ResolveReferenceViewParameters(World, ReferenceViewDepth, ReferenceFieldOfViewDegrees);
	const float ReferenceTanHalfFieldOfView = FSRScreenSpaceLineThickness::ComputeReferenceTanHalfFieldOfView(ReferenceFieldOfViewDegrees);

	for (int32 SegmentIndex = 0; SegmentIndex < SafeSegmentCount; ++SegmentIndex)
	{
		const float AlphaA = static_cast<float>(SegmentIndex) / static_cast<float>(SafeSegmentCount);
		const float AlphaB = static_cast<float>(SegmentIndex + 1) / static_cast<float>(SafeSegmentCount);
		const float AngleA = AlphaA * UE_TWO_PI;
		const float AngleB = AlphaB * UE_TWO_PI;
		float SinA = 0.0f;
		float CosA = 1.0f;
		float SinB = 0.0f;
		float CosB = 1.0f;
		FMath::SinCos(&SinA, &CosA, AngleA);
		FMath::SinCos(&SinB, &CosB, AngleB);
		const FVector StartPoint(
			WorldCenter.X,
			WorldCenter.Y + CosA * SafeRadius,
			WorldCenter.Z + SinA * SafeRadius);
		const FVector EndPoint(
			WorldCenter.X,
			WorldCenter.Y + CosB * SafeRadius,
			WorldCenter.Z + SinB * SafeRadius);
		const FVector SegmentMidpoint = (StartPoint + EndPoint) * 0.5f;
		const float SegmentThickness = FSRScreenSpaceLineThickness::ComputeWorldThicknessForScreenSpaceLineWithReferenceTan(
			CameraInfo,
			SegmentMidpoint,
			SafeThickness,
			ReferenceViewDepth,
			ReferenceTanHalfFieldOfView);
		OrbitLineBatch->DrawLine(StartPoint, EndPoint, Color, SROrbitLineDepthPriority, SegmentThickness, 0.0f);
	}

	return true;
}

void USROrbit::ReleaseOrbitRingVisual()
{
	ClearOrbitLineBatchVisual();
	OrbitLineBatch = nullptr;

	if (!IsValid(OrbitRingVisual))
	{
		return;
	}

	OrbitRingVisual->ClearRingVisual();
	OrbitRingVisual = nullptr;
}
