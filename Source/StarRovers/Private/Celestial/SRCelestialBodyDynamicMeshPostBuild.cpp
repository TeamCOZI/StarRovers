#include "Celestial/SRCelestialBodyDynamicMeshInternal.h"

namespace StarRovers::Celestial::DynamicMesh
{
void CompactPreparedSurfaceGridCells(
	TArray<FSRPlanetSurfaceGridCell>& PreparedSurfaceGridCells,
	TArray<int32>& CachedCellIndexByFlatId,
	int32 ValidCellCount)
{
	if (ValidCellCount == PreparedSurfaceGridCells.Num())
	{
		return;
	}

	TArray<FSRPlanetSurfaceGridCell> CompactedSurfaceGridCells;
	CompactedSurfaceGridCells.Reserve(ValidCellCount);
	TArray<int32> CompactedCellIndexByFlatId;
	CompactedCellIndexByFlatId.Init(INDEX_NONE, CachedCellIndexByFlatId.Num());
	for (int32 FlatIndex = 0; FlatIndex < CachedCellIndexByFlatId.Num(); ++FlatIndex)
	{
		const int32 ExistingCellIndex = CachedCellIndexByFlatId[FlatIndex];
		if (!PreparedSurfaceGridCells.IsValidIndex(ExistingCellIndex))
		{
			continue;
		}

		const int32 CompactedCellIndex = CompactedSurfaceGridCells.Add(MoveTemp(PreparedSurfaceGridCells[ExistingCellIndex]));
		CompactedCellIndexByFlatId[FlatIndex] = CompactedCellIndex;
	}

	PreparedSurfaceGridCells = MoveTemp(CompactedSurfaceGridCells);
	CachedCellIndexByFlatId = MoveTemp(CompactedCellIndexByFlatId);
}

FSRCelestialBodyDynamicMeshValidationStats ValidatePreparedDynamicMeshBuild(
	const TArray<UE::Geometry::FDynamicMesh3>& FaceDynamicMeshes)
{
	FSRCelestialBodyDynamicMeshValidationStats Stats;
	if (FaceDynamicMeshes.IsValidIndex(0))
	{
		Stats.FirstMeshVertexCount = FaceDynamicMeshes[0].VertexCount();
		Stats.FirstMeshTriangleCount = FaceDynamicMeshes[0].TriangleCount();
	}

	for (const UE::Geometry::FDynamicMesh3& FaceDynamicMesh : FaceDynamicMeshes)
	{
		if (FaceDynamicMesh.TriangleCount() <= 0)
		{
			continue;
		}

		++Stats.NonEmptyDynamicMeshCount;
		Stats.WeldedBoundaryEdgeCount += CountDynamicMeshBoundaryEdges(FaceDynamicMesh);
	}
	return Stats;
}
}
