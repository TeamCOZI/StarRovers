#pragma once

#include "CoreMinimal.h"
#include "Conveyor/SRConveyorTypes.h"

class AActor;
class USceneComponent;
class USplineComponent;
class USRPlanetSurfaceGrid;

namespace StarRovers::Conveyor
{
	struct FSRConveyorPCGSplineInputBuildSettings
	{
		bool bBuildPCGSplineInputs = false;
		float BeltWidth = 1.0f;
		float BeltSurfaceOffset = 0.0f;
		float PCGSplineHeightOffset = 0.0f;
		FName PCGSplineComponentTag = NAME_None;
	};

	struct STARROVERS_API FSRConveyorPCGSplineInputBuilder
	{
		static void Refresh(
			AActor* OwnerActor,
			USceneComponent* AttachParent,
			USRPlanetSurfaceGrid* SurfaceGrid,
			const TArray<FSRConveyorBeltPath>& BeltPaths,
			const FSRConveyorPCGSplineInputBuildSettings& Settings,
			TArray<TObjectPtr<USplineComponent>>& PCGSplineComponents);
	};
}
