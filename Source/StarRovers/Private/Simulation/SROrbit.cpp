#include "Simulation/SROrbit.h"

#include "Components/LineBatchComponent.h"
#include "Components/SceneComponent.h"
#include "Simulation/SRTimeControlSubsystem.h"
#include "Visual/SRLineThicknessUtils.h"

namespace
{
	const FName SROrbitLineTag(TEXT("StarRovers.OrbitLine"));
	const FName SROrbitLineRootTag(TEXT("StarRovers.OrbitLineRoot"));
	constexpr uint8 OrbitLineDepthPriority = SDPG_World;

	void AppendOrbitCircleLines(
		ULineBatchComponent* LineBatcher,
		const FVector& Center,
		const float Radius,
		const int32 SegmentCount,
		const FLinearColor& Color,
		const float BaseThickness,
		const FSRCameraInfo& CameraInfo,
		const float ReferenceViewDepth,
		const float ReferenceFieldOfViewDegrees)
	{
		if (!IsValid(LineBatcher) || Radius <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		const int32 SafeSegmentCount = FMath::Max(3, SegmentCount);
		const FColor SegmentColor = Color.ToFColor(true);
		FVector PreviousPoint = Center + FVector(0.0f, Radius, 0.0f);

		for (int32 SegmentIndex = 1; SegmentIndex <= SafeSegmentCount; ++SegmentIndex)
		{
			const float Alpha = static_cast<float>(SegmentIndex) / static_cast<float>(SafeSegmentCount);
			const float AngleRadians = Alpha * 2.0f * PI;
			const FVector CurrentPoint = Center + FVector(
				0.0f,
				FMath::Cos(AngleRadians) * Radius,
				FMath::Sin(AngleRadians) * Radius);
			const FVector SegmentMidpoint = (PreviousPoint + CurrentPoint) * 0.5f;
			const float Thickness = FSRLineThicknessUtils::ComputeWorldThicknessAtLocation(
				CameraInfo,
				SegmentMidpoint,
				BaseThickness,
				ReferenceViewDepth,
				ReferenceFieldOfViewDegrees);

			LineBatcher->DrawLine(
				PreviousPoint,
				CurrentPoint,
				SegmentColor,
				OrbitLineDepthPriority,
				FMath::Max(0.0f, Thickness),
				0.0f);

			PreviousPoint = CurrentPoint;
		}
	}
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

	EnsureOrbitLineBatcher();
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
	ReleaseOrbitLineBatcher();

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
	RefreshOrbitLineVisual();

	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !HasOrbit())
	{
		return;
	}

	OrbitElapsedTimeSeconds = FMath::Max(0.0f, OrbitElapsedTimeSeconds + ResolveSimulationDeltaSeconds(DeltaTime));
	const FVector OrbitLocation = ComputeOrbitLocationAtCurrentPhase();
	if (!Owner->GetActorLocation().Equals(OrbitLocation, 0.1f))
	{
		Owner->SetActorLocation(OrbitLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void USROrbit::ConfigureOrbit(AActor* NewParentBody, float NewOrbitRadius, float NewOrbitPeriodInPeriods, float NewStartingPhaseDegrees)
{
	ParentBody = NewParentBody;
	OrbitRadius = FMath::Max(0.0f, NewOrbitRadius);
	OrbitPeriodInPeriods = FMath::Max(0.0f, NewOrbitPeriodInPeriods);
	StartingPhaseDegrees = NewStartingPhaseDegrees;
	RefreshDerivedState();
	UpdateTickDependency();
	ResetOrbitSimulation();
	RefreshOrbitLineVisual();
	SetComponentTickEnabled(HasOrbit());
}

void USROrbit::ConfigureOrbitLineVisual(bool bNewShowOrbitLine, FLinearColor NewOrbitLineColor, float NewOrbitLineOpacity, int32 NewOrbitLineSegments, float NewOrbitLineThickness)
{
	bShowOrbitLine = bNewShowOrbitLine;
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
		}
	}
}

bool USROrbit::ShouldShowOrbitLine() const
{
	return bShowOrbitLine && HasOrbit() && OrbitRadius > KINDA_SMALL_NUMBER;
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
	EnsureOrbitLineBatcher();
	if (!IsValid(OrbitLineBatcher))
	{
		const AActor* Owner = GetOwner();
		if (IsValid(Owner) && Owner->HasActorBegunPlay() && ShouldShowOrbitLine())
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("USROrbit cannot draw orbit line for owner '%s': OrbitLineBatcher is null after lookup, bShowOrbitLine=true, OrbitRadius=%.2f, OrbitPeriodSeconds=%.2f, and no registered ULineBatchComponent named 'Orbit Line Batch' was available."),
				*Owner->GetName(),
				OrbitRadius,
				OrbitPeriodSeconds);
		}
		return;
	}

	OrbitLineBatcher->Flush();
	if (!ShouldShowOrbitLine())
	{
		return;
	}

	FSRCameraInfo CameraInfo;
	FSRLineThicknessUtils::TryBuildPrimaryCameraInfo(GetWorld(), CameraInfo);

	float ReferenceViewDepth = FSRLineThicknessUtils::DefaultReferenceViewDepth;
	float ReferenceFieldOfViewDegrees = FSRLineThicknessUtils::DefaultReferenceFieldOfViewDegrees;
	FSRLineThicknessUtils::ResolveReferenceView(GetWorld(), ReferenceViewDepth, ReferenceFieldOfViewDegrees);

	const FLinearColor LineColor(OrbitLineColor.R, OrbitLineColor.G, OrbitLineColor.B, GetOrbitLineOpacity());
	AppendOrbitCircleLines(
		OrbitLineBatcher,
		ComputeOrbitCenterLocation(),
		OrbitRadius,
		GetOrbitLineSegments(),
		LineColor,
		GetOrbitLineThickness(),
		CameraInfo,
		ReferenceViewDepth,
		ReferenceFieldOfViewDegrees);
}

