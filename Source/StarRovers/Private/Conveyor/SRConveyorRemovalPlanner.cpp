#include "Conveyor/SRConveyorRemovalPlanner.h"

#include "Conveyor/SRConveyorNetworkGeometry.h"
#include "Conveyor/SRConveyorSegmentBuilder.h"
#include "Conveyor/SRConveyorBeltPathSplitter.h"
#include "Surface/SRPlanetSurfaceGrid.h"

bool StarRovers::Conveyor::FSRConveyorRemovalPlanner::RemoveConveyorAtCell(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& CellId,
	int32 Layer,
	TArray<FSRConveyorBeltPath>& BeltPaths,
	TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
	FSRConveyorRemovalResult& OutResult)
{
	OutResult = FSRConveyorRemovalResult{};
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	const int32 SafeLayer = FMath::Max(0, Layer);
	const FSRConveyorLaneKey TargetLaneKey = FSRConveyorNetworkGeometry::MakeLaneKey(CellId, SafeLayer);
	const FSRConveyorSegment* RemovedSegment = Segments.Find(TargetLaneKey);
	if (!RemovedSegment)
	{
		return false;
	}

	OutResult.Layer = SafeLayer;
	OutResult.RemovedLaneKeys.Add(TargetLaneKey);
	OutResult.AffectedStructureDataAssets.AddUnique(RemovedSegment->StructureDataAsset.Get());

	TSet<FSRPlanetSurfaceGridCellId> OldAffectedCellIds;
	TSet<FSRPlanetSurfaceGridCellId> RetainedAffectedCellIds;
	TArray<FSRConveyorBeltPath> NewBeltPaths;
	NewBeltPaths.Reserve(BeltPaths.Num() + 1);

	bool bRemovedFromBeltPath = false;
	for (const FSRConveyorBeltPath& BeltPath : BeltPaths)
	{
		if (BeltPath.Layer != SafeLayer || !BeltPath.CellIds.Contains(CellId))
		{
			NewBeltPaths.Add(BeltPath);
			continue;
		}

		bRemovedFromBeltPath = true;
		for (const FSRPlanetSurfaceGridCellId& PathCellId : BeltPath.CellIds)
		{
			OldAffectedCellIds.Add(PathCellId);
		}

		const int32 FirstRetainedBeltPathIndex = NewBeltPaths.Num();
		FSRConveyorBeltPathSplitter::AppendMatchingSubPaths(
			BeltPath,
			[&CellId](const FSRPlanetSurfaceGridCellId& PathCellId)
		{
			return PathCellId != CellId;
		},
			NewBeltPaths);
		for (int32 RetainedBeltPathIndex = FirstRetainedBeltPathIndex; RetainedBeltPathIndex < NewBeltPaths.Num(); ++RetainedBeltPathIndex)
		{
			for (const FSRPlanetSurfaceGridCellId& RetainedCellId : NewBeltPaths[RetainedBeltPathIndex].CellIds)
			{
				RetainedAffectedCellIds.Add(RetainedCellId);
			}
		}
	}

	if (!bRemovedFromBeltPath)
	{
		Segments.Remove(TargetLaneKey);
		OldAffectedCellIds.Add(CellId);
	}

	BeltPaths = MoveTemp(NewBeltPaths);
	FSRConveyorSegmentBuilder::RebuildFromBeltPaths(SurfaceGrid, BeltPaths, Segments);

	if (SafeLayer == 0)
	{
		for (const FSRPlanetSurfaceGridCellId& OldCellId : OldAffectedCellIds)
		{
			if (!RetainedAffectedCellIds.Contains(OldCellId))
			{
				OutResult.ClearedCellIds.Add(OldCellId);
			}
		}

		if (OutResult.ClearedCellIds.IsEmpty())
		{
			OutResult.ClearedCellIds.Add(CellId);
		}
	}

	return true;
}

