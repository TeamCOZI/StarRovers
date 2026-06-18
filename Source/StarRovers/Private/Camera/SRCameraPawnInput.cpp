#include "Camera/SRCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Camera/SRPlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
void ASRCameraPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (DragHoldAction)
		{
			EnhancedInputComponent->BindAction(DragHoldAction, ETriggerEvent::Started, this, &ASRCameraPawn::HandleDragHoldStarted);
			EnhancedInputComponent->BindAction(DragHoldAction, ETriggerEvent::Completed, this, &ASRCameraPawn::HandleDragHoldCompleted);
			EnhancedInputComponent->BindAction(DragHoldAction, ETriggerEvent::Canceled, this, &ASRCameraPawn::HandleDragHoldCompleted);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ASRCameraPawn requires DragHoldAction before input binding."));
		}

		if (FocusSurfaceDragHoldAction)
		{
			EnhancedInputComponent->BindAction(FocusSurfaceDragHoldAction, ETriggerEvent::Started, this, &ASRCameraPawn::HandleFocusSurfaceDragHoldStarted);
			EnhancedInputComponent->BindAction(FocusSurfaceDragHoldAction, ETriggerEvent::Completed, this, &ASRCameraPawn::HandleFocusSurfaceDragHoldCompleted);
			EnhancedInputComponent->BindAction(FocusSurfaceDragHoldAction, ETriggerEvent::Canceled, this, &ASRCameraPawn::HandleFocusSurfaceDragHoldCompleted);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRCameraPawn requires FocusSurfaceDragHoldAction before left-click focus surface drag binding."));
		}

		if (DragDeltaAction)
		{
			EnhancedInputComponent->BindAction(DragDeltaAction, ETriggerEvent::Triggered, this, &ASRCameraPawn::HandleDragDelta);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ASRCameraPawn requires DragDeltaAction before input binding."));
		}

		if (ZoomAction)
		{
			EnhancedInputComponent->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &ASRCameraPawn::HandleZoom);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ASRCameraPawn requires ZoomAction before input binding."));
		}

		if (FocusSurfaceAction)
		{
			EnhancedInputComponent->BindAction(FocusSurfaceAction, ETriggerEvent::Triggered, this, &ASRCameraPawn::HandleFocusSurface);
			EnhancedInputComponent->BindAction(FocusSurfaceAction, ETriggerEvent::Completed, this, &ASRCameraPawn::HandleFocusSurfaceCompleted);
			EnhancedInputComponent->BindAction(FocusSurfaceAction, ETriggerEvent::Canceled, this, &ASRCameraPawn::HandleFocusSurfaceCompleted);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRCameraPawn requires FocusSurfaceAction before focus surface input binding."));
		}

		if (ResetFocusAction)
		{
			EnhancedInputComponent->BindAction(ResetFocusAction, ETriggerEvent::Started, this, &ASRCameraPawn::HandleResetFocus);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRCameraPawn requires ResetFocusAction before focus reset input binding."));
		}

		if (AlignFocusSurfaceGridAction)
		{
			EnhancedInputComponent->BindAction(AlignFocusSurfaceGridAction, ETriggerEvent::Started, this, &ASRCameraPawn::HandleAlignFocusSurfaceGrid);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRCameraPawn requires AlignFocusSurfaceGridAction before manual focus surface grid alignment binding."));
		}
	}
}

void ASRCameraPawn::ApplyMappingContext()
{
	if (bMappingContextApplied)
	{
		return;
	}
	if (!DefaultMappingContext)
	{
		UE_LOG(LogTemp, Error, TEXT("ASRCameraPawn requires DefaultMappingContext before applying input mapping."));
		return;
	}

	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return;
	}

	if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
		{
			InputSubsystem->AddMappingContext(DefaultMappingContext, 0);
			bMappingContextApplied = true;
		}
	}
}

