#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"

namespace UE::Geometry
{
	class FDynamicMesh3;
}

namespace StarRovers::SurfaceGridInteractionOverlayGeometry
{
	FString FormatSurfacePatchCellId(const FSRPlanetSurfaceGridCellId& CellId);

	void AppendInteractionFilledQuad(
		UE::Geometry::FDynamicMesh3& OverlayMesh,
		const FVector& Point0,
		const FVector& Point1,
		const FVector& Point2,
		const FVector& Point3,
		FLinearColor FillColor);

	bool IsContiguousRectangularCellRegion(const TArray<FSRPlanetSurfaceGridCellId>& CellIds);
}
