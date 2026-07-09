#pragma once

#include "CoreMinimal.h"

class AActor;

class FSRCameraCelestialAvoidanceResolver
{
public:
	static float ComputeScaledBodyRadius(const AActor* Actor);

	static bool ResolveAvoidanceSphere(
		const AActor* Actor,
		float CameraSurfacePadding,
		FVector& OutCenter,
		float& OutRadius);

	static float ClampZoomDistanceAgainstBodies(
		float ZoomDistance,
		const FVector& CandidatePawnLocation,
		const FVector& CameraDirection,
		const TArray<AActor*>& CelestialBodies,
		const AActor* ExcludedActor,
		float CameraSurfacePadding);
};
