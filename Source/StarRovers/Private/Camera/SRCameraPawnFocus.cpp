#include "Camera/SRCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "GameFramework/SpringArmComponent.h"
#include "Gravity/SRGravityParent.h"

namespace
{
	constexpr float DefaultFocusZoomMultiplier = 3.0f;
}
void ASRCameraPawn::FocusActor(AActor* NewFocusActor)
{
	FocusActorWithTransition(NewFocusActor, true);
}

void ASRCameraPawn::FocusActorWithTransition(AActor* NewFocusActor, bool bUseArcTransition)
{
	AActor* PreviousFocusedActor = FocusedActor.Get();
	FocusedActor = NewFocusActor;
	FocusDragOffset = FVector::ZeroVector;
	FocusSurfaceRotation = FQuat::Identity;
	FocusSurfaceTargetRotation = FQuat::Identity;
	FocusSurfaceRigAlignmentStartRotation = FQuat::Identity;
	FocusSurfaceRigAlignmentStartOffset = FVector::ZeroVector;
	FocusSurfaceRigAlignmentAxis = FVector::UpVector;
	FocusSurfaceRotationSmoothVelocity = FVector::ZeroVector;
	FocusSurfaceRigAlignmentCurrentAngleRadians = 0.0f;
	FocusSurfaceRigAlignmentTargetAngleRadians = 0.0f;
	bIsResettingFocusSurfaceRotation = false;
	bIsAligningFocusSurfaceRig = false;
	StopFocusArcTransition();
	ClearFocusSurfaceMotion();

	if (FocusedActor)
	{
		DragTargetLocation = GetFocusLocation();
		const FVector CurrentLocation = GetActorLocation();
		const FVector DesiredLocation = DragTargetLocation;
		FocusTrackingDelta = DesiredLocation - CurrentLocation;
		FocusTrackingDeltaVelocity = FVector::ZeroVector;

		if (!Camera)
		{
			UE_LOG(LogTemp, Error, TEXT("ASRCameraPawn requires Camera before focusing an actor."));
			BroadcastFocusedActorChangedIfNeeded(PreviousFocusedActor);
			return;
		}

		const float CurrentCameraFieldOfView = Camera->FieldOfView;
		const float DesiredFocusZoom = USRCelestialBodyRuntimeLibrary::GetCelestialFocusZoomDistance(
			FocusedActor,
			CurrentCameraFieldOfView,
			DefaultFocusZoomMultiplier);
		ZoomDistanceTarget = ClampZoomDistance(DesiredFocusZoom);
		if (bUseArcTransition)
		{
			BeginFocusArcTransition(ZoomDistanceTarget);
		}
	}

	BroadcastFocusedActorChangedIfNeeded(PreviousFocusedActor);
}

void ASRCameraPawn::ClearFocusActor()
{
	AActor* PreviousFocusedActor = FocusedActor.Get();
	if (FocusedActor)
	{
		DragTargetLocation = GetActorLocation();
	}

	FocusedActor = nullptr;
	FocusDragOffset = FVector::ZeroVector;
	FocusTrackingDelta = FVector::ZeroVector;
	FocusTrackingDeltaVelocity = FVector::ZeroVector;
	FocusSurfaceRotation = FQuat::Identity;
	FocusSurfaceTargetRotation = FQuat::Identity;
	FocusSurfaceRigAlignmentStartRotation = FQuat::Identity;
	FocusSurfaceRigAlignmentStartOffset = FVector::ZeroVector;
	FocusSurfaceRigAlignmentAxis = FVector::UpVector;
	FocusSurfaceRotationSmoothVelocity = FVector::ZeroVector;
	FocusSurfaceRigAlignmentCurrentAngleRadians = 0.0f;
	FocusSurfaceRigAlignmentTargetAngleRadians = 0.0f;
	bIsResettingFocusSurfaceRotation = false;
	bIsAligningFocusSurfaceRig = false;
	StopFocusArcTransition();
	ClearFocusSurfaceMotion();
	BroadcastFocusedActorChangedIfNeeded(PreviousFocusedActor);
}

AActor* ASRCameraPawn::GetFocusedActor() const
{
	return FocusedActor;
}

