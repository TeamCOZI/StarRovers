#include "Conveyor/SRConveyorNetworkComponent.h"

#include "SRConveyorNetworkComponentInternal.h"
#include "Structure/SRStructureDataAsset.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	void SortConveyorLaneKeysForBulkDeletion(TArray<FSRConveyorLaneKey>& LaneKeys)
	{
		LaneKeys.Sort([](const FSRConveyorLaneKey& Left, const FSRConveyorLaneKey& Right)
		{
			const int32 LeftFace = static_cast<int32>(Left.CellId.Face);
			const int32 RightFace = static_cast<int32>(Right.CellId.Face);
			if (LeftFace != RightFace)
			{
				return LeftFace < RightFace;
			}
			if (Left.CellId.CellY != Right.CellId.CellY)
			{
				return Left.CellId.CellY < Right.CellId.CellY;
			}
			if (Left.CellId.CellX != Right.CellId.CellX)
			{
				return Left.CellId.CellX < Right.CellId.CellX;
			}
			return Left.Layer < Right.Layer;
		});
	}
}

bool USRConveyorNetworkComponent::DoesConveyorSegmentReferenceLane(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRConveyorSegment& Segment,
	const FSRConveyorLaneKey& TargetLaneKey) const
{
	if (!IsValid(SurfaceGrid) || Segment.Lane.Layer != TargetLaneKey.Layer)
	{
		return false;
	}

	TArray<ESRConveyorGridDirection> Directions;
	Directions.Reserve(3);
	if (Segment.InputDirection != ESRConveyorGridDirection::None)
	{
		Directions.Add(Segment.InputDirection);
	}
	if (Segment.OutputDirection != ESRConveyorGridDirection::None)
	{
		Directions.Add(Segment.OutputDirection);
	}
	if (Segment.BranchOutputDirection != ESRConveyorGridDirection::None
		&& Segment.BranchOutputDirection != Segment.OutputDirection)
	{
		Directions.Add(Segment.BranchOutputDirection);
	}

	for (const ESRConveyorGridDirection Direction : Directions)
	{
		FSRConveyorLaneKey NeighborLaneKey;
		if (TryResolveNextLaneByDirection(SurfaceGrid, Segment, Direction, NeighborLaneKey)
			&& NeighborLaneKey == TargetLaneKey)
		{
			return true;
		}
	}

	return false;
}

bool USRConveyorNetworkComponent::GatherConnectedConveyorLaneKeysAtCell(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& CellId,
	int32 Layer,
	TArray<FSRConveyorLaneKey>& OutLaneKeys) const
{
	OutLaneKeys.Reset();
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	const FSRConveyorLaneKey StartLaneKey = MakeLaneKey(CellId, Layer);
	if (!Segments.Contains(StartLaneKey))
	{
		return false;
	}

	TSet<FSRConveyorLaneKey> VisitedLaneKeys;
	TArray<FSRConveyorLaneKey> OpenLaneKeys;
	VisitedLaneKeys.Add(StartLaneKey);
	OpenLaneKeys.Add(StartLaneKey);

	const ESRConveyorGridDirection Directions[] =
	{
		ESRConveyorGridDirection::NegativeU,
		ESRConveyorGridDirection::PositiveU,
		ESRConveyorGridDirection::NegativeV,
		ESRConveyorGridDirection::PositiveV,
	};

	for (int32 OpenIndex = 0; OpenIndex < OpenLaneKeys.Num(); ++OpenIndex)
	{
		const FSRConveyorLaneKey CurrentLaneKey = OpenLaneKeys[OpenIndex];
		const FSRConveyorSegment* CurrentSegment = Segments.Find(CurrentLaneKey);
		if (!CurrentSegment)
		{
			continue;
		}

		for (const ESRConveyorGridDirection Direction : Directions)
		{
			FSRConveyorLaneKey NeighborLaneKey;
			if (!TryResolveNextLaneByDirection(SurfaceGrid, *CurrentSegment, Direction, NeighborLaneKey)
				|| VisitedLaneKeys.Contains(NeighborLaneKey))
			{
				continue;
			}

			const FSRConveyorSegment* NeighborSegment = Segments.Find(NeighborLaneKey);
			if (!NeighborSegment)
			{
				continue;
			}

			if (!DoesConveyorSegmentReferenceLane(SurfaceGrid, *CurrentSegment, NeighborLaneKey)
				&& !DoesConveyorSegmentReferenceLane(SurfaceGrid, *NeighborSegment, CurrentLaneKey))
			{
				continue;
			}

			VisitedLaneKeys.Add(NeighborLaneKey);
			OpenLaneKeys.Add(NeighborLaneKey);
		}
	}

	OutLaneKeys = OpenLaneKeys;
	SortConveyorLaneKeysForBulkDeletion(OutLaneKeys);
	return !OutLaneKeys.IsEmpty();
}

