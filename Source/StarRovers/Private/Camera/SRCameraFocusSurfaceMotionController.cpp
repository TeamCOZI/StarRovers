#include "SRCameraFocusSurfaceMotionController.h"

#include "SRCameraFocusSurfaceRotationApplier.h"
#include "Camera/SRCameraPawnRuntimeState.h"

bool FSRCameraFocusSurfaceMotionController::Update(
	FSRCameraFocusSurfaceRuntimeState& FocusSurface,
	const FQuat& CameraRelativeQuat,
	const FSRCameraFocusSurfaceMotionSettings& Settings,
	float DeltaSeconds)
{
	const FVector2D CombinedLookInput = FocusSurface.Input.GetClampedToMaxSize(1.0f);
	const bool bHasDirectInput = !CombinedLookInput.IsNearlyZero();
	const float SafeLookSpeed = FMath::Max(0.0f, Settings.LookSpeed);
	const float SafeMinInertiaSpeed = FMath::Max(0.0f, Settings.MinInertiaSpeed);
	bool bAppliedDirectInput = false;

	const float InputInterpRate = bHasDirectInput
		? FMath::Max(0.0f, Settings.InputAcceleration)
		: FMath::Max(0.0f, Settings.InputDeceleration);
	if (InputInterpRate <= KINDA_SMALL_NUMBER)
	{
		FocusSurface.AcceleratedInput = CombinedLookInput;
	}
	else
	{
		FocusSurface.AcceleratedInput.X = FMath::FInterpConstantTo(FocusSurface.AcceleratedInput.X, CombinedLookInput.X, DeltaSeconds, InputInterpRate);
		FocusSurface.AcceleratedInput.Y = FMath::FInterpConstantTo(FocusSurface.AcceleratedInput.Y, CombinedLookInput.Y, DeltaSeconds, InputInterpRate);
		FocusSurface.AcceleratedInput = FocusSurface.AcceleratedInput.GetClampedToMaxSize(1.0f);
	}

	if (!bHasDirectInput && FocusSurface.AcceleratedInput.IsNearlyZero())
	{
		FocusSurface.AcceleratedInput = FVector2D::ZeroVector;
	}

	if (!FocusSurface.AcceleratedInput.IsNearlyZero() && SafeLookSpeed > KINDA_SMALL_NUMBER)
	{
		const FVector2D DegreesDelta = FVector2D(-FocusSurface.AcceleratedInput.X, FocusSurface.AcceleratedInput.Y) * SafeLookSpeed * DeltaSeconds;
		FSRCameraFocusSurfaceRotationApplier::ApplyDelta(FocusSurface, CameraRelativeQuat, DegreesDelta);
		FocusSurface.bPendingGridAutoAlignment = true;
		if (bHasDirectInput)
		{
			FocusSurface.AngularVelocity = FVector2D::ZeroVector;
		}
		bAppliedDirectInput = true;
	}

	if (!bHasDirectInput && !Settings.bIsDraggingFocusSurface && !FocusSurface.AngularVelocity.IsNearlyZero(SafeMinInertiaSpeed))
	{
		FSRCameraFocusSurfaceRotationApplier::ApplyDelta(FocusSurface, CameraRelativeQuat, FocusSurface.AngularVelocity * DeltaSeconds);
		FocusSurface.bPendingGridAutoAlignment = true;

		const float SafeDamping = FMath::Max(0.0f, Settings.InertiaDamping);
		if (SafeDamping <= KINDA_SMALL_NUMBER)
		{
			FocusSurface.AngularVelocity = FVector2D::ZeroVector;
		}
		else
		{
			FocusSurface.AngularVelocity.X = FMath::FInterpTo(FocusSurface.AngularVelocity.X, 0.0f, DeltaSeconds, SafeDamping);
			FocusSurface.AngularVelocity.Y = FMath::FInterpTo(FocusSurface.AngularVelocity.Y, 0.0f, DeltaSeconds, SafeDamping);
		}
	}

	if (FocusSurface.AngularVelocity.IsNearlyZero(SafeMinInertiaSpeed))
	{
		FocusSurface.AngularVelocity = FVector2D::ZeroVector;
	}

	return bAppliedDirectInput
		|| Settings.bIsDraggingFocusSurface
		|| !FocusSurface.AngularVelocity.IsNearlyZero();
}
