#include "Celestial/SRCelestialBodyDynamicMeshInternal.h"

namespace StarRovers::Celestial::DynamicMesh
{
FSRCelestialBodyDynamicMeshBaseCellView MakeDynamicMeshBaseCellView(
	int32 BaseCellIndex,
	const TArray<FSRDynamicMeshBasePrecomputedCell>* PrecomputedBaseCells,
	float PrecomputedBaseCellScale,
	const TArray<FSRPlanetSurfaceGridCell>& GeneratedBaseCells)
{
	FSRCelestialBodyDynamicMeshBaseCellView CellView;
	if (PrecomputedBaseCells)
	{
		const FSRDynamicMeshBasePrecomputedCell& PrecomputedCell = (*PrecomputedBaseCells)[BaseCellIndex];
		CellView.CellId = PrecomputedCell.CellId;
		CellView.LocalCenter = PrecomputedCell.LocalCenter * PrecomputedBaseCellScale;
		CellView.LocalNormal = PrecomputedCell.LocalNormal;
		CellView.Corner00 = PrecomputedCell.Corner00 * PrecomputedBaseCellScale;
		CellView.Corner10 = PrecomputedCell.Corner10 * PrecomputedBaseCellScale;
		CellView.Corner11 = PrecomputedCell.Corner11 * PrecomputedBaseCellScale;
		CellView.Corner01 = PrecomputedCell.Corner01 * PrecomputedBaseCellScale;
		CellView.FaceUVMin = PrecomputedCell.FaceUVMin;
		CellView.FaceUVMax = PrecomputedCell.FaceUVMax;
		CellView.ApproxSurfaceArea = PrecomputedCell.ApproxSurfaceArea * PrecomputedBaseCellScale * PrecomputedBaseCellScale;
		CellView.Neighbors = PrecomputedCell.Neighbors;
		return CellView;
	}

	const FSRPlanetSurfaceGridCell& GeneratedCell = GeneratedBaseCells[BaseCellIndex];
	CellView.CellId = GeneratedCell.CellId;
	CellView.LocalCenter = GeneratedCell.LocalCenter;
	CellView.LocalNormal = GeneratedCell.LocalNormal;
	CellView.Corner00 = GeneratedCell.Corner00;
	CellView.Corner10 = GeneratedCell.Corner10;
	CellView.Corner11 = GeneratedCell.Corner11;
	CellView.Corner01 = GeneratedCell.Corner01;
	CellView.FaceUVMin = GeneratedCell.FaceUVMin;
	CellView.FaceUVMax = GeneratedCell.FaceUVMax;
	CellView.ApproxSurfaceArea = GeneratedCell.ApproxSurfaceArea;
	CellView.Neighbors = GeneratedCell.Neighbors;
	return CellView;
}
}
