#include "Celestial/SRCelestialBodyDynamicMeshPipeline.h"

#include "Utility/SRLog.h"
#include "SRCelestialBodyLog.h"
#include "Utility/SRTimingLog.h"

namespace StarRovers::Celestial::DynamicMesh
{
void LogPreparedDynamicMeshBuildBegin(
	const FString& BodyName,
	const FString& SourceName,
	int32 FaceResolution,
	int32 CellCount,
	float SourceBodyRadius)
{
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("DynamicMesh '%s' BaseMetadata Begin Source='%s' Shape=CubeSphere Resolution=%d Cells=%d Radius=%.3f"),
		*BodyName,
		*SourceName,
		FaceResolution,
		CellCount,
		SourceBodyRadius));
}

void LogPreparedDynamicMeshPreBuild(
	const FString& BodyName,
	const FSRDynamicMeshGenerationSnapshot& DynamicMeshGeneration,
	const FSRCelestialBodyDynamicMeshOceanLevelClamp& OceanLevelClamp,
	const FSRCelestialBodyDynamicMeshPreBuildTimings& Timings,
	int32 OceanLevelSampleCount)
{
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("DynamicMesh '%s' BaseMetadata.PreBuildBreakdown Validation=%.2f ms MeshSetup=%.2f ms ContainerReserve=%.2f ms EdgeReserve=%.2f ms BiomeMap=%.2f ms Snapshot=%.2f ms OceanLevel=%.2f ms BaseGrid=%.2f ms BaseSourceMeta=%.2f ms PreBuildTotal=%.2f ms"),
		*BodyName,
		Timings.ValidationMs,
		Timings.MeshSetupMs,
		Timings.ContainerReserveMs,
		Timings.EdgeReserveMs,
		Timings.BiomeMaterialMapMs,
		Timings.SnapshotMs,
		Timings.OceanLevelMs,
		Timings.BaseCellsMs,
		Timings.BaseSourceMetadataMs,
		Timings.TotalMs));
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("DynamicMesh '%s' BaseMetadata.OceanLevelClamp Enabled=%s Resolved=%s OceanHeightOffset=%.3f ClampHeightOffset=%.3f Samples=%d"),
		*BodyName,
		DynamicMeshGeneration.bClampTerrainHeightToOceanLevel ? TEXT("true") : TEXT("false"),
		OceanLevelClamp.bApplyOceanLevelHeightClamp ? TEXT("true") : TEXT("false"),
		OceanLevelClamp.OceanLevelHeightOffset,
		OceanLevelClamp.OceanLevelClampHeightOffset,
		OceanLevelSampleCount));
}

