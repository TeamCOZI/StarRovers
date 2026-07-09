#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "Templates/Function.h"

namespace UE::Geometry
{
	class FDynamicMesh3;
}

namespace StarRovers::SurfaceGridWireCells
{
	using FCellLookup = TFunctionRef<bool(const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCell& OutCell)>;
	using FSurfacePointResolver = TFunctionRef<FVector(const FVector& LocalUnitDirection, float HeightOffset)>;

	void AppendGeneratedGridCell(
		UE::Geometry::FDynamicMesh3& GridMesh,
		const FSRPlanetSurfaceGridCell& Cell,
		const FLinearColor& LineColor,
		float LineThickness,
		float GridSurfaceOffset,
		TSet<uint64>& DrawnEdges);

	void AppendGridWireCell(
		UE::Geometry::FDynamicMesh3& GridMesh,
		const FSRPlanetSurfaceGridCell& Cell,
		const FLinearColor& LineColor,
		float LineThickness,
		bool bIncludeInEdgeSet,
		TSet<uint64>* DrawnEdges,
		bool bUsingGeneratedGridCells,
		float GridSurfaceOffset,
		FCellLookup GetCellById,
		FSurfacePointResolver ResolveLocalSurfacePoint);
}
