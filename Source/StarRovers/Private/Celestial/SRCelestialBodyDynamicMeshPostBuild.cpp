#include "Celestial/SRCelestialBodyDynamicMeshPipeline.h"

#include "SRCelestialBodyLog.h"
#include "Utility/SRTimingLog.h"

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

void FinalizePreparedDynamicMeshBuild(
	const FString& BodyName,
	uint32 DynamicMeshBuildHash,
	double TotalStart,
	const FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator& TerrainEdgeAccumulator,
	TArray<UE::Geometry::FDynamicMesh3>& FaceDynamicMeshes,
	TArray<FSRPlanetSurfaceGridCell>& PreparedSurfaceGridCells,
	TArray<int32>& CachedCellIndexByFlatId,
	TArray<FSRCelestialBodyDynamicMeshCellColorData>& PreparedColorDataByFlatId,
	FSRCelestialBodyPreparedDynamicMesh& OutPreparedMesh)
{
	const double PostBuildStart = GetDynamicMeshTimingSeconds();
	double PendingEdgesCheckMs = 0.0;
	double WeldedMeshCheckMs = 0.0;
	double PreparedMoveMs = 0.0;

	double PostBuildStageStart = GetDynamicMeshTimingSeconds();
	if (TerrainEdgeAccumulator.GetPendingEdgeCount() > 0)
	{
		UE_LOG(
			LogStarRoversCelestial,
			Warning,
			TEXT("Dynamic mesh '%s' code-generated base has %d unmatched source edges."),
			*BodyName,
			TerrainEdgeAccumulator.GetPendingEdgeCount());
	}
	PendingEdgesCheckMs = GetDynamicMeshTimingElapsedMilliseconds(PostBuildStageStart);

	PostBuildStageStart = GetDynamicMeshTimingSeconds();
	const FSRCelestialBodyDynamicMeshValidationStats ValidationStats = ValidatePreparedDynamicMeshBuild(FaceDynamicMeshes);
	if (ValidationStats.WeldedBoundaryEdgeCount > 0)
	{
		UE_LOG(
			LogStarRoversCelestial,
			Warning,
			TEXT("Dynamic mesh '%s' generated with %d open boundary edges after code-generated base welding."),
			*BodyName,
			ValidationStats.WeldedBoundaryEdgeCount);
	}
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("DynamicMesh '%s' BaseMetadata.WeldedMeshCheck BoundaryEdges=%d Meshes=%d Vertices=%d Triangles=%d"),
		*BodyName,
		ValidationStats.WeldedBoundaryEdgeCount,
		ValidationStats.NonEmptyDynamicMeshCount,
		ValidationStats.FirstMeshVertexCount,
		ValidationStats.FirstMeshTriangleCount));
	WeldedMeshCheckMs = GetDynamicMeshTimingElapsedMilliseconds(PostBuildStageStart);

	const FSRCelestialBodyDynamicMeshTerrainEdgeStats& TerrainEdgeStats = TerrainEdgeAccumulator.GetStats();
	PostBuildStageStart = GetDynamicMeshTimingSeconds();
	OutPreparedMesh.bValid = true;
	OutPreparedMesh.BuildHash = DynamicMeshBuildHash;
	OutPreparedMesh.FaceDynamicMeshes = MoveTemp(FaceDynamicMeshes);
	OutPreparedMesh.FeatureEdgeMaskCount = TerrainEdgeStats.FeatureEdgeMaskCount;
	OutPreparedMesh.SurfaceGridCells = MoveTemp(PreparedSurfaceGridCells);
	OutPreparedMesh.CellIndexByFlatId = MoveTemp(CachedCellIndexByFlatId);
	OutPreparedMesh.ColorDataByFlatId = MoveTemp(PreparedColorDataByFlatId);
	PreparedMoveMs = GetDynamicMeshTimingElapsedMilliseconds(PostBuildStageStart);
	OutPreparedMesh.BuildMilliseconds = GetDynamicMeshTimingElapsedMilliseconds(TotalStart);
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("DynamicMesh '%s' BaseMetadata.PostBuildBreakdown PendingEdges=%.2f ms WeldedMeshCheck=%.2f ms PreparedMove=%.2f ms PostBuildTotal=%.2f ms"),
		*BodyName,
		PendingEdgesCheckMs,
		WeldedMeshCheckMs,
		PreparedMoveMs,
		GetDynamicMeshTimingElapsedMilliseconds(PostBuildStart)));
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("DynamicMesh '%s' BaseMetadata.Prepared %.2f ms"),
		*BodyName,
		OutPreparedMesh.BuildMilliseconds));
}
}
