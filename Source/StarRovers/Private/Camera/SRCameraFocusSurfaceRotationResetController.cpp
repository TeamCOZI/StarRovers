#include "SRCameraFocusSurfaceRotationResetController.h"

#include "SRCameraMotionSmoothing.h"
#include "Camera/SRCameraPawnRuntimeState.h"

namespace
{
	FVector ComputeRigAlignmentFocusDragOffset(const FSRCameraFocusSurfaceRuntimeState& FocusSurface)
	{
		const FVector AlignmentAxis = FocusSurface.RigAlignmentAxis.GetSafeNormal();
		if (AlignmentAxis.IsNearlyZero())
		{
			return FVector::ZeroVector;
		}

		const FQuat CurrentDeltaRotation(AlignmentAxis, FocusSurface.RigAlignmentCurrentAngleRadians);
		return CurrentDeltaRotation.RotateVector(FocusSurface.RigAlignmentStartOffset);
	}
}

FSRCameraFocusSurfaceRotationResetUpdate FSRCameraFocusSurfaceRotationResetController::Update(
	FSRCameraFocusSurfaceRuntimeState& FocusSurface,
	float SmoothTime,
	float DeltaSeconds)
{
	FSRCameraFocusSurfaceRotationResetUpdate UpdateResult;
	if (!FocusSurface.bIsResettingRotation)
	{
		return UpdateResult;
	}

	if (FocusSurface.bIsAligningRig)
	{
		const FVector AlignmentAxis = FocusSurface.RigAlignmentAxis.GetSafeNormal();
		if (AlignmentAxis.IsNearlyZero())
		{
			FocusSurface.RotationSmoothVelocity = FVector::ZeroVector;
			FocusSurface.RigAlignmentCurrentAngleRadians = 0.0f;
			FocusSurface.RigAlignmentTargetAngleRadians = 0.0f;
			FocusSurface.bIsResettingRotation = false;
			FocusSurface.bIsAligningRig = false;
			return UpdateResult;
		}

		const float CurrentAngleDegrees = FMath::RadiansToDegrees(FocusSurface.RigAlignmentCurrentAngleRadians);
		const float TargetAngleDegrees = FMath::RadiansToDegrees(FocusSurface.RigAlignmentTargetAngleRadians);
		const FVector NewAngleDegrees = StarRovers::Camera::SmoothDampCameraVector(
			FVector(CurrentAngleDegrees, 0.0f, 0.0f),
			FVector(TargetAngleDegrees, 0.0f, 0.0f),
			FocusSurface.RotationSmoothVelocity,
			SmoothTime,
			DeltaSeconds);
		FocusSurface.RigAlignmentCurrentAngleRadians = FMath::DegreesToRadians(NewAngleDegrees.X);

		const FQuat CurrentDeltaRotation(
			AlignmentAxis,
			FocusSurface.RigAlignmentCurrentAngleRadians);
		const FQuat TargetDeltaRotation(
			AlignmentAxis,
			FocusSurface.RigAlignmentTargetAngleRadians);
		FocusSurface.Rotation = (CurrentDeltaRotation * FocusSurface.RigAlignmentStartRotation.GetNormalized()).GetNormalized();
		FocusSurface.TargetRotation = (TargetDeltaRotation * FocusSurface.RigAlignmentStartRotation.GetNormalized()).GetNormalized();

		const float RemainingAngleDegrees = FMath::Abs(TargetAngleDegrees - NewAngleDegrees.X);
		if (RemainingAngleDegrees <= 0.05f && FocusSurface.RotationSmoothVelocity.SizeSquared() <= KINDA_SMALL_NUMBER)
		{
			FocusSurface.RigAlignmentCurrentAngleRadians = FocusSurface.RigAlignmentTargetAngleRadians;
			FocusSurface.Rotation = FocusSurface.TargetRotation.GetNormalized();
			UpdateResult.FocusDragOffset = ComputeRigAlignmentFocusDragOffset(FocusSurface);
			UpdateResult.bHasFocusDragOffset = true;

			FocusSurface.RotationSmoothVelocity = FVector::ZeroVector;
			FocusSurface.RigAlignmentCurrentAngleRadians = 0.0f;
			FocusSurface.RigAlignmentTargetAngleRadians = 0.0f;
			FocusSurface.bIsResettingRotation = false;
			FocusSurface.bIsAligningRig = false;
			return UpdateResult;
		}

		UpdateResult.FocusDragOffset = ComputeRigAlignmentFocusDragOffset(FocusSurface);
		UpdateResult.bHasFocusDragOffset = true;
		UpdateResult.bIsActive = true;
		return UpdateResult;
	}

	FocusSurface.Rotation = SmoothDampQuat(
		FocusSurface.Rotation,
		FocusSurface.TargetRotation,
		FocusSurface.RotationSmoothVelocity,
		SmoothTime,
		DeltaSeconds);

	const FQuat RemainingRotation = (FocusSurface.TargetRotation.GetNormalized() * FocusSurface.Rotation.GetNormalized().Inverse()).GetNormalized();
	const float RemainingAngleDegrees = FMath::RadiansToDegrees(RemainingRotation.GetAngle());
	if (RemainingAngleDegrees <= 0.05f && FocusSurface.RotationSmoothVelocity.SizeSquared() <= KINDA_SMALL_NUMBER)
	{
		FocusSurface.Rotation = FocusSurface.TargetRotation.GetNormalized();
		FocusSurface.RotationSmoothVelocity = FVector::ZeroVector;
		FocusSurface.bIsResettingRotation = false;
		FocusSurface.bIsAligningRig = false;
		return UpdateResult;
	}

	UpdateResult.bIsActive = true;
	return UpdateResult;
}

