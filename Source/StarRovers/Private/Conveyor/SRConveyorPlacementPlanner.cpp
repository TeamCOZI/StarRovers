#include "Conveyor/SRConveyorPlacementPlanner.h"

#include "Conveyor/SRConveyorNetworkGeometry.h"
#include "Conveyor/SRConveyorPlacementValidator.h"
#include "Conveyor/SRConveyorSegmentBuilder.h"
#include "Conveyor/SRConveyorSegmentMerger.h"
#include "Surface/SRPlanetSurfaceGrid.h"

bool StarRovers::Conveyor::FSRConveyorPlacementPlanner::CanPlacePath(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TArray<FSRPlanetSurfaceGridCellId>& PathCellIds,
	int32 Layer,
	const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
	const TSet<FSRPlanetSurfaceGridCellId>& IgnoredOccupiedCellIds)
{
	if (!IsValid(SurfaceGrid) || PathCellIds.IsEmpty())
	{
		return false;
	}

	const int32 SafeLayer = FMath::Max(0, Layer);
	for (const FSRPlanetSurfaceGridCellId& CellId : PathCellIds)
	{
		const FSRConveyorLaneKey LaneKey = FSRConveyorNetworkGeometry::MakeLaneKey(CellId, SafeLayer);
		if (!FSRConveyorPlacementValidator::CanPlaceOrReuseLane(SurfaceGrid, Segments, LaneKey, IgnoredOccupiedCellIds))
		{
			return false;
		}
	}

	TArray<FSRConveyorSegment> UnusedProposedSegments;
	return FSRConveyorSegmentBuilder::BuildPathSegments(
		SurfaceGrid,
		PathCellIds,
		SafeLayer,
		Segments,
		UnusedProposedSegments);
}

bool StarRovers::Conveyor::FSRConveyorPlacementPlanner::BuildPlacementPlan(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TArray<FSRPlanetSurfaceGridCellId>& PathCellIds,
	int32 Layer,
	float LayerHeight,
	float DefaultLayerHeight,
	USRStructureDataAsset* StructureDataAsset,
	FName NetworkId,
	const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
	int32 CurrentBeltPathCount,
	FSRConveyorPlacementPlan& OutPlan)
{
	OutPlan = FSRConveyorPlacementPlan{};
	if (!IsValid(SurfaceGrid) || !IsValid(StructureDataAsset) || PathCellIds.IsEmpty())
	{
		return false;
	}

	const int32 SafeLayer = FMath::Max(0, Layer);
	OutPlan.Layer = SafeLayer;
	OutPlan.PreviousBeltPathCount = CurrentBeltPathCount;

	for (const FSRPlanetSurfaceGridCellId& CellId : PathCellIds)
	{
		const FSRConveyorLaneKey LaneKey = FSRConveyorNetworkGeometry::MakeLaneKey(CellId, SafeLayer);
		if (!Segments.Contains(LaneKey)
			&& !FSRConveyorPlacementValidator::CanPlaceNewLane(SurfaceGrid, Segments, LaneKey))
		{
			return false;
		}

		if (const FSRConveyorSegment* ExistingSegment = Segments.Find(LaneKey))
		{
			OutPlan.PreviousSegments.Add(LaneKey, *ExistingSegment);
			continue;
		}

		if (SafeLayer == 0)
		{
			FSRPlanetSurfaceGridCellInfo CellInfo;
			if (!SurfaceGrid->GetCellInfoById(CellId, CellInfo))
			{
				return false;
			}

			if (CellInfo.bOccupied
				&& FSRConveyorPlacementValidator::CanDestroyStructureForPlacement(SurfaceGrid, CellInfo.OccupantId))
			{
				OutPlan.DestructibleStructureCellIds.AddUnique(CellId);
			}
		}
	}

	if (!FSRConveyorSegmentBuilder::BuildPathSegments(
		SurfaceGrid,
		PathCellIds,
		SafeLayer,
		Segments,
		OutPlan.ProposedSegments,
		NetworkId,
		StructureDataAsset))
	{
		return false;
	}

	OutPlan.BeltPath.CellIds = PathCellIds;
	OutPlan.BeltPath.Layer = SafeLayer;
	OutPlan.BeltPath.LayerHeight = FSRConveyorPlacementValidator::ResolveLayerHeight(SurfaceGrid, LayerHeight, DefaultLayerHeight);
	OutPlan.BeltPath.NetworkId = NetworkId;
	OutPlan.BeltPath.StructureDataAsset = StructureDataAsset;
	return true;
}

void StarRovers::Conveyor::FSRConveyorPlacementPlanner::ApplyPlacementPlan(
	const FSRConveyorPlacementPlan& Plan,
	TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
	TArray<FSRConveyorBeltPath>& BeltPaths)
{
	for (const FSRConveyorSegment& Segment : Plan.ProposedSegments)
	{
		FSRConveyorSegmentMerger::MergeSegment(Segments, Segment);
	}

	BeltPaths.Add(Plan.BeltPath);
}

void StarRovers::Conveyor::FSRConveyorPlacementPlanner::RollbackPlacementPlan(
	const FSRConveyorPlacementPlan& Plan,
	TMap<FSRConveyorLaneKey, FSRConveyorSegment>& Segments,
	TArray<FSRConveyorBeltPath>& BeltPaths)
{
	for (const FSRPlanetSurfaceGridCellId& CellId : Plan.BeltPath.CellIds)
	{
		const FSRConveyorLaneKey LaneKey = FSRConveyorNetworkGeometry::MakeLaneKey(CellId, Plan.Layer);
		if (const FSRConveyorSegment* PreviousSegment = Plan.PreviousSegments.Find(LaneKey))
		{
			Segments.Add(LaneKey, *PreviousSegment);
		}
		else
		{
			Segments.Remove(LaneKey);
		}
	}

	BeltPaths.SetNum(Plan.PreviousBeltPathCount, EAllowShrinking::No);
}
