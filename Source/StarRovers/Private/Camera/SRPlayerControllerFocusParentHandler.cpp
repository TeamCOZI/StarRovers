#include "SRPlayerControllerFocusParentHandler.h"

#include "Assembly/SRAssemblyComponent.h"
#include "Camera/SRCameraPawn.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"

AActor* FSRPlayerControllerFocusParentHandler::ResolveParentFocusActor(
	FSRPlayerControllerRuntimeState& RuntimeState,
	bool bAugmentChoiceVisible,
	bool bAssemblyModeActive,
	ASRCameraPawn* CameraPawn,
	USRAssemblyComponent* AssemblyComponent)
{
	if (bAugmentChoiceVisible || bAssemblyModeActive)
	{
		return nullptr;
	}

	RuntimeState.bPendingInitialPrimaryStarFocus = false;

	if (!CameraPawn)
	{
		return nullptr;
	}

	AActor* CurrentFocusActor = CameraPawn->GetFocusedActor();
	if (!IsValid(CurrentFocusActor))
	{
		return nullptr;
	}

	AActor* ParentBody = nullptr;
	if (!USRCelestialBodyRuntimeLibrary::GetCelestialParentBody(CurrentFocusActor, ParentBody) || !IsValid(ParentBody))
	{
		return nullptr;
	}

	if (AssemblyComponent)
	{
		AssemblyComponent->ClearSurfaceGridInteraction(CurrentFocusActor);
	}

	return ParentBody;
}
