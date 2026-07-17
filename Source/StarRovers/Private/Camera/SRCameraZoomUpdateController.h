#pragma once

#include "CoreMinimal.h"

class FSRCameraZoomUpdateController
{
public:
	static float Update(
		float CurrentZoomDistance,
		float& ZoomDistanceTarget,
		const FVector& PivotLocation,
		float DeltaSeconds,
		bool bApplyImmediateZoom,
		TFunctionRef<float(float)> ClampZoomDistance,
		TFunctionRef<float(float, const FVector&)> ClampZoomDistanceAgainstSpace,
		TFunctionRef<float(float, const FVector&)> ClampZoomDistanceAgainstCelestialBodies);
};
