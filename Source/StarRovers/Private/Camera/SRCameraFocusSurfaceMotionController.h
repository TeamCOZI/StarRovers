#pragma once

#include "CoreMinimal.h"

struct FSRCameraFocusSurfaceRuntimeState;

struct FSRCameraFocusSurfaceMotionSettings
{
	float LookSpeed = 0.0f;
	float InputAcceleration = 0.0f;
	float InputDeceleration = 0.0f;
	float InertiaDamping = 0.0f;
	float MinInertiaSpeed = 0.0f;
	bool bIsDraggingFocusSurface = false;
};

class FSRCameraFocusSurfaceMotionController
{
public:
	static bool Update(
		FSRCameraFocusSurfaceRuntimeState& FocusSurface,
		const FQuat& CameraRelativeQuat,
		const FSRCameraFocusSurfaceMotionSettings& Settings,
		float DeltaSeconds);
};
