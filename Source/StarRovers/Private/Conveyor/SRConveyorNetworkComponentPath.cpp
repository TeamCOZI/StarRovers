#include "Conveyor/SRConveyorNetworkComponent.h"

#include "GameFramework/Actor.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	struct FSRConveyorPathSearchNode
	{
		FSRPlanetSurfaceGridCellId CellId;
		int32 CostFromStart = 0;
		int32 EstimatedCostToEnd = 0;
		int32 Sequence = 0;

		int32 GetTotalEstimatedCost() const
		{
			return CostFromStart + EstimatedCostToEnd;
		}
	};

	int32 GetConveyorPathHeuristicCost(
		const FSRPlanetSurfaceGridCellId& CellId,
		const FSRPlanetSurfaceGridCellId& EndCellId)
	{
		if (CellId.Face != EndCellId.Face)
		{
			return 0;
		}

		return FMath::Abs(CellId.CellX - EndCellId.CellX)
			+ FMath::Abs(CellId.CellY - EndCellId.CellY);
	}

	bool IsConveyorPathSearchNodeHigherPriority(
		const FSRConveyorPathSearchNode& A,
		const FSRConveyorPathSearchNode& B)
	{
		const int32 ATotalEstimatedCost = A.GetTotalEstimatedCost();
		const int32 BTotalEstimatedCost = B.GetTotalEstimatedCost();
		if (ATotalEstimatedCost != BTotalEstimatedCost)
		{
			return ATotalEstimatedCost < BTotalEstimatedCost;
		}
		if (A.EstimatedCostToEnd != B.EstimatedCostToEnd)
		{
			return A.EstimatedCostToEnd < B.EstimatedCostToEnd;
		}

		return A.Sequence < B.Sequence;
	}

	void PushConveyorPathSearchNode(
		TArray<FSRConveyorPathSearchNode>& Heap,
		const FSRConveyorPathSearchNode& Node)
	{
		int32 NodeIndex = Heap.Add(Node);
		while (NodeIndex > 0)
		{
			const int32 ParentIndex = (NodeIndex - 1) / 2;
			if (!IsConveyorPathSearchNodeHigherPriority(Heap[NodeIndex], Heap[ParentIndex]))
			{
				break;
			}

			Swap(Heap[NodeIndex], Heap[ParentIndex]);
			NodeIndex = ParentIndex;
		}
	}

	bool PopConveyorPathSearchNode(
		TArray<FSRConveyorPathSearchNode>& Heap,
		FSRConveyorPathSearchNode& OutNode)
	{
		if (Heap.IsEmpty())
		{
			return false;
		}

		OutNode = Heap[0];
		Heap[0] = Heap.Last();
		Heap.Pop(EAllowShrinking::No);

		int32 NodeIndex = 0;
		while (true)
		{
			const int32 LeftChildIndex = NodeIndex * 2 + 1;
			const int32 RightChildIndex = LeftChildIndex + 1;
			int32 BestChildIndex = NodeIndex;

			if (Heap.IsValidIndex(LeftChildIndex)
				&& IsConveyorPathSearchNodeHigherPriority(Heap[LeftChildIndex], Heap[BestChildIndex]))
			{
				BestChildIndex = LeftChildIndex;
			}

			if (Heap.IsValidIndex(RightChildIndex)
				&& IsConveyorPathSearchNodeHigherPriority(Heap[RightChildIndex], Heap[BestChildIndex]))
			{
				BestChildIndex = RightChildIndex;
			}

			if (BestChildIndex == NodeIndex)
			{
				break;
			}

			Swap(Heap[NodeIndex], Heap[BestChildIndex]);
			NodeIndex = BestChildIndex;
		}

		return true;
	}

	bool BuildConveyorPathFromCameFrom(
		const FSRPlanetSurfaceGridCellId& StartCellId,
		const FSRPlanetSurfaceGridCellId& EndCellId,
		const TMap<FSRPlanetSurfaceGridCellId, FSRPlanetSurfaceGridCellId>& CameFrom,
		TArray<FSRPlanetSurfaceGridCellId>& OutPath)
	{
		TArray<FSRPlanetSurfaceGridCellId> ReversedPath;
		FSRPlanetSurfaceGridCellId TraceCellId = EndCellId;
		ReversedPath.Add(TraceCellId);
		while (!(TraceCellId == StartCellId))
		{
			const FSRPlanetSurfaceGridCellId* PreviousCellId = CameFrom.Find(TraceCellId);
			if (!PreviousCellId)
			{
				OutPath.Reset();
				return false;
			}
			TraceCellId = *PreviousCellId;
			ReversedPath.Add(TraceCellId);
		}

		OutPath.Reset();
		OutPath.Reserve(ReversedPath.Num());
		for (int32 PathIndex = ReversedPath.Num() - 1; PathIndex >= 0; --PathIndex)
		{
			OutPath.Add(ReversedPath[PathIndex]);
		}

		return !OutPath.IsEmpty();
	}

	struct FSRConveyorFacePathSearchNode
	{
		int32 CellIndex = INDEX_NONE;
		int32 CostFromStart = 0;
		int32 EstimatedCostToEnd = 0;
		int32 Sequence = 0;

		int32 GetTotalEstimatedCost() const
		{
			return CostFromStart + EstimatedCostToEnd;
		}
	};

	int32 GetFaceLocalCellIndex(int32 Resolution, int32 CellX, int32 CellY)
	{
		if (Resolution <= 0 || CellX < 0 || CellY < 0 || CellX >= Resolution || CellY >= Resolution)
		{
			return INDEX_NONE;
		}

		return CellY * Resolution + CellX;
	}

	FSRPlanetSurfaceGridCellId MakeFaceLocalCellId(ESRCubeSphereFace Face, int32 Resolution, int32 CellIndex)
	{
		FSRPlanetSurfaceGridCellId CellId;
		CellId.Face = Face;
		if (Resolution <= 0 || CellIndex < 0)
		{
			CellId.CellX = INDEX_NONE;
			CellId.CellY = INDEX_NONE;
			return CellId;
		}

		CellId.CellX = CellIndex % Resolution;
		CellId.CellY = CellIndex / Resolution;
		return CellId;
	}

	int32 GetFaceLocalManhattanCost(int32 Resolution, int32 CellIndex, int32 EndCellIndex)
	{
		if (Resolution <= 0 || CellIndex < 0 || EndCellIndex < 0)
		{
			return 0;
		}

		const int32 CellX = CellIndex % Resolution;
		const int32 CellY = CellIndex / Resolution;
		const int32 EndCellX = EndCellIndex % Resolution;
		const int32 EndCellY = EndCellIndex / Resolution;
		return FMath::Abs(CellX - EndCellX) + FMath::Abs(CellY - EndCellY);
	}

	bool IsConveyorFacePathSearchNodeHigherPriority(
		const FSRConveyorFacePathSearchNode& A,
		const FSRConveyorFacePathSearchNode& B)
	{
		const int32 ATotalEstimatedCost = A.GetTotalEstimatedCost();
		const int32 BTotalEstimatedCost = B.GetTotalEstimatedCost();
		if (ATotalEstimatedCost != BTotalEstimatedCost)
		{
			return ATotalEstimatedCost < BTotalEstimatedCost;
		}
		if (A.EstimatedCostToEnd != B.EstimatedCostToEnd)
		{
			return A.EstimatedCostToEnd < B.EstimatedCostToEnd;
		}

		return A.Sequence < B.Sequence;
	}

	void PushConveyorFacePathSearchNode(
		TArray<FSRConveyorFacePathSearchNode>& Heap,
		const FSRConveyorFacePathSearchNode& Node)
	{
		int32 NodeIndex = Heap.Add(Node);
		while (NodeIndex > 0)
		{
			const int32 ParentIndex = (NodeIndex - 1) / 2;
			if (!IsConveyorFacePathSearchNodeHigherPriority(Heap[NodeIndex], Heap[ParentIndex]))
			{
				break;
			}

			Swap(Heap[NodeIndex], Heap[ParentIndex]);
			NodeIndex = ParentIndex;
		}
	}

	bool PopConveyorFacePathSearchNode(
		TArray<FSRConveyorFacePathSearchNode>& Heap,
		FSRConveyorFacePathSearchNode& OutNode)
	{
		if (Heap.IsEmpty())
		{
			return false;
		}

		OutNode = Heap[0];
		Heap[0] = Heap.Last();
		Heap.Pop(EAllowShrinking::No);

		int32 NodeIndex = 0;
		while (true)
		{
			const int32 LeftChildIndex = NodeIndex * 2 + 1;
			const int32 RightChildIndex = LeftChildIndex + 1;
			int32 BestChildIndex = NodeIndex;

			if (Heap.IsValidIndex(LeftChildIndex)
				&& IsConveyorFacePathSearchNodeHigherPriority(Heap[LeftChildIndex], Heap[BestChildIndex]))
			{
				BestChildIndex = LeftChildIndex;
			}

			if (Heap.IsValidIndex(RightChildIndex)
				&& IsConveyorFacePathSearchNodeHigherPriority(Heap[RightChildIndex], Heap[BestChildIndex]))
			{
				BestChildIndex = RightChildIndex;
			}

			if (BestChildIndex == NodeIndex)
			{
				break;
			}

			Swap(Heap[NodeIndex], Heap[BestChildIndex]);
			NodeIndex = BestChildIndex;
		}

		return true;
	}

	bool BuildFaceLocalConveyorPathFromCameFrom(
		ESRCubeSphereFace Face,
		int32 Resolution,
		int32 StartCellIndex,
		int32 EndCellIndex,
		const TArray<int32>& CameFrom,
		TArray<FSRPlanetSurfaceGridCellId>& OutPath)
	{
		TArray<int32> ReversedPath;
		int32 TraceCellIndex = EndCellIndex;
		ReversedPath.Add(TraceCellIndex);
		while (TraceCellIndex != StartCellIndex)
		{
			if (!CameFrom.IsValidIndex(TraceCellIndex))
			{
				OutPath.Reset();
				return false;
			}

			const int32 PreviousCellIndex = CameFrom[TraceCellIndex];
			if (PreviousCellIndex == INDEX_NONE)
			{
				OutPath.Reset();
				return false;
			}

			TraceCellIndex = PreviousCellIndex;
			ReversedPath.Add(TraceCellIndex);
		}

		OutPath.Reset();
		OutPath.Reserve(ReversedPath.Num());
		for (int32 PathIndex = ReversedPath.Num() - 1; PathIndex >= 0; --PathIndex)
		{
			OutPath.Add(MakeFaceLocalCellId(Face, Resolution, ReversedPath[PathIndex]));
		}

		return !OutPath.IsEmpty();
	}

	template <typename TCanUseFaceCell>
	bool TryAppendFaceLocalAxisSegment(
		ESRCubeSphereFace Face,
		int32 Resolution,
		int32 StartCellX,
		int32 StartCellY,
		int32 EndCellX,
		int32 EndCellY,
		bool bIncludeStart,
		const TCanUseFaceCell& CanUseFaceCell,
		TArray<FSRPlanetSurfaceGridCellId>& OutPath)
	{
		if (StartCellX != EndCellX && StartCellY != EndCellY)
		{
			return false;
		}

		const int32 StepX = StartCellX == EndCellX ? 0 : (StartCellX < EndCellX ? 1 : -1);
		const int32 StepY = StartCellY == EndCellY ? 0 : (StartCellY < EndCellY ? 1 : -1);
		int32 CellX = StartCellX;
		int32 CellY = StartCellY;
		bool bFirstCell = true;

		while (true)
		{
			if (bIncludeStart || !bFirstCell)
			{
				const int32 CellIndex = GetFaceLocalCellIndex(Resolution, CellX, CellY);
				if (CellIndex == INDEX_NONE || !CanUseFaceCell(CellIndex))
				{
					return false;
				}

				OutPath.Add(MakeFaceLocalCellId(Face, Resolution, CellIndex));
			}

			if (CellX == EndCellX && CellY == EndCellY)
			{
				break;
			}

			CellX += StepX;
			CellY += StepY;
			bFirstCell = false;
		}

		return true;
	}

	template <typename TCanUseFaceCell>
	bool TryBuildFaceLocalLPath(
		const FSRPlanetSurfaceGridCellId& StartCellId,
		const FSRPlanetSurfaceGridCellId& EndCellId,
		int32 Resolution,
		bool bMoveXFirst,
		const TCanUseFaceCell& CanUseFaceCell,
		TArray<FSRPlanetSurfaceGridCellId>& OutPath)
	{
		OutPath.Reset();
		const int32 CornerCellX = bMoveXFirst ? EndCellId.CellX : StartCellId.CellX;
		const int32 CornerCellY = bMoveXFirst ? StartCellId.CellY : EndCellId.CellY;

		if (!TryAppendFaceLocalAxisSegment(
				StartCellId.Face,
				Resolution,
				StartCellId.CellX,
				StartCellId.CellY,
				CornerCellX,
				CornerCellY,
				true,
				CanUseFaceCell,
				OutPath))
		{
			OutPath.Reset();
			return false;
		}

		if (!TryAppendFaceLocalAxisSegment(
				StartCellId.Face,
				Resolution,
				CornerCellX,
				CornerCellY,
				EndCellId.CellX,
				EndCellId.CellY,
				false,
				CanUseFaceCell,
				OutPath))
		{
			OutPath.Reset();
			return false;
		}

		return !OutPath.IsEmpty() && OutPath.Last() == EndCellId;
	}

	template <typename TCanUseFaceCell>
	bool TryBuildFaceLocalAStarPath(
		const FSRPlanetSurfaceGridCellId& StartCellId,
		const FSRPlanetSurfaceGridCellId& EndCellId,
		int32 Resolution,
		const TCanUseFaceCell& CanUseFaceCell,
		TArray<FSRPlanetSurfaceGridCellId>& OutPath)
	{
		OutPath.Reset();

		const int32 CellCount = Resolution * Resolution;
		const int32 StartCellIndex = GetFaceLocalCellIndex(Resolution, StartCellId.CellX, StartCellId.CellY);
		const int32 EndCellIndex = GetFaceLocalCellIndex(Resolution, EndCellId.CellX, EndCellId.CellY);
		if (CellCount <= 0
			|| StartCellIndex == INDEX_NONE
			|| EndCellIndex == INDEX_NONE
			|| !CanUseFaceCell(StartCellIndex)
			|| !CanUseFaceCell(EndCellIndex))
		{
			return false;
		}

		if (StartCellIndex == EndCellIndex)
		{
			OutPath.Add(StartCellId);
			return true;
		}

		TArray<FSRConveyorFacePathSearchNode> OpenHeap;
		TArray<int32> BestCostFromStart;
		TArray<int32> CameFrom;
		TArray<uint8> bClosed;
		BestCostFromStart.Init(MAX_int32, CellCount);
		CameFrom.Init(INDEX_NONE, CellCount);
		bClosed.Init(0, CellCount);

		int32 NextSequence = 0;
		FSRConveyorFacePathSearchNode StartNode;
		StartNode.CellIndex = StartCellIndex;
		StartNode.CostFromStart = 0;
		StartNode.EstimatedCostToEnd = GetFaceLocalManhattanCost(Resolution, StartCellIndex, EndCellIndex);
		StartNode.Sequence = NextSequence++;
		PushConveyorFacePathSearchNode(OpenHeap, StartNode);
		BestCostFromStart[StartCellIndex] = 0;

		const int32 NeighborOffsets[4][2] =
		{
			{ -1, 0 },
			{ 1, 0 },
			{ 0, -1 },
			{ 0, 1 },
		};

		FSRConveyorFacePathSearchNode CurrentNode;
		while (PopConveyorFacePathSearchNode(OpenHeap, CurrentNode))
		{
			if (!BestCostFromStart.IsValidIndex(CurrentNode.CellIndex)
				|| BestCostFromStart[CurrentNode.CellIndex] != CurrentNode.CostFromStart
				|| bClosed[CurrentNode.CellIndex] != 0)
			{
				continue;
			}

			if (CurrentNode.CellIndex == EndCellIndex)
			{
				return BuildFaceLocalConveyorPathFromCameFrom(
					StartCellId.Face,
					Resolution,
					StartCellIndex,
					EndCellIndex,
					CameFrom,
					OutPath);
			}

			bClosed[CurrentNode.CellIndex] = 1;
			const int32 CurrentCellX = CurrentNode.CellIndex % Resolution;
			const int32 CurrentCellY = CurrentNode.CellIndex / Resolution;
			for (const int32* NeighborOffset : NeighborOffsets)
			{
				const int32 NeighborCellX = CurrentCellX + NeighborOffset[0];
				const int32 NeighborCellY = CurrentCellY + NeighborOffset[1];
				const int32 NeighborCellIndex = GetFaceLocalCellIndex(Resolution, NeighborCellX, NeighborCellY);
				if (NeighborCellIndex == INDEX_NONE
					|| bClosed[NeighborCellIndex] != 0
					|| !CanUseFaceCell(NeighborCellIndex))
				{
					continue;
				}

				const int32 NeighborCostFromStart = CurrentNode.CostFromStart + 1;
				if (BestCostFromStart[NeighborCellIndex] <= NeighborCostFromStart)
				{
					continue;
				}

				BestCostFromStart[NeighborCellIndex] = NeighborCostFromStart;
				CameFrom[NeighborCellIndex] = CurrentNode.CellIndex;

				FSRConveyorFacePathSearchNode NeighborNode;
				NeighborNode.CellIndex = NeighborCellIndex;
				NeighborNode.CostFromStart = NeighborCostFromStart;
				NeighborNode.EstimatedCostToEnd = GetFaceLocalManhattanCost(Resolution, NeighborCellIndex, EndCellIndex);
				NeighborNode.Sequence = NextSequence++;
				PushConveyorFacePathSearchNode(OpenHeap, NeighborNode);
			}
		}

		return false;
	}

	template <typename TCanUseCell>
	bool TryBuildFaceLocalConveyorPath(
		const FSRPlanetSurfaceGridCellId& StartCellId,
		const FSRPlanetSurfaceGridCellId& EndCellId,
		int32 Resolution,
		const TCanUseCell& CanUseCell,
		TArray<FSRPlanetSurfaceGridCellId>& OutPath)
	{
		OutPath.Reset();
		if (StartCellId.Face != EndCellId.Face
			|| !StartCellId.IsValid(Resolution)
			|| !EndCellId.IsValid(Resolution))
		{
			return false;
		}

		const int32 CellCount = Resolution * Resolution;
		if (CellCount <= 0)
		{
			return false;
		}

		TArray<uint8> WalkableStateByIndex;
		WalkableStateByIndex.Init(0, CellCount);
		const auto CanUseFaceCell = [&CanUseCell, &WalkableStateByIndex, Face = StartCellId.Face, Resolution](int32 CellIndex)
		{
			if (!WalkableStateByIndex.IsValidIndex(CellIndex))
			{
				return false;
			}

			uint8& WalkableState = WalkableStateByIndex[CellIndex];
			if (WalkableState == 0)
			{
				const FSRPlanetSurfaceGridCellId CellId = MakeFaceLocalCellId(Face, Resolution, CellIndex);
				WalkableState = CanUseCell(CellId) ? 2 : 1;
			}

			return WalkableState == 2;
		};

		if (TryBuildFaceLocalLPath(StartCellId, EndCellId, Resolution, true, CanUseFaceCell, OutPath))
		{
			return true;
		}

		if (TryBuildFaceLocalLPath(StartCellId, EndCellId, Resolution, false, CanUseFaceCell, OutPath))
		{
			return true;
		}

		return TryBuildFaceLocalAStarPath(StartCellId, EndCellId, Resolution, CanUseFaceCell, OutPath);
	}
}

