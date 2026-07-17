#pragma once

#include "CoreMinimal.h"

class AActor;
class FSRCameraFocusSurfaceGridAlignmentResolver
{
public:
	static bool Resolve(
		AActor* FocusedActor,
		const FVector& RayOrigin,
		const FVector& RayDirection,
		const FVector& FocusLocation,
		const FQuat& ViewQuat,
		FVector& OutAxis,
		float& OutAngleRadians);
};
