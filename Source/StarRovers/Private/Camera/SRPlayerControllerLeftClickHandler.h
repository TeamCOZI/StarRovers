#pragma once

#include "CoreMinimal.h"
#include "Camera/SRPlayerControllerRuntimeState.h"
#include "Templates/Function.h"

class USRAssemblyComponent;

class FSRPlayerControllerLeftClickHandler
{
public:
	static void HandleWorldLeftClick(
		FSRPlayerControllerRuntimeState& RuntimeState,
		bool bShouldHandleAssemblyAreaSelectionDrag,
		USRAssemblyComponent* AssemblyComponent,
		AActor* SelectedActor,
		TFunctionRef<AActor*()> ResolveCursorHitActor,
		TFunctionRef<void(AActor*)> UpdateSelection,
		TFunctionRef<void(AActor*)> RequestFocusActor);
};
