#pragma once

#include "CoreMinimal.h"

class UWorld;

struct STARROVERS_API FSRCameraInfo
{
	bool bIsValid = false;
	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewForward = FVector::ForwardVector;
	float TanHalfVerticalFieldOfView = 0.0f;

	float ComputeDepthToWorldLocation(const FVector& WorldLocation) const;
};

class STARROVERS_API FSRLineThicknessUtils
{
public:
	// These defaults match the Star Rovers camera's authored startup view.
	static constexpr float DefaultReferenceViewDepth = 12000.0f;
	static constexpr float DefaultReferenceFieldOfViewDegrees = 30.0f;

	static bool TryBuildPrimaryCameraInfo(const UWorld* World, FSRCameraInfo& OutCameraInfo);
	static void ResolveReferenceView(const UWorld* World, float& OutReferenceViewDepth, float& OutReferenceFieldOfViewDegrees);

	// Treat ReferenceWorldThickness as the authored world thickness at the reference view,
	// then scale it by the current frustum so the line keeps a consistent on-screen thickness.
	static float ComputeWorldThicknessAtLocation(
		const FSRCameraInfo& CameraInfo,
		const FVector& WorldLocation,
		float ReferenceWorldThickness,
		float ReferenceViewDepth,
		float ReferenceFieldOfViewDegrees);
};
