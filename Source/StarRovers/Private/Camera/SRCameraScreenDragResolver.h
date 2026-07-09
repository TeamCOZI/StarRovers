#pragma once

#include "CoreMinimal.h"

class APlayerController;
class UCameraComponent;

class FSRCameraScreenDragResolver
{
public:
	static bool GetMouseScreenPosition(
		const APlayerController* PlayerController,
		FVector2D& OutMouseScreenPosition);

	static FVector ConvertScreenDragToDragOffset(
		const APlayerController* PlayerController,
		const UCameraComponent* Camera,
		const FVector& DragPlaneOrigin,
		float ReferenceZoomDistance,
		float ReferenceFieldOfView,
		float InputScaleMultiplier,
		const FVector2D& StartScreenPosition,
		const FVector2D& CurrentScreenPosition);
};