bool USRConveyorNetworkComponent::FindConveyorPath(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& StartCellId,
	const FSRPlanetSurfaceGridCellId& EndCellId,
	int32 Layer,
	TArray<FSRPlanetSurfaceGridCellId>& OutPath) const
{
	const TSet<FSRPlanetSurfaceGridCellId> EmptyBlockedCellIds;
	return FindConveyorPathAvoidingCells(
		SurfaceGrid,
		StartCellId,
		EndCellId,
		Layer,
		EmptyBlockedCellIds,
		OutPath);
}

bool USRConveyorNetworkComponent::FindConveyorPathAvoidingCells(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& StartCellId,
	const FSRPlanetSurfaceGridCellId& EndCellId,
	int32 Layer,
	const TSet<FSRPlanetSurfaceGridCellId>& AdditionalBlockedCellIds,
	TArray<FSRPlanetSurfaceGridCellId>& OutPath) const
{
	OutPath.Reset();
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	FSRPlanetSurfaceGridCell StartCell;
	FSRPlanetSurfaceGridCell EndCell;
	if (!SurfaceGrid->GetCellById(StartCellId, StartCell) || !SurfaceGrid->GetCellById(EndCellId, EndCell))
	{
		return false;
	}

	const int32 SafeLayer = FMath::Max(0, Layer);
	if (!(StartCellId == EndCellId) && AdditionalBlockedCellIds.Contains(EndCellId))
	{
		return false;
	}

	const auto CanUseCellForPathSearch = [this, SurfaceGrid, SafeLayer, StartCellId, EndCellId, &AdditionalBlockedCellIds](const FSRPlanetSurfaceGridCellId& CellId)
	{
		if (AdditionalBlockedCellIds.Contains(CellId))
		{
			return CellId == StartCellId;
		}

		const FSRConveyorLaneKey LaneKey = MakeLaneKey(CellId, SafeLayer);
		if (Segments.Contains(LaneKey))
		{
			return CellId == StartCellId || CellId == EndCellId;
		}

		return CanPlaceConveyorSegment(SurfaceGrid, LaneKey);
	};

	if (!CanUseCellForPathSearch(StartCellId))
	{
		return false;
	}

	if (StartCellId.Face == EndCellId.Face)
	{
		return TryBuildFaceLocalConveyorPath(
			StartCellId,
			EndCellId,
			SurfaceGrid->GetFaceResolution(),
			CanUseCellForPathSearch,
			OutPath);
	}

	TArray<FSRConveyorPathSearchNode> OpenHeap;
	TMap<FSRPlanetSurfaceGridCellId, int32> BestCostFromStart;
	TMap<FSRPlanetSurfaceGridCellId, FSRPlanetSurfaceGridCellId> CameFrom;
	int32 NextSequence = 0;

	FSRConveyorPathSearchNode StartNode;
	StartNode.CellId = StartCellId;
	StartNode.CostFromStart = 0;
	StartNode.EstimatedCostToEnd = GetConveyorPathHeuristicCost(StartCellId, EndCellId);
	StartNode.Sequence = NextSequence++;
	PushConveyorPathSearchNode(OpenHeap, StartNode);
	BestCostFromStart.Add(StartCellId, 0);

	FSRConveyorPathSearchNode CurrentNode;
	while (PopConveyorPathSearchNode(OpenHeap, CurrentNode))
	{
		const int32* CurrentBestCost = BestCostFromStart.Find(CurrentNode.CellId);
		if (!CurrentBestCost || *CurrentBestCost != CurrentNode.CostFromStart)
		{
			continue;
		}

		const FSRPlanetSurfaceGridCellId CurrentCellId = CurrentNode.CellId;
		if (CurrentCellId == EndCellId)
		{
			return BuildConveyorPathFromCameFrom(StartCellId, EndCellId, CameFrom, OutPath);
		}

		FSRPlanetSurfaceGridCellNeighbors Neighbors;
		if (!SurfaceGrid->GetCellNeighbors(CurrentCellId, Neighbors))
		{
			continue;
		}

		const ESRConveyorGridDirection Directions[] =
		{
			ESRConveyorGridDirection::NegativeU,
			ESRConveyorGridDirection::PositiveU,
			ESRConveyorGridDirection::NegativeV,
			ESRConveyorGridDirection::PositiveV,
		};

		for (const ESRConveyorGridDirection Direction : Directions)
		{
			FSRPlanetSurfaceGridCellId NeighborCellId;
			if (!GetNeighborCellIdByDirection(Neighbors, Direction, NeighborCellId))
			{
				continue;
			}

			if (!CanUseCellForPathSearch(NeighborCellId))
			{
				continue;
			}

			const int32 NeighborCostFromStart = CurrentNode.CostFromStart + 1;
			const int32* ExistingNeighborCost = BestCostFromStart.Find(NeighborCellId);
			if (ExistingNeighborCost && *ExistingNeighborCost <= NeighborCostFromStart)
			{
				continue;
			}

			BestCostFromStart.Add(NeighborCellId, NeighborCostFromStart);
			CameFrom.Add(NeighborCellId, CurrentCellId);

			FSRConveyorPathSearchNode NeighborNode;
			NeighborNode.CellId = NeighborCellId;
			NeighborNode.CostFromStart = NeighborCostFromStart;
			NeighborNode.EstimatedCostToEnd = GetConveyorPathHeuristicCost(NeighborCellId, EndCellId);
			NeighborNode.Sequence = NextSequence++;
			PushConveyorPathSearchNode(OpenHeap, NeighborNode);
		}
	}

	return false;
}

