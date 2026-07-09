#include "SRPlanetSurfaceGridFootprint.h"

bool StarRovers::SurfaceGridFootprint::BuildFootprintCellIds(
	const FSRPlanetSurfaceGridCellId& OriginCellId,
	int32 FootprintCellsX,
	int32 FootprintCellsY,
	int32 FaceResolution,
	FCellIndexQuery GetCellIndex,
	TArray<FSRPlanetSurfaceGridCellId>& OutCellIds)
{
	OutCellIds.Reset();

	const int32 SafeFootprintCellsX = FMath::Max(1, FootprintCellsX);
	const int32 SafeFootprintCellsY = FMath::Max(1, FootprintCellsY);
	const int32 SafeFaceResolution = FMath::Max(1, FaceResolution);
	if (!OriginCellId.IsValid(SafeFaceResolution))
	{
		return false;
	}

	const int32 StartCellX = OriginCellId.CellX - (SafeFootprintCellsX / 2);
	const int32 StartCellY = OriginCellId.CellY - (SafeFootprintCellsY / 2);
	const int32 EndCellX = StartCellX + SafeFootprintCellsX - 1;
	const int32 EndCellY = StartCellY + SafeFootprintCellsY - 1;
	if (StartCellX < 0 || StartCellY < 0 || EndCellX >= SafeFaceResolution || EndCellY >= SafeFaceResolution)
	{
		return false;
	}

	OutCellIds.Reserve(SafeFootprintCellsX * SafeFootprintCellsY);
	for (int32 CellY = StartCellY; CellY <= EndCellY; ++CellY)
	{
		for (int32 CellX = StartCellX; CellX <= EndCellX; ++CellX)
		{
			FSRPlanetSurfaceGridCellId FootprintCellId;
			FootprintCellId.Face = OriginCellId.Face;
			FootprintCellId.CellX = CellX;
			FootprintCellId.CellY = CellY;

			int32 FootprintCellIndex = INDEX_NONE;
			if (!GetCellIndex(FootprintCellId, FootprintCellIndex))
			{
				OutCellIds.Reset();
				return false;
			}

			OutCellIds.Add(FootprintCellId);
		}
	}

	return !OutCellIds.IsEmpty();
}
