#include "SRPlanetSurfaceGridCellQuery.h"

#include "Surface/SRPlanetSurfaceGridLibrary.h"

namespace StarRovers::SurfaceGridCellQuery
{
	bool GetCellById(
		const TArray<FSRPlanetSurfaceGridCell>& Cells,
		const FSRPlanetSurfaceGridCellId& CellId,
		FCellIndexLookup GetCellIndex,
		FSRPlanetSurfaceGridCell& OutCell)
	{
		int32 CellIndex = INDEX_NONE;
		if (!GetCellIndex(CellId, CellIndex))
		{
			OutCell = FSRPlanetSurfaceGridCell();
			return false;
		}

		OutCell = Cells[CellIndex];
		return true;
	}

	bool GetCellInfoById(
		const FSRPlanetSurfaceGridCellId& CellId,
		FStoredCellInfoLookup GetStoredCellInfoById,
		FRuntimeCellInfoResolver ResolveRuntimeCellInfo,
		FSRPlanetSurfaceGridCellInfo& OutCellInfo)
	{
		FSRPlanetSurfaceGridCellInfo FoundCellInfo;
		if (!GetStoredCellInfoById(CellId, FoundCellInfo))
		{
			OutCellInfo = FSRPlanetSurfaceGridCellInfo();
			return false;
		}

		OutCellInfo = ResolveRuntimeCellInfo(FoundCellInfo);
		return true;
	}

	bool GetCellNeighbors(
		const FSRPlanetSurfaceGridCellId& CellId,
		int32 FaceResolution,
		FCellIndexLookup GetCellIndex,
		FSRPlanetSurfaceGridCellNeighbors& OutNeighbors)
	{
		int32 CellIndex = INDEX_NONE;
		if (!GetCellIndex(CellId, CellIndex))
		{
			OutNeighbors = FSRPlanetSurfaceGridCellNeighbors();
			return false;
		}

		OutNeighbors = USRPlanetSurfaceGridLibrary::GetCubeSphereNeighborIds(CellId, FaceResolution);
		return true;
	}
}
