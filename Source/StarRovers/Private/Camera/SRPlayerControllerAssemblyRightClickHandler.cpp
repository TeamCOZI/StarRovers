#include "SRPlayerControllerAssemblyRightClickHandler.h"

#include "Assembly/SRAssemblyComponent.h"

namespace
{
	void UpdateSelectionIfChanged(
		AActor* SelectedActor,
		AActor* AssemblySelectedActor,
		TFunctionRef<void(AActor*)> UpdateSelection)
	{
		if (IsValid(AssemblySelectedActor) && SelectedActor != AssemblySelectedActor)
		{
			UpdateSelection(AssemblySelectedActor);
		}
	}
}

void FSRPlayerControllerAssemblyRightClickHandler::HandleRightClick(
	USRAssemblyComponent* AssemblyComponent,
	AActor* SelectedActor,
	bool bAssemblyAreaDeletionDragHoldActive,
	TFunctionRef<bool()> ClearSelectedStructureBuildOption,
	TFunctionRef<void(AActor*)> UpdateSelection)
{
	if ((AssemblyComponent && AssemblyComponent->IsAreaSelectionDragActive())
		|| bAssemblyAreaDeletionDragHoldActive
		|| (AssemblyComponent && AssemblyComponent->IsAreaDeletionDragActive()))
	{
		return;
	}

	if (AssemblyComponent && AssemblyComponent->IsAreaCopyPlacementActive())
	{
		AActor* AssemblySelectedActor = nullptr;
		if (AssemblyComponent->TryHandleAssemblyDelete(AssemblySelectedActor))
		{
			UpdateSelectionIfChanged(SelectedActor, AssemblySelectedActor, UpdateSelection);
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
		UpdateSelectionIfChanged(SelectedActor, AssemblySelectedActor, UpdateSelection);
	}
}