bool StarRovers::Conveyor::FSRConveyorRemovalPlanner::RemoveBeltPath(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRConveyorBeltPath& BeltPath,
	const TArray<FSRPlanetSurfaceGridCellId>& PlacedCellIds,
	TArray<FSRConveyorBeltPath>& BeltPaths,
	TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
	FSRConveyorRemovalResult& OutResult)
{
	OutResult = FSRConveyorRemovalResult{};
	if (!IsValid(SurfaceGrid) || BeltPath.CellIds.IsEmpty())
	{
		return false;
	}

	const int32 SafeLayer = FMath::Max(0, BeltPath.Layer);
	int32 RemovedBeltPathIndex = INDEX_NONE;
	for (int32 BeltPathIndex = 0; BeltPathIndex < BeltPaths.Num(); ++BeltPathIndex)
	{
		const FSRConveyorBeltPath& ExistingBeltPath = BeltPaths[BeltPathIndex];
		if (ExistingBeltPath.Layer == SafeLayer
			&& ExistingBeltPath.CellIds == BeltPath.CellIds
			&& ExistingBeltPath.NetworkId == BeltPath.NetworkId)
		{
			RemovedBeltPathIndex = BeltPathIndex;
			break;
		}
	}

	if (RemovedBeltPathIndex == INDEX_NONE)
	{
		return false;
	}

	OutResult.Layer = SafeLayer;
	OutResult.AffectedStructureDataAssets.AddUnique(BeltPaths[RemovedBeltPathIndex].StructureDataAsset.Get());
	BeltPaths.RemoveAt(RemovedBeltPathIndex, 1, EAllowShrinking::No);
	FSRConveyorSegmentBuilder::RebuildFromBeltPaths(SurfaceGrid, BeltPaths, Segments);

	if (SafeLayer == 0)
	{
		for (const FSRPlanetSurfaceGridCellId& PlacedCellId : PlacedCellIds)
		{
			if (!Segments.Contains(FSRConveyorNetworkGeometry::MakeLaneKey(PlacedCellId, SafeLayer)))
			{
				OutResult.ClearedCellIds.AddUnique(PlacedCellId);
			}
		}
	}

	return true;
}

bool StarRovers::Conveyor::FSRConveyorRemovalPlanner::RemoveLaneKeys(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TArray<FSRConveyorLaneKey>& LaneKeys,
	int32 Layer,
	TArray<FSRConveyorBeltPath>& BeltPaths,
	TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
	FSRConveyorRemovalResult& OutResult)
{
	OutResult = FSRConveyorRemovalResult{};
	if (!IsValid(SurfaceGrid) || LaneKeys.IsEmpty())
	{
		return false;
	}

	const int32 SafeLayer = FMath::Max(0, Layer);
	OutResult.Layer = SafeLayer;

	TSet<FSRConveyorLaneKey> DeleteLaneKeySet;
	DeleteLaneKeySet.Reserve(LaneKeys.Num());
	TArray<FSRPlanetSurfaceGridCellId> DeletedCellIds;
	for (const FSRConveyorLaneKey& LaneKey : LaneKeys)
	{
		DeleteLaneKeySet.Add(LaneKey);
		OutResult.RemovedLaneKeys.Add(LaneKey);
		DeletedCellIds.AddUnique(LaneKey.CellId);

		if (const FSRConveyorSegment* Segment = Segments.Find(LaneKey))
		{
			OutResult.AffectedStructureDataAssets.AddUnique(Segment->StructureDataAsset.Get());
		}
	}

	TArray<FSRConveyorBeltPath> NewBeltPaths;
	NewBeltPaths.Reserve(BeltPaths.Num() + 1);
	for (const FSRConveyorBeltPath& ExistingBeltPath : BeltPaths)
	{
		if (ExistingBeltPath.Layer != SafeLayer)
		{
			NewBeltPaths.Add(ExistingBeltPath);
			continue;
		}

		bool bRemovedFromBeltPath = false;
		FSRConveyorBeltPathSplitter::AppendMatchingSubPaths(
			ExistingBeltPath,
			[&DeleteLaneKeySet, SafeLayer, &bRemovedFromBeltPath](const FSRPlanetSurfaceGridCellId& PathCellId)
		{
			if (DeleteLaneKeySet.Contains(FSRConveyorNetworkGeometry::MakeLaneKey(PathCellId, SafeLayer)))
			{
				bRemovedFromBeltPath = true;
				return false;
			}

			return true;
		},
			NewBeltPaths);

		if (bRemovedFromBeltPath)
		{
			OutResult.AffectedStructureDataAssets.AddUnique(ExistingBeltPath.StructureDataAsset.Get());
		}
	}

	BeltPaths = MoveTemp(NewBeltPaths);
	FSRConveyorSegmentBuilder::RebuildFromBeltPaths(SurfaceGrid, BeltPaths, Segments);

	if (SafeLayer == 0)
	{
		for (const FSRPlanetSurfaceGridCellId& DeletedCellId : DeletedCellIds)
		{
			if (!Segments.Contains(FSRConveyorNetworkGeometry::MakeLaneKey(DeletedCellId, SafeLayer)))
			{
				OutResult.ClearedCellIds.AddUnique(DeletedCellId);
			}
		}
	}

	return true;
}

void StarRovers::Conveyor::FSRConveyorRemovalPlanner::CollectLaneKeysInCells(
	const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
	const TSet<FSRPlanetSurfaceGridCellId>& CellIds,
	TArray<FSRConveyorLaneKey>& OutLaneKeys)
{
	OutLaneKeys.Reset();
	if (CellIds.IsEmpty())
	{
		return;
	}

	for (const TPair<FSRConveyorLaneKey, FSRConveyorSegment>& SegmentPair : Segments)
	{
		if (CellIds.Contains(SegmentPair.Key.CellId))
		{
			OutLaneKeys.Add(SegmentPair.Key);
		}
	}
}
