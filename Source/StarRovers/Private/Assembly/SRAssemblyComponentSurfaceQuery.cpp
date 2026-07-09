#include "Assembly/SRAssemblyComponent.h"

#include "Camera/SRPlayerController.h"

ASRPlayerController* USRAssemblyComponent::GetOwnerController() const
{
	return Cast<ASRPlayerController>(GetOwner());
}

void USRAssemblyComponent::ResetHoverSampleCache()
{
	SurfaceState.ResetHoverSampleCache();
}