AActor* USROrbit::GetParentBody() const
{
	return ParentBody.Get();
}

float USROrbit::GetOrbitRadius() const
{
	return OrbitRadius;
}

float USROrbit::GetOrbitPeriodInPeriods() const
{
	return OrbitPeriodInPeriods;
}

float USROrbit::GetOrbitPeriodSeconds() const
{
	return OrbitPeriodSeconds;
}

float USROrbit::GetStartingPhaseDegrees() const
{
	return StartingPhaseDegrees;
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
	OrbitPeriodSeconds = FMath::Max(0.0f, OrbitPeriodInPeriods) * ResolveSecondsPerPeriod();
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
		return FMath::DegreesToRadians(StartingPhaseDegrees);
	}

	return FMath::DegreesToRadians(StartingPhaseDegrees) + ((OrbitElapsedTimeSeconds / OrbitPeriodSeconds) * UE_TWO_PI);
}

FVector USROrbit::ComputeOrbitLocationAtAngle(float AngleRadians) const
{
	const FVector OrbitCenter = ComputeOrbitCenterLocation();
	return FVector(
		OrbitCenter.X,
		OrbitCenter.Y + (FMath::Cos(AngleRadians) * OrbitRadius),
		OrbitCenter.Z + (FMath::Sin(AngleRadians) * OrbitRadius));
}

void USROrbit::EnsureOrbitLineBatcher()
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || IsValid(OrbitLineBatcher))
	{
		return;
	}

	TInlineComponentArray<ULineBatchComponent*> LineBatchComponents(Owner);
	Owner->GetComponents(LineBatchComponents);
	for (ULineBatchComponent* LineBatchComponent : LineBatchComponents)
	{
		if (IsValid(LineBatchComponent) && LineBatchComponent->GetFName() == TEXT("Orbit Line Batch"))
		{
			OrbitLineBatcher = LineBatchComponent;
			break;
		}
	}

	if (IsValid(OrbitLineBatcher))
	{
		OrbitLineBatcher->SetMobility(EComponentMobility::Movable);
		OrbitLineBatcher->SetUsingAbsoluteLocation(true);
		OrbitLineBatcher->SetUsingAbsoluteRotation(true);
		OrbitLineBatcher->SetUsingAbsoluteScale(true);
		OrbitLineBatcher->ComponentTags.AddUnique(SROrbitLineTag);
		OrbitLineBatcher->ComponentTags.AddUnique(SROrbitLineRootTag);
		return;
	}

	return;
}

void USROrbit::ReleaseOrbitLineBatcher()
{
	if (!IsValid(OrbitLineBatcher))
	{
		return;
	}

	OrbitLineBatcher->Flush();
	OrbitLineBatcher = nullptr;
}
