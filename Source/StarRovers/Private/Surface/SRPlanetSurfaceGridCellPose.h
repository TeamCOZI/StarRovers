#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "Templates/Function.h"

namespace StarRovers::SurfaceGridCellPose
{
	using FSurfacePointResolver = TFunctionRef<FVector(const FVector& LocalUnitDirection, float HeightOffset)>;

	void BuildCellWorldTransform(
		const FSRPlanetSurfaceGridCell& Cell,
		const FTransform& ComponentTransform,
		bool bUsingGeneratedGridCells,
		float GridSurfaceOffset,
		float HeightOffset,
		FSurfacePointResolver ResolveLocalSurfacePoint,
		FSurfacePointResolver ResolveWorldSurfacePoint,
		FTransform& OutTransform);

	void GetCellWorldCorners(
		const FSRPlanetSurfaceGridCell& Cell,
		const FTransform& ComponentTransform,
		bool bUsingGeneratedGridCells,
		float GridSurfaceOffset,
		FSurfacePointResolver ResolveWorldSurfacePoint,
		FVector& OutCorner00,
		FVector& OutCorner10,
		FVector& OutCorner11,
		FVector& OutCorner01);
}
