#include "Camera/SRCameraPawn.h"

#include "SRCameraPawnInternal.h"
#include "Camera/CameraComponent.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "GameFramework/SpringArmComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
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

bool ASRCameraPawn::TryComputeFocusSurfaceGridAlignmentRoll(
	const FQuat& ViewQuat,
	float ZoomDistance,
	float& OutRollRadians) const
{
	OutRollRadians = 0.0f;
	if (!IsValid(FocusedActor) || !SpringArm)
	{
		return false;
	}

	const USRPlanetSurfaceGrid* SurfaceGrid = USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(FocusedActor);
	if (!IsValid(SurfaceGrid) || SurfaceGrid->GetCellCount() <= 0)
	{
		return false;
	}

	const FVector ViewForward = ViewQuat.RotateVector(FVector::ForwardVector).GetSafeNormal();
	const FVector ViewUp = ViewQuat.RotateVector(FVector::UpVector).GetSafeNormal();
	if (ViewForward.IsNearlyZero() || ViewUp.IsNearlyZero())
	{
		return false;
	}

	const float SafeZoomDistance = FMath::Max(1.0f, ZoomDistance);
	const FVector CameraLocation = Camera ? Camera->GetComponentLocation() : GetActorLocation() - (ViewForward * SafeZoomDistance);
	FSRPlanetSurfaceGridCell HitCell;
	FVector HitLocation = FVector::ZeroVector;
	if (!SurfaceGrid->RaycastCell(CameraLocation, ViewForward, HitCell, HitLocation))
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

	float BestRollDistanceRadians = BIG_NUMBER;
	float BestRollTargetRadians = 0.0f;
	auto ConsiderProjectedGridAxis = [
		&BestRollDistanceRadians,
		&BestRollTargetRadians,
		&ViewForward,
		&ViewUp](const FVector& WorldAxis)
	{
		const FVector ProjectedAxis = FVector::VectorPlaneProject(WorldAxis, ViewForward).GetSafeNormal();
		if (ProjectedAxis.IsNearlyZero())
		{
			return;
		}

		const float SinAngle = FVector::DotProduct(ViewForward, FVector::CrossProduct(ViewUp, ProjectedAxis));
		const float CosAngle = FMath::Clamp(FVector::DotProduct(ViewUp, ProjectedAxis), -1.0f, 1.0f);
		const float CandidateRollRadians = FMath::Atan2(SinAngle, CosAngle);
		const float CandidateRollDistanceRadians = FMath::Abs(CandidateRollRadians);
		if (CandidateRollDistanceRadians < BestRollDistanceRadians)
		{
			BestRollDistanceRadians = CandidateRollDistanceRadians;
			BestRollTargetRadians = CandidateRollRadians;
		}
	};

	ConsiderProjectedGridAxis(GridUWorld);
	ConsiderProjectedGridAxis(-GridUWorld);
	ConsiderProjectedGridAxis(GridVWorld);
	ConsiderProjectedGridAxis(-GridVWorld);
	if (BestRollDistanceRadians >= BIG_NUMBER)
	{
		return false;
	}

	OutRollRadians = BestRollTargetRadians;
	return true;
}

