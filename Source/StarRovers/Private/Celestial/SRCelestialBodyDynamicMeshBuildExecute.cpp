#include "Celestial/SRCelestialBodyDynamicMeshPipeline.h"

namespace StarRovers::Celestial::DynamicMesh
{
	void BuildPreparedDynamicMeshCellsAndFinalize(
		const FString& BodyName,
		const FSRToonOutlineSettings& ToonOutlineSettings,
		const FSRCelestialBodyDynamicMeshBuildConfig& BuildConfig,
		const FSRCelestialBodyDynamicMeshPreBuildData& PreBuildData,
		const FSRCelestialBodyDynamicMeshPreBuildTimings& PreBuildTimings,
		int32 PendingTerrainEdgeReserveCount,
		bool bProfileBuildBreakdown,
		float BodyScale,
		uint32 DynamicMeshBuildHash,
		double TotalStart,
		FSRCelestialBodyDynamicMeshPreparedBuildStorage& BuildStorage,
		FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator& TerrainEdgeAccumulator,
		FSRCelestialBodyPreparedDynamicMesh& OutPreparedMesh)
	{
		const FSRCelestialBodyDynamicMeshSurfaceCellBuildMetrics SurfaceCellBuildMetrics = BuildPreparedDynamicMeshSurfaceCells(
			PreBuildData.BaseCellSource,
			PreBuildData.DynamicMeshGenerationSnapshot,
			PreBuildData.OceanLevelClamp,
			PreBuildData.BiomeMaterialSlotIndexById,
			BuildConfig.FaceResolution,
			BuildConfig.TerrainHeightStep,
			BodyScale,
			bProfileBuildBreakdown,
			BuildStorage.FaceDynamicMeshes,
			BuildStorage.WeldedVertexIds,
			BuildStorage.PreparedSurfaceGridCells,
			BuildStorage.CachedCellIndexByFlatId,
			BuildStorage.PreparedColorDataByFlatId,
			TerrainEdgeAccumulator);
		LogPreparedDynamicMeshBuildSummary(
			BodyName,
			ToonOutlineSettings,
			PreBuildData.BaseCellSource,
			SurfaceCellBuildMetrics,
			TerrainEdgeAccumulator,
			PreBuildTimings,
			PendingTerrainEdgeReserveCount,
			bProfileBuildBreakdown);

		FinalizePreparedDynamicMeshBuild(
			BodyName,
			DynamicMeshBuildHash,
			TotalStart,
			TerrainEdgeAccumulator,
			BuildStorage.FaceDynamicMeshes,
			BuildStorage.PreparedSurfaceGridCells,
			BuildStorage.CachedCellIndexByFlatId,
			BuildStorage.PreparedColorDataByFlatId,
			OutPreparedMesh);
	}
}
