#include "Camera/SRPlayerController.h"

#include "Assembly/SRAssemblyComponent.h"
#include "Camera/SRCameraPawn.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"

namespace
{
	constexpr int32 RuntimeAssemblyInputMappingPriority = 1;
	constexpr TCHAR RuntimeAreaCopyMirrorActionName[] = TEXT("IA_RuntimeAssemblyAreaCopyMirrorAction");
	constexpr TCHAR RuntimePickStructureActionName[] = TEXT("IA_RuntimeAssemblyPickStructureAction");
	constexpr TCHAR RuntimeAssemblyInputMappingContextName[] = TEXT("IMC_RuntimeAssemblyInput");
}

void ASRPlayerController::EnsureAssemblyAreaCopyMirrorInputAction()
{
	if (AssemblyAreaCopyMirrorAction)
	{
		return;
	}

	AssemblyAreaCopyMirrorAction = NewObject<UInputAction>(this, RuntimeAreaCopyMirrorActionName);
	if (AssemblyAreaCopyMirrorAction)
	{
		AssemblyAreaCopyMirrorAction->ValueType = EInputActionValueType::Boolean;
		AssemblyAreaCopyMirrorAction->ActionDescription = NSLOCTEXT("StarRovers", "AssemblyAreaCopyMirrorActionDescription", "Mirror area copy placement");
	}
}

void ASRPlayerController::EnsureAssemblyPickStructureInputAction()
{
	if (AssemblyPickStructureAction)
	{
		AssemblyPickStructureAction->bConsumeInput = false;
		return;
	}

	AssemblyPickStructureAction = NewObject<UInputAction>(this, RuntimePickStructureActionName);
	if (AssemblyPickStructureAction)
	{
		AssemblyPickStructureAction->ValueType = EInputActionValueType::Boolean;
		AssemblyPickStructureAction->bConsumeInput = false;
		AssemblyPickStructureAction->ActionDescription = NSLOCTEXT("StarRovers", "AssemblyPickStructureActionDescription", "Pick hovered structure for construction");
	}
}

void ASRPlayerController::ApplyRuntimeAssemblyInputMapping()
{
	if (bRuntimeAssemblyInputMappingApplied)
	{
		return;
	}

	EnsureAssemblyAreaCopyMirrorInputAction();
	EnsureAssemblyPickStructureInputAction();

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);
	if (!InputSubsystem)
	{
		return;
	}

	if (!RuntimeAssemblyInputMappingContext)
	{
		RuntimeAssemblyInputMappingContext = NewObject<UInputMappingContext>(this, RuntimeAssemblyInputMappingContextName);
		if (AssemblyAreaCopyMirrorAction)
		{
			RuntimeAssemblyInputMappingContext->MapKey(AssemblyAreaCopyMirrorAction.Get(), EKeys::F);
		}
		if (AssemblyPickStructureAction)
		{
			RuntimeAssemblyInputMappingContext->MapKey(AssemblyPickStructureAction.Get(), EKeys::Z);
		}
	}

	InputSubsystem->AddMappingContext(RuntimeAssemblyInputMappingContext, RuntimeAssemblyInputMappingPriority);
	bRuntimeAssemblyInputMappingApplied = true;
}

void ASRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	EnsureAssemblyAreaCopyMirrorInputAction();
	EnsureAssemblyPickStructureInputAction();

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (LeftClickAction)
		{
			EnhancedInputComponent->BindAction(LeftClickAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleLeftClick);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ASRPlayerController requires LeftClickAction before input binding."));
		}

		if (FocusParentAction)
		{
			EnhancedInputComponent->BindAction(FocusParentAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleFocusParent);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ASRPlayerController requires FocusParentAction before input binding."));
		}

		if (AssemblyAreaDeletionDragHoldAction)
		{
			EnhancedInputComponent->BindAction(AssemblyAreaDeletionDragHoldAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleAssemblyAreaDeletionDragStarted);
			EnhancedInputComponent->BindAction(AssemblyAreaDeletionDragHoldAction, ETriggerEvent::Completed, this, &ASRPlayerController::HandleAssemblyAreaDeletionDragCompleted);
			EnhancedInputComponent->BindAction(AssemblyAreaDeletionDragHoldAction, ETriggerEvent::Canceled, this, &ASRPlayerController::HandleAssemblyAreaDeletionDragCompleted);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires AssemblyAreaDeletionDragHoldAction before assembly area deletion drag binding."));
		}

		if (AssemblyAreaSelectionDeleteAction)
		{
			EnhancedInputComponent->BindAction(AssemblyAreaSelectionDeleteAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleAssemblyAreaSelectionDelete);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires AssemblyAreaSelectionDeleteAction before assembly area selection delete binding."));
		}

		if (AssemblyAreaSelectionCopyAction)
		{
			EnhancedInputComponent->BindAction(AssemblyAreaSelectionCopyAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleAssemblyAreaSelectionCopy);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires AssemblyAreaSelectionCopyAction before assembly area selection copy binding."));
		}

		if (AssemblyAreaCopyMirrorAction)
		{
			EnhancedInputComponent->BindAction(AssemblyAreaCopyMirrorAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleAssemblyAreaCopyMirror);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires AssemblyAreaCopyMirrorAction before assembly area copy mirror binding."));
		}

		if (AssemblyPickStructureAction)
		{
			EnhancedInputComponent->BindAction(AssemblyPickStructureAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleAssemblyPickStructure);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires AssemblyPickStructureAction before assembly pick structure binding."));
		}

		if (DeleteStructureAction)
		{
			EnhancedInputComponent->BindAction(DeleteStructureAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleRightClick);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires DeleteStructureAction before right-click structure deletion binding."));
		}

		if (RotatePlacementCounterClockwiseAction)
		{
			EnhancedInputComponent->BindAction(RotatePlacementCounterClockwiseAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleRotatePlacementCounterClockwise);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires RotatePlacementCounterClockwiseAction before placement rotation binding."));
		}

		if (RotatePlacementClockwiseAction)
		{
			EnhancedInputComponent->BindAction(RotatePlacementClockwiseAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleRotatePlacementClockwise);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires RotatePlacementClockwiseAction before placement rotation binding."));
		}

		if (ConveyorWaypointAction)
		{
			EnhancedInputComponent->BindAction(ConveyorWaypointAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleConveyorPlacementWaypoint);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires ConveyorWaypointAction before conveyor waypoint binding."));
		}

		if (BulkDeleteConveyorModifierAction)
		{
			EnhancedInputComponent->BindAction(BulkDeleteConveyorModifierAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleBulkDeleteConveyorModifierStarted);
			EnhancedInputComponent->BindAction(BulkDeleteConveyorModifierAction, ETriggerEvent::Completed, this, &ASRPlayerController::HandleBulkDeleteConveyorModifierEnded);
			EnhancedInputComponent->BindAction(BulkDeleteConveyorModifierAction, ETriggerEvent::Canceled, this, &ASRPlayerController::HandleBulkDeleteConveyorModifierEnded);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires BulkDeleteConveyorModifierAction before conveyor bulk deletion modifier binding."));
		}

		if (AssemblyShiftModifierAction)
		{
			EnhancedInputComponent->BindAction(AssemblyShiftModifierAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleAssemblyShiftModifierStarted);
			EnhancedInputComponent->BindAction(AssemblyShiftModifierAction, ETriggerEvent::Completed, this, &ASRPlayerController::HandleAssemblyShiftModifierEnded);
			EnhancedInputComponent->BindAction(AssemblyShiftModifierAction, ETriggerEvent::Canceled, this, &ASRPlayerController::HandleAssemblyShiftModifierEnded);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires AssemblyShiftModifierAction before assembly shift modifier binding."));
		}

		if (AssemblyUndoRedoAction)
		{
			EnhancedInputComponent->BindAction(AssemblyUndoRedoAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleAssemblyUndoRedoAction);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires AssemblyUndoRedoAction before assembly undo/redo binding."));
		}
	}

	ApplyRuntimeAssemblyInputMapping();
}

