#include "Conveyor/SRConveyorBeltPathSplitter.h"

bool StarRovers::Conveyor::FSRConveyorBeltPathSplitter::AppendMatchingSubPaths(
	const FSRConveyorBeltPath& BeltPath,
	TFunctionRef<bool(const FSRPlanetSurfaceGridCellId& CellId)> ShouldKeepCell,
	TArray<FSRConveyorBeltPath>& OutBeltPaths)
{
	bool bAddedSubPath = false;
	TArray<FSRPlanetSurfaceGridCellId> CurrentSubPath;
	auto FlushCurrentSubPath = [&]()
	{
		if (CurrentSubPath.IsEmpty())
		{
			return;
		}

		FSRConveyorBeltPath SplitBeltPath = BeltPath;
		SplitBeltPath.CellIds = CurrentSubPath;
		OutBeltPaths.Add(SplitBeltPath);
		CurrentSubPath.Reset();
		bAddedSubPath = true;
	};

	for (const FSRPlanetSurfaceGridCellId& PathCellId : BeltPath.CellIds)
	{
		if (ShouldKeepCell(PathCellId))
		{
			CurrentSubPath.Add(PathCellId);
			continue;
		}

		FlushCurrentSubPath();
	}
	FlushCurrentSubPath();
	return bAddedSubPath;
}