bool USRConveyorNetworkComponent::GetConnectedConveyorCellIdsAtCell(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& CellId,
	int32 Layer,
	TArray<FSRPlanetSurfaceGridCellId>& OutCellIds) const
{
	OutCellIds.Reset();

	TArray<FSRConveyorLaneKey> ConnectedLaneKeys;
	if (!GatherConnectedConveyorLaneKeysAtCell(SurfaceGrid, CellId, Layer, ConnectedLaneKeys))
	{
		return false;
	}

	OutCellIds.Reserve(ConnectedLaneKeys.Num());
	for (const FSRConveyorLaneKey& LaneKey : ConnectedLaneKeys)
	{
		OutCellIds.AddUnique(LaneKey.CellId);
	}
	return !OutCellIds.IsEmpty();
}

bool USRConveyorNetworkComponent::GetConnectedConveyorVisualPathsAtCell(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& CellId,
	int32 Layer,
	TArray<FSRConveyorVisualPath>& OutVisualPaths) const
{
	OutVisualPaths.Reset();

	TArray<FSRConveyorLaneKey> ConnectedLaneKeys;
	if (!GatherConnectedConveyorLaneKeysAtCell(SurfaceGrid, CellId, Layer, ConnectedLaneKeys))
	{
		return false;
	}

	TSet<FSRConveyorLaneKey> ConnectedLaneKeySet;
	ConnectedLaneKeySet.Reserve(ConnectedLaneKeys.Num());
	for (const FSRConveyorLaneKey& LaneKey : ConnectedLaneKeys)
	{
		ConnectedLaneKeySet.Add(LaneKey);
	}

	const int32 SafeLayer = FMath::Max(0, Layer);
	for (const FSRConveyorVisualPath& VisualPath : VisualPaths)
	{
		if (VisualPath.Layer != SafeLayer)
		{
			continue;
		}

		TArray<FSRPlanetSurfaceGridCellId> CurrentSubPath;
		auto FlushCurrentSubPath = [&]()
		{
			if (CurrentSubPath.IsEmpty())
			{
				return;
			}

			FSRConveyorVisualPath SplitVisualPath = VisualPath;
			SplitVisualPath.CellIds = CurrentSubPath;
			OutVisualPaths.Add(SplitVisualPath);
			CurrentSubPath.Reset();
		};

		for (const FSRPlanetSurfaceGridCellId& PathCellId : VisualPath.CellIds)
		{
			if (ConnectedLaneKeySet.Contains(MakeLaneKey(PathCellId, SafeLayer)))
			{
				CurrentSubPath.Add(PathCellId);
				continue;
			}

			FlushCurrentSubPath();
		}
		FlushCurrentSubPath();
	}

	return !OutVisualPaths.IsEmpty();
}

