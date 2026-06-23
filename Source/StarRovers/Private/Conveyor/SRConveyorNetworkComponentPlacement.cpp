#include "Conveyor/SRConveyorNetworkComponent.h"

#include "SRConveyorNetworkComponentInternal.h"
#include "Surface/SRPlanetSurfaceGrid.h"

bool USRConveyorNetworkComponent::TryPlaceConveyorPath(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TArray<FSRPlanetSurfaceGridCellId>& PathCellIds,
	int32 Layer,
	float LayerHeight,
	USRStructureDataAsset* StructureDataAsset,
	FName NetworkId)
{
	if (!IsValid(SurfaceGrid) || !IsValid(StructureDataAsset) || PathCellIds.IsEmpty())
	{
		return false;
	}

	const int32 SafeLayer = FMath::Max(0, Layer);
	const float SafeLayerHeight = ResolveConveyorLayerHeight(SurfaceGrid, LayerHeight);
	const int32 PreviousVisualPathCount = VisualPaths.Num();
	for (const FSRPlanetSurfaceGridCellId& CellId : PathCellIds)
	{
		const FSRConveyorLaneKey LaneKey = MakeLaneKey(CellId, SafeLayer);
		if (!Segments.Contains(LaneKey) && !CanPlaceConveyorSegment(SurfaceGrid, LaneKey))
		{
			return false;
		}
	}

	TMap<FSRConveyorLaneKey, FSRConveyorSegment> PreviousSegments;
	for (const FSRPlanetSurfaceGridCellId& CellId : PathCellIds)
	{
		const FSRConveyorLaneKey LaneKey = MakeLaneKey(CellId, SafeLayer);
		if (const FSRConveyorSegment* ExistingSegment = Segments.Find(LaneKey))
		{
			PreviousSegments.Add(LaneKey, *ExistingSegment);
		}
	}

	auto RollbackConveyorData = [&]()
	{
		for (const FSRPlanetSurfaceGridCellId& CellId : PathCellIds)
		{
			const FSRConveyorLaneKey LaneKey = MakeLaneKey(CellId, SafeLayer);
			if (const FSRConveyorSegment* PreviousSegment = PreviousSegments.Find(LaneKey))
			{
				Segments.Add(LaneKey, *PreviousSegment);
			}
			else
			{
				Segments.Remove(LaneKey);
			}
		}
		VisualPaths.SetNum(PreviousVisualPathCount, EAllowShrinking::No);
	};

	for (int32 PathIndex = 0; PathIndex < PathCellIds.Num(); ++PathIndex)
	{
		const FSRPlanetSurfaceGridCellId& CellId = PathCellIds[PathIndex];
		ESRConveyorGridDirection InputDirection = ESRConveyorGridDirection::None;
		ESRConveyorGridDirection OutputDirection = ESRConveyorGridDirection::None;
		if (PathIndex > 0)
		{
			ESRConveyorGridDirection PreviousDirection = ESRConveyorGridDirection::None;
			if (FindDirectionBetweenCells(SurfaceGrid, CellId, PathCellIds[PathIndex - 1], PreviousDirection))
			{
				InputDirection = PreviousDirection;
			}
		}
		if (PathIndex + 1 < PathCellIds.Num())
		{
			FindDirectionBetweenCells(SurfaceGrid, CellId, PathCellIds[PathIndex + 1], OutputDirection);
		}

		FSRConveyorSegment Segment;
		Segment.Lane = MakeLaneKey(CellId, SafeLayer);
		Segment.InputDirection = InputDirection;
		Segment.OutputDirection = OutputDirection;
		Segment.Shape = ResolveSegmentShape(InputDirection, OutputDirection);
		Segment.NetworkId = NetworkId;
		Segment.StructureDataAsset = StructureDataAsset;
		Segments.Add(Segment.Lane, Segment);
	}

	FSRConveyorVisualPath VisualPath;
	VisualPath.CellIds = PathCellIds;
	VisualPath.Layer = SafeLayer;
	VisualPath.LayerHeight = SafeLayerHeight;
	VisualPath.NetworkId = NetworkId;
	VisualPath.StructureDataAsset = StructureDataAsset;
	VisualPaths.Add(VisualPath);

	if (SafeLayer == 0)
	{
		TArray<FSRPlanetSurfaceGridCellId> OccupiedCellIds = PathCellIds;
		if (!SurfaceGrid->SetCellsOccupied(OccupiedCellIds, true, NetworkId.IsNone() ? FName(TEXT("Conveyor")) : NetworkId))
		{
			RollbackConveyorData();
			return false;
		}
	}

	if (bSpawnConveyorBeltActors)
	{
		MarkConveyorActorGroupDirty(StructureDataAsset, SafeLayer);
		MarkConveyorActorGroupPlacementDiagnosticPending(StructureDataAsset, SafeLayer);
		ScheduleDirtyConveyorActorGroupRefresh(SurfaceGrid);
	}

	RefreshConveyorVisuals(SurfaceGrid);
	RefreshPCGSplineInputs(SurfaceGrid);
	RequestPCGGeneration();
	RefreshPathDebugLines(SurfaceGrid);
	SetComponentTickEnabled(true);
	return true;
}

