#include "Camera/SRCameraPawn.h"

#include "Utility/SRLog.h"
#include "SRCameraFocusSurfaceRigAlignmentController.h"
#include "SRCameraInputInteractionGate.h"
#include "SRCameraScreenDragResolver.h"
#include "Camera/CameraComponent.h"
#include "Camera/SRPlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "GameFramework/SpringArmComponent.h"

namespace
{
	constexpr int32 RuntimeCameraInputMappingPriority = 0;
	constexpr TCHAR RuntimeCameraInputMappingContextName[] = TEXT("IMC_RuntimeCameraInput");
	constexpr TCHAR RuntimeDragHoldActionName[] = TEXT("IA_RuntimeCameraDragHoldAction");
	constexpr TCHAR RuntimeFocusSurfaceDragHoldActionName[] = TEXT("IA_RuntimeFocusSurfaceDragHoldAction");
	constexpr TCHAR RuntimeDragDeltaActionName[] = TEXT("IA_RuntimeCameraDragDeltaAction");
	constexpr TCHAR RuntimeZoomActionName[] = TEXT("IA_RuntimeCameraZoomAction");

	bool EnsureRuntimeInputAction(
		UObject* Owner,
		TObjectPtr<UInputAction>& Action,
		const TCHAR* ActionName,
		EInputActionValueType ValueType,
		const FText& Description)
	{
		if (Action)
		{
			return false;
		}

		Action = NewObject<UInputAction>(Owner, ActionName);
		if (Action)
		{
			Action->ValueType = ValueType;
			Action->bConsumeInput = false;
			Action->ActionDescription = Description;
		}
		return Action != nullptr;
	}

	UInputAction* FindMappedInputActionByKey(const UInputMappingContext* MappingContext, const FKey& Key)
	{
		if (!MappingContext)
		{
			return nullptr;
		}

		for (const FEnhancedActionKeyMapping& Mapping : MappingContext->GetMappings())
		{
			if (Mapping.Key == Key && Mapping.Action)
			{
				return const_cast<UInputAction*>(Mapping.Action.Get());
			}
		}

		return nullptr;
	}

	bool IsInputActionMappedToKey(const UInputMappingContext* MappingContext, const UInputAction* Action, const FKey& Key)
	{
		if (!MappingContext || !Action)
		{
			return false;
		}

		for (const FEnhancedActionKeyMapping& Mapping : MappingContext->GetMappings())
		{
			if (Mapping.Key == Key && Mapping.Action == Action)
			{
				return true;
			}
		}

		return false;
	}

	bool ResolveMappedInputActionByKey(
		const UInputMappingContext* MappingContext,
		const FKey& Key,
		TObjectPtr<UInputAction>& Action)
	{
		if (IsInputActionMappedToKey(MappingContext, Action.Get(), Key))
		{
			return false;
		}

		if (UInputAction* MappedAction = FindMappedInputActionByKey(MappingContext, Key))
		{
			Action = MappedAction;
			return true;
		}

		return false;
	}
}

void ASRCameraPawn::EnsureRuntimeCameraInputActions()
{
	ResolveMappedInputActionByKey(DefaultMappingContext, EKeys::LeftMouseButton, DragHoldAction);
	ResolveMappedInputActionByKey(DefaultMappingContext, EKeys::RightMouseButton, FocusSurfaceDragHoldAction);
	ResolveMappedInputActionByKey(DefaultMappingContext, EKeys::Mouse2D, DragDeltaAction);
	ResolveMappedInputActionByKey(DefaultMappingContext, EKeys::MouseWheelAxis, ZoomAction);

	const bool bMapAllCameraActions = !DefaultMappingContext;
	const bool bCreatedDragHoldAction = bMapAllCameraActions && EnsureRuntimeInputAction(
		this,
		DragHoldAction,
		RuntimeDragHoldActionName,
		EInputActionValueType::Boolean,
		NSLOCTEXT("StarRovers", "RuntimeCameraDragHoldActionDescription", "Hold left mouse button for camera and assembly drag"));
	const bool bCreatedFocusSurfaceDragHoldAction = bMapAllCameraActions && EnsureRuntimeInputAction(
		this,
		FocusSurfaceDragHoldAction,
		RuntimeFocusSurfaceDragHoldActionName,
		EInputActionValueType::Boolean,
		NSLOCTEXT("StarRovers", "RuntimeFocusSurfaceDragHoldActionDescription", "Hold right mouse button for focused surface drag"));
	const bool bCreatedDragDeltaAction = bMapAllCameraActions && EnsureRuntimeInputAction(
		this,
		DragDeltaAction,
		RuntimeDragDeltaActionName,
		EInputActionValueType::Axis2D,
		NSLOCTEXT("StarRovers", "RuntimeCameraDragDeltaActionDescription", "Mouse drag delta"));
	const bool bCreatedZoomAction = bMapAllCameraActions && EnsureRuntimeInputAction(
		this,
		ZoomAction,
		RuntimeZoomActionName,
		EInputActionValueType::Axis1D,
		NSLOCTEXT("StarRovers", "RuntimeCameraZoomActionDescription", "Mouse wheel zoom"));

	const bool bNeedsRuntimeMappingContext = bMapAllCameraActions
		|| bCreatedDragHoldAction
		|| bCreatedFocusSurfaceDragHoldAction
		|| bCreatedDragDeltaAction
		|| bCreatedZoomAction;
	if (!bNeedsRuntimeMappingContext || RuntimeCameraInputMappingContext)
	{
		return;
	}

	RuntimeCameraInputMappingContext = NewObject<UInputMappingContext>(this, RuntimeCameraInputMappingContextName);
	if (!RuntimeCameraInputMappingContext)
	{
		return;
	}

	if (DragHoldAction && (bMapAllCameraActions || bCreatedDragHoldAction))
	{
		RuntimeCameraInputMappingContext->MapKey(DragHoldAction.Get(), EKeys::LeftMouseButton);
	}
	if (FocusSurfaceDragHoldAction && (bMapAllCameraActions || bCreatedFocusSurfaceDragHoldAction))
	{
		RuntimeCameraInputMappingContext->MapKey(FocusSurfaceDragHoldAction.Get(), EKeys::RightMouseButton);
	}
	if (DragDeltaAction && (bMapAllCameraActions || bCreatedDragDeltaAction))
	{
		RuntimeCameraInputMappingContext->MapKey(DragDeltaAction.Get(), EKeys::Mouse2D);
	}
	if (ZoomAction && (bMapAllCameraActions || bCreatedZoomAction))
	{
		RuntimeCameraInputMappingContext->MapKey(ZoomAction.Get(), EKeys::MouseWheelAxis);
	}
}

void ASRCameraPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	EnsureRuntimeCameraInputActions();

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
			SR_LOG(Camera, LogTemp, Error, TEXT("ASRCameraPawn requires DragHoldAction before input binding."));
		}

		if (FocusSurfaceDragHoldAction)
		{
			EnhancedInputComponent->BindAction(FocusSurfaceDragHoldAction, ETriggerEvent::Started, this, &ASRCameraPawn::HandleFocusSurfaceDragHoldStarted);
			EnhancedInputComponent->BindAction(FocusSurfaceDragHoldAction, ETriggerEvent::Completed, this, &ASRCameraPawn::HandleFocusSurfaceDragHoldCompleted);
			EnhancedInputComponent->BindAction(FocusSurfaceDragHoldAction, ETriggerEvent::Canceled, this, &ASRCameraPawn::HandleFocusSurfaceDragHoldCompleted);
		}
		else
		{
			SR_LOG(Camera, LogTemp, Warning, TEXT("ASRCameraPawn requires FocusSurfaceDragHoldAction before left-click focus surface drag binding."));
		}

		if (DragDeltaAction)
		{
			EnhancedInputComponent->BindAction(DragDeltaAction, ETriggerEvent::Triggered, this, &ASRCameraPawn::HandleDragDelta);
		}
		else
		{
			SR_LOG(Camera, LogTemp, Error, TEXT("ASRCameraPawn requires DragDeltaAction before input binding."));
		}

		if (ZoomAction)
		{
			EnhancedInputComponent->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &ASRCameraPawn::HandleZoom);
		}
		else
		{
			SR_LOG(Camera, LogTemp, Error, TEXT("ASRCameraPawn requires ZoomAction before input binding."));
		}

		if (FocusSurfaceAction)
		{
			EnhancedInputComponent->BindAction(FocusSurfaceAction, ETriggerEvent::Triggered, this, &ASRCameraPawn::HandleFocusSurface);
			EnhancedInputComponent->BindAction(FocusSurfaceAction, ETriggerEvent::Completed, this, &ASRCameraPawn::HandleFocusSurfaceCompleted);
			EnhancedInputComponent->BindAction(FocusSurfaceAction, ETriggerEvent::Canceled, this, &ASRCameraPawn::HandleFocusSurfaceCompleted);
		}
		else
		{
			SR_LOG(Camera, LogTemp, Warning, TEXT("ASRCameraPawn requires FocusSurfaceAction before focus surface input binding."));
		}

		if (ResetFocusAction)
		{
			EnhancedInputComponent->BindAction(ResetFocusAction, ETriggerEvent::Started, this, &ASRCameraPawn::HandleResetFocus);
		}
		else
		{
			SR_LOG(Camera, LogTemp, Warning, TEXT("ASRCameraPawn requires ResetFocusAction before focus reset input binding."));
		}
	}
}

void ASRCameraPawn::ApplyMappingContext()
{
	if (bMappingContextApplied)
	{
		return;
	}

	EnsureRuntimeCameraInputActions();

	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return;
	}

	if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
		{
			bool bAddedMappingContext = false;
			if (DefaultMappingContext)
			{
				InputSubsystem->AddMappingContext(DefaultMappingContext, 0);
				bAddedMappingContext = true;
			}
			if (RuntimeCameraInputMappingContext)
			{
				InputSubsystem->AddMappingContext(RuntimeCameraInputMappingContext, RuntimeCameraInputMappingPriority);
				bAddedMappingContext = true;
			}
			if (!bAddedMappingContext)
			{
				SR_LOG(Camera, LogTemp, Error, TEXT("ASRCameraPawn requires a DefaultMappingContext or runtime camera input mapping before applying input mapping."));
				return;
			}

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
