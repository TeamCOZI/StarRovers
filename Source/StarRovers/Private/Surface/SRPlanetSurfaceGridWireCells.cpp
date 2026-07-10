#include "SRPlanetSurfaceGridWireCells.h"

#include "SRPlanetSurfaceGridWireGeometry.h"
#include "SRPlanetSurfaceGridWirePrimitives.h"

#include "DynamicMesh/DynamicMesh3.h"

using namespace StarRovers::SurfaceGridWireGeometry;

void StarRovers::SurfaceGridWireCells::AppendGeneratedGridCell(
	UE::Geometry::FDynamicMesh3& GridMesh,
	const FSRPlanetSurfaceGridCell& Cell,
	const FLinearColor& LineColor,
	float LineThickness,
	float GridSurfaceOffset,
	TSet<uint64>& DrawnEdges)
{
	auto AppendDedupedSegment = [&GridMesh, &LineColor, LineThickness, &DrawnEdges](const FVector& PointA, const FVector& PointB)
	{
		const uint64 EdgeKey = BuildGridEdgeKey(PointA, PointB);
		bool bAlreadyDrawn = false;
		DrawnEdges.Add(EdgeKey, &bAlreadyDrawn);
		if (bAlreadyDrawn)
		{
			return;
		}

		SurfaceGridWirePrimitives::AppendGridWireSegment(GridMesh, PointA, PointB, LineColor, LineThickness);
	};

	for (int32 EdgeIndex = 0; EdgeIndex < 4; ++EdgeIndex)
	{
		FVector EdgePointA;
		FVector EdgePointB;
		if (GetGridCellEdgePoints(Cell, EdgeIndex, EdgePointA, EdgePointB))
		{
			AppendDedupedSegment(
				OffsetGeneratedGridWirePoint(EdgePointA, GridSurfaceOffset),
				OffsetGeneratedGridWirePoint(EdgePointB, GridSurfaceOffset));
		}
	}

	for (const FSRPlanetSurfaceGridLineSegment& SideLineSegment : Cell.SideLineSegments)
	{
		AppendDedupedSegment(
			OffsetGeneratedGridWirePoint(SideLineSegment.LocalPointA, GridSurfaceOffset),
			OffsetGeneratedGridWirePoint(SideLineSegment.LocalPointB, GridSurfaceOffset));
	}
}

void StarRovers::SurfaceGridWireCells::AppendGridWireCell(
	UE::Geometry::FDynamicMesh3& GridMesh,
	const FSRPlanetSurfaceGridCell& Cell,
	const FLinearColor& LineColor,
	float LineThickness,
	bool bIncludeInEdgeSet,
	TSet<uint64>* DrawnEdges,
	bool bUsingGeneratedGridCells,
	float GridSurfaceOffset,
	FCellLookup GetCellById,
	FSurfacePointResolver ResolveLocalSurfacePoint)
{
	auto AppendDedupedSegment = [&GridMesh, &LineColor, LineThickness, bIncludeInEdgeSet, DrawnEdges](
		const FVector& PointA,
		const FVector& PointB)
	{
		if (bIncludeInEdgeSet && DrawnEdges)
		{
			const uint64 EdgeKey = BuildGridEdgeKey(PointA, PointB);
			bool bAlreadyDrawn = false;
			DrawnEdges->Add(EdgeKey, &bAlreadyDrawn);
			if (bAlreadyDrawn)
			{
				return;
			}
		}

		SurfaceGridWirePrimitives::AppendGridWireSegment(GridMesh, PointA, PointB, LineColor, LineThickness);
	};

	if (bUsingGeneratedGridCells)
	{
		for (int32 EdgeIndex = 0; EdgeIndex < 4; ++EdgeIndex)
		{
			FVector EdgePointA;
			FVector EdgePointB;
			if (GetGridCellEdgePoints(Cell, EdgeIndex, EdgePointA, EdgePointB))
			{
				AppendDedupedSegment(
					OffsetGeneratedGridWirePoint(EdgePointA, GridSurfaceOffset),
					OffsetGeneratedGridWirePoint(EdgePointB, GridSurfaceOffset));
			}
		}

		for (const FSRPlanetSurfaceGridLineSegment& SideLineSegment : Cell.SideLineSegments)
		{
			AppendDedupedSegment(
				OffsetGeneratedGridWirePoint(SideLineSegment.LocalPointA, GridSurfaceOffset),
				OffsetGeneratedGridWirePoint(SideLineSegment.LocalPointB, GridSurfaceOffset));
		}

		for (int32 EdgeIndex = 0; EdgeIndex < 4; ++EdgeIndex)
		{
			const FSRPlanetSurfaceGridCellId NeighborId = GetGridCellEdgeNeighborId(Cell, EdgeIndex);
			if (NeighborId == Cell.CellId)
			{
				continue;
			}

			FSRPlanetSurfaceGridCell NeighborCell;
			if (!GetCellById(NeighborId, NeighborCell))
			{
				continue;
			}

			FVector CellPointA;
			FVector CellPointB;
			FVector NeighborPointA;
			FVector NeighborPointB;
			if (!TryGetMatchingGridCellEdgePoints(Cell, EdgeIndex, NeighborCell, CellPointA, CellPointB, NeighborPointA, NeighborPointB))
			{
				continue;
			}

			if (FVector::DistSquared(CellPointA, NeighborPointA) > KINDA_SMALL_NUMBER)
			{
				AppendDedupedSegment(
					OffsetGeneratedGridWirePoint(CellPointA, GridSurfaceOffset),
					OffsetGeneratedGridWirePoint(NeighborPointA, GridSurfaceOffset));
			}
			if (FVector::DistSquared(CellPointB, NeighborPointB) > KINDA_SMALL_NUMBER)
			{
				AppendDedupedSegment(
					OffsetGeneratedGridWirePoint(CellPointB, GridSurfaceOffset),
					OffsetGeneratedGridWirePoint(NeighborPointB, GridSurfaceOffset));
			}
		}
		return;
	}

	auto AppendEdge = [&GridMesh, &LineColor, LineThickness, bIncludeInEdgeSet, DrawnEdges, GridSurfaceOffset, ResolveLocalSurfacePoint](
		const FVector& CornerA,
		const FVector& CornerB)
	{
		if (bIncludeInEdgeSet && DrawnEdges)
		{
			const uint64 EdgeKey = BuildGridEdgeKey(CornerA, CornerB);
			bool bAlreadyDrawn = false;
			DrawnEdges->Add(EdgeKey, &bAlreadyDrawn);
			if (bAlreadyDrawn)
			{
				return;
			}
		}

		SurfaceGridWirePrimitives::AppendGridWireEdge(
			GridMesh,
			CornerA,
			CornerB,
			LineColor,
			LineThickness,
			GridSurfaceOffset,
			ResolveLocalSurfacePoint);
	};

	for (int32 EdgeIndex = 0; EdgeIndex < 4; ++EdgeIndex)
	{
		FVector EdgePointA;
		FVector EdgePointB;
		if (GetGridCellEdgePoints(Cell, EdgeIndex, EdgePointA, EdgePointB))
		{
			AppendEdge(EdgePointA, EdgePointB);
		}
	}
}
