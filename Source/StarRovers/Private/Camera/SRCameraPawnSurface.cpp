#include "Camera/SRCameraPawn.h"

#include "SRCameraPawnInternal.h"
#include "Camera/CameraComponent.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "GameFramework/SpringArmComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	constexpr float TwoPiRadians = PI * 2.0f;

	float NormalizeRadians(float AngleRadians)
	{
		float NormalizedAngle = FMath::Fmod(AngleRadians + PI, TwoPiRadians);
		if (NormalizedAngle < 0.0f)
		{
			NormalizedAngle += TwoPiRadians;
		}
		return NormalizedAngle - PI;
	}

	FQuat SmoothDampQuat(
		const FQuat& Current,
		const FQuat& Target,
		FVector& CurrentAngularVelocity,
		const float SmoothTime,
		const float DeltaTime)
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
		const FVector NewRemainingDeltaDegrees = StarRovers::Camera::SmoothDampVector(
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
	const FQuat& ViewQuat,
	float ZoomDistance,
	FVector& OutAxis,
	float& OutAngleRadians) const
{
	OutAxis = FVector::ZeroVector;
	OutAngleRadians = 0.0f;
	if (!IsValid(FocusedActor) || !SpringArm)
	{
		return false;
	}

	USRPlanetSurfaceGrid* SurfaceGrid = USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(FocusedActor);
	if (!IsValid(SurfaceGrid) || SurfaceGrid->GetCellCount() <= 0)
	{
		return false;
	}

	if (AActor* SurfaceOwner = SurfaceGrid->GetOwner())
	{
		SurfaceOwner->UpdateComponentTransforms();
	}
	SurfaceGrid->UpdateComponentToWorld();

	const FVector ViewForward = ViewQuat.RotateVector(FVector::ForwardVector).GetSafeNormal();
	const FVector ViewRight = ViewQuat.RotateVector(FVector::RightVector).GetSafeNormal();
	const FVector ViewUp = ViewQuat.RotateVector(FVector::UpVector).GetSafeNormal();
	if (ViewForward.IsNearlyZero() || ViewRight.IsNearlyZero() || ViewUp.IsNearlyZero())
	{
		return false;
	}

	const float SafeZoomDistance = FMath::Max(1.0f, ZoomDistance);
	FVector RayOrigin = Camera ? Camera->GetComponentLocation() : GetActorLocation() - (ViewForward * SafeZoomDistance);
	FVector RayDirection = Camera ? Camera->GetForwardVector().GetSafeNormal() : ViewForward;
	if (RayDirection.IsNearlyZero())
	{
		RayDirection = ViewForward;
	}

	FSRPlanetSurfaceGridCell HitCell;
	FVector HitLocation = FVector::ZeroVector;
	if (!SurfaceGrid->RaycastCell(RayOrigin, RayDirection, HitCell, HitLocation))
	{
		return false;
	}

	FTransform CellWorldTransform = FTransform::Identity;
	const bool bHasCellWorldTransform = SurfaceGrid->GetCellWorldTransform(HitCell.CellId, 0.0f, CellWorldTransform);
	const FVector FocusCenter = GetFocusLocation();
	FVector AlignmentAxis = (HitLocation - FocusCenter).GetSafeNormal();
	if (AlignmentAxis.IsNearlyZero() && bHasCellWorldTransform)
	{
		AlignmentAxis = (CellWorldTransform.GetLocation() - FocusCenter).GetSafeNormal();
	}
	if (AlignmentAxis.IsNearlyZero() && bHasCellWorldTransform)
	{
		AlignmentAxis = CellWorldTransform.GetRotation().GetUpVector().GetSafeNormal();
	}
	if (AlignmentAxis.IsNearlyZero())
	{
		return false;
	}

	FVector Corner00 = FVector::ZeroVector;
	FVector Corner10 = FVector::ZeroVector;
	FVector Corner11 = FVector::ZeroVector;
	FVector Corner01 = FVector::ZeroVector;
	if (!SurfaceGrid->GetCellWorldCorners(HitCell.CellId, Corner00, Corner10, Corner11, Corner01))
	{
		return false;
	}

	const FVector GridUWorld = (((Corner10 + Corner11) * 0.5f) - ((Corner00 + Corner01) * 0.5f)).GetSafeNormal();
	const FVector GridVWorld = (((Corner01 + Corner11) * 0.5f) - ((Corner00 + Corner10) * 0.5f)).GetSafeNormal();
	if (GridUWorld.IsNearlyZero() || GridVWorld.IsNearlyZero())
	{
		return false;
	}

	bool bFoundCandidate = false;
	float BestCandidateScore = BIG_NUMBER;
	float BestAngleDistanceRadians = BIG_NUMBER;
	float BestAngleRadians = 0.0f;
	auto EvaluateAlignmentAngle = [
		&bFoundCandidate,
		&BestCandidateScore,
		&BestAngleDistanceRadians,
		&BestAngleRadians,
		&AlignmentAxis,
		&ViewForward,
		&ViewRight,
		&ViewUp](const FVector& WorldAxis, float CandidateAngleRadians)
	{
		const float NormalizedAngleRadians = NormalizeRadians(CandidateAngleRadians);
		const FQuat CandidateDelta(AlignmentAxis, NormalizedAngleRadians);
		const FVector CandidateForward = CandidateDelta.RotateVector(ViewForward).GetSafeNormal();
		const FVector CandidateRight = CandidateDelta.RotateVector(ViewRight).GetSafeNormal();
		const FVector CandidateUp = CandidateDelta.RotateVector(ViewUp).GetSafeNormal();
		if (CandidateForward.IsNearlyZero() || CandidateRight.IsNearlyZero() || CandidateUp.IsNearlyZero())
		{
			return;
		}

		const FVector ProjectedAxis = FVector::VectorPlaneProject(WorldAxis, CandidateForward).GetSafeNormal();
		if (ProjectedAxis.IsNearlyZero())
		{
			return;
		}

		const float VerticalError = FMath::Abs(FVector::DotProduct(ProjectedAxis, CandidateRight));
		const float DirectionError = 1.0f - FMath::Abs(FVector::DotProduct(ProjectedAxis, CandidateUp));
		const float CandidateScore = (VerticalError + DirectionError) * 1000.0f + FMath::Abs(NormalizedAngleRadians);
		const float AngleDistanceRadians = FMath::Abs(NormalizedAngleRadians);
		if (!bFoundCandidate
			|| CandidateScore < BestCandidateScore - UE_SMALL_NUMBER
			|| (FMath::IsNearlyEqual(CandidateScore, BestCandidateScore, UE_SMALL_NUMBER)
				&& AngleDistanceRadians < BestAngleDistanceRadians))
		{
			bFoundCandidate = true;
			BestCandidateScore = CandidateScore;
			BestAngleDistanceRadians = AngleDistanceRadians;
			BestAngleRadians = NormalizedAngleRadians;
		}
	};

	auto ConsiderProjectedGridAxis = [
		&AlignmentAxis,
		&ViewRight,
		&EvaluateAlignmentAngle](const FVector& WorldAxis)
	{
		const FVector NormalizedWorldAxis = WorldAxis.GetSafeNormal();
		if (NormalizedWorldAxis.IsNearlyZero())
		{
			return;
		}

		const float AxisDotGrid = FVector::DotProduct(AlignmentAxis, NormalizedWorldAxis);
		const float AxisDotRight = FVector::DotProduct(AlignmentAxis, ViewRight);
		const float CosCoefficient = FVector::DotProduct(NormalizedWorldAxis, ViewRight) - (AxisDotGrid * AxisDotRight);
		const float SinCoefficient = FVector::DotProduct(NormalizedWorldAxis, FVector::CrossProduct(AlignmentAxis, ViewRight));
		const float ConstantCoefficient = AxisDotGrid * AxisDotRight;
		const float WaveAmplitude = FMath::Sqrt(
			FMath::Square(CosCoefficient)
			+ FMath::Square(SinCoefficient));
		if (WaveAmplitude <= UE_SMALL_NUMBER)
		{
			EvaluateAlignmentAngle(NormalizedWorldAxis, 0.0f);
			return;
		}

		const float WavePhase = FMath::Atan2(SinCoefficient, CosCoefficient);
		const float RawCosValue = -ConstantCoefficient / WaveAmplitude;
		if (RawCosValue < -1.0f - KINDA_SMALL_NUMBER || RawCosValue > 1.0f + KINDA_SMALL_NUMBER)
		{
			EvaluateAlignmentAngle(NormalizedWorldAxis, WavePhase);
			EvaluateAlignmentAngle(NormalizedWorldAxis, WavePhase + PI);
			return;
		}

		const float AngleOffset = FMath::Acos(FMath::Clamp(RawCosValue, -1.0f, 1.0f));
		EvaluateAlignmentAngle(NormalizedWorldAxis, WavePhase + AngleOffset);
		EvaluateAlignmentAngle(NormalizedWorldAxis, WavePhase - AngleOffset);
	};

	ConsiderProjectedGridAxis(GridUWorld);
	ConsiderProjectedGridAxis(-GridUWorld);
	ConsiderProjectedGridAxis(GridVWorld);
	ConsiderProjectedGridAxis(-GridVWorld);
	if (!bFoundCandidate)
	{
		return false;
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[Camera] Focus surface grid alignment center cell: Face=%d X=%d Y=%d Ray=%s Hit=%s Axis=%s AngleDegrees=%.3f FocusedActor=%s"),
		HitCell.CellId.Face,
		HitCell.CellId.CellX,
		HitCell.CellId.CellY,
		Camera ? TEXT("CameraComponentCenter") : TEXT("ViewForwardFallback"),
		*HitLocation.ToCompactString(),
		*AlignmentAxis.ToCompactString(),
		FMath::RadiansToDegrees(BestAngleRadians),
		*GetNameSafe(FocusedActor.Get()));

	OutAxis = AlignmentAxis;
	OutAngleRadians = BestAngleRadians;
	return true;
}