FSRConveyorLaneKey USRConveyorNetworkComponent::MakeLaneKey(const FSRPlanetSurfaceGridCellId& CellId, int32 Layer)
{
	FSRConveyorLaneKey LaneKey;
	LaneKey.CellId = CellId;
	LaneKey.Layer = FMath::Max(0, Layer);
	return LaneKey;
}

ESRConveyorGridDirection USRConveyorNetworkComponent::GetOppositeDirection(ESRConveyorGridDirection Direction)
{
	switch (Direction)
	{
	case ESRConveyorGridDirection::NegativeU:
		return ESRConveyorGridDirection::PositiveU;
	case ESRConveyorGridDirection::PositiveU:
		return ESRConveyorGridDirection::NegativeU;
	case ESRConveyorGridDirection::NegativeV:
		return ESRConveyorGridDirection::PositiveV;
	case ESRConveyorGridDirection::PositiveV:
		return ESRConveyorGridDirection::NegativeV;
	default:
		return ESRConveyorGridDirection::None;
	}
}

ESRConveyorSegmentShape USRConveyorNetworkComponent::ResolveSegmentShape(ESRConveyorGridDirection InputDirection, ESRConveyorGridDirection OutputDirection)
{
	if (InputDirection == ESRConveyorGridDirection::None || OutputDirection == ESRConveyorGridDirection::None)
	{
		return ESRConveyorSegmentShape::End;
	}

	return GetOppositeDirection(InputDirection) == OutputDirection
		? ESRConveyorSegmentShape::Straight
		: ESRConveyorSegmentShape::Corner;
}

