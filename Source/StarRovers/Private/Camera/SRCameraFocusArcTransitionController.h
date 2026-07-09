#pragma once

#include "CoreMinimal.h"

struct FSRCameraFocusArcTransitionState;

struct FSRCameraFocusArcTransitionUpdate
{
	FVector TargetLocation = FVector::ZeroVector;
	FVector NewLocation = FVector::ZeroVector;
	float ZoomDistanceTarget = 0.0f;
	bool bFinished = false;
};

class FSRCameraFocusArcTransitionController
{
public:
	static bool Begin(
		FSRCameraFocusArcTransitionState& Transition,
		const FVector& CurrentLocation,
		const FVector& TargetLocation,
		float StartZoomDistance,
		float FinalZoomDistance,
		float PeakZoomDistance);

	static bool Update(
		FSRCameraFocusArcTransitionState& Transition,
		float DeltaSeconds,
		float Duration,
		float MinArcHeight,
		float ArcHeightMultiplier,
		const FVector& TargetLocation,
		FSRCameraFocusArcTransitionUpdate& OutUpdate);
};