void ASRCameraPawn::UpdateFocusSurface(float DeltaSeconds)
{
	if (DeltaSeconds <= UE_SMALL_NUMBER || !ShouldAllowFocusSurface())
	{
		ClearFocusSurfaceMotion();
		return;
	}

	const FVector2D CombinedLookInput = FocusSurface.Input.GetClampedToMaxSize(1.0f);
	const bool bHasDirectInput = !CombinedLookInput.IsNearlyZero();
	const float SafeLookSpeed = FMath::Max(0.0f, FocusSurfaceSpeed);
	const float SafeMinInertiaSpeed = FMath::Max(0.0f, FocusSurfaceMinInertiaSpeed);
	bool bAppliedDirectInput = false;

	const float InputInterpRate = bHasDirectInput
		? FMath::Max(0.0f, FocusSurfaceInputAcceleration)
		: FMath::Max(0.0f, FocusSurfaceInputDeceleration);
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
		ApplyFocusSurfaceDelta(DegreesDelta);
		FocusSurface.bPendingGridAutoAlignment = true;
		if (bHasDirectInput)
		{
			FocusSurface.AngularVelocity = FVector2D::ZeroVector;
		}
		bAppliedDirectInput = true;
	}

	if (!bHasDirectInput && !bIsDraggingFocusSurface && !FocusSurface.AngularVelocity.IsNearlyZero(SafeMinInertiaSpeed))
	{
		ApplyFocusSurfaceDelta(FocusSurface.AngularVelocity * DeltaSeconds);
		FocusSurface.bPendingGridAutoAlignment = true;

		const float SafeDamping = FMath::Max(0.0f, FocusSurfaceInertiaDamping);
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

	const bool bHasRemainingFocusSurfaceMotion = bAppliedDirectInput
		|| bIsDraggingFocusSurface
		|| !FocusSurface.AngularVelocity.IsNearlyZero();
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
			bIsFocusSurfaceActive = false;
			return;
		}

		const float CurrentAngleDegrees = FMath::RadiansToDegrees(FocusSurface.RigAlignmentCurrentAngleRadians);
		const float TargetAngleDegrees = FMath::RadiansToDegrees(FocusSurface.RigAlignmentTargetAngleRadians);
		const FVector NewAngleDegrees = StarRovers::Camera::SmoothDampVector(
			FVector(CurrentAngleDegrees, 0.0f, 0.0f),
			FVector(TargetAngleDegrees, 0.0f, 0.0f),
			FocusSurface.RotationSmoothVelocity,
			FocusFollowSmoothTime,
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
		ApplyFocusSurfaceRigAlignmentLocation();

		const float RemainingAngleDegrees = FMath::Abs(TargetAngleDegrees - NewAngleDegrees.X);
		if (RemainingAngleDegrees <= 0.05f && FocusSurface.RotationSmoothVelocity.SizeSquared() <= KINDA_SMALL_NUMBER)
		{
			FocusSurface.RigAlignmentCurrentAngleRadians = FocusSurface.RigAlignmentTargetAngleRadians;
			FocusSurface.Rotation = FocusSurface.TargetRotation.GetNormalized();
			ApplyFocusSurfaceRigAlignmentLocation();
			FocusSurface.RotationSmoothVelocity = FVector::ZeroVector;
			FocusSurface.RigAlignmentCurrentAngleRadians = 0.0f;
			FocusSurface.RigAlignmentTargetAngleRadians = 0.0f;
			FocusSurface.bIsResettingRotation = false;
			FocusSurface.bIsAligningRig = false;
			bIsFocusSurfaceActive = false;
			return;
		}

		bIsFocusSurfaceActive = true;
		return;
	}

	FocusSurface.Rotation = SmoothDampQuat(
		FocusSurface.Rotation,
		FocusSurface.TargetRotation,
		FocusSurface.RotationSmoothVelocity,
		FocusFollowSmoothTime,
		DeltaSeconds);

	const FQuat RemainingRotation = (FocusSurface.TargetRotation.GetNormalized() * FocusSurface.Rotation.GetNormalized().Inverse()).GetNormalized();
	const float RemainingAngleDegrees = FMath::RadiansToDegrees(RemainingRotation.GetAngle());
	if (RemainingAngleDegrees <= 0.05f && FocusSurface.RotationSmoothVelocity.SizeSquared() <= KINDA_SMALL_NUMBER)
	{
		FocusSurface.Rotation = FocusSurface.TargetRotation.GetNormalized();
		ApplyFocusSurfaceRigAlignmentLocation();
		FocusSurface.RotationSmoothVelocity = FVector::ZeroVector;
		FocusSurface.bIsResettingRotation = false;
		FocusSurface.bIsAligningRig = false;
		bIsFocusSurfaceActive = false;
		return;
	}

	ApplyFocusSurfaceRigAlignmentLocation();
	bIsFocusSurfaceActive = true;
}

