#pragma once

#include "CoreMinimal.h"

#include "Camera/SRCameraPawnRuntimeState.h"

class AActor;
class UWorld;

class FSRCameraSpaceBoundaryResolver
{
public:
	static bool Resolve(
		UWorld* World,
		const AActor* ExcludedActor,
		FSRCameraSpaceBoundaryCacheState& Cache,
		FVector& OutCenter,
		float& OutRadius);

private:
	static bool IsSpaceBoundaryActor(const AActor* Candidate);
	static bool TryResolveSpaceBoundaryActor(const AActor* Candidate, FVector& OutCenter, float& OutRadius);
};
