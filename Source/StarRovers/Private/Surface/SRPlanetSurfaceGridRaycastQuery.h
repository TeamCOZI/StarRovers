#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridRuntimeState.h"

namespace StarRovers::SurfaceGridRaycast
{
	bool RaycastGeneratedGrid(
		const TArray<FSRPlanetSurfaceGridCell>& Cells,
		const FSRPlanetSurfaceGridRaycastState& RaycastState,
		const FTransform& ComponentTransform,
		const FVector& RayOrigin,
		const FVector& RayDirection,
		int32& OutCellIndex,
		FVector& OutHitLocation,
		bool& bOutAttempted);

	void RebuildRaycastIndex(
		FSRPlanetSurfaceGridRaycastState& RaycastState,
		const TArray<FSRPlanetSurfaceGridCell>& Cells,
		bool bUsingGeneratedGridCells,
		int32 FaceResolution,
		float GridSurfaceOffset);

	bool IntersectRayWithSphere(
		const FVector& RayOrigin,
		const FVector& RayDirection,
		const FVector& SphereCenter,
		float SphereRadius,
		FVector& OutHitLocation);
}
