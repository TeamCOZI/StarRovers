#include "SRPlayerControllerRotationInputHandler.h"

#include "Assembly/SRAssemblyComponent.h"
#include "Camera/SRCameraPawn.h"

bool FSRPlayerControllerRotationInputHandler::TryHandlePlacementRotationInput(
	FSRPlayerControllerRuntimeState& RuntimeState,
	bool bAssemblyModeActive,
	bool bPointerOverBlockingUI,
	USRAssemblyComponent* AssemblyComponent,
	bool bHasSelectedStructureDataAsset,
	int32 StepDelta)
{
	const uint64 CurrentFrame = GFrameCounter;
	if (RuntimeState.IsDuplicatePlacementRotationInput(CurrentFrame, StepDelta))
	{
		return true;
	}

	if (!bAssemblyModeActive || bPointerOverBlockingUI)
	{
		return false;
	}

	if (AssemblyComponent && AssemblyComponent->IsAreaCopyPlacementActive())
	{
		if (!AssemblyComponent->RotateAreaCopyPlacement(StepDelta))
		{
			return false;
		}
	}
	else if (bHasSelectedStructureDataAsset)
	{
		if (!AssemblyComponent || !AssemblyComponent->RotateStructurePlacement(StepDelta))
		{
			return false;
		}
	}
	else
	{
		return false;
	}

	RuntimeState.StorePlacementRotationInput(CurrentFrame, StepDelta);
	return true;
}

bool FSRPlayerControllerRotationInputHandler::TryHandleSurfaceViewRotationInput(
	FSRPlayerControllerRuntimeState& RuntimeState,
	bool bAssemblyModeActive,
	bool bPointerOverBlockingUI,
	ASRCameraPawn* CameraPawn,
	int32 StepDelta)
{
	const uint64 CurrentFrame = GFrameCounter;
	if (RuntimeState.IsDuplicatePlacementRotationInput(CurrentFrame, StepDelta))
	{
		return true;
	}

	if (!bAssemblyModeActive || bPointerOverBlockingUI)
	{
		return false;
	}

	if (!CameraPawn || !CameraPawn->RotateFocusSurfaceViewBySteps(StepDelta))
	{
		return false;
	}

	RuntimeState.StorePlacementRotationInput(CurrentFrame, StepDelta);
	return true;
}
