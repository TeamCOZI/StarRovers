#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"

class AActor;
class ASRPlayerController;
class USRPlanetSurfaceGrid;

namespace StarRovers::Assembly
{
	struct FSRAssemblySurfaceCursorTarget
	{
		AActor* FocusedActor = nullptr;
		USRPlanetSurfaceGrid* SurfaceGrid = nullptr;
		FSRPlanetSurfaceGridCell Cell;
		FVector HitLocation = FVector::ZeroVector;
	};

	class STARROVERS_API FSRAssemblySurfaceCursorQuery
	{
	public:
		static bool TryGetCursorRay(const ASRPlayerController* PlayerController, FVector& OutRayOrigin, FVector& OutRayDirection);
		static bool TryGetFocusedSurfaceGrid(const ASRPlayerController* PlayerController, AActor*& OutFocusedActor, USRPlanetSurfaceGrid*& OutSurfaceGrid);
		static bool TryProjectCursorToSurfaceCell(
			const ASRPlayerController* PlayerController,
			USRPlanetSurfaceGrid* SurfaceGrid,
			FSRPlanetSurfaceGridCell& OutCell,
			FVector& OutHitLocation);
		static bool TryResolveSurfaceCell(const ASRPlayerController* PlayerController, FSRAssemblySurfaceCursorTarget& OutTarget);
	};
}
