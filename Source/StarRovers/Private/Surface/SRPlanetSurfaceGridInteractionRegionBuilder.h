#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "Templates/Function.h"

namespace UE::Geometry
{
	class FDynamicMesh3;
}

namespace StarRovers::SurfaceGridInteractionRegionBuilder
{
	using FCellLookup = TFunctionRef<bool(const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCell& OutCell)>;
	using FSurfacePointResolver = TFunctionRef<FVector(const FVector& LocalUnitDirection, float HeightOffset)>;

	void AppendInteractionCellRegionBoundary(
		UE::Geometry::FDynamicMesh3& OverlayMesh,
		const TArray<FSRPlanetSurfaceGridCellId>& CellIds,
		const FLinearColor& LineColor,
		float LineThickness,
		bool bIncludeFill,
		TSet<uint64>* SharedDrawnEdges,
		bool bUsingGeneratedGridCells,
		float GridSurfaceOffset,
		FCellLookup GetCellById,
		FSurfacePointResolver ResolveLocalSurfacePoint);

	void AppendInteractionCellRegion(
		UE::Geometry::FDynamicMesh3& OverlayMesh,
		const TArray<FSRPlanetSurfaceGridCellId>& CellIds,
		const FLinearColor& LineColor,
		float LineThickness,
		bool bPreferCompactRectangles,
		bool bUsingGeneratedGridCells,
		float GridSurfaceOffset,
		FCellLookup GetCellById,
		FSurfacePointResolver ResolveLocalSurfacePoint);

	bool TryAppendRectangularInteractionCellRegion(
		UE::Geometry::FDynamicMesh3& OverlayMesh,
		const TArray<FSRPlanetSurfaceGridCellId>& CellIds,
		const FLinearColor& LineColor,
		float LineThickness,
		bool bUsingGeneratedGridCells,
		float GridSurfaceOffset,
		FCellLookup GetCellById,
		FSurfacePointResolver ResolveLocalSurfacePoint);
}
