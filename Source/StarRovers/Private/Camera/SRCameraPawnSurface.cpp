#include "Camera/SRCameraPawn.h"

#include "SRCameraFocusSurfaceGridAlignmentResolver.h"
#include "SRCameraFocusSurfaceMotionController.h"
#include "SRCameraFocusSurfaceRotationApplier.h"
#include "SRCameraFocusSurfaceRotationResetController.h"
#include "SRCameraFocusSurfaceRigAlignmentController.h"
#include "SRCameraMotionSmoothing.h"
#include "Camera/CameraComponent.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "GameFramework/SpringArmComponent.h"

namespace
{
	bool IntersectRayWithSphere(
		const FVector& RayOrigin,
		const FVector& RayDirection,
		const FVector& SphereCenter,
		float SphereRadius,
		FVector& OutHitLocation)
	{
		const FVector SafeRayDirection = RayDirection.GetSafeNormal();
		const float SafeSphereRadius = FMath::Max(0.0f, SphereRadius);
		if (SafeRayDirection.IsNearlyZero() || SafeSphereRadius <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		const FVector RayOriginToCenter = RayOrigin - SphereCenter;
		const float B = FVector::DotProduct(RayOriginToCenter, SafeRayDirection);
		const float C = RayOriginToCenter.SizeSquared() - FMath::Square(SafeSphereRadius);
		const float Discriminant = FMath::Square(B) - C;
		if (Discriminant < 0.0f)
		{
			return false;
		}

		const float SqrtDiscriminant = FMath::Sqrt(Discriminant);
		float HitDistance = -B - SqrtDiscriminant;
		if (HitDistance < 0.0f)
		{
			HitDistance = -B + SqrtDiscriminant;
		}
		if (HitDistance < 0.0f)
		{
			return false;
		}

		OutHitLocation = RayOrigin + (SafeRayDirection * HitDistance);
		return true;
	}

	FQuat GetShortestIdentityOffset(const FQuat& RotationOffset)
	{
		FQuat ShortestOffset = RotationOffset.GetNormalized();
		if (ShortestOffset.W < 0.0f)
		{
			ShortestOffset.X *= -1.0f;
			ShortestOffset.Y *= -1.0f;
			ShortestOffset.Z *= -1.0f;
			ShortestOffset.W *= -1.0f;
		}
		return ShortestOffset;
	}

	FVector ComputeSurfaceCameraLocation(const FVector& PivotLocation, const FQuat& SurfaceRotation, float ZoomDistance)
	{
		const FVector ArmForward = SurfaceRotation.GetNormalized().RotateVector(FVector::ForwardVector).GetSafeNormal();
		if (ArmForward.IsNearlyZero())
		{
			return PivotLocation;
		}

		return PivotLocation - (ArmForward * FMath::Max(1.0f, ZoomDistance));
	}
}

bool ASRCameraPawn::ShouldAllowFocusSurface() const
{
	if (!IsValid(FocusedActor) || !USRCelestialBodyRuntimeLibrary::IsCelestialBodyActor(FocusedActor))
	{
		return false;
	}

	return true;
}

bool ASRCameraPawn::TryComputeFocusSurfaceGridAlignmentDelta(
	const FVector& PivotLocation,
	const FQuat& SurfaceRotation,
	float ZoomDistance,
	FVector& OutAxis,
	float& OutAngleRadians) const
{
	const float SafeZoomDistance = FMath::Max(1.0f, ZoomDistance);
	const FQuat BaseViewQuat = GetViewRotationForZoom(SafeZoomDistance).Quaternion();
	const FQuat ViewQuat = (SurfaceRotation.GetNormalized() * BaseViewQuat).GetNormalized();
	const FVector RayOrigin = ComputeSurfaceCameraLocation(PivotLocation, SurfaceRotation, SafeZoomDistance);
	const FVector RayDirection = ViewQuat.RotateVector(FVector::ForwardVector).GetSafeNormal();

	return FSRCameraFocusSurfaceGridAlignmentResolver::Resolve(
		FocusedActor.Get(),
		RayOrigin,
		RayDirection,
		GetFocusLocation(),
		ViewQuat,
		OutAxis,
		OutAngleRadians);
}

bool ASRCameraPawn::ResolveFocusSurfaceCenterTargetRotation(
	const FQuat& ReferenceRotation,
	const FVector& PivotLocation,
	float ZoomDistance,
	FQuat& OutTargetRotation) const
{
	const FQuat NormalizedReferenceRotation = ReferenceRotation.GetNormalized();
	OutTargetRotation = NormalizedReferenceRotation;
	if (!bHasFocusSurfaceCenterTarget || !ShouldAllowFocusSurface())
	{
		return false;
	}

	const FVector TargetDirection = FocusedActor->GetActorTransform()
		.TransformVectorNoScale(FocusSurfaceCenterTargetActorLocalDirection)
		.GetSafeNormal();
	if (TargetDirection.IsNearlyZero())
	{
		return false;
	}

	const FVector BodyCenter = GetFocusLocation();
	const float TargetRadius = FMath::Max(1.0f, FocusSurfaceCenterTargetRadius);
	const float CurrentZoomDistance = FMath::Max(1.0f, ZoomDistance);
	const FQuat BaseViewQuat = GetViewRotationForZoom(CurrentZoomDistance).Quaternion();
	const FQuat ViewQuat = (NormalizedReferenceRotation * BaseViewQuat).GetNormalized();
	const FVector CameraLocation = ComputeSurfaceCameraLocation(PivotLocation, NormalizedReferenceRotation, CurrentZoomDistance);
	const FVector CameraForward = ViewQuat.RotateVector(FVector::ForwardVector).GetSafeNormal();

	FVector CurrentCenterHitLocation = FVector::ZeroVector;
	FVector CurrentCenterDirection = FVector::ZeroVector;
	if (IntersectRayWithSphere(
		CameraLocation,
		CameraForward,
		BodyCenter,
		TargetRadius,
		CurrentCenterHitLocation))
	{
		CurrentCenterDirection = (CurrentCenterHitLocation - BodyCenter).GetSafeNormal();
	}
	if (CurrentCenterDirection.IsNearlyZero())
	{
		CurrentCenterDirection = (CameraLocation - BodyCenter).GetSafeNormal();
	}
	if (CurrentCenterDirection.IsNearlyZero())
	{
		return false;
	}

	const FQuat AlignmentDelta = FQuat::FindBetweenNormals(CurrentCenterDirection, TargetDirection);
	OutTargetRotation = (AlignmentDelta * NormalizedReferenceRotation).GetNormalized();
	return true;
}

bool ASRCameraPawn::UpdateFocusSurfaceCenterTargetRotation(
	float DeltaSeconds,
	const FVector& PivotLocation,
	float ZoomDistance,
	bool& bOutActive)
{
	bOutActive = false;
	FQuat TargetRotation = FQuat::Identity;
	if (!ResolveFocusSurfaceCenterTargetRotation(FocusSurface.TargetRotation, PivotLocation, ZoomDistance, TargetRotation))
	{
		return false;
	}

	FocusSurface.TargetRotation = TargetRotation;
	FocusSurfaceCenterTargetRotationOffset = StarRovers::Camera::SmoothDampCameraQuat(
		FocusSurfaceCenterTargetRotationOffset,
		FQuat::Identity,
		FocusSurface.RotationSmoothVelocity,
		FocusFollowSmoothTime,
		DeltaSeconds);
	FocusSurfaceCenterTargetRotationOffset = GetShortestIdentityOffset(FocusSurfaceCenterTargetRotationOffset);
	FocusSurface.Rotation = (FocusSurfaceCenterTargetRotationOffset.Inverse() * TargetRotation).GetNormalized();

	const float RemainingAngleDegrees = FMath::RadiansToDegrees(FocusSurfaceCenterTargetRotationOffset.GetAngle());
	if (RemainingAngleDegrees <= 0.05f && FocusSurface.RotationSmoothVelocity.SizeSquared() <= KINDA_SMALL_NUMBER)
	{
		FocusSurfaceCenterTargetRotationOffset = FQuat::Identity;
		FocusSurface.Rotation = TargetRotation;
		FocusSurface.TargetRotation = TargetRotation;
		FocusSurface.RotationSmoothVelocity = FVector::ZeroVector;
		FocusSurface.bIsResettingRotation = false;
		FocusSurface.bIsAligningRig = false;
		return true;
	}

	FocusSurface.bIsResettingRotation = true;
	FocusSurface.bIsAligningRig = false;
	bOutActive = true;
	return true;
}

void ASRCameraPawn::ClearFocusSurfaceCenterTarget()
{
	bHasFocusSurfaceCenterTarget = false;
	FocusSurfaceCenterTargetActorLocalDirection = FVector::ZeroVector;
	FocusSurfaceCenterTargetRotationOffset = FQuat::Identity;
	FocusSurfaceCenterTargetRadius = 0.0f;
}

void ASRCameraPawn::UpdateFocusSurface(float DeltaSeconds)
{
	if (DeltaSeconds <= UE_SMALL_NUMBER || !ShouldAllowFocusSurface())
	{
		ClearFocusSurfaceMotion();
		return;
	}

	const float CurrentZoomDistance = FMath::Max(1.0f, SpringArm ? SpringArm->TargetArmLength : ZoomDistanceTarget);
	const FQuat BaseViewQuat = GetViewRotationForZoom(CurrentZoomDistance).Quaternion();
	FSRCameraFocusSurfaceMotionSettings MotionSettings;
	MotionSettings.LookSpeed = FocusSurfaceSpeed;
	MotionSettings.InputAcceleration = FocusSurfaceInputAcceleration;
	MotionSettings.InputDeceleration = FocusSurfaceInputDeceleration;
	MotionSettings.InertiaDamping = FocusSurfaceInertiaDamping;
	MotionSettings.MinInertiaSpeed = FocusSurfaceMinInertiaSpeed;
	MotionSettings.bIsDraggingFocusSurface = bIsDraggingFocusSurface;
	const bool bHasRemainingFocusSurfaceMotion = FSRCameraFocusSurfaceMotionController::Update(
		FocusSurface,
		BaseViewQuat,
		MotionSettings,
		DeltaSeconds);
	bIsFocusSurfaceActive = bHasRemainingFocusSurfaceMotion;
	if (!bHasRemainingFocusSurfaceMotion && FocusSurface.bPendingGridAutoAlignment)
	{
		bIsFocusSurfaceActive = false;
	}
}

void ASRCameraPawn::UpdateFocusSurfaceRotation(float DeltaSeconds, const FVector& PivotLocation, float ZoomDistance)
{
	if (DeltaSeconds <= UE_SMALL_NUMBER)
	{
		return;
	}

	if (!ShouldAllowFocusSurface())
	{
		FocusSurface.ResetRotation();
		FocusSurface.bPendingGridAutoAlignment = false;
		ClearFocusSurfaceCenterTarget();
		return;
	}

	if (bHasFocusSurfaceCenterTarget)
	{
		bool bCenterTargetActive = false;
		if (!UpdateFocusSurfaceCenterTargetRotation(DeltaSeconds, PivotLocation, ZoomDistance, bCenterTargetActive))
		{
			ClearFocusSurfaceCenterTarget();
			FocusSurface.bIsResettingRotation = false;
			bIsFocusSurfaceActive = false;
			return;
		}

		bIsFocusSurfaceActive = bCenterTargetActive;
		return;
	}

	if (!FocusSurface.bIsResettingRotation)
	{
		return;
	}

	const FSRCameraFocusSurfaceRotationResetUpdate RotationUpdate =
		FSRCameraFocusSurfaceRotationResetController::Update(FocusSurface, FocusFollowSmoothTime, DeltaSeconds);
	if (RotationUpdate.bHasFocusDragOffset)
	{
		FocusDragOffset = RotationUpdate.FocusDragOffset;
		DragTargetLocation = GetFocusLocation() + FocusDragOffset;
		FocusTrackingDelta = FVector::ZeroVector;
		FocusTrackingDeltaVelocity = FVector::ZeroVector;
	}
	if (!FocusSurface.bIsResettingRotation)
	{
		ClearFocusSurfaceCenterTarget();
	}
	bIsFocusSurfaceActive = RotationUpdate.bIsActive;
}

bool ASRCameraPawn::RotateFocusSurfaceViewBySteps(int32 StepDelta)
{
	if (StepDelta == 0 || !ShouldAllowFocusSurface())
	{
		return false;
	}

	const float CurrentZoomDistance = FMath::Max(1.0f, SpringArm ? SpringArm->TargetArmLength : ZoomDistanceTarget);
	const FQuat BaseViewQuat = GetViewRotationForZoom(CurrentZoomDistance).Quaternion();
	const FQuat ViewQuat = (FocusSurface.Rotation.GetNormalized() * BaseViewQuat).GetNormalized();

	FVector RotationAxis = FVector::ZeroVector;
	float UnusedAlignmentAngleRadians = 0.0f;
	if (!TryComputeFocusSurfaceGridAlignmentDelta(GetActorLocation(), FocusSurface.Rotation, CurrentZoomDistance, RotationAxis, UnusedAlignmentAngleRadians))
	{
		RotationAxis = (GetActorLocation() - GetFocusLocation()).GetSafeNormal();
		if (RotationAxis.IsNearlyZero())
		{
			RotationAxis = (-ViewQuat.RotateVector(FVector::ForwardVector)).GetSafeNormal();
		}
	}

	RotationAxis = RotationAxis.GetSafeNormal();
	if (RotationAxis.IsNearlyZero())
	{
		return false;
	}

	FocusArcTransition.Reset();
	ClearFocusSurfaceCenterTarget();
	ClearFocusSurfaceMotion();

	const float RotationAngleRadians = FMath::DegreesToRadians(90.0f * static_cast<float>(StepDelta));
	if (!FSRCameraFocusSurfaceRigAlignmentController::StartRigAlignment(
		FocusSurface,
		GetActorLocation() - GetFocusLocation(),
		RotationAxis,
		RotationAngleRadians))
	{
		return false;
	}

	bIsFocusSurfaceActive = true;
	return true;
}

bool ASRCameraPawn::CenterFocusedSurfaceLocation(const FVector& WorldLocation, bool bSnapImmediately)
{
	if (!ShouldAllowFocusSurface())
	{
		return false;
	}

	const FVector BodyCenter = GetFocusLocation();
	const FVector WorldDirection = WorldLocation - BodyCenter;
	const float TargetRadius = WorldDirection.Size();
	const FVector ActorLocalDirection = FocusedActor->GetActorTransform()
		.InverseTransformVectorNoScale(WorldDirection)
		.GetSafeNormal();
	return CenterFocusedSurfaceActorLocalDirection(ActorLocalDirection, TargetRadius, bSnapImmediately);
}

bool ASRCameraPawn::CenterFocusedSurfaceActorLocalDirection(
	const FVector& ActorLocalDirection,
	float SurfaceRadius,
	bool bSnapImmediately)
{
	if (!ShouldAllowFocusSurface() || !Camera)
	{
		return false;
	}

	const FVector TargetActorLocalDirection = ActorLocalDirection.GetSafeNormal();
	const float TargetRadius = FMath::Max(1.0f, SurfaceRadius);
	if (TargetActorLocalDirection.IsNearlyZero() || TargetRadius <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	FocusArcTransition.Reset();
	ClearFocusSurfaceMotion();
	FocusSurface.ResetRigAlignment();
	FocusDragOffset = FVector::ZeroVector;
	DragTargetLocation = ClampPivotLocationInsideSpace(GetFocusLocation());
	bHasFocusSurfaceCenterTarget = true;
	FocusSurfaceCenterTargetActorLocalDirection = TargetActorLocalDirection;
	FocusSurfaceCenterTargetRotationOffset = FQuat::Identity;
	FocusSurfaceCenterTargetRadius = TargetRadius;

	const float CurrentZoomDistance = FMath::Max(1.0f, SpringArm ? SpringArm->TargetArmLength : ZoomDistanceTarget);
	ZoomDistanceTarget = ClampZoomDistance(ZoomDistanceTarget);

	FQuat TargetRotation = FQuat::Identity;
	if (!ResolveFocusSurfaceCenterTargetRotation(FocusSurface.Rotation, GetActorLocation(), CurrentZoomDistance, TargetRotation))
	{
		ClearFocusSurfaceCenterTarget();
		return false;
	}

	FocusSurface.TargetRotation = TargetRotation;
	FocusSurfaceCenterTargetRotationOffset = GetShortestIdentityOffset(
		TargetRotation * FocusSurface.Rotation.GetNormalized().Inverse());
	if (bSnapImmediately)
	{
		FocusTrackingDelta = FVector::ZeroVector;
		FocusTrackingDeltaVelocity = FVector::ZeroVector;
		FocusSurface.Rotation = TargetRotation;
		FocusSurface.ResetRigAlignment();
		ApplyCameraFrame(DragTargetLocation, CurrentZoomDistance);
		ClearFocusSurfaceCenterTarget();
		return true;
	}

	FocusTrackingDelta = DragTargetLocation - GetActorLocation();
	FocusTrackingDeltaVelocity = FVector::ZeroVector;
	FocusSurface.RotationSmoothVelocity = FVector::ZeroVector;
	FocusSurface.bIsResettingRotation = true;
	FocusSurface.bIsAligningRig = false;
	bIsFocusSurfaceActive = true;
	return true;
}

bool ASRCameraPawn::ShouldDragFocusedSurface() const
{
	return ShouldAllowFocusSurface();
}

void ASRCameraPawn::HandleFocusSurfaceDrag(const FVector2D& DragDelta)
{
	if (!ShouldAllowFocusSurface() || DragDelta.IsNearlyZero())
	{
		return;
	}

	const float CurrentZoomDistance = FMath::Max(1.0f, SpringArm ? SpringArm->TargetArmLength : ZoomDistanceTarget);
	const float UnclampedAdaptiveScale = GetScreenSpaceInputScale(CurrentZoomDistance)
		* FMath::Max(0.0f, RightDragInputScaleMultiplier);
	const float SafeRightDragInputScaleMax = FMath::Max(0.0f, RightDragInputScaleMax);
	const float AdaptiveScale = SafeRightDragInputScaleMax > KINDA_SMALL_NUMBER
		? FMath::Min(UnclampedAdaptiveScale, SafeRightDragInputScaleMax)
		: UnclampedAdaptiveScale;
	const FVector2D DegreesDelta(
		DragDelta.X * FMath::Max(0.0f, SurfaceRotateSensitivity) * AdaptiveScale,
		DragDelta.Y * FMath::Max(0.0f, SurfaceRotateSensitivity) * AdaptiveScale);
	if (DegreesDelta.IsNearlyZero())
	{
		return;
	}

	ClearFocusSurfaceCenterTarget();
	const FQuat BaseViewQuat = GetViewRotationForZoom(CurrentZoomDistance).Quaternion();
	FSRCameraFocusSurfaceRotationApplier::ApplyDelta(FocusSurface, BaseViewQuat, DegreesDelta);
	FocusSurface.bPendingGridAutoAlignment = true;

	const UWorld* World = GetWorld();
	const float DeltaSeconds = World ? World->GetDeltaSeconds() : 0.0f;
	if (DeltaSeconds > UE_SMALL_NUMBER)
	{
		FocusSurface.AngularVelocity = DegreesDelta / DeltaSeconds;
	}
	bIsFocusSurfaceActive = true;
}

void ASRCameraPawn::ClearFocusSurfaceMotion()
{
	FocusSurface.ResetMotion();
	bIsFocusSurfaceActive = false;
	bIsDraggingFocusSurface = false;
}
