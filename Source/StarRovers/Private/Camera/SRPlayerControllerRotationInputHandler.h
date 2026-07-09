#pragma once

#include "CoreMinimal.h"
#include "Camera/SRPlayerControllerRuntimeState.h"

class ASRCameraPawn;
class USRAssemblyComponent;

class FSRPlayerControllerRotationInputHandler
{
public:
	static bool TryHandlePlacementRotationInput(
		FSRPlayerControllerRuntimeState& RuntimeState,
		bool bAssemblyModeActive,
		bool bPointerOverBlockingUI,
		USRAssemblyComponent* AssemblyComponent,
		bool bHasSelectedStructureDataAsset,
		int32 StepDelta);

	static bool TryHandleSurfaceViewRotationInput(
		FSRPlayerControllerRuntimeState& RuntimeState,
		bool bAssemblyModeActive,
		bool bPointerOverBlockingUI,
		ASRCameraPawn* CameraPawn,
		int32 StepDelta);
};