bool USRConveyorNetworkComponent::GetNeighborCellIdByDirection(const FSRPlanetSurfaceGridCellNeighbors& Neighbors, ESRConveyorGridDirection Direction, FSRPlanetSurfaceGridCellId& OutCellId)
{
	switch (Direction)
	{
	case ESRConveyorGridDirection::NegativeU:
		OutCellId = Neighbors.NegativeU;
		return true;
	case ESRConveyorGridDirection::PositiveU:
		OutCellId = Neighbors.PositiveU;
		return true;
	case ESRConveyorGridDirection::NegativeV:
		OutCellId = Neighbors.NegativeV;
		return true;
	case ESRConveyorGridDirection::PositiveV:
		OutCellId = Neighbors.PositiveV;
		return true;
	default:
		OutCellId = FSRPlanetSurfaceGridCellId();
		return false;
	}
}

bool USRConveyorNetworkComponent::FindDirectionBetweenCells(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& FromCellId, const FSRPlanetSurfaceGridCellId& ToCellId, ESRConveyorGridDirection& OutDirection)
{
	OutDirection = ESRConveyorGridDirection::None;
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	FSRPlanetSurfaceGridCellNeighbors Neighbors;
	if (!SurfaceGrid->GetCellNeighbors(FromCellId, Neighbors))
	{
		return false;
	}

	const ESRConveyorGridDirection Directions[] =
	{
		ESRConveyorGridDirection::NegativeU,
		ESRConveyorGridDirection::PositiveU,
		ESRConveyorGridDirection::NegativeV,
		ESRConveyorGridDirection::PositiveV,
	};
	for (const ESRConveyorGridDirection Direction : Directions)
	{
		FSRPlanetSurfaceGridCellId NeighborCellId;
		if (GetNeighborCellIdByDirection(Neighbors, Direction, NeighborCellId) && NeighborCellId == ToCellId)
		{
			OutDirection = Direction;
			return true;
		}
	}

	return false;
}

