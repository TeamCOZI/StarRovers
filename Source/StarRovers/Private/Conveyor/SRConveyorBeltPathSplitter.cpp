#include "Conveyor/SRConveyorBeltPathSplitter.h"

bool StarRovers::Conveyor::FSRConveyorBeltPathSplitter::AppendMatchingSubPaths(
	const FSRConveyorBeltPath& BeltPath,
	TFunctionRef<bool(const FSRPlanetSurfaceGridCellId& CellId)> ShouldKeepCell,
	TArray<FSRConveyorBeltPath>& OutBeltPaths)
{
	bool bAddedSubPath = false;
	TArray<FSRPlanetSurfaceGridCellId> CurrentSubPath;
	CurrentSubPath.Reserve(BeltPath.CellIds.Num());
	auto FlushCurrentSubPath = [&]()
	{
		if (CurrentSubPath.IsEmpty())
		{
			return;
		}

		FSRConveyorBeltPath SplitBeltPath;
		SplitBeltPath.CellIds = MoveTemp(CurrentSubPath);
		SplitBeltPath.Layer = BeltPath.Layer;
		SplitBeltPath.LayerHeight = BeltPath.LayerHeight;
		SplitBeltPath.NetworkId = BeltPath.NetworkId;
		SplitBeltPath.StructureDataAsset = BeltPath.StructureDataAsset;
		OutBeltPaths.Add(MoveTemp(SplitBeltPath));
		CurrentSubPath.Reset(BeltPath.CellIds.Num());
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
