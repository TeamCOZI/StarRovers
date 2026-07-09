#pragma once

#include "CoreMinimal.h"

class USpringArmComponent;

class FSRCameraZoomUpdateController
{
public:
	static void Update(
		USpringArmComponent* SpringArm,
		float& ZoomDistanceTarget,
		const FVector& PivotLocation,
		float DeltaSeconds,
		bool bApplyImmediateZoom,
		TFunctionRef<float(float)> ClampZoomDistance,
		TFunctionRef<float(float, const FVector&)> ClampZoomDistanceAgainstSpace,
		TFunctionRef<float(float, const FVector&)> ClampZoomDistanceAgainstCelestialBodies,
		TFunctionRef<void(float)> ApplyZoomDrivenViewRotation);
};
