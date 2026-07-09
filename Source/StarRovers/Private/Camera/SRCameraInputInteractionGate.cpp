#include "SRCameraInputInteractionGate.h"

#include "Camera/SRPlayerController.h"

bool FSRCameraInputInteractionGate::TryConsumeDragHoldStart(ASRPlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return false;
	}

	if (PlayerController->IsPointerOverBlockingUI())
	{
		return true;
	}

	if (PlayerController->ShouldHandleAssemblyAreaSelectionDrag())
	{
		PlayerController->BeginAssemblyAreaSelectionDrag();
		return true;
	}

	if (PlayerController->ShouldHandleAssemblyPlacementDrag())
	{
		PlayerController->BeginAssemblyPlacementDrag();
		return true;
	}

	return PlayerController->ShouldBlockAssemblyCameraDrag();
}

void FSRCameraInputInteractionGate::CompleteDragHold(ASRPlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return;
	}

	PlayerController->EndAssemblyPlacementDrag();
	PlayerController->EndAssemblyAreaSelectionDrag();
}

bool FSRCameraInputInteractionGate::ShouldBlockFocusSurfaceDragHoldStart(const ASRPlayerController* PlayerController)
{
	return PlayerController
		&& (PlayerController->IsPointerOverBlockingUI() || PlayerController->IsAssemblyModeActive());
}

void FSRCameraInputInteractionGate::CompleteFocusSurfaceDragHold(ASRPlayerController* PlayerController)
{
	if (PlayerController)
	{
		PlayerController->EndAssemblyAreaDeletionDrag();
	}
}

bool FSRCameraInputInteractionGate::TryConsumeDragDelta(ASRPlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return false;
	}

	if (PlayerController->ContinueAssemblyAreaSelectionDrag())
	{
		return true;
	}

	if (PlayerController->ContinueAssemblyAreaDeletionDrag())
	{
		return true;
	}

	return PlayerController->ContinueAssemblyPlacementDrag();
}

bool FSRCameraInputInteractionGate::ShouldBlockZoom(const ASRPlayerController* PlayerController)
{
	return PlayerController && PlayerController->IsPointerOverBlockingUI();
}