int32 USRConveyorNetworkComponent::GetConveyorDirectionClockwiseOrder(ESRConveyorGridDirection Direction)
{
	switch (Direction)
	{
	case ESRConveyorGridDirection::NegativeV:
		return 0;
	case ESRConveyorGridDirection::PositiveU:
		return 1;
	case ESRConveyorGridDirection::PositiveV:
		return 2;
	case ESRConveyorGridDirection::NegativeU:
		return 3;
	default:
		return MAX_int32;
	}
}

void USRConveyorNetworkComponent::SortConveyorDirectionsClockwise(TArray<ESRConveyorGridDirection>& Directions)
{
	Directions.Sort([](ESRConveyorGridDirection Left, ESRConveyorGridDirection Right)
	{
		return USRConveyorNetworkComponent::GetConveyorDirectionClockwiseOrder(Left)
			< USRConveyorNetworkComponent::GetConveyorDirectionClockwiseOrder(Right);
	});
}

void USRConveyorNetworkComponent::CollectConveyorInputDirections(const FSRConveyorSegment& Segment, TArray<ESRConveyorGridDirection>& OutDirections)
{
	OutDirections.Reset();
	auto AddDirection = [&OutDirections](ESRConveyorGridDirection Direction)
	{
		if (Direction != ESRConveyorGridDirection::None && !OutDirections.Contains(Direction))
		{
			OutDirections.Add(Direction);
		}
	};

	AddDirection(Segment.InputDirection);
	AddDirection(Segment.MergeInputDirection);
	AddDirection(Segment.SecondMergeInputDirection);
	SortConveyorDirectionsClockwise(OutDirections);
}

