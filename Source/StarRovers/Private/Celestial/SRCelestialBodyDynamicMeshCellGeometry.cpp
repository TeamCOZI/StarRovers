#include "Celestial/SRCelestialBodyDynamicMeshPipeline.h"

namespace StarRovers::Celestial::DynamicMesh
{
namespace
{
void FillDynamicMeshSurfaceCellSourceHashes(
	FSRCelestialBodyDynamicMeshSurfaceCellGeometry& Geometry,
	const FSRCelestialBodyDynamicMeshBaseCellView& BaseCell,
	const TArray<FSRDynamicMeshBaseSourceMetadataCell>* BaseSourceMetadataCells,
	int32 BaseCellIndex)
{
	if (BaseSourceMetadataCells && BaseSourceMetadataCells->IsValidIndex(BaseCellIndex))
	{
		const FSRDynamicMeshBaseSourceMetadataCell& SourceMetadata = (*BaseSourceMetadataCells)[BaseCellIndex];
		Geometry.SourcePositionHashes[0] = SourceMetadata.CornerHash00;
		Geometry.SourcePositionHashes[1] = SourceMetadata.CornerHash10;
		Geometry.SourcePositionHashes[2] = SourceMetadata.CornerHash11;
		Geometry.SourcePositionHashes[3] = SourceMetadata.CornerHash01;
		return;
	}

	Geometry.SourcePositionHashes[0] = HashSourcePosition(BaseCell.Corner00);
	Geometry.SourcePositionHashes[1] = HashSourcePosition(BaseCell.Corner10);
	Geometry.SourcePositionHashes[2] = HashSourcePosition(BaseCell.Corner11);
	Geometry.SourcePositionHashes[3] = HashSourcePosition(BaseCell.Corner01);
}
}

FSRCelestialBodyDynamicMeshSurfaceCellGeometry BuildDynamicMeshSurfaceCellGeometry(
	const FSRCelestialBodyDynamicMeshBaseCellView& BaseCell,
	const FSRPlanetTerrainSample& TerrainSample,
	const FVector& CellDirection,
	const TArray<FSRDynamicMeshBaseSourceMetadataCell>* BaseSourceMetadataCells,
	int32 BaseCellIndex,
	bool bProfileBuildBreakdown,
	double& CellTransformMs,
	double& SourceHashMs)
{
	FSRCelestialBodyDynamicMeshSurfaceCellGeometry Geometry;
	{
		const double InnerStart = bProfileBuildBreakdown ? GetDynamicMeshTimingSeconds() : 0.0;
		const float SourceCellRadius = FMath::Max(BaseCell.LocalCenter.Length(), 1.0f);
		const float CellScale = FMath::Max(0.01f, (SourceCellRadius + TerrainSample.HeightOffset) / SourceCellRadius);
		Geometry.TargetCellCenter = CellDirection * (SourceCellRadius + TerrainSample.HeightOffset);
		Geometry.TargetPositions[0] = BaseCell.Corner00 * CellScale;
		Geometry.TargetPositions[1] = BaseCell.Corner10 * CellScale;
		Geometry.TargetPositions[2] = BaseCell.Corner11 * CellScale;
		Geometry.TargetPositions[3] = BaseCell.Corner01 * CellScale;

		Geometry.CellNormal = FVector::CrossProduct(Geometry.TargetPositions[1] - Geometry.TargetPositions[0], Geometry.TargetPositions[2] - Geometry.TargetPositions[0]).GetSafeNormal();
		if (FVector::DotProduct(Geometry.CellNormal, CellDirection) < 0.0f)
		{
			Swap(Geometry.TargetPositions[1], Geometry.TargetPositions[3]);
			Geometry.CellNormal *= -1.0f;
			Geometry.bSwappedCellWinding = true;
		}
		if (Geometry.CellNormal.IsNearlyZero())
		{
			Geometry.CellNormal = CellDirection;
		}
		if (bProfileBuildBreakdown)
		{
			CellTransformMs += GetDynamicMeshTimingElapsedMilliseconds(InnerStart);
		}
	}

	{
		const double InnerStart = bProfileBuildBreakdown ? GetDynamicMeshTimingSeconds() : 0.0;
		FillDynamicMeshSurfaceCellSourceHashes(Geometry, BaseCell, BaseSourceMetadataCells, BaseCellIndex);
		if (bProfileBuildBreakdown)
		{
			SourceHashMs += GetDynamicMeshTimingElapsedMilliseconds(InnerStart);
		}
	}

	if (Geometry.bSwappedCellWinding)
	{
		Swap(Geometry.SourcePositionHashes[1], Geometry.SourcePositionHashes[3]);
	}

	for (int32 CornerIndex = 0; CornerIndex < 4; ++CornerIndex)
	{
		Geometry.SurfaceVertexKeys[CornerIndex] = MakeCelestialBodyDynamicMeshTerrainVertexKey(Geometry.SourcePositionHashes[CornerIndex], TerrainSample.HeightOffset);
	}
	return Geometry;
}
}