bool USRConveyorNetworkComponent::TryRemoveConnectedConveyorsAtCell(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& CellId,
	int32 Layer)
{
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	TArray<FSRConveyorLaneKey> ConnectedLaneKeys;
	if (!GatherConnectedConveyorLaneKeysAtCell(SurfaceGrid, CellId, Layer, ConnectedLaneKeys))
	{
		return false;
	}

	TSet<FSRConveyorLaneKey> DeleteLaneKeySet;
	DeleteLaneKeySet.Reserve(ConnectedLaneKeys.Num());
	TArray<USRStructureDataAsset*> AffectedStructureDataAssets;
	TArray<FSRPlanetSurfaceGridCellId> DeletedCellIds;
	for (const FSRConveyorLaneKey& LaneKey : ConnectedLaneKeys)
	{
		DeleteLaneKeySet.Add(LaneKey);
		DeletedCellIds.AddUnique(LaneKey.CellId);
		ConveyorItemsByLane.Remove(LaneKey);

		if (const FSRConveyorSegment* Segment = Segments.Find(LaneKey))
		{
			AffectedStructureDataAssets.AddUnique(Segment->StructureDataAsset.Get());
		}
	}

	if (bShowTransportItemVisuals)
	{
		RefreshConveyorItemVisuals(SurfaceGrid, 0.0f);
	}

	const int32 SafeLayer = FMath::Max(0, Layer);
	TArray<FSRConveyorVisualPath> NewVisualPaths;
	NewVisualPaths.Reserve(VisualPaths.Num() + 1);
	for (const FSRConveyorVisualPath& VisualPath : VisualPaths)
	{
		if (VisualPath.Layer != SafeLayer)
		{
			NewVisualPaths.Add(VisualPath);
			continue;
		}

		bool bRemovedFromVisualPath = false;
		TArray<FSRPlanetSurfaceGridCellId> CurrentSubPath;
		auto FlushCurrentSubPath = [&]()
		{
			if (CurrentSubPath.IsEmpty())
			{
				return;
			}

			FSRConveyorVisualPath SplitVisualPath = VisualPath;
			SplitVisualPath.CellIds = CurrentSubPath;
			NewVisualPaths.Add(SplitVisualPath);
			CurrentSubPath.Reset();
		};

		for (const FSRPlanetSurfaceGridCellId& PathCellId : VisualPath.CellIds)
		{
			if (DeleteLaneKeySet.Contains(MakeLaneKey(PathCellId, SafeLayer)))
			{
				bRemovedFromVisualPath = true;
				FlushCurrentSubPath();
				continue;
			}

			CurrentSubPath.Add(PathCellId);
		}
		FlushCurrentSubPath();

		if (bRemovedFromVisualPath)
		{
			AffectedStructureDataAssets.AddUnique(VisualPath.StructureDataAsset.Get());
		}
	}

	VisualPaths = MoveTemp(NewVisualPaths);
	RebuildSegmentsFromVisualPaths(SurfaceGrid);

	if (SafeLayer == 0)
	{
		TArray<FSRPlanetSurfaceGridCellId> ClearedCellIds;
		for (const FSRPlanetSurfaceGridCellId& DeletedCellId : DeletedCellIds)
		{
			if (!Segments.Contains(MakeLaneKey(DeletedCellId, SafeLayer)))
			{
				ClearedCellIds.AddUnique(DeletedCellId);
			}
		}

		if (!ClearedCellIds.IsEmpty())
		{
			SurfaceGrid->SetCellsOccupied(ClearedCellIds, false, NAME_None);
		}
	}

	if (bSpawnConveyorBeltActors)
	{
		for (USRStructureDataAsset* StructureDataAsset : AffectedStructureDataAssets)
		{
			MarkConveyorActorGroupDirty(StructureDataAsset, SafeLayer);
			MarkConveyorActorGroupDeletionDiagnosticPending(StructureDataAsset, SafeLayer);
		}
		ScheduleDirtyConveyorActorGroupRefresh(SurfaceGrid);
	}
	else
	{
		DestroyPlacedConveyorActors();
		for (USRStructureDataAsset* StructureDataAsset : AffectedStructureDataAssets)
		{
			LogConveyorMutationMemoryDiagnostics(TEXT("ConveyorBulkDelete.DestroyPlacedActors"), MakeActorGroupKey(StructureDataAsset, SafeLayer), StarRovers::Conveyor::ShouldForceGCOnConveyorDelete());
		}
	}

	RefreshConveyorVisuals(SurfaceGrid);
	RefreshPCGSplineInputs(SurfaceGrid);
	RequestPCGGeneration();
	RefreshPathDebugLines(SurfaceGrid);
	SetComponentTickEnabled(HasDirtyConveyorActorGroups() || ShouldKeepTransportTickEnabled() || bShowPathDebugLine || bShowConnectionDebugLine);
	return true;
}
