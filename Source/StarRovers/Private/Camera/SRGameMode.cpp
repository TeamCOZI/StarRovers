#include "Camera/SRGameMode.h"

#include "Camera/SRCameraPawn.h"
#include "Camera/SRPlayerController.h"

ASRGameMode::ASRGameMode()
{
	DefaultPawnClass = ASRCameraPawn::StaticClass();
	PlayerControllerClass = ASRPlayerController::StaticClass();
}
