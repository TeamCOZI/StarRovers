#include "SRCameraPivotUpdateController.h"

#include "SRCameraFocusArcTransitionController.h"
#include "SRCameraMotionSmoothing.h"
#include "Camera/SRCameraPawnRuntimeState.h"

namespace
{
	constexpr float DefaultDragInterpSpeed = 10.0f;
}

FSRCameraPivotUpdateResult FSRCameraPivotUpdateController::Update(
	FVector& DragTargetLocation,
	FVector& FocusDragOffset,
	FVector& FocusTrackingDelta,
	FVector& FocusTrackingDeltaVelocity,
	FSRCameraFocusArcTransitionState& FocusArcTransition,
	float& ZoomDistanceTarget,
	const FSRCameraPivotUpdateSettings& Settings,
	TFunctionRef<FVector(const FVector&)> ClampPivotLocation,
	TFunctionRef<float(float)> ClampZoomDistance)
{
	if (Settings.bHasFocusedActor)
	{
		DragTargetLocation = Settings.FocusLocation + FocusDragOffset;
	}

	const FVector ClampedDragTargetLocation = ClampPivotLocation(DragTargetLocation);
	if (!ClampedDragTargetLocation.Equals(DragTargetLocation, KINDA_SMALL_NUMBER))
	{
		DragTargetLocation = ClampedDragTargetLocation;
		if (Settings.bHasFocusedActor)
		{
			FocusDragOffset = DragTargetLocation - Settings.FocusLocation;
		}
	}

	FSRCameraPivotUpdateResult Result;
	const FVector DesiredLocation = DragTargetLocation;
	Result.NewLocation = DesiredLocation;

	if (Settings.bIsDragging)
	{
		FocusArcTransition.Reset();
		FocusTrackingDelta = FVector::ZeroVector;
		FocusTrackingDeltaVelocity = FVector::ZeroVector;
		Result.NewLocation = DesiredLocation;
	}
	else
	{
		if (FocusArcTransition.bActive && !Settings.bHasFocusedActor)
		{
			FocusArcTransition.Reset();
		}

		FSRCameraFocusArcTransitionUpdate ArcTransitionUpdate;
		if (FocusArcTransition.bActive)
		{
			const FVector ArcTargetLocation = ClampPivotLocation(Settings.FocusLocation + FocusDragOffset);
			if (FSRCameraFocusArcTransitionController::Update(
				FocusArcTransition,
				Settings.DeltaSeconds,
				Settings.FocusArcTransitionDuration,
				Settings.FocusArcMinHeight,
				Settings.FocusArcHeightMultiplier,
				ArcTargetLocation,
				ArcTransitionUpdate))
			{
				DragTargetLocation = ArcTransitionUpdate.TargetLocation;
				ZoomDistanceTarget = ClampZoomDistance(ArcTransitionUpdate.ZoomDistanceTarget);
				Result.NewLocation = ArcTransitionUpdate.NewLocation;
				Result.bUpdatedFocusArcTransition = true;
				FocusTrackingDelta = FVector::ZeroVector;
				FocusTrackingDeltaVelocity = FVector::ZeroVector;
			}
		}
	}

	if (!Settings.bIsDragging && !Result.bUpdatedFocusArcTransition && Settings.bHasFocusedActor)
	{
		FocusTrackingDelta = StarRovers::Camera::SmoothDampCameraVector(
			FocusTrackingDelta,
			FVector::ZeroVector,
			FocusTrackingDeltaVelocity,
			Settings.FocusFollowSmoothTime,
			Settings.DeltaSeconds);

		if (FocusTrackingDelta.SizeSquared() <= KINDA_SMALL_NUMBER && FocusTrackingDeltaVelocity.SizeSquared() <= KINDA_SMALL_NUMBER)
		{
			FocusTrackingDelta = FVector::ZeroVector;
			FocusTrackingDeltaVelocity = FVector::ZeroVector;
		}

		Result.NewLocation = DesiredLocation - FocusTrackingDelta;
	}
	else if (!Settings.bIsDragging && !Result.bUpdatedFocusArcTransition)
	{
		FocusTrackingDelta = FVector::ZeroVector;
		FocusTrackingDeltaVelocity = FVector::ZeroVector;
		Result.NewLocation = FMath::VInterpTo(Settings.CurrentLocation, DesiredLocation, Settings.DeltaSeconds, DefaultDragInterpSpeed);
	}

	Result.NewLocation = ClampPivotLocation(Result.NewLocation);
	return Result;
}
