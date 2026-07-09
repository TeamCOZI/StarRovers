#pragma once

#include "CoreMinimal.h"
#include "Conveyor/SRConveyorTypes.h"

class UPCGComponent;
class USceneComponent;
class USRPlanetSurfaceGrid;

namespace StarRovers::Conveyor
{
	struct FSRConveyorPCGSplineMeshRebaseSettings
	{
		float BeltWidth = 1.0f;
		float BeltSurfaceOffset = 0.0f;
		float PCGSplineHeightOffset = 0.0f;
		FTransform ComponentTransform = FTransform::Identity;
		TWeakObjectPtr<USceneComponent> AttachParent;
	};

	struct STARROVERS_API FSRConveyorPCGSplineMeshRebaser
	{
		static void Rebase(
			UPCGComponent* PCGComponent,
			USRPlanetSurfaceGrid* SurfaceGrid,
			const TArray<FSRConveyorBeltPath>& BeltPaths,
			const FSRConveyorPCGSplineMeshRebaseSettings& Settings);
	};
}
