#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

namespace UE::Geometry
{
	class FDynamicMesh3;
}

namespace StarRovers::SurfaceGridWirePrimitives
{
	using FSurfacePointResolver = TFunctionRef<FVector(const FVector& LocalUnitDirection, float HeightOffset)>;

	void AppendGridWireEdge(
		UE::Geometry::FDynamicMesh3& GridMesh,
		const FVector& LocalDirectionA,
		const FVector& LocalDirectionB,
		const FLinearColor& LineColor,
		float LineThickness,
		float GridSurfaceOffset,
		FSurfacePointResolver ResolveLocalSurfacePoint);

	void AppendGridWireSegment(
		UE::Geometry::FDynamicMesh3& GridMesh,
		const FVector& LocalPointA,
		const FVector& LocalPointB,
		const FLinearColor& LineColor,
		float LineThickness);
}