void LogPreparedDynamicMeshBuildSummary(
	const FString& BodyName,
	const FSRToonOutlineSettings& ToonOutlineSettings,
	const FSRCelestialBodyDynamicMeshBaseCellSource& BaseCellSource,
	const FSRCelestialBodyDynamicMeshSurfaceCellBuildMetrics& SurfaceCellBuildMetrics,
	const FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator& TerrainEdgeAccumulator,
	const FSRCelestialBodyDynamicMeshPreBuildTimings& Timings,
	int32 PendingTerrainEdgeReserveCount,
	bool bProfileBuildBreakdown)
{
	const FSRCelestialBodyDynamicMeshTerrainEdgeStats& TerrainEdgeStats = TerrainEdgeAccumulator.GetStats();
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("DynamicMesh '%s' BaseMetadata.BuildCells %.2f ms BaseCells=%d Cells=%d EdgeRegisters=%d EdgeMatches=%d SideWalls=%d PendingEdges=%d MaxPendingEdges=%d PendingReserve=%d"),
		*BodyName,
		Timings.BaseCellsMs + Timings.BaseSourceMetadataMs + SurfaceCellBuildMetrics.CellLoopMs,
		BaseCellSource.GeneratedBaseCells.Num(),
		SurfaceCellBuildMetrics.ValidCellCount,
		TerrainEdgeStats.RegisterCount,
		TerrainEdgeStats.MatchCount,
		TerrainEdgeStats.SideWallCount,
		TerrainEdgeAccumulator.GetPendingEdgeCount(),
		TerrainEdgeStats.MaxPendingEdgeCount,
		PendingTerrainEdgeReserveCount));
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("DynamicMesh '%s' BaseMetadata.ToonOutline FeatureMasks=%d AngleThreshold=%.2f Thickness=%.4f"),
		*BodyName,
		TerrainEdgeStats.FeatureEdgeMaskCount,
		FMath::Clamp(ToonOutlineSettings.FeatureEdgeAngleThresholdDegrees, 0.0f, 90.0f),
		FMath::Clamp(ToonOutlineSettings.ToonLineThickness, 0.0f, 0.25f)));

	const int32 SideWallUnpatchedTriangleCount = FMath::Max(
		0,
		TerrainEdgeStats.SideWallFailedTriangleCount - TerrainEdgeStats.SideWallFallbackTriangleCount);
	const TCHAR* SideWallPatchStatus = SideWallUnpatchedTriangleCount > 0
		? TEXT("UnpatchedTriangles")
		: (TerrainEdgeStats.SideWallFailedTriangleCount > 0 ? TEXT("FallbackPatched") : TEXT("NoFallbackNeeded"));
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("DynamicMesh '%s' BaseMetadata.SideWallPatch FailedTriangles=%d FallbackTriangles=%d UnpatchedTriangles=%d Status=%s"),
		*BodyName,
		TerrainEdgeStats.SideWallFailedTriangleCount,
		TerrainEdgeStats.SideWallFallbackTriangleCount,
		SideWallUnpatchedTriangleCount,
		SideWallPatchStatus));
	if (SideWallUnpatchedTriangleCount > 0)
	{
		SR_LOG(DynamicMesh, LogStarRoversCelestial,
			Warning,
			TEXT("Dynamic mesh '%s' left %d side-wall triangles unpatched after fallback."),
			*BodyName,
			SideWallUnpatchedTriangleCount);
	}

	FSRTimingLog::AddLine(FString::Printf(
		TEXT("DynamicMesh '%s' BaseMetadata.BuildBreakdown Profile=%s BaseGridSource=%s BaseSourceMetaSource=%s BiomeMap=%.2f ms Snapshot=%.2f ms OceanLevel=%.2f ms BaseGrid=%.2f ms BaseSourceMeta=%.2f ms CellLoop=%.2f ms TerrainSample=%.2f ms Transform=%.2f ms SourceHash=%.2f ms SurfaceAppend=%.2f ms ColorData=%.2f ms CacheCell=%.2f ms EdgeRegister=%.2f ms"),
		*BodyName,
		bProfileBuildBreakdown ? TEXT("true") : TEXT("false"),
		BaseCellSource.bUsingPrecomputedBaseCells ? TEXT("Precomputed") : TEXT("Generated"),
		BaseCellSource.bUsingPrecomputedSourceMetadata ? TEXT("Precomputed") : TEXT("Generated"),
		Timings.BiomeMaterialMapMs,
		Timings.SnapshotMs,
		Timings.OceanLevelMs,
		Timings.BaseCellsMs,
		Timings.BaseSourceMetadataMs,
		SurfaceCellBuildMetrics.CellLoopMs,
		SurfaceCellBuildMetrics.TerrainSampleMs,
		SurfaceCellBuildMetrics.CellTransformMs,
		SurfaceCellBuildMetrics.SourceHashMs,
		SurfaceCellBuildMetrics.SurfaceAppendMs,
		SurfaceCellBuildMetrics.ColorDataMs,
		SurfaceCellBuildMetrics.CacheCellMs,
		SurfaceCellBuildMetrics.TerrainEdgeRegisterMs));
}
}
