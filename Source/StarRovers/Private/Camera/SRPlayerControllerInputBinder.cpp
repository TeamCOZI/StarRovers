#include "SRPlayerControllerInputBinder.h"

#include "Camera/SRPlayerController.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"

void FSRPlayerControllerInputBinder::BindInputActions(
	ASRPlayerController& PlayerController,
	UInputComponent* InputComponent)
{
	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInputComponent)
	{
		return;
	}

	if (PlayerController.LeftClickAction)
	{
		EnhancedInputComponent->BindAction(PlayerController.LeftClickAction, ETriggerEvent::Started, &PlayerController, &ASRPlayerController::HandleLeftClick);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ASRPlayerController requires LeftClickAction before input binding."));
	}

	if (PlayerController.FocusParentAction)
	{
		EnhancedInputComponent->BindAction(PlayerController.FocusParentAction, ETriggerEvent::Started, &PlayerController, &ASRPlayerController::HandleFocusParent);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ASRPlayerController requires FocusParentAction before input binding."));
	}

	if (PlayerController.AssemblyAreaDeletionDragHoldAction)
	{
		EnhancedInputComponent->BindAction(PlayerController.AssemblyAreaDeletionDragHoldAction, ETriggerEvent::Started, &PlayerController, &ASRPlayerController::HandleAssemblyAreaDeletionDragStarted);
		EnhancedInputComponent->BindAction(PlayerController.AssemblyAreaDeletionDragHoldAction, ETriggerEvent::Completed, &PlayerController, &ASRPlayerController::HandleAssemblyAreaDeletionDragCompleted);
		EnhancedInputComponent->BindAction(PlayerController.AssemblyAreaDeletionDragHoldAction, ETriggerEvent::Canceled, &PlayerController, &ASRPlayerController::HandleAssemblyAreaDeletionDragCompleted);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires AssemblyAreaDeletionDragHoldAction before assembly area deletion drag binding."));
	}

	if (PlayerController.AssemblyAreaSelectionDeleteAction)
	{
		EnhancedInputComponent->BindAction(PlayerController.AssemblyAreaSelectionDeleteAction, ETriggerEvent::Started, &PlayerController, &ASRPlayerController::HandleAssemblyAreaSelectionDelete);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires AssemblyAreaSelectionDeleteAction before assembly area selection delete binding."));
	}

	if (PlayerController.AssemblyAreaSelectionCopyAction)
	{
		EnhancedInputComponent->BindAction(PlayerController.AssemblyAreaSelectionCopyAction, ETriggerEvent::Started, &PlayerController, &ASRPlayerController::HandleAssemblyAreaSelectionCopy);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires AssemblyAreaSelectionCopyAction before assembly area selection copy binding."));
	}

	if (PlayerController.AssemblyAreaCopyMirrorAction)
	{
		EnhancedInputComponent->BindAction(PlayerController.AssemblyAreaCopyMirrorAction, ETriggerEvent::Started, &PlayerController, &ASRPlayerController::HandleAssemblyAreaCopyMirror);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires AssemblyAreaCopyMirrorAction before assembly area copy mirror binding."));
	}

	if (PlayerController.AssemblyPickStructureAction)
	{
		EnhancedInputComponent->BindAction(PlayerController.AssemblyPickStructureAction, ETriggerEvent::Started, &PlayerController, &ASRPlayerController::HandleAssemblyPickStructure);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires AssemblyPickStructureAction before assembly pick structure binding."));
	}

	if (PlayerController.DeleteStructureAction)
	{
		EnhancedInputComponent->BindAction(PlayerController.DeleteStructureAction, ETriggerEvent::Started, &PlayerController, &ASRPlayerController::HandleRightClick);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires DeleteStructureAction before right-click structure deletion binding."));
	}

	if (PlayerController.RotatePlacementCounterClockwiseAction)
	{
		EnhancedInputComponent->BindAction(PlayerController.RotatePlacementCounterClockwiseAction, ETriggerEvent::Started, &PlayerController, &ASRPlayerController::HandleRotatePlacementCounterClockwise);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires RotatePlacementCounterClockwiseAction before surface view rotation binding."));
	}

	if (PlayerController.RotatePlacementClockwiseAction)
	{
		EnhancedInputComponent->BindAction(PlayerController.RotatePlacementClockwiseAction, ETriggerEvent::Started, &PlayerController, &ASRPlayerController::HandleRotatePlacementClockwise);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires RotatePlacementClockwiseAction before surface view rotation binding."));
	}

	if (PlayerController.RotateAssemblyPlacementAction)
	{
		EnhancedInputComponent->BindAction(PlayerController.RotateAssemblyPlacementAction, ETriggerEvent::Started, &PlayerController, &ASRPlayerController::HandleRotateAssemblyPlacement);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires RotateAssemblyPlacementAction before assembly placement rotation binding."));
	}

	if (PlayerController.ConveyorWaypointAction)
	{
		EnhancedInputComponent->BindAction(PlayerController.ConveyorWaypointAction, ETriggerEvent::Started, &PlayerController, &ASRPlayerController::HandleConveyorPlacementWaypoint);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires ConveyorWaypointAction before conveyor waypoint binding."));
	}

	if (PlayerController.BulkDeleteConveyorModifierAction)
	{
		EnhancedInputComponent->BindAction(PlayerController.BulkDeleteConveyorModifierAction, ETriggerEvent::Started, &PlayerController, &ASRPlayerController::HandleBulkDeleteConveyorModifierStarted);
		EnhancedInputComponent->BindAction(PlayerController.BulkDeleteConveyorModifierAction, ETriggerEvent::Completed, &PlayerController, &ASRPlayerController::HandleBulkDeleteConveyorModifierEnded);
		EnhancedInputComponent->BindAction(PlayerController.BulkDeleteConveyorModifierAction, ETriggerEvent::Canceled, &PlayerController, &ASRPlayerController::HandleBulkDeleteConveyorModifierEnded);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires BulkDeleteConveyorModifierAction before conveyor bulk deletion modifier binding."));
	}

	if (PlayerController.AssemblyShiftModifierAction)
	{
		EnhancedInputComponent->BindAction(PlayerController.AssemblyShiftModifierAction, ETriggerEvent::Started, &PlayerController, &ASRPlayerController::HandleAssemblyShiftModifierStarted);
		EnhancedInputComponent->BindAction(PlayerController.AssemblyShiftModifierAction, ETriggerEvent::Completed, &PlayerController, &ASRPlayerController::HandleAssemblyShiftModifierEnded);
		EnhancedInputComponent->BindAction(PlayerController.AssemblyShiftModifierAction, ETriggerEvent::Canceled, &PlayerController, &ASRPlayerController::HandleAssemblyShiftModifierEnded);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires AssemblyShiftModifierAction before assembly shift modifier binding."));
	}

	if (PlayerController.AssemblyUndoRedoAction)
	{
		EnhancedInputComponent->BindAction(PlayerController.AssemblyUndoRedoAction, ETriggerEvent::Started, &PlayerController, &ASRPlayerController::HandleAssemblyUndoRedoAction);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires AssemblyUndoRedoAction before assembly undo/redo binding."));
	}

	if (PlayerController.StructureSelectionTabAction)
	{
		EnhancedInputComponent->BindAction(PlayerController.StructureSelectionTabAction, ETriggerEvent::Started, &PlayerController, &ASRPlayerController::HandleStructureSelectionTab);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ASRPlayerController requires StructureSelectionTabAction before structure selection tab binding."));
	}
}
