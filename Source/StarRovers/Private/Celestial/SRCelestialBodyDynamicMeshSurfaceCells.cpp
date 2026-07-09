#include "Celestial/SRCelestialBodyDynamicMeshPipeline.h"

#include "Surface/SRPlanetSurfaceGrid.h"

namespace StarRovers::Celestial::DynamicMesh
{
FSRCelestialBodyDynamicMeshQuadRenderData AppendDynamicMeshSurfaceCellQuad(
	TArray<UE::Geometry::FDynamicMesh3>& FaceDynamicMeshes,
	TMap<FSRCelestialBodyDynamicMeshTerrainVertexKey, int32>& WeldedVertexIds,
	const FSRPlanetSurfaceGridCellId& CellId,
	const FSRCelestialBodyDynamicMeshSurfaceCellGeometry& CellGeometry,
	const FSRPlanetTerrainSample& TerrainSample,
	int32 MaterialId,
	bool bProfileBuildBreakdown,
	double& SurfaceAppendMs)
{
	const double InnerStart = bProfileBuildBreakdown ? GetDynamicMeshTimingSeconds() : 0.0;
	FSRCelestialBodyDynamicMeshQuadRenderData SurfaceRenderData = AppendFlatColoredDynamicMeshQuad(
		FaceDynamicMeshes,
		WeldedVertexIds,
		GetCubeSphereFaceComponentIndex(CellId.Face),
		CellGeometry.TargetPositions[0],
		CellGeometry.TargetPositions[1],
		CellGeometry.TargetPositions[2],
		CellGeometry.TargetPositions[3],
		TerrainSample.SurfaceColor,
		MaterialId,
		false,
		CellGeometry.SurfaceVertexKeys);
	if (bProfileBuildBreakdown)
	{
		SurfaceAppendMs += GetDynamicMeshTimingElapsedMilliseconds(InnerStart);
	}
	return SurfaceRenderData;
}

void RecordDynamicMeshSurfaceColorData(
	TArray<FSRCelestialBodyDynamicMeshCellColorData>& PreparedColorDataByFlatId,
	int32 CellFlatIndex,
	const FSRCelestialBodyDynamicMeshQuadRenderData& SurfaceRenderData,
	bool bProfileBuildBreakdown,
	double& ColorDataMs)
{
	const double InnerStart = bProfileBuildBreakdown ? GetDynamicMeshTimingSeconds() : 0.0;
	if (PreparedColorDataByFlatId.IsValidIndex(CellFlatIndex))
	{
		PreparedColorDataByFlatId[CellFlatIndex].SurfaceColorElements.Append(SurfaceRenderData.ColorElements);
	}
	if (bProfileBuildBreakdown)
	{
		ColorDataMs += GetDynamicMeshTimingElapsedMilliseconds(InnerStart);
	}
}

void CacheDynamicMeshSurfaceGridCell(
	TArray<FSRPlanetSurfaceGridCell>& PreparedSurfaceGridCells,
	TArray<int32>& CachedCellIndexByFlatId,
	int32 CellFlatIndex,
	const FSRCelestialBodyDynamicMeshBaseCellView& BaseCell,
	const FSRCelestialBodyDynamicMeshSurfaceCellGeometry& CellGeometry,
	const FSRPlanetTerrainSample& TerrainSample,
	float BodyScale,
	bool bProfileBuildBreakdown,
	double& CacheCellMs)
{
	const double InnerStart = bProfileBuildBreakdown ? GetDynamicMeshTimingSeconds() : 0.0;
	const int32 CachedCellIndex = CellFlatIndex;
	FSRPlanetSurfaceGridCell& CachedCell = PreparedSurfaceGridCells[CachedCellIndex];
	CachedCell.CellId = BaseCell.CellId;
	CachedCell.FaceUVMin = BaseCell.FaceUVMin;
	CachedCell.FaceUVMax = BaseCell.FaceUVMax;
	CachedCell.LocalCenter = CellGeometry.TargetCellCenter * BodyScale;
	CachedCell.LocalNormal = CellGeometry.CellNormal;
	CachedCell.Corner00 = CellGeometry.TargetPositions[0] * BodyScale;
	CachedCell.Corner10 = CellGeometry.TargetPositions[1] * BodyScale;
	CachedCell.Corner11 = CellGeometry.TargetPositions[2] * BodyScale;
	CachedCell.Corner01 = CellGeometry.TargetPositions[3] * BodyScale;
	CachedCell.ApproxSurfaceArea =
		(FVector::CrossProduct(CachedCell.Corner10 - CachedCell.Corner00, CachedCell.Corner11 - CachedCell.Corner00).Size() * 0.5f)
		+ (FVector::CrossProduct(CachedCell.Corner11 - CachedCell.Corner00, CachedCell.Corner01 - CachedCell.Corner00).Size() * 0.5f);
	CachedCell.Biome = TerrainSample.Biome;
	CachedCell.BiomeId = TerrainSample.BiomeId;
	CachedCell.WaterRole = TerrainSample.WaterRole;
	CachedCell.SurfaceTemperature = TerrainSample.Temperature;
	CachedCell.TemperatureState = USRPlanetSurfaceGrid::ResolveTemperatureStateFromSurfaceTemperature(TerrainSample.Temperature);
	CachedCell.Neighbors = BaseCell.Neighbors;

	CachedCellIndexByFlatId[CellFlatIndex] = CachedCellIndex;
	if (bProfileBuildBreakdown)
	{
		CacheCellMs += GetDynamicMeshTimingElapsedMilliseconds(InnerStart);
	}
}

FSRCelestialBodyDynamicMeshSurfaceCellBuildMetrics BuildPreparedDynamicMeshSurfaceCells(
	const FSRCelestialBodyDynamicMeshBaseCellSource& BaseCellSource,
	const FSRDynamicMeshGenerationSnapshot& DynamicMeshGeneration,
	const FSRCelestialBodyDynamicMeshOceanLevelClamp& OceanLevelClamp,
	const TMap<FName, int32>& BiomeMaterialSlotIndexById,
	int32 FaceResolution,
	float TerrainHeightStep,
	float BodyScale,
	bool bProfileBuildBreakdown,
	TArray<UE::Geometry::FDynamicMesh3>& FaceDynamicMeshes,
	TMap<FSRCelestialBodyDynamicMeshTerrainVertexKey, int32>& WeldedVertexIds,
	TArray<FSRPlanetSurfaceGridCell>& PreparedSurfaceGridCells,
	TArray<int32>& CachedCellIndexByFlatId,
	TArray<FSRCelestialBodyDynamicMeshCellColorData>& PreparedColorDataByFlatId,
	FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator& TerrainEdgeAccumulator)
{
	FSRCelestialBodyDynamicMeshSurfaceCellBuildMetrics SurfaceCellBuildMetrics;
	const double BuildCellsStart = GetDynamicMeshTimingSeconds();
	const int32 BaseCellCount = BaseCellSource.bUsingPrecomputedBaseCells
		? BaseCellSource.PrecomputedBaseCells->Num()
		: BaseCellSource.GeneratedBaseCells.Num();
	for (int32 BaseCellIndex = 0; BaseCellIndex < BaseCellCount; ++BaseCellIndex)
	{
		const FSRCelestialBodyDynamicMeshBaseCellView BaseCell = MakeDynamicMeshBaseCellView(
			BaseCellIndex,
			BaseCellSource.PrecomputedBaseCells,
			BaseCellSource.PrecomputedBaseCellScale,
			BaseCellSource.GeneratedBaseCells);
		const FSRPlanetSurfaceGridCellId CellId = BaseCell.CellId;
		if (!CellId.IsValid(FaceResolution))
		{
			continue;
		}
		const int32 CellFlatIndex = ((static_cast<int32>(CellId.Face) * FaceResolution) + CellId.CellY) * FaceResolution + CellId.CellX;
		if (!PreparedSurfaceGridCells.IsValidIndex(CellFlatIndex) || !CachedCellIndexByFlatId.IsValidIndex(CellFlatIndex))
		{
			continue;
		}

		const FVector CellDirection = BaseCell.LocalCenter.GetSafeNormal();
		if (CellDirection.IsNearlyZero())
		{
			continue;
		}

		FSRBiomeSampleContext TerrainSampleContext;
		TerrainSampleContext.LocalUnitDirection = CellDirection;
		TerrainSampleContext.Face = CellId.Face;
		TerrainSampleContext.CellX = CellId.CellX;
		TerrainSampleContext.CellY = CellId.CellY;
		TerrainSampleContext.FaceResolution = FaceResolution;
		TerrainSampleContext.FaceUV = (BaseCell.FaceUVMin + BaseCell.FaceUVMax) * 0.5f;

		FSRPlanetTerrainSample TerrainSample;
		if (bProfileBuildBreakdown)
		{
			const double InnerStart = GetDynamicMeshTimingSeconds();
			TerrainSample = SampleTerrainForDynamicMesh(
				TerrainSampleContext,
				DynamicMeshGeneration,
				TerrainHeightStep,
				OceanLevelClamp.bApplyOceanLevelHeightClamp,
				OceanLevelClamp.OceanLevelClampHeightOffset);
			SurfaceCellBuildMetrics.TerrainSampleMs += GetDynamicMeshTimingElapsedMilliseconds(InnerStart);
		}
		else
		{
			TerrainSample = SampleTerrainForDynamicMesh(
				TerrainSampleContext,
				DynamicMeshGeneration,
				TerrainHeightStep,
				OceanLevelClamp.bApplyOceanLevelHeightClamp,
				OceanLevelClamp.OceanLevelClampHeightOffset);
		}

		const FSRCelestialBodyDynamicMeshSurfaceCellGeometry CellGeometry = BuildDynamicMeshSurfaceCellGeometry(
			BaseCell,
			TerrainSample,
			CellDirection,
			BaseCellSource.BaseSourceMetadataCells,
			BaseCellIndex,
			bProfileBuildBreakdown,
			SurfaceCellBuildMetrics.CellTransformMs,
			SurfaceCellBuildMetrics.SourceHashMs);
		const int32 MaterialId = BiomeMaterialSlotIndexById.FindRef(TerrainSample.BiomeId);
		const FSRCelestialBodyDynamicMeshQuadRenderData SurfaceRenderData = AppendDynamicMeshSurfaceCellQuad(
			FaceDynamicMeshes,
			WeldedVertexIds,
			CellId,
			CellGeometry,
			TerrainSample,
			MaterialId,
			bProfileBuildBreakdown,
			SurfaceCellBuildMetrics.SurfaceAppendMs);
		RecordDynamicMeshSurfaceColorData(
			PreparedColorDataByFlatId,
			CellFlatIndex,
			SurfaceRenderData,
			bProfileBuildBreakdown,
			SurfaceCellBuildMetrics.ColorDataMs);
		CacheDynamicMeshSurfaceGridCell(
			PreparedSurfaceGridCells,
			CachedCellIndexByFlatId,
			CellFlatIndex,
			BaseCell,
			CellGeometry,
			TerrainSample,
			BodyScale,
			bProfileBuildBreakdown,
			SurfaceCellBuildMetrics.CacheCellMs);
		TerrainEdgeAccumulator.RegisterCellEdges(
			CellGeometry,
			TerrainSample,
			SurfaceRenderData,
			MaterialId,
			CellId,
			bProfileBuildBreakdown,
			SurfaceCellBuildMetrics.TerrainEdgeRegisterMs);
		++SurfaceCellBuildMetrics.ValidCellCount;
	}
	TerrainEdgeAccumulator.FlushPendingSideWallFeatureMaskEdges();
	CompactPreparedSurfaceGridCells(PreparedSurfaceGridCells, CachedCellIndexByFlatId, SurfaceCellBuildMetrics.ValidCellCount);
	SurfaceCellBuildMetrics.CellLoopMs = GetDynamicMeshTimingElapsedMilliseconds(BuildCellsStart);
	return SurfaceCellBuildMetrics;
}
}