void ASRPlayerController::HandleLeftClick()
{
	if (IsPointerOverBlockingUi())
	{
		return;
	}

	if (ShouldHandleAssemblyAreaSelectionDrag()
		&& (!AssemblyComponent || !AssemblyComponent->IsAreaCopyPlacementActive()))
	{
		return;
	}

	bPendingInitialPrimaryStarFocus = false;

	AActor* AssemblySelectedActor = nullptr;
	if (AssemblyComponent && AssemblyComponent->TryHandleAssemblyClick(AssemblySelectedActor))
	{
		if (IsValid(AssemblySelectedActor) && SelectedActor != AssemblySelectedActor)
		{
			UpdateSelection(AssemblySelectedActor);
		}
		return;
	}

	FHitResult CursorHitResult;
	const bool bHasCursorHit = GetHitResultUnderCursor(ECC_Visibility, false, CursorHitResult);

	AActor* HitActor = bHasCursorHit ? CursorHitResult.GetActor() : nullptr;
	AActor* SelectedBody = USRCelestialBodyRuntimeLibrary::IsCelestialBodyActor(HitActor) ? HitActor : nullptr;
	if (!SelectedBody)
	{
		if (const ASRCameraPawn* CameraPawn = Cast<ASRCameraPawn>(GetPawn()))
		{
			AActor* CurrentFocusActor = CameraPawn->GetFocusedActor();
			if (IsValid(CurrentFocusActor)
				&& USRCelestialBodyRuntimeLibrary::IsCelestialBodyActor(CurrentFocusActor)
				&& !USRCelestialBodyRuntimeLibrary::IsCelestialStarActor(CurrentFocusActor))
			{
				return;
			}
		}
	}

	RequestFocusActor(SelectedBody);
}

void ASRPlayerController::HandleRightClick()
{
	if (IsPointerOverBlockingUi())
	{
		return;
	}

	if ((AssemblyComponent && AssemblyComponent->IsAreaSelectionDragActive())
		|| bAssemblyAreaDeletionDragHoldActive
		|| (AssemblyComponent && AssemblyComponent->IsAreaDeletionDragActive()))
	{
		return;
	}

	if (AssemblyComponent && AssemblyComponent->IsAreaCopyPlacementActive())
	{
		AActor* AssemblySelectedActor = nullptr;
		if (AssemblyComponent->TryHandleAssemblyDelete(AssemblySelectedActor)
			&& IsValid(AssemblySelectedActor)
			&& SelectedActor != AssemblySelectedActor)
		{
			UpdateSelection(AssemblySelectedActor);
		}
		return;
	}

	if (ClearSelectedStructureBuildOption())
	{
		return;
	}

	AActor* AssemblySelectedActor = nullptr;
	if (AssemblyComponent && AssemblyComponent->TryHandleAssemblyDelete(AssemblySelectedActor))
	{
		if (IsValid(AssemblySelectedActor) && SelectedActor != AssemblySelectedActor)
		{
			UpdateSelection(AssemblySelectedActor);
		}
	}
}

void ASRPlayerController::HandleAssemblyAreaDeletionDragStarted()
{
	if (IsPointerOverBlockingUi())
	{
		return;
	}

	if (AssemblyComponent && AssemblyComponent->IsAreaSelectionDragActive())
	{
		return;
	}

	bAssemblyAreaDeletionDragHoldActive = true;
}

void ASRPlayerController::HandleAssemblyAreaDeletionDragCompleted()
{
	bAssemblyAreaDeletionDragHoldActive = false;
	EndAssemblyAreaDeletionDrag();
}

void ASRPlayerController::HandleAssemblyAreaSelectionDelete()
{
	if (IsPointerOverBlockingUi() || !IsAssemblyModeActive() || !AssemblyComponent)
	{
		return;
	}

	AssemblyComponent->TryDeleteAreaSelection();
}

void ASRPlayerController::HandleAssemblyAreaSelectionCopy()
{
	if (IsPointerOverBlockingUi() || !IsAssemblyModeActive() || !AssemblyComponent)
	{
		return;
	}

	const bool bControlDown = IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
	if (!bControlDown)
	{
		return;
	}

	AssemblyComponent->TryBeginAreaSelectionCopyPlacement();
}

