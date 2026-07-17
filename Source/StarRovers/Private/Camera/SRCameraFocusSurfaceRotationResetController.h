#pragma once

#include "CoreMinimal.h"

struct FSRCameraFocusSurfaceRuntimeState;

struct FSRCameraFocusSurfaceRotationResetUpdate
{
	FVector FocusDragOffset = FVector::ZeroVector;
	bool bHasFocusDragOffset = false;
	bool bIsActive = false;
};

class FSRCameraFocusSurfaceRotationResetController
{
public:
	static FSRCameraFocusSurfaceRotationResetUpdate Update(
		FSRCameraFocusSurfaceRuntimeState& FocusSurface,
		float SmoothTime,
		float DeltaSeconds);
};
