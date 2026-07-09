#include "Camera/SRCameraPawn.h"

#include "SRCameraFocusSurfaceGridAlignmentResolver.h"
#include "SRCameraFocusSurfaceMotionController.h"
#include "SRCameraFocusSurfaceRotationApplier.h"
#include "SRCameraFocusSurfaceRotationResetController.h"
#include "SRCameraFocusSurfaceRigAlignmentController.h"
#include "Camera/CameraComponent.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "GameFramework/SpringArmComponent.h"

bool ASRCameraPawn::ShouldAllowFocusSurface() const
{
	if (!IsValid(FocusedActor) || !USRCelestialBodyRuntimeLibrary::IsCelestialBodyActor(FocusedActor))
	{
		return false;
	}

	return true;
}

bool ASRCameraPawn::TryComputeFocusSurfaceGridAlignmentDelta(
	const FQuat& ViewQuat,
	float ZoomDistance,
	FVector& OutAxis,
	float& OutAngleRadians) const
{
	if (!SpringArm)
	{
		OutAxis = FVector::ZeroVector;
		OutAngleRadians = 0.0f;
		return false;
	}

	return FSRCameraFocusSurfaceGridAlignmentResolver::Resolve(
		FocusedActor.Get(),
		Camera,
		GetActorLocation(),
		GetFocusLocation(),
		ViewQuat,
		ZoomDistance,
		OutAxis,
		OutAngleRadians);
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

void ASRCameraPawn::UpdateFocusSurfaceRotation(float DeltaSeconds)
{
	if (DeltaSeconds <= UE_SMALL_NUMBER)
	{
		return;
	}

	if (!ShouldAllowFocusSurface())
	{
		FocusSurface.ResetRotation();
		FocusSurface.bPendingGridAutoAlignment = false;
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

	ApplyZoomDrivenViewRotation(CurrentZoomDistance);
	UpdateComponentTransforms();
	if (SpringArm)
	{
		SpringArm->UpdateComponentToWorld();
	}
	if (Camera)
	{
		Camera->UpdateComponentToWorld();
	}

	FVector RotationAxis = FVector::ZeroVector;
	float UnusedAlignmentAngleRadians = 0.0f;
	if (!TryComputeFocusSurfaceGridAlignmentDelta(ViewQuat, CurrentZoomDistance, RotationAxis, UnusedAlignmentAngleRadians))
	{
		RotationAxis = (GetActorLocation() - GetFocusLocation()).GetSafeNormal();
		if (RotationAxis.IsNearlyZero() && Camera)
		{
			RotationAxis = (-Camera->GetForwardVector()).GetSafeNormal();
		}
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