void ASRCameraPawn::HandleDragHoldStarted()
{
	if (ASRPlayerController* PlayerController = Cast<ASRPlayerController>(GetController()))
	{
		if (PlayerController->ShouldHandleAssemblyPlacementDrag())
		{
			PlayerController->BeginAssemblyPlacementDrag();
			bIsDragging = false;
			bHasDragStartMousePosition = false;
			return;
		}

		if (PlayerController->ShouldBlockAssemblyCameraDrag())
		{
			bIsDragging = false;
			bHasDragStartMousePosition = false;
			return;
		}
	}

	StopFocusArcTransition();
	bIsDragging = true;
	bHasDragStartMousePosition = false;

	if (bIsDragging)
	{
		FocusTrackingDelta = FVector::ZeroVector;
		FocusTrackingDeltaVelocity = FVector::ZeroVector;
		DragStartFocusDragOffset = FocusDragOffset;
		DragStartTargetLocation = DragTargetLocation;
		bHasDragStartMousePosition = GetMouseScreenPosition(DragStartMouseScreenPosition);
	}
}

void ASRCameraPawn::HandleDragHoldCompleted()
{
	if (ASRPlayerController* PlayerController = Cast<ASRPlayerController>(GetController()))
	{
		PlayerController->EndAssemblyPlacementDrag();
	}

	bIsDragging = false;
	bHasDragStartMousePosition = false;
}

void ASRCameraPawn::HandleFocusSurfaceDragHoldStarted()
{
	if (ASRPlayerController* PlayerController = Cast<ASRPlayerController>(GetController()))
	{
		if (PlayerController->IsAssemblyModeActive())
		{
			bIsDraggingFocusSurface = false;
			return;
		}
	}

	StopFocusArcTransition();
	bIsDraggingFocusSurface = ShouldDragFocusedSurface();
	if (!bIsDraggingFocusSurface)
	{
		return;
	}

	bIsDragging = false;
	bHasDragStartMousePosition = false;
	FocusSurfaceTargetRotation = FocusSurfaceRotation.GetNormalized();
	FocusSurfaceRotationSmoothVelocity = FVector::ZeroVector;
	bIsResettingFocusSurfaceRotation = false;
	FocusSurfaceAngularVelocity = FVector2D::ZeroVector;
	bIsFocusSurfaceActive = true;
}

void ASRCameraPawn::HandleFocusSurfaceDragHoldCompleted()
{
	bIsDraggingFocusSurface = false;
}

void ASRCameraPawn::HandleDragDelta(const FInputActionValue& Value)
{
	const FVector2D DragDelta = Value.Get<FVector2D>();
	if (DragDelta.IsNearlyZero())
	{
		return;
	}

	if (bIsDraggingFocusSurface)
	{
		HandleFocusSurfaceDrag(DragDelta);
		return;
	}

	if (ASRPlayerController* PlayerController = Cast<ASRPlayerController>(GetController()))
	{
		if (PlayerController->ContinueAssemblyPlacementDrag())
		{
			bIsDragging = false;
			bHasDragStartMousePosition = false;
			return;
		}
	}

	if (!bIsDragging)
	{
		return;
	}

	FVector2D CurrentMouseScreenPosition = FVector2D::ZeroVector;
	if (GetMouseScreenPosition(CurrentMouseScreenPosition))
	{
		if (!bHasDragStartMousePosition)
		{
			DragStartFocusDragOffset = FocusDragOffset;
			DragStartTargetLocation = DragTargetLocation;
			DragStartMouseScreenPosition = CurrentMouseScreenPosition;
			bHasDragStartMousePosition = true;
			return;
		}

		const FVector DragOffsetDelta = ConvertScreenDragToDragOffset(DragStartMouseScreenPosition, CurrentMouseScreenPosition);

		if (FocusedActor)
		{
			FocusDragOffset = DragStartFocusDragOffset + DragOffsetDelta;
			DragTargetLocation = GetFocusLocation() + FocusDragOffset;
			DragTargetLocation = ClampPivotLocationInsideSpace(DragTargetLocation);
			FocusDragOffset = DragTargetLocation - GetFocusLocation();
			return;
		}

		DragTargetLocation = DragStartTargetLocation + DragOffsetDelta;
		DragTargetLocation = ClampPivotLocationInsideSpace(DragTargetLocation);
		return;
	}

	bHasDragStartMousePosition = false;
}

void ASRCameraPawn::HandleZoom(const FInputActionValue& Value)
{
	const float AxisValue = Value.Get<float>();
	if (FMath::IsNearlyZero(AxisValue))
	{
		return;
	}

	ZoomDistanceTarget = ClampZoomDistance(ZoomDistanceTarget - (AxisValue * GetZoomSpeed()));
}

