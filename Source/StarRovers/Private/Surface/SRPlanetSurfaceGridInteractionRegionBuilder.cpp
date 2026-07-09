#include "SRPlanetSurfaceGridInteractionRegionBuilder.h"

#include "SRPlanetSurfaceGridInteractionOverlayGeometry.h"
#include "SRPlanetSurfaceGridWireGeometry.h"
#include "SRPlanetSurfaceGridWirePrimitives.h"

#include "DynamicMesh/DynamicMesh3.h"

using namespace StarRovers::SurfaceGridInteractionOverlayGeometry;
using namespace StarRovers::SurfaceGridWireGeometry;

namespace
{
	void AppendInteractionBoundarySegment(
		UE::Geometry::FDynamicMesh3& OverlayMesh,
		const FVector& PointA,
		const FVector& PointB,
		const FLinearColor& LineColor,
		float LineThickness,
		float HighlightOffset,
		TSet<uint64>& DrawnEdges,
		bool bUsingGeneratedGridCells,
		float GridSurfaceOffset,
		StarRovers::SurfaceGridInteractionRegionBuilder::FSurfacePointResolver ResolveLocalSurfacePoint)
	{
		FVector DrawPointA = PointA;
		FVector DrawPointB = PointB;
		if (bUsingGeneratedGridCells)
		{
			DrawPointA = OffsetGeneratedGridWirePoint(DrawPointA, GridSurfaceOffset + HighlightOffset);
			DrawPointB = OffsetGeneratedGridWirePoint(DrawPointB, GridSurfaceOffset + HighlightOffset);
		}

		if (FVector::DistSquared(DrawPointA, DrawPointB) <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		const uint64 EdgeKey = BuildGridEdgeKey(DrawPointA, DrawPointB);
		if (DrawnEdges.Contains(EdgeKey))
		{
			return;
		}
		DrawnEdges.Add(EdgeKey);

		if (bUsingGeneratedGridCells)
		{
			StarRovers::SurfaceGridWirePrimitives::AppendGridWireSegment(
				OverlayMesh,
				DrawPointA,
				DrawPointB,
				LineColor,
				LineThickness);
			return;
		}

		StarRovers::SurfaceGridWirePrimitives::AppendGridWireEdge(
			OverlayMesh,
			DrawPointA,
			DrawPointB,
			LineColor,
			LineThickness,
			GridSurfaceOffset,
			ResolveLocalSurfacePoint);
	}

	void AppendInteractionCellFill(
		UE::Geometry::FDynamicMesh3& OverlayMesh,
		const FSRPlanetSurfaceGridCell& Cell,
		const FLinearColor& LineColor,
		float HighlightOffset,
		bool bUsingGeneratedGridCells,
		float GridSurfaceOffset,
		StarRovers::SurfaceGridInteractionRegionBuilder::FSurfacePointResolver ResolveLocalSurfacePoint)
	{
		if (bUsingGeneratedGridCells)
		{
			AppendInteractionFilledQuad(
				OverlayMesh,
				OffsetGeneratedGridWirePoint(Cell.Corner00, GridSurfaceOffset + HighlightOffset),
				OffsetGeneratedGridWirePoint(Cell.Corner10, GridSurfaceOffset + HighlightOffset),
				OffsetGeneratedGridWirePoint(Cell.Corner11, GridSurfaceOffset + HighlightOffset),
				OffsetGeneratedGridWirePoint(Cell.Corner01, GridSurfaceOffset + HighlightOffset),
				LineColor);
			return;
		}

		AppendInteractionFilledQuad(
			OverlayMesh,
			ResolveLocalSurfacePoint(Cell.Corner00.GetSafeNormal(), GridSurfaceOffset + HighlightOffset),
			ResolveLocalSurfacePoint(Cell.Corner10.GetSafeNormal(), GridSurfaceOffset + HighlightOffset),
			ResolveLocalSurfacePoint(Cell.Corner11.GetSafeNormal(), GridSurfaceOffset + HighlightOffset),
			ResolveLocalSurfacePoint(Cell.Corner01.GetSafeNormal(), GridSurfaceOffset + HighlightOffset),
			LineColor);
	}
}

void StarRovers::SurfaceGridInteractionRegionBuilder::AppendInteractionCellRegionBoundary(
	UE::Geometry::FDynamicMesh3& OverlayMesh,
	const TArray<FSRPlanetSurfaceGridCellId>& CellIds,
	const FLinearColor& LineColor,
	float LineThickness,
	bool bIncludeFill,
	TSet<uint64>* SharedDrawnEdges,
	bool bUsingGeneratedGridCells,
	float GridSurfaceOffset,
	FCellLookup GetCellById,
	FSurfacePointResolver ResolveLocalSurfacePoint)
{
	if (CellIds.IsEmpty())
	{
		return;
	}

	TSet<FSRPlanetSurfaceGridCellId> RegionCellIdSet;
	RegionCellIdSet.Reserve(CellIds.Num());
	TArray<FSRPlanetSurfaceGridCell> RegionCells;
	RegionCells.Reserve(CellIds.Num());
	for (const FSRPlanetSurfaceGridCellId& CellId : CellIds)
	{
		RegionCellIdSet.Add(CellId);

		FSRPlanetSurfaceGridCell Cell;
		if (GetCellById(CellId, Cell))
		{
			RegionCells.Add(Cell);
		}
	}

	if (RegionCells.IsEmpty())
	{
		return;
	}

	TSet<uint64> LocalDrawnEdges;
	if (!SharedDrawnEdges)
	{
		LocalDrawnEdges.Reserve(RegionCells.Num() * 4);
	}
	TSet<uint64>& DrawnEdges = SharedDrawnEdges ? *SharedDrawnEdges : LocalDrawnEdges;
	const float HighlightOffset = FMath::Max(0.5f, LineThickness * 0.25f);

	if (bIncludeFill)
	{
		for (const FSRPlanetSurfaceGridCell& Cell : RegionCells)
		{
			AppendInteractionCellFill(
				OverlayMesh,
				Cell,
				LineColor,
				HighlightOffset,
				bUsingGeneratedGridCells,
				GridSurfaceOffset,
				ResolveLocalSurfacePoint);
		}
	}

	for (const FSRPlanetSurfaceGridCell& Cell : RegionCells)
	{
		for (int32 EdgeIndex = 0; EdgeIndex < 4; ++EdgeIndex)
		{
			const FSRPlanetSurfaceGridCellId NeighborId = GetGridCellEdgeNeighborId(Cell, EdgeIndex);
			if (!(NeighborId == Cell.CellId) && RegionCellIdSet.Contains(NeighborId))
			{
				continue;
			}

			FVector EdgePointA;
			FVector EdgePointB;
			if (GetGridCellEdgePoints(Cell, EdgeIndex, EdgePointA, EdgePointB))
			{
				AppendInteractionBoundarySegment(
					OverlayMesh,
					EdgePointA,
					EdgePointB,
					LineColor,
					LineThickness,
					HighlightOffset,
					DrawnEdges,
					bUsingGeneratedGridCells,
					GridSurfaceOffset,
					ResolveLocalSurfacePoint);
			}
		}

		if (!bUsingGeneratedGridCells)
		{
			continue;
		}

		for (const FSRPlanetSurfaceGridLineSegment& SideLineSegment : Cell.SideLineSegments)
		{
			if (SideLineSegment.bHasAdjacentCell && RegionCellIdSet.Contains(SideLineSegment.AdjacentCellId))
			{
				continue;
			}

			AppendInteractionBoundarySegment(
				OverlayMesh,
				SideLineSegment.LocalPointA,
				SideLineSegment.LocalPointB,
				LineColor,
				LineThickness,
				HighlightOffset,
				DrawnEdges,
				bUsingGeneratedGridCells,
				GridSurfaceOffset,
				ResolveLocalSurfacePoint);
		}
	}
}

void StarRovers::SurfaceGridInteractionRegionBuilder::AppendInteractionCellRegion(
	UE::Geometry::FDynamicMesh3& OverlayMesh,
	const TArray<FSRPlanetSurfaceGridCellId>& CellIds,
	const FLinearColor& LineColor,
	float LineThickness,
	bool bPreferCompactRectangles,
	bool bUsingGeneratedGridCells,
	float GridSurfaceOffset,
	FCellLookup GetCellById,
	FSurfacePointResolver ResolveLocalSurfacePoint)
{
	if (CellIds.IsEmpty())
	{
		return;
	}

	constexpr int32 CompactRegionCellThreshold = 16;
	if (bPreferCompactRectangles && CellIds.Num() >= CompactRegionCellThreshold)
	{
		TArray<FSRPlanetSurfaceGridCellId> FaceCellIds[6];
		bool bCanGroupByFace = true;
		for (const FSRPlanetSurfaceGridCellId& CellId : CellIds)
		{
			const int32 FaceIndex = static_cast<int32>(CellId.Face);
			if (FaceIndex >= 0 && FaceIndex < UE_ARRAY_COUNT(FaceCellIds))
			{
				FaceCellIds[FaceIndex].Add(CellId);
				continue;
			}

			bCanGroupByFace = false;
			break;
		}

		bool bAllFaceRegionsRectangular = bCanGroupByFace;
		if (bAllFaceRegionsRectangular)
		{
			for (const TArray<FSRPlanetSurfaceGridCellId>& RegionCellIds : FaceCellIds)
			{
				if (RegionCellIds.IsEmpty())
				{
					continue;
				}

				if (!IsContiguousRectangularCellRegion(RegionCellIds))
				{
					bAllFaceRegionsRectangular = false;
					break;
				}
			}
		}

		if (bAllFaceRegionsRectangular)
		{
			for (const TArray<FSRPlanetSurfaceGridCellId>& RegionCellIds : FaceCellIds)
			{
				if (!RegionCellIds.IsEmpty())
				{
					TryAppendRectangularInteractionCellRegion(
						OverlayMesh,
						RegionCellIds,
						LineColor,
						LineThickness,
						bUsingGeneratedGridCells,
						GridSurfaceOffset,
						GetCellById,
						ResolveLocalSurfacePoint);
				}
			}

			return;
		}
	}

	AppendInteractionCellRegionBoundary(
		OverlayMesh,
		CellIds,
		LineColor,
		LineThickness,
		true,
		nullptr,
		bUsingGeneratedGridCells,
		GridSurfaceOffset,
		GetCellById,
		ResolveLocalSurfacePoint);
}

bool StarRovers::SurfaceGridInteractionRegionBuilder::TryAppendRectangularInteractionCellRegion(
	UE::Geometry::FDynamicMesh3& OverlayMesh,
	const TArray<FSRPlanetSurfaceGridCellId>& CellIds,
	const FLinearColor& LineColor,
	float LineThickness,
	bool bUsingGeneratedGridCells,
	float GridSurfaceOffset,
	FCellLookup GetCellById,
	FSurfacePointResolver ResolveLocalSurfacePoint)
{
	if (CellIds.IsEmpty())
	{
		return true;
	}

	const ESRCubeSphereFace Face = CellIds[0].Face;
	int32 MinCellX = CellIds[0].CellX;
	int32 MaxCellX = CellIds[0].CellX;
	int32 MinCellY = CellIds[0].CellY;
	int32 MaxCellY = CellIds[0].CellY;
	TSet<FSRPlanetSurfaceGridCellId> RegionCellIdSet;
	RegionCellIdSet.Reserve(CellIds.Num());

	for (const FSRPlanetSurfaceGridCellId& CellId : CellIds)
	{
		if (CellId.Face != Face)
		{
			return false;
		}

		RegionCellIdSet.Add(CellId);
		MinCellX = FMath::Min(MinCellX, CellId.CellX);
		MaxCellX = FMath::Max(MaxCellX, CellId.CellX);
		MinCellY = FMath::Min(MinCellY, CellId.CellY);
		MaxCellY = FMath::Max(MaxCellY, CellId.CellY);
	}

	const int32 RegionCellsX = MaxCellX - MinCellX + 1;
	const int32 RegionCellsY = MaxCellY - MinCellY + 1;
	if (RegionCellsX <= 0 || RegionCellsY <= 0 || RegionCellIdSet.Num() != RegionCellsX * RegionCellsY)
	{
		return false;
	}

	TSet<uint64> DrawnEdges;
	DrawnEdges.Reserve((RegionCellsX + RegionCellsY) * 2);
	const float HighlightOffset = FMath::Max(0.5f, LineThickness * 0.25f);

	auto AppendBoundaryEdge = [
		&OverlayMesh,
		&LineColor,
		LineThickness,
		HighlightOffset,
		&DrawnEdges,
		bUsingGeneratedGridCells,
		GridSurfaceOffset,
		GetCellById,
		ResolveLocalSurfacePoint](
		const FSRPlanetSurfaceGridCellId& CellId,
		int32 EdgeIndex)
	{
		FSRPlanetSurfaceGridCell Cell;
		if (!GetCellById(CellId, Cell))
		{
			return;
		}

		FVector EdgePointA;
		FVector EdgePointB;
		if (!GetGridCellEdgePoints(Cell, EdgeIndex, EdgePointA, EdgePointB))
		{
			return;
		}

		AppendInteractionBoundarySegment(
			OverlayMesh,
			EdgePointA,
			EdgePointB,
			LineColor,
			LineThickness,
			HighlightOffset,
			DrawnEdges,
			bUsingGeneratedGridCells,
			GridSurfaceOffset,
			ResolveLocalSurfacePoint);
	};

	for (int32 CellX = MinCellX; CellX <= MaxCellX; ++CellX)
	{
		FSRPlanetSurfaceGridCellId TopCellId;
		TopCellId.Face = Face;
		TopCellId.CellX = CellX;
		TopCellId.CellY = MinCellY;
		AppendBoundaryEdge(TopCellId, 0);

		FSRPlanetSurfaceGridCellId BottomCellId;
		BottomCellId.Face = Face;
		BottomCellId.CellX = CellX;
		BottomCellId.CellY = MaxCellY;
		AppendBoundaryEdge(BottomCellId, 2);
	}

	for (int32 CellY = MinCellY; CellY <= MaxCellY; ++CellY)
	{
		FSRPlanetSurfaceGridCellId LeftCellId;
		LeftCellId.Face = Face;
		LeftCellId.CellX = MinCellX;
		LeftCellId.CellY = CellY;
		AppendBoundaryEdge(LeftCellId, 3);

		FSRPlanetSurfaceGridCellId RightCellId;
		RightCellId.Face = Face;
		RightCellId.CellX = MaxCellX;
		RightCellId.CellY = CellY;
		AppendBoundaryEdge(RightCellId, 1);
	}

	return true;
}
