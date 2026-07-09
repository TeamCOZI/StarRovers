#include "SRCameraZoomUpdateController.h"

#include "GameFramework/SpringArmComponent.h"

namespace
{
	constexpr float DefaultZoomInterpSpeed = 8.0f;
}

void FSRCameraZoomUpdateController::Update(
	USpringArmComponent* SpringArm,
	float& ZoomDistanceTarget,
	const FVector& PivotLocation,
	float DeltaSeconds,
	bool bApplyImmediateZoom,
	TFunctionRef<float(float)> ClampZoomDistance,
	TFunctionRef<float(float, const FVector&)> ClampZoomDistanceAgainstSpace,
	TFunctionRef<float(float, const FVector&)> ClampZoomDistanceAgainstCelestialBodies,
	TFunctionRef<void(float)> ApplyZoomDrivenViewRotation)
{
	if (!SpringArm)
	{
		return;
	}

	ZoomDistanceTarget = ClampZoomDistance(ZoomDistanceTarget);
	SpringArm->TargetArmLength = ClampZoomDistance(SpringArm->TargetArmLength);
	ApplyZoomDrivenViewRotation(ZoomDistanceTarget);

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
		SpringArm->TargetArmLength = ZoomDistanceTarget;
		ApplyZoomDrivenViewRotation(SpringArm->TargetArmLength);
		return;
	}

	const float InterpolatedZoom = FMath::FInterpTo(SpringArm->TargetArmLength, ZoomDistanceTarget, DeltaSeconds, DefaultZoomInterpSpeed);
	SpringArm->TargetArmLength = ApplySpatialConstraints(ClampZoomDistance(InterpolatedZoom));
	ApplyZoomDrivenViewRotation(SpringArm->TargetArmLength);
}
