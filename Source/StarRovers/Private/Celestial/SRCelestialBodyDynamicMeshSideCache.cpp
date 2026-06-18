#include "Celestial/SRCelestialBodyDynamicMeshInternal.h"

namespace StarRovers::Celestial::DynamicMesh
{
int32 ResolvePreparedSurfaceGridCellIndex(
	const TArray<int32>& CachedCellIndexByFlatId,
	int32 FaceResolution,
	const FSRPlanetSurfaceGridCellId& CellId)
{
	if (!CellId.IsValid(FaceResolution))
	{
		return INDEX_NONE;
	}

	const int32 FlatIndex = ((static_cast<int32>(CellId.Face) * FaceResolution) + CellId.CellY) * FaceResolution + CellId.CellX;
	return CachedCellIndexByFlatId.IsValidIndex(FlatIndex) ? CachedCellIndexByFlatId[FlatIndex] : INDEX_NONE;
}

void AddPreparedSurfaceGridSideLineSegment(
	TArray<FSRPlanetSurfaceGridCell>& PreparedSurfaceGridCells,
	const TArray<int32>& CachedCellIndexByFlatId,
	int32 FaceResolution,
	float BodyScale,
	const FSRPlanetSurfaceGridCellId& CellId,
	const FSRPlanetSurfaceGridCellId& AdjacentCellId,
	bool bHasAdjacentCell,
	const FVector& PointA,
	const FVector& PointB)
{
	const int32 CellIndex = ResolvePreparedSurfaceGridCellIndex(CachedCellIndexByFlatId, FaceResolution, CellId);
	if (!PreparedSurfaceGridCells.IsValidIndex(CellIndex) || FVector::DistSquared(PointA, PointB) <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	FSRPlanetSurfaceGridLineSegment SideLineSegment;
	SideLineSegment.LocalPointA = PointA * BodyScale;
	SideLineSegment.LocalPointB = PointB * BodyScale;
	SideLineSegment.bHasAdjacentCell = bHasAdjacentCell;
	SideLineSegment.AdjacentCellId = AdjacentCellId;
	PreparedSurfaceGridCells[CellIndex].SideLineSegments.Add(SideLineSegment);
}

void AddPreparedSurfaceGridSideFace(
	TArray<FSRPlanetSurfaceGridCell>& PreparedSurfaceGridCells,
	const TArray<int32>& CachedCellIndexByFlatId,
	int32 FaceResolution,
	float BodyScale,
	const FSRPlanetSurfaceGridCellId& CellId,
	const FSRPlanetSurfaceGridCellId& AdjacentCellId,
	bool bHasAdjacentCell,
	const FVector& Point0,
	const FVector& Point1,
	const FVector& Point2,
	const FVector& Point3)
{
	const int32 CellIndex = ResolvePreparedSurfaceGridCellIndex(CachedCellIndexByFlatId, FaceResolution, CellId);
	if (!PreparedSurfaceGridCells.IsValidIndex(CellIndex))
	{
		return;
	}

	FSRPlanetSurfaceGridSideFace SideFace;
	SideFace.LocalPoint0 = Point0 * BodyScale;
	SideFace.LocalPoint1 = Point1 * BodyScale;
	SideFace.LocalPoint2 = Point2 * BodyScale;
	SideFace.LocalPoint3 = Point3 * BodyScale;
	SideFace.bHasAdjacentCell = bHasAdjacentCell;
	SideFace.AdjacentCellId = AdjacentCellId;
	PreparedSurfaceGridCells[CellIndex].SideFaces.Add(SideFace);
}

void AddPreparedSurfaceGridSideWallOutline(
	TArray<FSRPlanetSurfaceGridCell>& PreparedSurfaceGridCells,
	const TArray<int32>& CachedCellIndexByFlatId,
	int32 FaceResolution,
	float BodyScale,
	const FSRPlanetSurfaceGridCellId& CellId,
	const FSRPlanetSurfaceGridCellId& AdjacentCellId,
	bool bHasAdjacentCell,
	const FVector& Point0,
	const FVector& Point1,
	const FVector& Point2,
	const FVector& Point3)
{
	AddPreparedSurfaceGridSideLineSegment(PreparedSurfaceGridCells, CachedCellIndexByFlatId, FaceResolution, BodyScale, CellId, AdjacentCellId, bHasAdjacentCell, Point0, Point1);
	AddPreparedSurfaceGridSideLineSegment(PreparedSurfaceGridCells, CachedCellIndexByFlatId, FaceResolution, BodyScale, CellId, AdjacentCellId, bHasAdjacentCell, Point1, Point2);
	AddPreparedSurfaceGridSideLineSegment(PreparedSurfaceGridCells, CachedCellIndexByFlatId, FaceResolution, BodyScale, CellId, AdjacentCellId, bHasAdjacentCell, Point2, Point3);
	AddPreparedSurfaceGridSideLineSegment(PreparedSurfaceGridCells, CachedCellIndexByFlatId, FaceResolution, BodyScale, CellId, AdjacentCellId, bHasAdjacentCell, Point3, Point0);
}
}
