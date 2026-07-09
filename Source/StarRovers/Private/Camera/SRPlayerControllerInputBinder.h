#pragma once

#include "CoreMinimal.h"

class ASRPlayerController;
class UInputComponent;

class FSRPlayerControllerInputBinder
{
public:
	static void BindInputActions(
		ASRPlayerController& PlayerController,
		UInputComponent* InputComponent);
};
