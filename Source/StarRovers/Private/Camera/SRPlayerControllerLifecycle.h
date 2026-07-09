#pragma once

#include "CoreMinimal.h"

class ASRPlayerController;

class FSRPlayerControllerLifecycle
{
public:
	static void BeginPlay(ASRPlayerController& PlayerController);
	static void Tick(ASRPlayerController& PlayerController);

private:
	static void ConfigureInputMode(ASRPlayerController& PlayerController);
	static void InitializeWidgets(ASRPlayerController& PlayerController);
};
