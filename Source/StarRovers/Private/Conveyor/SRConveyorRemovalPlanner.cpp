#include "Conveyor/SRConveyorRemovalPlanner.h"

#include "Conveyor/SRConveyorNetworkGeometry.h"
#include "Conveyor/SRConveyorSegmentBuilder.h"
#include "Conveyor/SRConveyorBeltPathSplitter.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	void AddUniqueCellId(
		TSet<FSRPlanetSurfaceGridCellId>& InOutSeenCellIds,
		TArray<FSRPlanetSurfaceGridCellId>& OutCellIds,
		const FSRPlanetSurfaceGridCellId& CellId)
	{
		if (InOutSeenCellIds.Contains(CellId))
		{
			return;
		}

		InOutSeenCellIds.Add(CellId);
		OutCellIds.Add(CellId);
	}

	void AddUniqueStructureDataAsset(
		TSet<USRStructureDataAsset*>& InOutSeenStructureDataAssets,
		TArray<USRStructureDataAsset*>& OutStructureDataAssets,
		USRStructureDataAsset* StructureDataAsset)
	{
		if (InOutSeenStructureDataAssets.Contains(StructureDataAsset))
		{
			return;
		}

		InOutSeenStructureDataAssets.Add(StructureDataAsset);
		OutStructureDataAssets.Add(StructureDataAsset);
	}
}

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
	OutResult.AffectedStructureDataAssets.Add(RemovedSegment->StructureDataAsset.Get());

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
			if (PathCellId == CellId)
			{
				continue;
			}

			RetainedAffectedCellIds.Add(PathCellId);
		}

		FSRConveyorBeltPathSplitter::AppendMatchingSubPaths(
			BeltPath,
			[&CellId](const FSRPlanetSurfaceGridCellId& PathCellId)
		{
			return PathCellId != CellId;
		},
			NewBeltPaths);
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
		OutResult.ClearedCellIds.Reserve(OldAffectedCellIds.Num());
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
	OutResult.AffectedStructureDataAssets.Add(BeltPaths[RemovedBeltPathIndex].StructureDataAsset.Get());
	BeltPaths.RemoveAt(RemovedBeltPathIndex, 1, EAllowShrinking::No);
	FSRConveyorSegmentBuilder::RebuildFromBeltPaths(SurfaceGrid, BeltPaths, Segments);

	if (SafeLayer == 0)
	{
		TSet<FSRPlanetSurfaceGridCellId> ClearedCellIdSet;
		ClearedCellIdSet.Reserve(PlacedCellIds.Num());
		OutResult.ClearedCellIds.Reserve(PlacedCellIds.Num());
		for (const FSRPlanetSurfaceGridCellId& PlacedCellId : PlacedCellIds)
		{
			if (!Segments.Contains(FSRConveyorNetworkGeometry::MakeLaneKey(PlacedCellId, SafeLayer)))
			{
				AddUniqueCellId(ClearedCellIdSet, OutResult.ClearedCellIds, PlacedCellId);
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
	TSet<FSRPlanetSurfaceGridCellId> DeletedCellIdSet;
	DeletedCellIdSet.Reserve(LaneKeys.Num());
	TArray<FSRPlanetSurfaceGridCellId> DeletedCellIds;
	DeletedCellIds.Reserve(LaneKeys.Num());
	TSet<USRStructureDataAsset*> AffectedStructureDataAssetSet;
	AffectedStructureDataAssetSet.Reserve(LaneKeys.Num());
	OutResult.RemovedLaneKeys.Reserve(LaneKeys.Num());
	OutResult.AffectedStructureDataAssets.Reserve(LaneKeys.Num());
	for (const FSRConveyorLaneKey& LaneKey : LaneKeys)
	{
		DeleteLaneKeySet.Add(LaneKey);
		OutResult.RemovedLaneKeys.Add(LaneKey);
		AddUniqueCellId(DeletedCellIdSet, DeletedCellIds, LaneKey.CellId);

		if (const FSRConveyorSegment* Segment = Segments.Find(LaneKey))
		{
			AddUniqueStructureDataAsset(
				AffectedStructureDataAssetSet,
				OutResult.AffectedStructureDataAssets,
				Segment->StructureDataAsset.Get());
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
			AddUniqueStructureDataAsset(
				AffectedStructureDataAssetSet,
				OutResult.AffectedStructureDataAssets,
				ExistingBeltPath.StructureDataAsset.Get());
		}
	}

	BeltPaths = MoveTemp(NewBeltPaths);
	FSRConveyorSegmentBuilder::RebuildFromBeltPaths(SurfaceGrid, BeltPaths, Segments);

	if (SafeLayer == 0)
	{
		TSet<FSRPlanetSurfaceGridCellId> ClearedCellIdSet;
		ClearedCellIdSet.Reserve(DeletedCellIds.Num());
		OutResult.ClearedCellIds.Reserve(DeletedCellIds.Num());
		for (const FSRPlanetSurfaceGridCellId& DeletedCellId : DeletedCellIds)
		{
			if (!Segments.Contains(FSRConveyorNetworkGeometry::MakeLaneKey(DeletedCellId, SafeLayer)))
			{
				AddUniqueCellId(ClearedCellIdSet, OutResult.ClearedCellIds, DeletedCellId);
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