bool USRConveyorNetworkComponent::TryRemoveConveyorAtCell(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& CellId,
	int32 Layer)
{
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	const int32 SafeLayer = FMath::Max(0, Layer);
	const FSRConveyorLaneKey TargetLaneKey = MakeLaneKey(CellId, SafeLayer);
	const FSRConveyorSegment* RemovedSegment = Segments.Find(TargetLaneKey);
	if (!RemovedSegment)
	{
		return false;
	}
	USRStructureDataAsset* RemovedStructureDataAsset = RemovedSegment->StructureDataAsset.Get();
	ConveyorItemsByLane.Remove(TargetLaneKey);
	if (bShowTransportItemVisuals)
	{
		RefreshConveyorItemVisuals(SurfaceGrid, 0.0f);
	}

	TSet<FSRPlanetSurfaceGridCellId> OldAffectedCellIds;
	TSet<FSRPlanetSurfaceGridCellId> RetainedAffectedCellIds;
	TArray<FSRConveyorVisualPath> NewVisualPaths;
	NewVisualPaths.Reserve(VisualPaths.Num() + 1);

	bool bRemovedFromVisualPath = false;
	for (const FSRConveyorVisualPath& VisualPath : VisualPaths)
	{
		if (VisualPath.Layer != SafeLayer || !VisualPath.CellIds.Contains(CellId))
		{
			NewVisualPaths.Add(VisualPath);
			continue;
		}

		bRemovedFromVisualPath = true;
		for (const FSRPlanetSurfaceGridCellId& PathCellId : VisualPath.CellIds)
		{
			OldAffectedCellIds.Add(PathCellId);
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
			NewVisualPaths.Add(SplitVisualPath);
			for (const FSRPlanetSurfaceGridCellId& RetainedCellId : CurrentSubPath)
			{
				RetainedAffectedCellIds.Add(RetainedCellId);
			}
			CurrentSubPath.Reset();
		};

		for (const FSRPlanetSurfaceGridCellId& PathCellId : VisualPath.CellIds)
		{
			if (PathCellId == CellId)
			{
				FlushCurrentSubPath();
				continue;
			}

			CurrentSubPath.Add(PathCellId);
		}
		FlushCurrentSubPath();
	}

	if (!bRemovedFromVisualPath)
	{
		Segments.Remove(TargetLaneKey);
		OldAffectedCellIds.Add(CellId);
	}

	VisualPaths = MoveTemp(NewVisualPaths);
	RebuildSegmentsFromVisualPaths(SurfaceGrid);

	if (SafeLayer == 0)
	{
		TArray<FSRPlanetSurfaceGridCellId> ClearedCellIds;
		for (const FSRPlanetSurfaceGridCellId& OldCellId : OldAffectedCellIds)
		{
			if (!RetainedAffectedCellIds.Contains(OldCellId))
			{
				ClearedCellIds.Add(OldCellId);
			}
		}

		if (ClearedCellIds.IsEmpty())
		{
			ClearedCellIds.Add(CellId);
		}
		SurfaceGrid->SetCellsOccupied(ClearedCellIds, false, NAME_None);
	}

	if (bSpawnConveyorBeltActors)
	{
		MarkConveyorActorGroupDirty(RemovedStructureDataAsset, SafeLayer);
		MarkConveyorActorGroupDeletionDiagnosticPending(RemovedStructureDataAsset, SafeLayer);
		ScheduleDirtyConveyorActorGroupRefresh(SurfaceGrid);
	}
	else
	{
		DestroyPlacedConveyorActors();
		LogConveyorMutationMemoryDiagnostics(TEXT("ConveyorDelete.DestroyPlacedActors"), MakeActorGroupKey(RemovedStructureDataAsset, SafeLayer), StarRovers::Conveyor::ShouldForceGCOnConveyorDelete());
	}
	RefreshConveyorVisuals(SurfaceGrid);
	RefreshPCGSplineInputs(SurfaceGrid);
	RequestPCGGeneration();
	RefreshPathDebugLines(SurfaceGrid);
	SetComponentTickEnabled(HasDirtyConveyorActorGroups() || ShouldKeepTransportTickEnabled());
	return true;
}
