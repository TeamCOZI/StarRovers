#include "Camera/SRCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Gravity/SRGravityParent.h"

namespace
{
	constexpr float DefaultFocusZoomMultiplier = 3.0f;

	float ComputeActorVisiblePrimitiveRadius(const AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return 0.0f;
		}

		float LargestRadius = 0.0f;
		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(Actor);
		Actor->GetComponents(PrimitiveComponents);
		for (const UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (!IsValid(PrimitiveComponent)
				|| !PrimitiveComponent->IsVisible()
				|| PrimitiveComponent->ComponentHasTag(TEXT("StarRovers.FocusCollision")))
			{
				continue;
			}

			LargestRadius = FMath::Max(LargestRadius, PrimitiveComponent->Bounds.SphereRadius);
		}

		return LargestRadius;
	}

	float ResolveActorFocusZoomDistance(
		const AActor* Actor,
		float CameraFieldOfViewDegrees,
		float FocusZoomMultiplier,
		float SmallActorFocusZoomDistance)
	{
		if (!IsValid(Actor))
		{
			return 0.0f;
		}

		if (USRCelestialBodyRuntimeLibrary::IsCelestialBodyActor(Actor))
		{
			return USRCelestialBodyRuntimeLibrary::GetCelestialFocusZoomDistance(
				Actor,
				CameraFieldOfViewDegrees,
				FocusZoomMultiplier);
		}

		const float SafeFallbackDistance = FMath::Max(0.0f, SmallActorFocusZoomDistance);
		const float VisiblePrimitiveRadius = ComputeActorVisiblePrimitiveRadius(Actor);
		if (VisiblePrimitiveRadius <= KINDA_SMALL_NUMBER)
		{
			return SafeFallbackDistance;
		}

		const float SafeFieldOfViewDegrees = FMath::Clamp(CameraFieldOfViewDegrees, 5.0f, 170.0f);
		const float HalfFieldOfViewRadians = FMath::DegreesToRadians(SafeFieldOfViewDegrees * 0.5f);
		const float FramedDistance = VisiblePrimitiveRadius / FMath::Tan(HalfFieldOfViewRadians);
		return FMath::Max(SafeFallbackDistance, FramedDistance * FMath::Max(0.0f, FocusZoomMultiplier));
	}
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
		const float DesiredFocusZoom = ResolveActorFocusZoomDistance(
			FocusedActor,
			CurrentCameraFieldOfView,
			DefaultFocusZoomMultiplier,
			SmallActorFocusZoomDistance);
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
	StopFocusArcTransition();
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
	FocusSurface.TargetRotation = FQuat::Identity;
	FocusSurface.RigAlignmentStartRotation = FocusSurface.Rotation.GetNormalized();
	FocusSurface.RigAlignmentStartOffset = FVector::ZeroVector;
	FocusSurface.RigAlignmentAxis = FVector::UpVector;
	FocusSurface.RotationSmoothVelocity = FVector::ZeroVector;
	FocusSurface.RigAlignmentCurrentAngleRadians = 0.0f;
	FocusSurface.RigAlignmentTargetAngleRadians = 0.0f;
	FocusSurface.bIsResettingRotation = true;
	FocusSurface.bIsAligningRig = false;
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

	const float DesiredFocusZoom = ResolveActorFocusZoomDistance(
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

	FocusArcTransition.StartLocation = CurrentLocation;
	FocusArcTransition.Elapsed = 0.0f;
	FocusArcTransition.StartZoomDistance = FMath::Max(0.0f, SpringArm->TargetArmLength);
	FocusArcTransition.FinalZoomDistance = ClampZoomDistance(FinalZoomDistance);
	const float DesiredPeakZoomDistance = FMath::Max(
		FMath::Max(FocusArcTransition.StartZoomDistance, FocusArcTransition.FinalZoomDistance),
		TravelDistance * FMath::Max(0.0f, FocusArcZoomOutDistanceMultiplier));
	FocusArcTransition.PeakZoomDistance = ClampZoomDistance(DesiredPeakZoomDistance);
	FocusArcTransition.bActive = true;
	FocusTrackingDelta = FVector::ZeroVector;
	FocusTrackingDeltaVelocity = FVector::ZeroVector;
}

void ASRCameraPawn::StopFocusArcTransition()
{
	FocusArcTransition.Reset();
}

bool ASRCameraPawn::UpdateFocusArcTransition(float DeltaSeconds, FVector& OutNewLocation)
{
	if (!FocusArcTransition.bActive)
	{
		return false;
	}

	if (DeltaSeconds <= UE_SMALL_NUMBER || !IsValid(FocusedActor))
	{
		StopFocusArcTransition();
		return false;
	}

	FocusArcTransition.Elapsed += DeltaSeconds;
	const float SafeDuration = FMath::Max(0.10f, FocusArcTransitionDuration);
	const float Alpha = FMath::Clamp(FocusArcTransition.Elapsed / SafeDuration, 0.0f, 1.0f);
	const float SmoothAlpha = Alpha * Alpha * (3.0f - (2.0f * Alpha));

	const FVector TargetLocation = ClampPivotLocationInsideSpace(GetFocusLocation() + FocusDragOffset);
	DragTargetLocation = TargetLocation;
	const float TravelDistance = FVector::Dist(FocusArcTransition.StartLocation, TargetLocation);
	const float ArcHeight = TravelDistance > KINDA_SMALL_NUMBER
		? FMath::Max(FMath::Max(0.0f, FocusArcMinHeight), TravelDistance * FMath::Max(0.0f, FocusArcHeightMultiplier))
		: 0.0f;
	const float ArcAlpha = 4.0f * SmoothAlpha * (1.0f - SmoothAlpha);
	OutNewLocation = FMath::Lerp(FocusArcTransition.StartLocation, TargetLocation, SmoothAlpha)
		- (FVector::XAxisVector * ArcHeight * ArcAlpha);

	const float ZoomPhaseAlpha = SmoothAlpha < 0.5f
		? SmoothAlpha * 2.0f
		: (SmoothAlpha - 0.5f) * 2.0f;
	ZoomDistanceTarget = SmoothAlpha < 0.5f
		? FMath::Lerp(FocusArcTransition.StartZoomDistance, FocusArcTransition.PeakZoomDistance, ZoomPhaseAlpha)
		: FMath::Lerp(FocusArcTransition.PeakZoomDistance, FocusArcTransition.FinalZoomDistance, ZoomPhaseAlpha);
	ZoomDistanceTarget = ClampZoomDistance(ZoomDistanceTarget);

	if (Alpha >= 1.0f - UE_SMALL_NUMBER)
	{
		OutNewLocation = TargetLocation;
		ZoomDistanceTarget = FocusArcTransition.FinalZoomDistance;
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
