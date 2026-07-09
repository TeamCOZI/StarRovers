#pragma once

#include "CoreMinimal.h"

class AActor;

class FSRCameraFocusZoomResolver
{
public:
	static float ResolveActorFocusZoomDistance(
		const AActor* Actor,
		float CameraFieldOfViewDegrees,
		float FocusZoomMultiplier,
		float SmallActorFocusZoomDistance);

private:
	static float ComputeActorVisiblePrimitiveRadius(const AActor* Actor);
};
