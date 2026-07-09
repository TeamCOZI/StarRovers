#include "Camera/SRCameraPawn.h"

#include "SRCameraFocusArcTransitionController.h"
#include "SRCameraFocusSurfaceRigAlignmentController.h"
#include "SRCameraFocusZoomResolver.h"
#include "Camera/CameraComponent.h"
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
	FocusSurface.ResetRotation();
	FocusArcTransition.Reset();
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
		const float DesiredFocusZoom = FSRCameraFocusZoomResolver::ResolveActorFocusZoomDistance(
			FocusedActor,
			CurrentCameraFieldOfView,
			DefaultFocusZoomMultiplier,
			SmallActorFocusZoomDistance);
		ZoomDistanceTarget = ClampZoomDistance(DesiredFocusZoom);
		if (bUseArcTransition)
		{
			if (SpringArm)
			{
				const FVector TargetLocation = ClampPivotLocationInsideSpace(GetFocusLocation() + FocusDragOffset);
				const float StartZoomDistance = FMath::Max(0.0f, SpringArm->TargetArmLength);
				const float FinalZoomDistance = ClampZoomDistance(ZoomDistanceTarget);
				const float TravelDistance = FVector::Dist(CurrentLocation, TargetLocation);
				const float DesiredPeakZoomDistance = FMath::Max(
					FMath::Max(StartZoomDistance, FinalZoomDistance),
					TravelDistance * FMath::Max(0.0f, FocusArcZoomOutDistanceMultiplier));
				const float PeakZoomDistance = ClampZoomDistance(DesiredPeakZoomDistance);
				if (FSRCameraFocusArcTransitionController::Begin(
					FocusArcTransition,
					CurrentLocation,
					TargetLocation,
					StartZoomDistance,
					FinalZoomDistance,
					PeakZoomDistance))
				{
					FocusTrackingDelta = FVector::ZeroVector;
					FocusTrackingDeltaVelocity = FVector::ZeroVector;
				}
			}
			else
			{
				FocusArcTransition.Reset();
			}
		}
	}

	BroadcastFocusedActorChangedIfNeeded(PreviousFocusedActor);
}

void ASRCameraPawn::ClearFocusActor()
{
	AActor* PreviousFocusedActor = FocusedActor.Get();
	const bool bHadFocusedActor = FocusedActor != nullptr;
	const FQuat CurrentCameraWorldRotation = Camera ? Camera->GetComponentQuat().GetNormalized() : FQuat::Identity;
	const float CurrentZoomDistance = SpringArm ? SpringArm->TargetArmLength : ZoomDistanceTarget;
	if (FocusedActor)
	{
		DragTargetLocation = GetActorLocation();
	}

	FocusedActor = nullptr;
	FocusDragOffset = FVector::ZeroVector;
	FocusTrackingDelta = FVector::ZeroVector;
	FocusTrackingDeltaVelocity = FVector::ZeroVector;
	FocusSurface.ResetRotation();
	FocusArcTransition.Reset();
	ClearFocusSurfaceMotion();
	if (bHadFocusedActor && Camera)
	{
		const FQuat FreeViewRotation = GetViewRotationForZoom(CurrentZoomDistance).Quaternion().GetNormalized();
		const FQuat DesiredPawnRotation = (CurrentCameraWorldRotation * FreeViewRotation.Inverse()).GetNormalized();
		SetActorRotation(DesiredPawnRotation.Rotator().GetNormalized());
		ApplyZoomDrivenViewRotation(CurrentZoomDistance);
		UpdateComponentTransforms();
		if (SpringArm)
		{
			SpringArm->UpdateComponentToWorld();
		}
		Camera->UpdateComponentToWorld();
	}
	BroadcastFocusedActorChangedIfNeeded(PreviousFocusedActor);
}

AActor* ASRCameraPawn::GetFocusedActor() const
{
	return FocusedActor;
}

void ASRCameraPawn::SnapToFocusTarget()
{
	FocusArcTransition.Reset();
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
	FocusArcTransition.Reset();
	FSRCameraFocusSurfaceRigAlignmentController::StartRotationReset(FocusSurface);
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

	const float DesiredFocusZoom = FSRCameraFocusZoomResolver::ResolveActorFocusZoomDistance(
		FocusedActor,
		Camera->FieldOfView,
		DefaultFocusZoomMultiplier,
		SmallActorFocusZoomDistance);
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