void ASRCameraPawn::SnapToFocusTarget()
{
	StopFocusArcTransition();
	FocusTrackingDelta = FVector::ZeroVector;
	FocusTrackingDeltaVelocity = FVector::ZeroVector;
	if (FocusedActor)
	{
		DragTargetLocation = GetFocusLocation() + FocusDragOffset;
	}

	const FVector DesiredLocation = ClampPivotLocationInsideSpace(DragTargetLocation);
	DragTargetLocation = DesiredLocation;

	if (SpringArm)
	{
		ZoomDistanceTarget = ClampZoomDistance(ZoomDistanceTarget);
		SpringArm->TargetArmLength = ZoomDistanceTarget;
		ApplyZoomDrivenViewRotation(SpringArm->TargetArmLength);
	}
	SetActorLocation(DesiredLocation);
	RefreshScreenSpaceThicknessReferenceView();
}

void ASRCameraPawn::ResetFocus()
{
	if (!IsValid(FocusedActor))
	{
		return;
	}

	FocusDragOffset = FVector::ZeroVector;
	StopFocusArcTransition();
	FocusSurfaceTargetRotation = FQuat::Identity;
	FocusSurfaceRigAlignmentStartRotation = FocusSurfaceRotation.GetNormalized();
	FocusSurfaceRigAlignmentStartOffset = FVector::ZeroVector;
	FocusSurfaceRigAlignmentAxis = FVector::UpVector;
	FocusSurfaceRotationSmoothVelocity = FVector::ZeroVector;
	FocusSurfaceRigAlignmentCurrentAngleRadians = 0.0f;
	FocusSurfaceRigAlignmentTargetAngleRadians = 0.0f;
	bIsResettingFocusSurfaceRotation = true;
	bIsAligningFocusSurfaceRig = false;
	ClearFocusSurfaceMotion();
	bIsDragging = false;
	bHasDragStartMousePosition = false;

	DragTargetLocation = GetFocusLocation();

	const FVector CurrentLocation = GetActorLocation();
	const FVector DesiredLocation = DragTargetLocation;
	FocusTrackingDelta = DesiredLocation - CurrentLocation;
	FocusTrackingDeltaVelocity = FVector::ZeroVector;

	if (!Camera)
	{
		UE_LOG(LogTemp, Error, TEXT("ASRCameraPawn requires Camera before resetting focused camera view."));
		return;
	}

	const float DesiredFocusZoom = USRCelestialBodyRuntimeLibrary::GetCelestialFocusZoomDistance(
		FocusedActor,
		Camera->FieldOfView,
		DefaultFocusZoomMultiplier);
	ZoomDistanceTarget = ClampZoomDistance(DesiredFocusZoom);
}

FSRFocusedActorChangedSignature& ASRCameraPawn::OnFocusedActorChanged()
{
	return FocusedActorChangedEvent;
}

void ASRCameraPawn::BroadcastFocusedActorChangedIfNeeded(AActor* PreviousFocusedActor)
{
	if (PreviousFocusedActor != FocusedActor)
	{
		FocusedActorChangedEvent.Broadcast(FocusedActor.Get());
	}
}

void ASRCameraPawn::BeginFocusArcTransition(float FinalZoomDistance)
{
	if (!IsValid(FocusedActor) || !SpringArm)
	{
		StopFocusArcTransition();
		return;
	}

	const FVector TargetLocation = ClampPivotLocationInsideSpace(GetFocusLocation() + FocusDragOffset);
	const FVector CurrentLocation = GetActorLocation();
	const float TravelDistance = FVector::Dist(CurrentLocation, TargetLocation);
	if (TravelDistance <= KINDA_SMALL_NUMBER)
	{
		StopFocusArcTransition();
		return;
	}

	FocusArcTransitionStartLocation = CurrentLocation;
	FocusArcTransitionElapsed = 0.0f;
	FocusArcTransitionStartZoomDistance = FMath::Max(0.0f, SpringArm->TargetArmLength);
	FocusArcTransitionFinalZoomDistance = ClampZoomDistance(FinalZoomDistance);
	const float DesiredPeakZoomDistance = FMath::Max(
		FMath::Max(FocusArcTransitionStartZoomDistance, FocusArcTransitionFinalZoomDistance),
		TravelDistance * FMath::Max(0.0f, FocusArcZoomOutDistanceMultiplier));
	FocusArcTransitionPeakZoomDistance = ClampZoomDistance(DesiredPeakZoomDistance);
	bIsFocusArcTransitionActive = true;
	FocusTrackingDelta = FVector::ZeroVector;
	FocusTrackingDeltaVelocity = FVector::ZeroVector;
}

