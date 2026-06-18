#include "Celestial/SRCelestialBodyDynamicMeshInternal.h"

namespace StarRovers::Celestial::DynamicMesh
{
FSRCelestialBodyDynamicMeshQuadRenderData AppendDynamicMeshSurfaceCellQuad(
	TArray<UE::Geometry::FDynamicMesh3>& FaceDynamicMeshes,
	TMap<FSRTerrainVertexKey, int32>& WeldedVertexIds,
	const FSRPlanetSurfaceGridCellId& CellId,
	const FSRCelestialBodyDynamicMeshSurfaceCellGeometry& CellGeometry,
	const FSRPlanetTerrainSample& TerrainSample,
	int32 MaterialId,
	bool bProfileBuildBreakdown,
	double& SurfaceAppendMs)
{
	const double InnerStart = bProfileBuildBreakdown ? SRCelestialNowSeconds() : 0.0;
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
		SurfaceAppendMs += SRCelestialElapsedMilliseconds(InnerStart);
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
	const double InnerStart = bProfileBuildBreakdown ? SRCelestialNowSeconds() : 0.0;
	if (PreparedColorDataByFlatId.IsValidIndex(CellFlatIndex))
	{
		PreparedColorDataByFlatId[CellFlatIndex].SurfaceColorElements.Append(SurfaceRenderData.ColorElements);
	}
	if (bProfileBuildBreakdown)
	{
		ColorDataMs += SRCelestialElapsedMilliseconds(InnerStart);
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
	const double InnerStart = bProfileBuildBreakdown ? SRCelestialNowSeconds() : 0.0;
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
	CachedCell.Neighbors = BaseCell.Neighbors;

	CachedCellIndexByFlatId[CellFlatIndex] = CachedCellIndex;
	if (bProfileBuildBreakdown)
	{
		CacheCellMs += SRCelestialElapsedMilliseconds(InnerStart);
	}
}
}
