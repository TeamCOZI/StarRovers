#pragma once

#include "CoreMinimal.h"

class USRAssemblyComponent;

class FSRPlayerControllerSurfaceSelectionState
{
public:
	static void ClearSelectedActorSurfacePreview(AActor* SelectedActor);

	static void ClearFocusedActorHover(AActor* FocusedActor, USRAssemblyComponent* AssemblyComponent);
};
