#include "SRPlayerControllerLeftClickHandler.h"

#include "Assembly/SRAssemblyComponent.h"
#include "SRPlayerControllerFocusClickResolver.h"

void FSRPlayerControllerLeftClickHandler::HandleWorldLeftClick(
	FSRPlayerControllerRuntimeState& RuntimeState,
	bool bShouldHandleAssemblyAreaSelectionDrag,
	USRAssemblyComponent* AssemblyComponent,
	AActor* SelectedActor,
	TFunctionRef<AActor*()> ResolveCursorHitActor,
	TFunctionRef<void(AActor*)> UpdateSelection,
	TFunctionRef<void(AActor*)> RequestFocusActor)
{
	if (bShouldHandleAssemblyAreaSelectionDrag
		&& (!AssemblyComponent || !AssemblyComponent->IsAreaCopyPlacementActive()))
	{
		return;
	}

	RuntimeState.bPendingInitialPrimaryStarFocus = false;

	AActor* AssemblySelectedActor = nullptr;
	if (AssemblyComponent && AssemblyComponent->TryHandleAssemblyClick(AssemblySelectedActor))
	{
		if (IsValid(AssemblySelectedActor) && SelectedActor != AssemblySelectedActor)
		{
			UpdateSelection(AssemblySelectedActor);
		}
		return;
	}

	AActor* HitActor = ResolveCursorHitActor();
	AActor* SelectedFocusActor = FSRPlayerControllerFocusClickResolver::ResolveFocusableActor(HitActor);
	if (!IsValid(SelectedFocusActor))
	{
		return;
	}

	RequestFocusActor(SelectedFocusActor);
}
