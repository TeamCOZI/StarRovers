#include "Camera/SRCameraPawn.h"

#include "SRCameraFocusSurfaceRigAlignmentController.h"
#include "SRCameraInputInteractionGate.h"
#include "SRCameraScreenDragResolver.h"
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
	if (FSRCameraInputInteractionGate::TryConsumeDragHoldStart(Cast<ASRPlayerController>(GetController())))
	{
		bIsDragging = false;
		bHasDragStartMousePosition = false;
		return;
	}

	FocusArcTransition.Reset();
	bIsDragging = true;
	bHasDragStartMousePosition = false;

	if (bIsDragging)
	{
		FocusTrackingDelta = FVector::ZeroVector;
		FocusTrackingDeltaVelocity = FVector::ZeroVector;
		DragStartFocusDragOffset = FocusDragOffset;
		DragStartTargetLocation = DragTargetLocation;
		bHasDragStartMousePosition = FSRCameraScreenDragResolver::GetMouseScreenPosition(
			Cast<APlayerController>(GetController()),
			DragStartMouseScreenPosition);
	}
}

void ASRCameraPawn::HandleDragHoldCompleted()
{
	FSRCameraInputInteractionGate::CompleteDragHold(Cast<ASRPlayerController>(GetController()));

	bIsDragging = false;
	bHasDragStartMousePosition = false;
}

void ASRCameraPawn::HandleFocusSurfaceDragHoldStarted()
{
	if (FSRCameraInputInteractionGate::ShouldBlockFocusSurfaceDragHoldStart(Cast<ASRPlayerController>(GetController())))
	{
		bIsDraggingFocusSurface = false;
		return;
	}

	FocusArcTransition.Reset();
	bIsDraggingFocusSurface = ShouldDragFocusedSurface();
	if (!bIsDraggingFocusSurface)
	{
		return;
	}

	bIsDragging = false;
	bHasDragStartMousePosition = false;
	FSRCameraFocusSurfaceRigAlignmentController::StopRotationResetForDrag(FocusSurface);
	bIsFocusSurfaceActive = true;
}

void ASRCameraPawn::HandleFocusSurfaceDragHoldCompleted()
{
	FSRCameraInputInteractionGate::CompleteFocusSurfaceDragHold(Cast<ASRPlayerController>(GetController()));

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

	if (FSRCameraInputInteractionGate::TryConsumeDragDelta(Cast<ASRPlayerController>(GetController())))
	{
		bIsDragging = false;
		bHasDragStartMousePosition = false;
		return;
	}

	if (!bIsDragging)
	{
		return;
	}

	FVector2D CurrentMouseScreenPosition = FVector2D::ZeroVector;
	if (FSRCameraScreenDragResolver::GetMouseScreenPosition(Cast<APlayerController>(GetController()), CurrentMouseScreenPosition))
	{
		if (!bHasDragStartMousePosition)
		{
			DragStartFocusDragOffset = FocusDragOffset;
			DragStartTargetLocation = DragTargetLocation;
			DragStartMouseScreenPosition = CurrentMouseScreenPosition;
			bHasDragStartMousePosition = true;
			return;
		}

		const FVector DragOffsetDelta = FSRCameraScreenDragResolver::ConvertScreenDragToDragOffset(
			Cast<APlayerController>(GetController()),
			Camera,
			DragStartTargetLocation,
			GetScreenSpaceThicknessReferenceZoomDistance(),
			GetScreenSpaceThicknessReferenceFieldOfView(),
			LeftDragInputScaleMultiplier,
			DragStartMouseScreenPosition,
			CurrentMouseScreenPosition);

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

	if (FSRCameraInputInteractionGate::ShouldBlockZoom(Cast<ASRPlayerController>(GetController())))
	{
		return;
	}

	ZoomDistanceTarget = ClampZoomDistance(ZoomDistanceTarget - (AxisValue * GetZoomSpeed()));
}

void ASRCameraPawn::HandleFocusSurface(const FInputActionValue& Value)
{
	FocusSurface.Input = Value.Get<FVector2D>();
	if (FocusSurface.Input.IsNearlyZero())
	{
		bIsFocusSurfaceActive = false;
		return;
	}

	bIsFocusSurfaceActive = ShouldAllowFocusSurface();
}

void ASRCameraPawn::HandleFocusSurfaceCompleted()
{
	FocusSurface.Input = FVector2D::ZeroVector;
	bIsFocusSurfaceActive = false;
}

void ASRCameraPawn::HandleResetFocus()
{
	ResetFocus();
}

bool ASRCameraPawn::TryStartFocusSurfaceGridAlignment()
{
	if (!ShouldAllowFocusSurface())
	{
		return false;
	}

	const float CurrentZoomDistance = FMath::Max(1.0f, SpringArm ? SpringArm->TargetArmLength : ZoomDistanceTarget);
	const FQuat BaseViewQuat = GetViewRotationForZoom(CurrentZoomDistance).Quaternion();
	const FQuat ViewQuat = (FocusSurface.Rotation.GetNormalized() * BaseViewQuat).GetNormalized();

	ApplyZoomDrivenViewRotation(CurrentZoomDistance);
	UpdateComponentTransforms();
	if (SpringArm)
	{
		SpringArm->UpdateComponentToWorld();
	}
	if (Camera)
	{
		Camera->UpdateComponentToWorld();
	}

	FVector AlignmentAxis = FVector::ZeroVector;
	float AlignmentAngleRadians = 0.0f;
	if (!TryComputeFocusSurfaceGridAlignmentDelta(ViewQuat, CurrentZoomDistance, AlignmentAxis, AlignmentAngleRadians)
		|| FMath::IsNearlyZero(AlignmentAngleRadians))
	{
		return false;
	}

	AlignmentAxis = AlignmentAxis.GetSafeNormal();
	if (AlignmentAxis.IsNearlyZero())
	{
		return false;
	}

	if (!FSRCameraFocusSurfaceRigAlignmentController::StartRigAlignment(
		FocusSurface,
		GetActorLocation() - GetFocusLocation(),
		AlignmentAxis,
		AlignmentAngleRadians))
	{
		return false;
	}

	ClearFocusSurfaceMotion();
	return true;
}
