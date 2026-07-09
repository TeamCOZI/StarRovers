#pragma once

#include "CoreMinimal.h"

class AActor;
class UCameraComponent;

class FSRCameraFocusSurfaceGridAlignmentResolver
{
public:
	static bool Resolve(
		AActor* FocusedActor,
		const UCameraComponent* Camera,
		const FVector& PawnLocation,
		const FVector& FocusLocation,
		const FQuat& ViewQuat,
		float ZoomDistance,
		FVector& OutAxis,
		float& OutAngleRadians);
};