void ASRCameraPawn::ApplyFocusSurfaceRigAlignmentLocation()
{
	if (!FocusSurface.bIsAligningRig || !IsValid(FocusedActor))
	{
		return;
	}

	const FVector AlignmentAxis = FocusSurface.RigAlignmentAxis.GetSafeNormal();
	if (AlignmentAxis.IsNearlyZero())
	{
		return;
	}

	const FQuat CurrentDeltaRotation(AlignmentAxis, FocusSurface.RigAlignmentCurrentAngleRadians);
	const FVector UpdatedFocusDragOffset = CurrentDeltaRotation.RotateVector(FocusSurface.RigAlignmentStartOffset);
	FocusDragOffset = UpdatedFocusDragOffset;
	DragTargetLocation = GetFocusLocation() + FocusDragOffset;
	FocusTrackingDelta = FVector::ZeroVector;
	FocusTrackingDeltaVelocity = FVector::ZeroVector;
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

	StopFocusArcTransition();
	ClearFocusSurfaceMotion();

	const float RotationAngleRadians = FMath::DegreesToRadians(90.0f * static_cast<float>(StepDelta));
	const FQuat CurrentSurfaceRotation = FocusSurface.Rotation.GetNormalized();
	const FQuat RotationDelta(RotationAxis, RotationAngleRadians);
	FocusSurface.TargetRotation = (RotationDelta * CurrentSurfaceRotation).GetNormalized();
	FocusSurface.RigAlignmentStartRotation = CurrentSurfaceRotation;
	FocusSurface.RigAlignmentStartOffset = GetActorLocation() - GetFocusLocation();
	FocusSurface.RigAlignmentAxis = RotationAxis;
	FocusSurface.RigAlignmentCurrentAngleRadians = 0.0f;
	FocusSurface.RigAlignmentTargetAngleRadians = RotationAngleRadians;
	FocusSurface.RotationSmoothVelocity = FVector::ZeroVector;
	FocusSurface.bIsResettingRotation = true;
	FocusSurface.bIsAligningRig = true;
	bIsFocusSurfaceActive = true;
	return true;
}

