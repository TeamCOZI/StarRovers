#include "SRPlanetSurfaceGridFootprint.h"

namespace
{
	int32 ResolveFootprintHoverAnchorOffset(int32 FootprintCells)
	{
		return (FMath::Max(1, FootprintCells) - 1) / 2;
	}
}

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

	const int32 AnchorOffsetX = ResolveFootprintHoverAnchorOffset(SafeFootprintCellsX);
	const int32 AnchorOffsetY = ResolveFootprintHoverAnchorOffset(SafeFootprintCellsY);
	const int32 GridOriginCellX = OriginCellId.CellX - AnchorOffsetX;
	const int32 GridOriginCellY = OriginCellId.CellY - AnchorOffsetY;
	const int32 EndCellX = GridOriginCellX + SafeFootprintCellsX - 1;
	const int32 EndCellY = GridOriginCellY + SafeFootprintCellsY - 1;
	if (GridOriginCellX < 0 || GridOriginCellY < 0 || EndCellX >= SafeFaceResolution || EndCellY >= SafeFaceResolution)
	{
		return false;
	}

	OutCellIds.Reserve(SafeFootprintCellsX * SafeFootprintCellsY);
	for (int32 LocalY = 0; LocalY < SafeFootprintCellsY; ++LocalY)
	{
		for (int32 LocalX = 0; LocalX < SafeFootprintCellsX; ++LocalX)
		{
			FSRPlanetSurfaceGridCellId FootprintCellId;
			FootprintCellId.Face = OriginCellId.Face;
			FootprintCellId.CellX = GridOriginCellX + LocalX;
			FootprintCellId.CellY = GridOriginCellY + LocalY;

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
