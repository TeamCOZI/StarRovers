#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

class AActor;
class USRAssemblyComponent;

class FSRPlayerControllerAssemblyRightClickHandler
{
public:
	static void HandleRightClick(
		USRAssemblyComponent* AssemblyComponent,
		AActor* SelectedActor,
		bool bAssemblyAreaDeletionDragHoldActive,
		TFunctionRef<bool()> ClearSelectedStructureBuildOption,
		TFunctionRef<void(AActor*)> UpdateSelection);
};
