#include "SRPlayerControllerAssemblyInputHandler.h"

#include "Assembly/SRAssemblyComponent.h"

void FSRPlayerControllerAssemblyInputHandler::HandleAreaDeletionDragStarted(
	FSRPlayerControllerRuntimeState& RuntimeState,
	bool bPointerOverBlockingUI,
	USRAssemblyComponent* AssemblyComponent)
{
	if (bPointerOverBlockingUI)
	{
		return;
	}

	if (AssemblyComponent && AssemblyComponent->IsAreaSelectionDragActive())
	{
		return;
	}

	RuntimeState.bAssemblyAreaDeletionDragHoldActive = true;
}

void FSRPlayerControllerAssemblyInputHandler::HandleAreaDeletionDragCompleted(
	FSRPlayerControllerRuntimeState& RuntimeState,
	USRAssemblyComponent* AssemblyComponent)
{
	RuntimeState.bAssemblyAreaDeletionDragHoldActive = false;
	if (AssemblyComponent)
	{
		AssemblyComponent->EndAreaDeletionDrag();
	}
}

void FSRPlayerControllerAssemblyInputHandler::HandleAreaSelectionDelete(
	bool bPointerOverBlockingUI,
	bool bAssemblyModeActive,
	USRAssemblyComponent* AssemblyComponent)
{
	if (bPointerOverBlockingUI || !bAssemblyModeActive || !AssemblyComponent)
	{
		return;
	}

	AssemblyComponent->TryDeleteAreaSelection();
}

void FSRPlayerControllerAssemblyInputHandler::HandleAreaSelectionCopy(
	bool bPointerOverBlockingUI,
	bool bAssemblyModeActive,
	bool bControlDown,
	USRAssemblyComponent* AssemblyComponent)
{
	if (bPointerOverBlockingUI || !bAssemblyModeActive || !AssemblyComponent)
	{
		return;
	}

	if (!bControlDown)
	{
		return;
	}

	AssemblyComponent->TryBeginAreaSelectionCopyPlacement();
}

void FSRPlayerControllerAssemblyInputHandler::HandleAreaCopyMirror(
	bool bPointerOverBlockingUI,
	bool bAssemblyModeActive,
	bool bAssemblyShiftModifierActive,
	USRAssemblyComponent* AssemblyComponent)
{
	if (bPointerOverBlockingUI || !bAssemblyModeActive || !AssemblyComponent)
	{
		return;
	}

	if (!AssemblyComponent->IsAreaCopyPlacementActive())
	{
		return;
	}

	AssemblyComponent->MirrorAreaCopyPlacement(bAssemblyShiftModifierActive);
}

void FSRPlayerControllerAssemblyInputHandler::HandlePickStructure(
	bool bPointerOverBlockingUI,
	bool bAssemblyModeActive,
	bool bControlDown,
	TFunctionRef<bool()> TrySelectBuildOptionFromHoveredCell)
{
	if (bPointerOverBlockingUI || !bAssemblyModeActive)
	{
		return;
	}

	if (bControlDown)
	{
		return;
	}

	TrySelectBuildOptionFromHoveredCell();
}

void FSRPlayerControllerAssemblyInputHandler::HandleUndoRedoAction(
	bool bAugmentChoiceVisible,
	bool bAssemblyModeActive,
	bool bControlDown,
	bool bAssemblyShiftModifierActive,
	USRAssemblyComponent* AssemblyComponent)
{
	if (bAugmentChoiceVisible || !bAssemblyModeActive || !AssemblyComponent)
	{
		return;
	}

	if (!bControlDown)
	{
		return;
	}

	if (bAssemblyShiftModifierActive)
	{
		AssemblyComponent->TryRedoAssemblyPlacement();
		return;
	}

	AssemblyComponent->TryUndoAssemblyPlacement();
}

void FSRPlayerControllerAssemblyInputHandler::HandleConveyorPlacementWaypoint(
	bool bPointerOverBlockingUI,
	bool bControlDown,
	USRAssemblyComponent* AssemblyComponent)
{
	if (bPointerOverBlockingUI)
	{
		return;
	}

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
