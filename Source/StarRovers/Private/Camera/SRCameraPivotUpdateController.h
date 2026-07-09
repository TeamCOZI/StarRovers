#pragma once

#include "CoreMinimal.h"

struct FSRCameraFocusArcTransitionState;

struct FSRCameraPivotUpdateSettings
{
	FVector CurrentLocation = FVector::ZeroVector;
	FVector FocusLocation = FVector::ZeroVector;
	float DeltaSeconds = 0.0f;
	float FocusFollowSmoothTime = 0.0f;
	float FocusArcTransitionDuration = 0.0f;
	float FocusArcMinHeight = 0.0f;
	float FocusArcHeightMultiplier = 0.0f;
	bool bIsDragging = false;
	bool bHasFocusedActor = false;
};

struct FSRCameraPivotUpdateResult
{
	FVector NewLocation = FVector::ZeroVector;
	bool bUpdatedFocusArcTransition = false;
};

class FSRCameraPivotUpdateController
{
public:
	static FSRCameraPivotUpdateResult Update(
		FVector& DragTargetLocation,
		FVector& FocusDragOffset,
		FVector& FocusTrackingDelta,
		FVector& FocusTrackingDeltaVelocity,
		FSRCameraFocusArcTransitionState& FocusArcTransition,
		float& ZoomDistanceTarget,
		const FSRCameraPivotUpdateSettings& Settings,
		TFunctionRef<FVector(const FVector&)> ClampPivotLocation,
		TFunctionRef<float(float)> ClampZoomDistance);
};