void ASRCameraPawn::HandleFocusSurface(const FInputActionValue& Value)
{
	FocusSurfaceInput = Value.Get<FVector2D>();
	if (FocusSurfaceInput.IsNearlyZero())
	{
		bIsFocusSurfaceActive = false;
		return;
	}

	bIsFocusSurfaceActive = ShouldAllowFocusSurface();
}

void ASRCameraPawn::HandleFocusSurfaceCompleted()
{
	FocusSurfaceInput = FVector2D::ZeroVector;
	bIsFocusSurfaceActive = false;
}

void ASRCameraPawn::HandleResetFocus()
{
	ResetFocus();
}

void ASRCameraPawn::HandleAlignFocusSurfaceGrid()
{
	if (!ShouldAllowFocusSurface())
	{
		return;
	}

	const float CurrentZoomDistance = FMath::Max(1.0f, SpringArm ? SpringArm->TargetArmLength : ZoomDistanceTarget);
	const FQuat BaseViewQuat = GetViewRotationForZoom(CurrentZoomDistance).Quaternion();
	const FQuat ViewQuat = (BaseViewQuat * FocusSurfaceRotation.GetNormalized()).GetNormalized();

	float RollRadians = 0.0f;
	if (!TryComputeFocusSurfaceGridAlignmentRoll(ViewQuat, CurrentZoomDistance, RollRadians)
		|| FMath::IsNearlyZero(RollRadians))
	{
		return;
	}

	const FVector ViewForward = ViewQuat.RotateVector(FVector::ForwardVector).GetSafeNormal();
	if (ViewForward.IsNearlyZero())
	{
		return;
	}

	const FQuat AlignedViewQuat = (FQuat(ViewForward, RollRadians).GetNormalized() * ViewQuat).GetNormalized();
	FocusSurfaceTargetRotation = (BaseViewQuat.Inverse() * AlignedViewQuat).GetNormalized();
	FocusSurfaceRotationSmoothVelocity = FVector::ZeroVector;
	bIsResettingFocusSurfaceRotation = true;
	ClearFocusSurfaceMotion();
}

bool ASRCameraPawn::GetMouseScreenPosition(FVector2D& OutMouseScreenPosition) const
{
	OutMouseScreenPosition = FVector2D::ZeroVector;

	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
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

FVector ASRCameraPawn::ConvertScreenDragToDragOffset(const FVector2D& StartScreenPosition, const FVector2D& CurrentScreenPosition) const
{
	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
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

	const FVector PlaneOrigin = DragStartTargetLocation;
	const FVector PlaneNormal = Camera
		? Camera->GetForwardVector().GetSafeNormal()
		: SpringArm ? SpringArm->GetForwardVector().GetSafeNormal() : FVector::ForwardVector;
	if (PlaneNormal.SizeSquared() <= UE_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	const float DistanceToDragPlane = FVector::DotProduct(PlaneOrigin - Camera->GetComponentLocation(), PlaneNormal);
	if (DistanceToDragPlane <= UE_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	const float ReferenceZoomDistance = FMath::Max(1.0f, GetScreenSpaceThicknessReferenceZoomDistance());
	const float ReferenceFieldOfView = FMath::Clamp(GetScreenSpaceThicknessReferenceFieldOfView(), 5.0f, 170.0f);
	const float ReferenceTanHalfFieldOfView = FMath::Tan(FMath::DegreesToRadians(ReferenceFieldOfView * 0.5f));
	const float ReferenceWorldUnitsPerPixelVertical = (ReferenceZoomDistance * ReferenceTanHalfFieldOfView * 2.0f)
		/ static_cast<float>(ViewportHeight);
	const float AdaptiveScale = GetScreenSpaceInputScale(DistanceToDragPlane)
		* FMath::Max(0.0f, LeftDragInputScaleMultiplier);
	const float WorldUnitsPerPixelVertical = ReferenceWorldUnitsPerPixelVertical * AdaptiveScale;
	const float WorldUnitsPerPixelHorizontal = WorldUnitsPerPixelVertical;

	const FVector2D ScreenDelta = CurrentScreenPosition - StartScreenPosition;
	return (-Camera->GetRightVector() * (ScreenDelta.X * WorldUnitsPerPixelHorizontal))
		+ (Camera->GetUpVector() * (ScreenDelta.Y * WorldUnitsPerPixelVertical));
}