FQuat FSRCameraFocusSurfaceRotationResetController::SmoothDampQuat(
	const FQuat& Current,
	const FQuat& Target,
	FVector& CurrentAngularVelocity,
	float SmoothTime,
	float DeltaTime)
{
	if (DeltaTime <= UE_SMALL_NUMBER)
	{
		return Current.GetNormalized();
	}

	const FQuat NormalizedTarget = Target.GetNormalized();
	const FQuat NormalizedCurrent = Current.GetNormalized();
	FQuat RemainingRotation = (NormalizedTarget * NormalizedCurrent.Inverse()).GetNormalized();
	if (RemainingRotation.W < 0.0f)
	{
		RemainingRotation.X *= -1.0f;
		RemainingRotation.Y *= -1.0f;
		RemainingRotation.Z *= -1.0f;
		RemainingRotation.W *= -1.0f;
	}

	const FRotator RemainingRotator = RemainingRotation.Rotator().GetNormalized();
	const FVector RemainingDeltaDegrees(RemainingRotator.Pitch, RemainingRotator.Yaw, RemainingRotator.Roll);
	const FVector NewRemainingDeltaDegrees = StarRovers::Camera::SmoothDampCameraVector(
		RemainingDeltaDegrees,
		FVector::ZeroVector,
		CurrentAngularVelocity,
		SmoothTime,
		DeltaTime);

	if (NewRemainingDeltaDegrees.SizeSquared() <= KINDA_SMALL_NUMBER
		&& CurrentAngularVelocity.SizeSquared() <= KINDA_SMALL_NUMBER)
	{
		CurrentAngularVelocity = FVector::ZeroVector;
		return NormalizedTarget;
	}

	const FQuat NewRemainingRotation = FRotator(
		NewRemainingDeltaDegrees.X,
		NewRemainingDeltaDegrees.Y,
		NewRemainingDeltaDegrees.Z).Quaternion().GetNormalized();
	return (NewRemainingRotation.Inverse() * NormalizedTarget).GetNormalized();
}
