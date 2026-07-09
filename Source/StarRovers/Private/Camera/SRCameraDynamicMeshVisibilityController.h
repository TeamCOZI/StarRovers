#pragma once

#include "CoreMinimal.h"

#include "Camera/SRCameraPawnRuntimeState.h"

class AActor;
class APlayerController;
class UCameraComponent;
class USRCelestialBodyRegistrySubsystem;

class FSRCameraDynamicMeshVisibilityController
{
public:
	static bool Apply(
		USRCelestialBodyRegistrySubsystem* CelestialRegistry,
		const UCameraComponent* Camera,
		const APlayerController* PlayerController,
		AActor* FocusedActor,
		FSRCameraDynamicMeshVisibilityState& VisibilityState,
		AActor*& OutDirectionalLightTarget);

private:
	static bool ShouldUseDynamicMesh(
		const AActor* BodyActor,
		const UCameraComponent* Camera,
		const APlayerController* PlayerController,
		const AActor* FocusedActor,
		float& OutScreenSizeRatio);

	static float ResolveViewportAspectRatio(const APlayerController* PlayerController);
};