void USRConveyorNetworkComponent::CollectConveyorOutputDirections(const FSRConveyorSegment& Segment, TArray<ESRConveyorGridDirection>& OutDirections)
{
	OutDirections.Reset();
	auto AddDirection = [&OutDirections](ESRConveyorGridDirection Direction)
	{
		if (Direction != ESRConveyorGridDirection::None && !OutDirections.Contains(Direction))
		{
			OutDirections.Add(Direction);
		}
	};

	AddDirection(Segment.OutputDirection);
	AddDirection(Segment.BranchOutputDirection);
	AddDirection(Segment.SecondBranchOutputDirection);
	SortConveyorDirectionsClockwise(OutDirections);
}

namespace
{
	bool AreConveyorBranchCountsValid(int32 InputDirectionCount, int32 OutputDirectionCount)
	{
		constexpr int32 MaxConveyorBranchDirectionCount = 3;
		if (InputDirectionCount > MaxConveyorBranchDirectionCount
			|| OutputDirectionCount > MaxConveyorBranchDirectionCount)
		{
			return false;
		}

		return InputDirectionCount <= 1 || OutputDirectionCount <= 1;
	}
}

bool USRConveyorNetworkComponent::CanMergeConveyorSegment(const FSRConveyorSegment& Segment) const
{
	const FSRConveyorSegment* ExistingSegment = Segments.Find(Segment.Lane);
	if (!ExistingSegment)
	{
		return true;
	}

	TArray<ESRConveyorGridDirection> InputDirections;
	TArray<ESRConveyorGridDirection> OutputDirections;
	CollectConveyorInputDirections(*ExistingSegment, InputDirections);
	CollectConveyorOutputDirections(*ExistingSegment, OutputDirections);
	auto CanAddDirection = [](TArray<ESRConveyorGridDirection>& Directions, ESRConveyorGridDirection IncomingDirection)
	{
		if (IncomingDirection == ESRConveyorGridDirection::None || Directions.Contains(IncomingDirection))
		{
			return true;
		}
		if (Directions.Num() >= 3)
		{
			return false;
		}

		Directions.Add(IncomingDirection);
		return true;
	};

	if (!CanAddDirection(InputDirections, Segment.InputDirection)
		|| !CanAddDirection(InputDirections, Segment.MergeInputDirection)
		|| !CanAddDirection(InputDirections, Segment.SecondMergeInputDirection)
		|| !CanAddDirection(OutputDirections, Segment.OutputDirection)
		|| !CanAddDirection(OutputDirections, Segment.BranchOutputDirection)
		|| !CanAddDirection(OutputDirections, Segment.SecondBranchOutputDirection))
	{
		return false;
	}

	return AreConveyorBranchCountsValid(InputDirections.Num(), OutputDirections.Num());
}

