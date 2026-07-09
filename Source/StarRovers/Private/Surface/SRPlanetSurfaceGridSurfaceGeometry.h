#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

namespace StarRovers::SurfaceGridSurfaceGeometry
{
	using FSurfaceHeightSampler = TFunctionRef<float(const FVector& LocalUnitDirection)>;

	FVector ResolveLocalSurfacePoint(
		const FVector& LocalUnitDirection,
		float PlanetRadius,
		bool bUseProceduralSurfaceNormal,
		FSurfaceHeightSampler SampleSurfaceHeightOffset,
		float HeightOffset);

	FVector ResolveWorldSurfacePoint(
		const FVector& LocalUnitDirection,
		const FTransform& ComponentTransform,
		float PlanetRadius,
		bool bUseProceduralSurfaceNormal,
		FSurfaceHeightSampler SampleSurfaceHeightOffset,
		float HeightOffset);

	FVector ComputeProceduralSurfaceNormal(
		FVector LocalUnitDirection,
		float PlanetRadius,
		FSurfaceHeightSampler SampleSurfaceHeightOffset);
}
