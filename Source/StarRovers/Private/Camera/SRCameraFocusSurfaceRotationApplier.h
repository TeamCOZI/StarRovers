#pragma once

#include "CoreMinimal.h"

struct FSRCameraFocusSurfaceRuntimeState;

class FSRCameraFocusSurfaceRotationApplier
{
public:
	static bool ApplyDelta(
		FSRCameraFocusSurfaceRuntimeState& FocusSurface,
		const FQuat& CameraRelativeQuat,
		const FVector2D& DegreesDelta);
};
