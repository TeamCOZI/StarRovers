#pragma once

#include "CoreMinimal.h"

struct FSRCameraFocusSurfaceRuntimeState;

class FSRCameraFocusSurfaceRigAlignmentController
{
public:
	static bool StartRigAlignment(
		FSRCameraFocusSurfaceRuntimeState& FocusSurface,
		const FVector& RigAlignmentStartOffset,
		const FVector& AlignmentAxis,
		float AlignmentAngleRadians);

	static void StartRotationReset(FSRCameraFocusSurfaceRuntimeState& FocusSurface);
	static void StopRotationResetForDrag(FSRCameraFocusSurfaceRuntimeState& FocusSurface);
};
