#include "SRCameraZoomUpdateController.h"

namespace
{
	constexpr float DefaultZoomInterpSpeed = 8.0f;
}

float FSRCameraZoomUpdateController::Update(
	float CurrentZoomDistance,
	float& ZoomDistanceTarget,
	const FVector& PivotLocation,
	float DeltaSeconds,
	bool bApplyImmediateZoom,
	TFunctionRef<float(float)> ClampZoomDistance,
	TFunctionRef<float(float, const FVector&)> ClampZoomDistanceAgainstSpace,
	TFunctionRef<float(float, const FVector&)> ClampZoomDistanceAgainstCelestialBodies)
{
	ZoomDistanceTarget = ClampZoomDistance(ZoomDistanceTarget);
	CurrentZoomDistance = ClampZoomDistance(CurrentZoomDistance);

	auto ApplySpatialConstraints = [
		&ClampZoomDistanceAgainstSpace,
		&ClampZoomDistanceAgainstCelestialBodies,
		&PivotLocation](float ZoomDistance)
	{
		ZoomDistance = ClampZoomDistanceAgainstSpace(ZoomDistance, PivotLocation);
		ZoomDistance = ClampZoomDistanceAgainstCelestialBodies(ZoomDistance, PivotLocation);
		return ClampZoomDistanceAgainstSpace(ZoomDistance, PivotLocation);
	};

	ZoomDistanceTarget = ApplySpatialConstraints(ZoomDistanceTarget);

	if (bApplyImmediateZoom)
	{
		return ZoomDistanceTarget;
	}

	const float InterpolatedZoom = FMath::FInterpTo(CurrentZoomDistance, ZoomDistanceTarget, DeltaSeconds, DefaultZoomInterpSpeed);
	return ApplySpatialConstraints(ClampZoomDistance(InterpolatedZoom));
}
