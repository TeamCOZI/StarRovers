#include "SRCameraFocusArcTransitionController.h"

#include "Camera/SRCameraPawnRuntimeState.h"

bool FSRCameraFocusArcTransitionController::Begin(
	FSRCameraFocusArcTransitionState& Transition,
	const FVector& CurrentLocation,
	const FVector& TargetLocation,
	float StartZoomDistance,
	float FinalZoomDistance,
	float PeakZoomDistance)
{
	Transition.Reset();

	const float TravelDistance = FVector::Dist(CurrentLocation, TargetLocation);
	if (TravelDistance <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	Transition.StartLocation = CurrentLocation;
	Transition.Elapsed = 0.0f;
	Transition.StartZoomDistance = FMath::Max(0.0f, StartZoomDistance);
	Transition.FinalZoomDistance = FMath::Max(0.0f, FinalZoomDistance);
	Transition.PeakZoomDistance = FMath::Max(0.0f, PeakZoomDistance);
	Transition.bActive = true;
	return true;
}

bool FSRCameraFocusArcTransitionController::Update(
	FSRCameraFocusArcTransitionState& Transition,
	float DeltaSeconds,
	float Duration,
	float MinArcHeight,
	float ArcHeightMultiplier,
	const FVector& TargetLocation,
	FSRCameraFocusArcTransitionUpdate& OutUpdate)
{
	OutUpdate = FSRCameraFocusArcTransitionUpdate();
	if (!Transition.bActive)
	{
		return false;
	}

	if (DeltaSeconds <= UE_SMALL_NUMBER)
	{
		Transition.Reset();
		return false;
	}

	Transition.Elapsed += DeltaSeconds;
	const float SafeDuration = FMath::Max(0.10f, Duration);
	const float Alpha = FMath::Clamp(Transition.Elapsed / SafeDuration, 0.0f, 1.0f);
	const float SmoothAlpha = Alpha * Alpha * (3.0f - (2.0f * Alpha));

	const float TravelDistance = FVector::Dist(Transition.StartLocation, TargetLocation);
	const float ArcHeight = TravelDistance > KINDA_SMALL_NUMBER
		? FMath::Max(FMath::Max(0.0f, MinArcHeight), TravelDistance * FMath::Max(0.0f, ArcHeightMultiplier))
		: 0.0f;
	const float ArcAlpha = 4.0f * SmoothAlpha * (1.0f - SmoothAlpha);

	OutUpdate.TargetLocation = TargetLocation;
	OutUpdate.NewLocation = FMath::Lerp(Transition.StartLocation, TargetLocation, SmoothAlpha)
		- (FVector::XAxisVector * ArcHeight * ArcAlpha);

	const float ZoomPhaseAlpha = SmoothAlpha < 0.5f
		? SmoothAlpha * 2.0f
		: (SmoothAlpha - 0.5f) * 2.0f;
	OutUpdate.ZoomDistanceTarget = SmoothAlpha < 0.5f
		? FMath::Lerp(Transition.StartZoomDistance, Transition.PeakZoomDistance, ZoomPhaseAlpha)
		: FMath::Lerp(Transition.PeakZoomDistance, Transition.FinalZoomDistance, ZoomPhaseAlpha);

	if (Alpha >= 1.0f - UE_SMALL_NUMBER)
	{
		OutUpdate.NewLocation = TargetLocation;
		OutUpdate.ZoomDistanceTarget = Transition.FinalZoomDistance;
		OutUpdate.bFinished = true;
		Transition.Reset();
	}

	return true;
}