void ASRCameraPawn::UpdateFocusSurface(float DeltaSeconds)
{
	if (DeltaSeconds <= UE_SMALL_NUMBER || !ShouldAllowFocusSurface())
	{
		ClearFocusSurfaceMotion();
		return;
	}

	const FVector2D CombinedLookInput = FocusSurfaceInput.GetClampedToMaxSize(1.0f);
	const bool bHasDirectInput = !CombinedLookInput.IsNearlyZero();
	const float SafeLookSpeed = FMath::Max(0.0f, FocusSurfaceSpeed);
	const float SafeMinInertiaSpeed = FMath::Max(0.0f, FocusSurfaceMinInertiaSpeed);
	bool bAppliedDirectInput = false;

	const float InputInterpRate = bHasDirectInput
		? FMath::Max(0.0f, FocusSurfaceInputAcceleration)
		: FMath::Max(0.0f, FocusSurfaceInputDeceleration);
	if (InputInterpRate <= KINDA_SMALL_NUMBER)
	{
		FocusSurfaceAcceleratedInput = CombinedLookInput;
	}
	else
	{
		FocusSurfaceAcceleratedInput.X = FMath::FInterpConstantTo(FocusSurfaceAcceleratedInput.X, CombinedLookInput.X, DeltaSeconds, InputInterpRate);
		FocusSurfaceAcceleratedInput.Y = FMath::FInterpConstantTo(FocusSurfaceAcceleratedInput.Y, CombinedLookInput.Y, DeltaSeconds, InputInterpRate);
		FocusSurfaceAcceleratedInput = FocusSurfaceAcceleratedInput.GetClampedToMaxSize(1.0f);
	}

	if (!bHasDirectInput && FocusSurfaceAcceleratedInput.IsNearlyZero())
	{
		FocusSurfaceAcceleratedInput = FVector2D::ZeroVector;
	}

	if (!FocusSurfaceAcceleratedInput.IsNearlyZero() && SafeLookSpeed > KINDA_SMALL_NUMBER)
	{
		const FVector2D DegreesDelta = FVector2D(-FocusSurfaceAcceleratedInput.X, FocusSurfaceAcceleratedInput.Y) * SafeLookSpeed * DeltaSeconds;
		ApplyFocusSurfaceDelta(DegreesDelta);
		bPendingFocusSurfaceGridAutoAlignment = true;
		if (bHasDirectInput)
		{
			FocusSurfaceAngularVelocity = FVector2D::ZeroVector;
		}
		bAppliedDirectInput = true;
	}

	if (!bHasDirectInput && !bIsDraggingFocusSurface && !FocusSurfaceAngularVelocity.IsNearlyZero(SafeMinInertiaSpeed))
	{
		ApplyFocusSurfaceDelta(FocusSurfaceAngularVelocity * DeltaSeconds);
		bPendingFocusSurfaceGridAutoAlignment = true;

		const float SafeDamping = FMath::Max(0.0f, FocusSurfaceInertiaDamping);
		if (SafeDamping <= KINDA_SMALL_NUMBER)
		{
			FocusSurfaceAngularVelocity = FVector2D::ZeroVector;
		}
		else
		{
			FocusSurfaceAngularVelocity.X = FMath::FInterpTo(FocusSurfaceAngularVelocity.X, 0.0f, DeltaSeconds, SafeDamping);
			FocusSurfaceAngularVelocity.Y = FMath::FInterpTo(FocusSurfaceAngularVelocity.Y, 0.0f, DeltaSeconds, SafeDamping);
		}
	}

	if (FocusSurfaceAngularVelocity.IsNearlyZero(SafeMinInertiaSpeed))
	{
		FocusSurfaceAngularVelocity = FVector2D::ZeroVector;
	}

	const bool bHasRemainingFocusSurfaceMotion = bAppliedDirectInput
		|| bIsDraggingFocusSurface
		|| !FocusSurfaceAngularVelocity.IsNearlyZero();
	bIsFocusSurfaceActive = bHasRemainingFocusSurfaceMotion;
	if (!bHasRemainingFocusSurfaceMotion && bPendingFocusSurfaceGridAutoAlignment)
	{
		bPendingFocusSurfaceGridAutoAlignment = false;
		TryStartFocusSurfaceGridAlignment();
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
		FocusSurfaceRotation = FQuat::Identity;
		FocusSurfaceTargetRotation = FQuat::Identity;
		FocusSurfaceRotationSmoothVelocity = FVector::ZeroVector;
		bIsResettingFocusSurfaceRotation = false;
		bPendingFocusSurfaceGridAutoAlignment = false;
		return;
	}

	if (!bIsResettingFocusSurfaceRotation)
	{
		return;
	}

	FocusSurfaceRotation = SmoothDampQuat(
		FocusSurfaceRotation,
		FocusSurfaceTargetRotation,
		FocusSurfaceRotationSmoothVelocity,
		FocusFollowSmoothTime,
		DeltaSeconds);

	const FQuat RemainingRotation = (FocusSurfaceTargetRotation.GetNormalized() * FocusSurfaceRotation.GetNormalized().Inverse()).GetNormalized();
	const float RemainingAngleDegrees = FMath::RadiansToDegrees(RemainingRotation.GetAngle());
	if (RemainingAngleDegrees <= 0.05f && FocusSurfaceRotationSmoothVelocity.SizeSquared() <= KINDA_SMALL_NUMBER)
	{
		FocusSurfaceRotation = FocusSurfaceTargetRotation.GetNormalized();
		FocusSurfaceRotationSmoothVelocity = FVector::ZeroVector;
		bIsResettingFocusSurfaceRotation = false;
		bIsFocusSurfaceActive = false;
		return;
	}

	bIsFocusSurfaceActive = true;
}

void ASRCameraPawn::ApplyFocusSurfaceDelta(const FVector2D& DegreesDelta)
{
	if (DegreesDelta.IsNearlyZero())
	{
		return;
	}

	FocusSurfaceRotationSmoothVelocity = FVector::ZeroVector;
	bIsResettingFocusSurfaceRotation = false;

	FQuat CurrentRotation = FocusSurfaceRotation.GetNormalized();
	if (!FMath::IsNearlyZero(DegreesDelta.X))
	{
		const FVector CurrentUpAxis = CurrentRotation.RotateVector(FVector::UpVector).GetSafeNormal();
		const FQuat YawDelta(CurrentUpAxis, FMath::DegreesToRadians(DegreesDelta.X));
		CurrentRotation = (YawDelta * CurrentRotation).GetNormalized();
	}

	if (!FMath::IsNearlyZero(DegreesDelta.Y))
	{
		const FVector CurrentRightAxis = CurrentRotation.RotateVector(FVector::RightVector).GetSafeNormal();
		const FQuat PitchDelta(CurrentRightAxis, FMath::DegreesToRadians(DegreesDelta.Y));
		CurrentRotation = (PitchDelta * CurrentRotation).GetNormalized();
	}

	FocusSurfaceRotation = CurrentRotation;
	FocusSurfaceTargetRotation = CurrentRotation;
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
	bPendingFocusSurfaceGridAutoAlignment = true;

	const UWorld* World = GetWorld();
	const float DeltaSeconds = World ? World->GetDeltaSeconds() : 0.0f;
	if (DeltaSeconds > UE_SMALL_NUMBER)
	{
		FocusSurfaceAngularVelocity = DegreesDelta / DeltaSeconds;
	}
	bIsFocusSurfaceActive = true;
}

void ASRCameraPawn::ClearFocusSurfaceMotion()
{
	FocusSurfaceInput = FVector2D::ZeroVector;
	FocusSurfaceAcceleratedInput = FVector2D::ZeroVector;
	FocusSurfaceAngularVelocity = FVector2D::ZeroVector;
	bIsFocusSurfaceActive = false;
	bIsDraggingFocusSurface = false;
	bPendingFocusSurfaceGridAutoAlignment = false;
}
