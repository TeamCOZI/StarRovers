#pragma once

#include "CoreMinimal.h"
#include "Camera/SRPlayerControllerRuntimeState.h"

class ASRCameraPawn;
class USRAssemblyComponent;

class FSRPlayerControllerFocusParentHandler
{
public:
	static AActor* ResolveParentFocusActor(
		FSRPlayerControllerRuntimeState& RuntimeState,
		bool bAugmentChoiceVisible,
		bool bAssemblyModeActive,
		ASRCameraPawn* CameraPawn,
		USRAssemblyComponent* AssemblyComponent);
};