void USRConveyorNetworkComponent::MergeConveyorSegment(const FSRConveyorSegment& Segment)
{
	FSRConveyorSegment* ExistingSegment = Segments.Find(Segment.Lane);
	if (!ExistingSegment)
	{
		Segments.Add(Segment.Lane, Segment);
		return;
	}

	MergeConveyorInputDirection(*ExistingSegment, Segment.InputDirection);
	MergeConveyorInputDirection(*ExistingSegment, Segment.MergeInputDirection);
	MergeConveyorInputDirection(*ExistingSegment, Segment.SecondMergeInputDirection);
	MergeConveyorOutputDirection(*ExistingSegment, Segment.OutputDirection);
	MergeConveyorOutputDirection(*ExistingSegment, Segment.BranchOutputDirection);
	MergeConveyorOutputDirection(*ExistingSegment, Segment.SecondBranchOutputDirection);
	if (ExistingSegment->NetworkId.IsNone())
	{
		ExistingSegment->NetworkId = Segment.NetworkId;
	}
	if (!IsValid(ExistingSegment->StructureDataAsset.Get()) && IsValid(Segment.StructureDataAsset.Get()))
	{
		ExistingSegment->StructureDataAsset = Segment.StructureDataAsset;
	}

	ExistingSegment->Shape = ResolveSegmentShape(ExistingSegment->InputDirection, ExistingSegment->OutputDirection);
}

