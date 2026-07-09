#include "SRCameraScreenDragResolver.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"

bool FSRCameraScreenDragResolver::GetMouseScreenPosition(
	const APlayerController* PlayerController,
	FVector2D& OutMouseScreenPosition)
{
	OutMouseScreenPosition = FVector2D::ZeroVector;
	if (!PlayerController)
	{
		return false;
	}

	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (!PlayerController->GetMousePosition(MouseX, MouseY))
	{
		return false;
	}

	OutMouseScreenPosition = FVector2D(MouseX, MouseY);
	return true;
}

FVector FSRCameraScreenDragResolver::ConvertScreenDragToDragOffset(
	const APlayerController* PlayerController,
	const UCameraComponent* Camera,
	const FVector& DragPlaneOrigin,
	float ReferenceZoomDistance,
	float ReferenceFieldOfView,
	float InputScaleMultiplier,
	const FVector2D& StartScreenPosition,
	const FVector2D& CurrentScreenPosition)
{
	if (!PlayerController || !Camera)
	{
		return FVector::ZeroVector;
	}

	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
	if (ViewportWidth <= 0 || ViewportHeight <= 0)
	{
		return FVector::ZeroVector;
	}

	const FVector PlaneNormal = Camera->GetForwardVector().GetSafeNormal();
	if (PlaneNormal.SizeSquared() <= UE_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	const float DistanceToDragPlane = FVector::DotProduct(DragPlaneOrigin - Camera->GetComponentLocation(), PlaneNormal);
	if (DistanceToDragPlane <= UE_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	const float SafeReferenceZoomDistance = FMath::Max(1.0f, ReferenceZoomDistance);
	const float SafeReferenceFieldOfView = FMath::Clamp(ReferenceFieldOfView, 5.0f, 170.0f);
	const float ReferenceTanHalfFieldOfView = FMath::Tan(FMath::DegreesToRadians(SafeReferenceFieldOfView * 0.5f));
	if (ReferenceTanHalfFieldOfView <= UE_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	const float CurrentFieldOfView = FMath::Clamp(Camera->FieldOfView, 5.0f, 170.0f);
	const float CurrentTanHalfFieldOfView = FMath::Tan(FMath::DegreesToRadians(CurrentFieldOfView * 0.5f));
	const float ScreenSpaceInputScale = FMath::Max(
		(FMath::Max(1.0f, DistanceToDragPlane) * CurrentTanHalfFieldOfView)
			/ (SafeReferenceZoomDistance * ReferenceTanHalfFieldOfView),
		UE_SMALL_NUMBER);
	const float ReferenceWorldUnitsPerPixelVertical = (SafeReferenceZoomDistance * ReferenceTanHalfFieldOfView * 2.0f)
		/ static_cast<float>(ViewportHeight);
	const float AdaptiveScale = ScreenSpaceInputScale * FMath::Max(0.0f, InputScaleMultiplier);
	const float WorldUnitsPerPixelVertical = ReferenceWorldUnitsPerPixelVertical * AdaptiveScale;
	const float WorldUnitsPerPixelHorizontal = WorldUnitsPerPixelVertical;

	const FVector2D ScreenDelta = CurrentScreenPosition - StartScreenPosition;
	return (-Camera->GetRightVector() * (ScreenDelta.X * WorldUnitsPerPixelHorizontal))
		+ (Camera->GetUpVector() * (ScreenDelta.Y * WorldUnitsPerPixelVertical));
}
