#pragma once

#include "CoreMinimal.h"

class ASRPlayerController;

class FSRCameraInputInteractionGate
{
public:
	static bool TryConsumeDragHoldStart(ASRPlayerController* PlayerController);
	static void CompleteDragHold(ASRPlayerController* PlayerController);
	static bool ShouldBlockFocusSurfaceDragHoldStart(const ASRPlayerController* PlayerController);
	static void CompleteFocusSurfaceDragHold(ASRPlayerController* PlayerController);
	static bool TryConsumeDragDelta(ASRPlayerController* PlayerController);
	static bool ShouldBlockZoom(const ASRPlayerController* PlayerController);
};