void USRConveyorNetworkComponent::MergeConveyorInputDirection(FSRConveyorSegment& ExistingSegment, ESRConveyorGridDirection IncomingDirection)
{
	if (IncomingDirection == ESRConveyorGridDirection::None
		|| ExistingSegment.InputDirection == IncomingDirection
		|| ExistingSegment.MergeInputDirection == IncomingDirection
		|| ExistingSegment.SecondMergeInputDirection == IncomingDirection)
	{
		return;
	}

	if (ExistingSegment.InputDirection == ESRConveyorGridDirection::None)
	{
		ExistingSegment.InputDirection = IncomingDirection;
		return;
	}

	if (ExistingSegment.MergeInputDirection == ESRConveyorGridDirection::None)
	{
		ExistingSegment.MergeInputDirection = IncomingDirection;
		ExistingSegment.NextInputDirectionIndex = 0;
		return;
	}

	if (ExistingSegment.SecondMergeInputDirection == ESRConveyorGridDirection::None)
	{
		ExistingSegment.SecondMergeInputDirection = IncomingDirection;
		ExistingSegment.NextInputDirectionIndex = 0;
	}
}

void USRConveyorNetworkComponent::MergeConveyorOutputDirection(FSRConveyorSegment& ExistingSegment, ESRConveyorGridDirection IncomingDirection)
{
	if (IncomingDirection == ESRConveyorGridDirection::None
		|| ExistingSegment.OutputDirection == IncomingDirection
		|| ExistingSegment.BranchOutputDirection == IncomingDirection
		|| ExistingSegment.SecondBranchOutputDirection == IncomingDirection)
	{
		return;
	}

	if (ExistingSegment.OutputDirection == ESRConveyorGridDirection::None)
	{
		ExistingSegment.OutputDirection = IncomingDirection;
		return;
	}

	if (ExistingSegment.BranchOutputDirection == ESRConveyorGridDirection::None)
	{
		ExistingSegment.BranchOutputDirection = IncomingDirection;
		ExistingSegment.NextOutputDirectionIndex = 0;
		return;
	}

	if (ExistingSegment.SecondBranchOutputDirection == ESRConveyorGridDirection::None)
	{
		ExistingSegment.SecondBranchOutputDirection = IncomingDirection;
		ExistingSegment.NextOutputDirectionIndex = 0;
	}
}

bool USRConveyorNetworkComponent::CanPlaceConveyorSegment(USRPlanetSurfaceGrid* SurfaceGrid, const FSRConveyorLaneKey& LaneKey) const
{
	if (!IsValid(SurfaceGrid) || Segments.Contains(LaneKey))
	{
		return false;
	}

	FSRPlanetSurfaceGridCellInfo CellInfo;
	if (!SurfaceGrid->GetCellInfoById(LaneKey.CellId, CellInfo))
	{
		return false;
	}

	if (LaneKey.Layer > 0)
	{
		return true;
	}

	if (!CellInfo.bOccupied)
	{
		return CellInfo.bCanConstruct;
	}

	return CanDestroyNaturalStructureForConveyorPlacement(SurfaceGrid, CellInfo.OccupantId);
}

bool USRConveyorNetworkComponent::CanDestroyNaturalStructureForConveyorPlacement(USRPlanetSurfaceGrid* SurfaceGrid, FName OccupantId) const
{
	if (!IsValid(SurfaceGrid) || OccupantId.IsNone())
	{
		return false;
	}

	AActor* SurfaceOwner = SurfaceGrid->GetOwner();
	const USRStructureInstanceManagerComponent* StructureInstanceManager = IsValid(SurfaceOwner)
		? SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>()
		: nullptr;
	return IsValid(StructureInstanceManager)
		&& StructureInstanceManager->CanDestroyNaturalStructureForConstruction(OccupantId);
}

float USRConveyorNetworkComponent::ResolveConveyorLayerHeight(USRPlanetSurfaceGrid* SurfaceGrid, float RequestedLayerHeight) const
{
	const float TerrainHeightStep = IsValid(SurfaceGrid) ? SurfaceGrid->GetTerrainHeightStep() : 0.0f;
	if (TerrainHeightStep > KINDA_SMALL_NUMBER)
	{
		return TerrainHeightStep;
	}

	if (RequestedLayerHeight > KINDA_SMALL_NUMBER)
	{
		return RequestedLayerHeight;
	}

	return DefaultLayerHeight;
}
