#include "Camera/SRGameMode.h"

#include "Camera/SRCameraPawn.h"
#include "Camera/SRPlayerController.h"
#include "Logistics/SRSpaceshipActor.h"

ASRGameMode::ASRGameMode()
{
	DefaultPawnClass = ASRCameraPawn::StaticClass();
	PlayerControllerClass = ASRPlayerController::StaticClass();
}

TSubclassOf<ASRSpaceshipActor> ASRGameMode::ResolveSpaceLogisticsSpaceshipActorClass() const
{
	if (SpaceLogisticsSpaceshipActorClass.IsNull())
	{
		return nullptr;
	}

	UClass* LoadedClass = SpaceLogisticsSpaceshipActorClass.LoadSynchronous();
	return IsValid(LoadedClass) && LoadedClass->IsChildOf(ASRSpaceshipActor::StaticClass())
		? LoadedClass
		: nullptr;
}