void ASRCameraPawn::ApplyFocusSurfaceDelta(const FVector2D& DegreesDelta)
{
	if (DegreesDelta.IsNearlyZero())
	{
		return;
	}

	FocusSurface.RotationSmoothVelocity = FVector::ZeroVector;
	FocusSurface.bIsResettingRotation = false;
	FocusSurface.bIsAligningRig = false;
	FocusSurface.RigAlignmentCurrentAngleRadians = 0.0f;
	FocusSurface.RigAlignmentTargetAngleRadians = 0.0f;

	FQuat CurrentRotation = FocusSurface.Rotation.GetNormalized();
	const float CurrentZoomDistance = FMath::Max(1.0f, SpringArm ? SpringArm->TargetArmLength : ZoomDistanceTarget);
	const FQuat BaseViewQuat = GetViewRotationForZoom(CurrentZoomDistance).Quaternion();
	const FQuat CameraRelativeQuat = BaseViewQuat.GetNormalized();
	if (!FMath::IsNearlyZero(DegreesDelta.X))
	{
		const FQuat CurrentViewQuat = (CurrentRotation * CameraRelativeQuat).GetNormalized();
		const FVector CurrentUpAxis = CurrentViewQuat.RotateVector(FVector::UpVector).GetSafeNormal();
		const FQuat YawDelta(CurrentUpAxis, FMath::DegreesToRadians(DegreesDelta.X));
		CurrentRotation = (YawDelta * CurrentRotation).GetNormalized();
	}

	if (!FMath::IsNearlyZero(DegreesDelta.Y))
	{
		const FQuat CurrentViewQuat = (CurrentRotation * CameraRelativeQuat).GetNormalized();
		const FVector CurrentRightAxis = CurrentViewQuat.RotateVector(FVector::RightVector).GetSafeNormal();
		const FQuat PitchDelta(CurrentRightAxis, FMath::DegreesToRadians(DegreesDelta.Y));
		CurrentRotation = (PitchDelta * CurrentRotation).GetNormalized();
	}

	FocusSurface.Rotation = CurrentRotation;
	FocusSurface.TargetRotation = CurrentRotation;
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

	ApplyFocusSurfaceDelta(DegreesDelta);
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
