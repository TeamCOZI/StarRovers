#include "SRCameraFocusSurfaceRotationApplier.h"

#include "Camera/SRCameraPawnRuntimeState.h"

bool FSRCameraFocusSurfaceRotationApplier::ApplyDelta(
	FSRCameraFocusSurfaceRuntimeState& FocusSurface,
	const FQuat& CameraRelativeQuat,
	const FVector2D& DegreesDelta)
{
	if (DegreesDelta.IsNearlyZero())
	{
		return false;
	}

	FocusSurface.RotationSmoothVelocity = FVector::ZeroVector;
	FocusSurface.bIsResettingRotation = false;
	FocusSurface.bIsAligningRig = false;
	FocusSurface.RigAlignmentCurrentAngleRadians = 0.0f;
	FocusSurface.RigAlignmentTargetAngleRadians = 0.0f;

	FQuat CurrentRotation = FocusSurface.Rotation.GetNormalized();
	const FQuat NormalizedCameraRelativeQuat = CameraRelativeQuat.GetNormalized();
	if (!FMath::IsNearlyZero(DegreesDelta.X))
	{
		const FQuat CurrentViewQuat = (CurrentRotation * NormalizedCameraRelativeQuat).GetNormalized();
		const FVector CurrentUpAxis = CurrentViewQuat.RotateVector(FVector::UpVector).GetSafeNormal();
		const FQuat YawDelta(CurrentUpAxis, FMath::DegreesToRadians(DegreesDelta.X));
		CurrentRotation = (YawDelta * CurrentRotation).GetNormalized();
	}

	if (!FMath::IsNearlyZero(DegreesDelta.Y))
	{
		const FQuat CurrentViewQuat = (CurrentRotation * NormalizedCameraRelativeQuat).GetNormalized();
		const FVector CurrentRightAxis = CurrentViewQuat.RotateVector(FVector::RightVector).GetSafeNormal();
		const FQuat PitchDelta(CurrentRightAxis, FMath::DegreesToRadians(DegreesDelta.Y));
		CurrentRotation = (PitchDelta * CurrentRotation).GetNormalized();
	}

	FocusSurface.Rotation = CurrentRotation;
	FocusSurface.TargetRotation = CurrentRotation;
	return true;
}