void ASRPlayerController::HandleAssemblyAreaCopyMirror()
{
	if (IsPointerOverBlockingUi() || !IsAssemblyModeActive() || !AssemblyComponent)
	{
		return;
	}

	if (!AssemblyComponent->IsAreaCopyPlacementActive())
	{
		return;
	}

	AssemblyComponent->MirrorAreaCopyPlacement(IsAssemblyShiftModifierActive());
}

void ASRPlayerController::HandleAssemblyPickStructure()
{
	if (IsPointerOverBlockingUi() || !IsAssemblyModeActive())
	{
		return;
	}

	const bool bControlDown = IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
	if (bControlDown)
	{
		return;
	}

	TrySelectBuildOptionFromHoveredCell();
}

void ASRPlayerController::HandleAssemblyUndoRedoAction()
{
	if (!IsAssemblyModeActive() || !AssemblyComponent)
	{
		return;
	}

	const bool bControlDown = IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
	if (!bControlDown)
	{
		return;
	}

	if (IsAssemblyShiftModifierActive())
	{
		AssemblyComponent->TryRedoAssemblyPlacement();
		return;
	}

	AssemblyComponent->TryUndoAssemblyPlacement();
}

void ASRPlayerController::HandleFocusParent()
{
	if (IsAssemblyModeActive())
	{
		return;
	}

	bPendingInitialPrimaryStarFocus = false;

	ASRCameraPawn* CameraPawn = Cast<ASRCameraPawn>(GetPawn());
	if (!CameraPawn)
	{
		return;
	}

	AActor* CurrentFocusActor = CameraPawn->GetFocusedActor();
	if (!IsValid(CurrentFocusActor))
	{
		return;
	}

	AActor* ParentBody = nullptr;
	if (!USRCelestialBodyRuntimeLibrary::GetCelestialParentBody(CurrentFocusActor, ParentBody) || !IsValid(ParentBody))
	{
		return;
	}

	if (AssemblyComponent)
	{
		AssemblyComponent->ClearSurfaceGridInteraction(CurrentFocusActor);
	}
	SetAssemblyModeActive(false);
	RequestFocusActor(ParentBody);
}

void ASRPlayerController::HandleConveyorPlacementWaypoint()
{
	if (IsPointerOverBlockingUi())
	{
		return;
	}

	const bool bControlDown = IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
	if (bControlDown)
	{
		if (AssemblyComponent)
		{
			AssemblyComponent->TryBeginAreaSelectionCopyPlacement();
		}
		return;
	}

	if (AssemblyComponent)
	{
		AssemblyComponent->TryAddConveyorPlacementDragWaypoint();
	}
}

void ASRPlayerController::HandleRotatePlacementCounterClockwise()
{
	TryHandlePlacementRotationInput(-1);
}

void ASRPlayerController::HandleRotatePlacementClockwise()
{
	TryHandlePlacementRotationInput(1);
}

void ASRPlayerController::HandleBulkDeleteConveyorModifierStarted()
{
	bConveyorBulkDeleteModifierActive = true;
}

void ASRPlayerController::HandleBulkDeleteConveyorModifierEnded()
{
	bConveyorBulkDeleteModifierActive = false;
}

bool ASRPlayerController::IsConveyorBulkDeleteModifierActive() const
{
	return bConveyorBulkDeleteModifierActive;
}

void ASRPlayerController::HandleAssemblyShiftModifierStarted()
{
	bAssemblyShiftModifierActive = true;
}

void ASRPlayerController::HandleAssemblyShiftModifierEnded()
{
	bAssemblyShiftModifierActive = false;
}

bool ASRPlayerController::IsAssemblyShiftModifierActive() const
{
	return bAssemblyShiftModifierActive;
}

bool ASRPlayerController::TryHandlePlacementRotationInput(int32 StepDelta)
{
	const uint64 CurrentFrame = GFrameCounter;
	if (LastPlacementRotationInputFrame == CurrentFrame && LastPlacementRotationInputStepDelta == StepDelta)
	{
		return true;
	}

	if (!RotateStructurePlacement(StepDelta))
	{
		return false;
	}

	LastPlacementRotationInputFrame = CurrentFrame;
	LastPlacementRotationInputStepDelta = StepDelta;
	return true;
}
