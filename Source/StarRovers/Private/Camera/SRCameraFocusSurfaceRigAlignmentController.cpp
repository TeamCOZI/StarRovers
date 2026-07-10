#include "SRCameraFocusSurfaceRigAlignmentController.h"

#include "Camera/SRCameraPawnRuntimeState.h"

bool FSRCameraFocusSurfaceRigAlignmentController::StartRigAlignment(
	FSRCameraFocusSurfaceRuntimeState& FocusSurface,
	const FVector& RigAlignmentStartOffset,
	const FVector& AlignmentAxis,
	float AlignmentAngleRadians)
{
	const FVector SafeAlignmentAxis = AlignmentAxis.GetSafeNormal();
	if (SafeAlignmentAxis.IsNearlyZero() || FMath::IsNearlyZero(AlignmentAngleRadians))
	{
		return false;
	}

	if (FocusSurface.bIsAligningRig)
	{
		const FVector ExistingAlignmentAxis = FocusSurface.RigAlignmentAxis.GetSafeNormal();
		const float AxisDot = FVector::DotProduct(ExistingAlignmentAxis, SafeAlignmentAxis);
		if (!ExistingAlignmentAxis.IsNearlyZero() && FMath::Abs(AxisDot) >= 0.999f)
		{
			const float SignedAlignmentAngleRadians = AxisDot >= 0.0f
				? AlignmentAngleRadians
				: -AlignmentAngleRadians;
			FocusSurface.RigAlignmentTargetAngleRadians += SignedAlignmentAngleRadians;
			const FQuat TargetDelta(ExistingAlignmentAxis, FocusSurface.RigAlignmentTargetAngleRadians);
			FocusSurface.TargetRotation = (TargetDelta * FocusSurface.RigAlignmentStartRotation.GetNormalized()).GetNormalized();
			FocusSurface.RotationSmoothVelocity = FVector::ZeroVector;
			FocusSurface.bIsResettingRotation = true;
			return true;
		}
	}

	const FQuat CurrentSurfaceRotation = FocusSurface.Rotation.GetNormalized();
	const FQuat AlignmentDelta(SafeAlignmentAxis, AlignmentAngleRadians);
	FocusSurface.TargetRotation = (AlignmentDelta * CurrentSurfaceRotation).GetNormalized();
	FocusSurface.RigAlignmentStartRotation = CurrentSurfaceRotation;
	FocusSurface.RigAlignmentStartOffset = RigAlignmentStartOffset;
	FocusSurface.RigAlignmentAxis = SafeAlignmentAxis;
	FocusSurface.RigAlignmentCurrentAngleRadians = 0.0f;
	FocusSurface.RigAlignmentTargetAngleRadians = AlignmentAngleRadians;
	FocusSurface.RotationSmoothVelocity = FVector::ZeroVector;
	FocusSurface.bIsResettingRotation = true;
	FocusSurface.bIsAligningRig = true;
	return true;
}

void FSRCameraFocusSurfaceRigAlignmentController::StartRotationReset(FSRCameraFocusSurfaceRuntimeState& FocusSurface)
{
	FocusSurface.TargetRotation = FQuat::Identity;
	FocusSurface.RigAlignmentStartRotation = FocusSurface.Rotation.GetNormalized();
	FocusSurface.RigAlignmentStartOffset = FVector::ZeroVector;
	FocusSurface.RigAlignmentAxis = FVector::UpVector;
	FocusSurface.RotationSmoothVelocity = FVector::ZeroVector;
	FocusSurface.RigAlignmentCurrentAngleRadians = 0.0f;
	FocusSurface.RigAlignmentTargetAngleRadians = 0.0f;
	FocusSurface.bIsResettingRotation = true;
	FocusSurface.bIsAligningRig = false;
}

void FSRCameraFocusSurfaceRigAlignmentController::StopRotationResetForDrag(FSRCameraFocusSurfaceRuntimeState& FocusSurface)
{
	FocusSurface.TargetRotation = FocusSurface.Rotation.GetNormalized();
	FocusSurface.RotationSmoothVelocity = FVector::ZeroVector;
	FocusSurface.bIsResettingRotation = false;
	FocusSurface.bIsAligningRig = false;
	FocusSurface.RigAlignmentCurrentAngleRadians = 0.0f;
	FocusSurface.RigAlignmentTargetAngleRadians = 0.0f;
	FocusSurface.AngularVelocity = FVector2D::ZeroVector;
}