void ASRCameraPawn::StopFocusArcTransition()
{
	bIsFocusArcTransitionActive = false;
	FocusArcTransitionElapsed = 0.0f;
	FocusArcTransitionStartLocation = FVector::ZeroVector;
	FocusArcTransitionStartZoomDistance = 0.0f;
	FocusArcTransitionFinalZoomDistance = 0.0f;
	FocusArcTransitionPeakZoomDistance = 0.0f;
}

bool ASRCameraPawn::UpdateFocusArcTransition(float DeltaSeconds, FVector& OutNewLocation)
{
	if (!bIsFocusArcTransitionActive)
	{
		return false;
	}

	if (DeltaSeconds <= UE_SMALL_NUMBER || !IsValid(FocusedActor))
	{
		StopFocusArcTransition();
		return false;
	}

	FocusArcTransitionElapsed += DeltaSeconds;
	const float SafeDuration = FMath::Max(0.10f, FocusArcTransitionDuration);
	const float Alpha = FMath::Clamp(FocusArcTransitionElapsed / SafeDuration, 0.0f, 1.0f);
	const float SmoothAlpha = Alpha * Alpha * (3.0f - (2.0f * Alpha));

	const FVector TargetLocation = ClampPivotLocationInsideSpace(GetFocusLocation() + FocusDragOffset);
	DragTargetLocation = TargetLocation;
	const float TravelDistance = FVector::Dist(FocusArcTransitionStartLocation, TargetLocation);
	const float ArcHeight = TravelDistance > KINDA_SMALL_NUMBER
		? FMath::Max(FMath::Max(0.0f, FocusArcMinHeight), TravelDistance * FMath::Max(0.0f, FocusArcHeightMultiplier))
		: 0.0f;
	const float ArcAlpha = 4.0f * SmoothAlpha * (1.0f - SmoothAlpha);
	OutNewLocation = FMath::Lerp(FocusArcTransitionStartLocation, TargetLocation, SmoothAlpha)
		- (FVector::XAxisVector * ArcHeight * ArcAlpha);

	const float ZoomPhaseAlpha = SmoothAlpha < 0.5f
		? SmoothAlpha * 2.0f
		: (SmoothAlpha - 0.5f) * 2.0f;
	ZoomDistanceTarget = SmoothAlpha < 0.5f
		? FMath::Lerp(FocusArcTransitionStartZoomDistance, FocusArcTransitionPeakZoomDistance, ZoomPhaseAlpha)
		: FMath::Lerp(FocusArcTransitionPeakZoomDistance, FocusArcTransitionFinalZoomDistance, ZoomPhaseAlpha);
	ZoomDistanceTarget = ClampZoomDistance(ZoomDistanceTarget);

	if (Alpha >= 1.0f - UE_SMALL_NUMBER)
	{
		OutNewLocation = TargetLocation;
		ZoomDistanceTarget = FocusArcTransitionFinalZoomDistance;
		StopFocusArcTransition();
	}

	return true;
}

bool ASRCameraPawn::HasExitedFocusedActorGravityField() const
{
	if (!IsValid(FocusedActor))
	{
		return false;
	}

	const USRGravityParent* GravityParent = FocusedActor->FindComponentByClass<USRGravityParent>();
	if (!IsValid(GravityParent))
	{
		return false;
	}

	const float GravityRadius = GravityParent->GetGravityRadius();
	if (GravityRadius <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	return FocusDragOffset.SizeSquared() > FMath::Square(GravityRadius);
}

FVector ASRCameraPawn::GetFocusLocation() const
{
	if (!FocusedActor)
	{
		return DragTargetLocation;
	}

	return FocusedActor->GetActorLocation();
}
