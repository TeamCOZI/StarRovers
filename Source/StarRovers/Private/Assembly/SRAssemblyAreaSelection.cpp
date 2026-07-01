#include "Assembly/SRAssemblyAreaSelection.h"

#include "Surface/SRPlanetSurfaceGrid.h"

namespace StarRovers::Assembly
{
	namespace
	{
		struct FSRAssemblyAreaFaceBridge
		{
			FSRPlanetSurfaceGridCellId StartFaceCellId;
			FSRPlanetSurfaceGridCellId EndFaceCellId;
		};

		void AppendAssemblyAreaRectCellIds(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const FSRPlanetSurfaceGridCellId& CornerA,
			const FSRPlanetSurfaceGridCellId& CornerB,
			TArray<FSRPlanetSurfaceGridCellId>& OutCellIds)
		{
			if (!IsValid(SurfaceGrid) || CornerA.Face != CornerB.Face)
			{
				return;
			}

			const int32 MinX = FMath::Min(CornerA.CellX, CornerB.CellX);
			const int32 MaxX = FMath::Max(CornerA.CellX, CornerB.CellX);
			const int32 MinY = FMath::Min(CornerA.CellY, CornerB.CellY);
			const int32 MaxY = FMath::Max(CornerA.CellY, CornerB.CellY);

			for (int32 CellY = MinY; CellY <= MaxY; ++CellY)
			{
				for (int32 CellX = MinX; CellX <= MaxX; ++CellX)
				{
					FSRPlanetSurfaceGridCellId CellId;
					CellId.Face = CornerA.Face;
					CellId.CellX = CellX;
					CellId.CellY = CellY;

					FSRPlanetSurfaceGridCell Cell;
					if (SurfaceGrid->GetCellById(CellId, Cell))
					{
						OutCellIds.AddUnique(CellId);
					}
				}
			}
		}

		void CollectAssemblyAreaFaceBridges(
			USRPlanetSurfaceGrid* SurfaceGrid,
			ESRCubeSphereFace StartFace,
			ESRCubeSphereFace EndFace,
			TArray<FSRAssemblyAreaFaceBridge>& OutBridges)
		{
			OutBridges.Reset();
			if (!IsValid(SurfaceGrid) || StartFace == EndFace)
			{
				return;
			}

			const int32 FaceResolution = SurfaceGrid->GetFaceResolution();
			if (FaceResolution <= 0)
			{
				return;
			}

			for (int32 CellY = 0; CellY < FaceResolution; ++CellY)
			{
				for (int32 CellX = 0; CellX < FaceResolution; ++CellX)
				{
					FSRPlanetSurfaceGridCellId StartCellId;
					StartCellId.Face = StartFace;
					StartCellId.CellX = CellX;
					StartCellId.CellY = CellY;

					FSRPlanetSurfaceGridCellNeighbors Neighbors;
					if (!SurfaceGrid->GetCellNeighbors(StartCellId, Neighbors))
					{
						continue;
					}

					const FSRPlanetSurfaceGridCellId NeighborCellIds[] =
					{
						Neighbors.NegativeU,
						Neighbors.PositiveU,
						Neighbors.NegativeV,
						Neighbors.PositiveV,
					};

					for (const FSRPlanetSurfaceGridCellId& NeighborCellId : NeighborCellIds)
					{
						if (NeighborCellId.Face != EndFace)
						{
							continue;
						}

						FSRAssemblyAreaFaceBridge Bridge;
						Bridge.StartFaceCellId = StartCellId;
						Bridge.EndFaceCellId = NeighborCellId;
						OutBridges.Add(Bridge);
					}
				}
			}
		}

		bool AreAssemblyAreaBridgeCoordsConstant(
			const TArray<FSRAssemblyAreaFaceBridge>& Bridges,
			bool bUseStartFace,
			bool bUseX)
		{
			if (Bridges.IsEmpty())
			{
				return false;
			}

			const FSRPlanetSurfaceGridCellId& FirstCellId = bUseStartFace ? Bridges[0].StartFaceCellId : Bridges[0].EndFaceCellId;
			const int32 FirstCoord = bUseX ? FirstCellId.CellX : FirstCellId.CellY;
			for (const FSRAssemblyAreaFaceBridge& Bridge : Bridges)
			{
				const FSRPlanetSurfaceGridCellId& CellId = bUseStartFace ? Bridge.StartFaceCellId : Bridge.EndFaceCellId;
				const int32 Coord = bUseX ? CellId.CellX : CellId.CellY;
				if (Coord != FirstCoord)
				{
					return false;
				}
			}

			return true;
		}

		int32 GetAssemblyAreaBridgeAxisCoord(const FSRPlanetSurfaceGridCellId& CellId, bool bUseY)
		{
			return bUseY ? CellId.CellY : CellId.CellX;
		}

		const FSRAssemblyAreaFaceBridge* FindClosestAssemblyAreaBridgeByAxisCoord(
			const TArray<FSRAssemblyAreaFaceBridge>& Bridges,
			bool bUseStartFace,
			bool bUseY,
			int32 TargetCoord)
		{
			const FSRAssemblyAreaFaceBridge* BestBridge = nullptr;
			int32 BestDistance = MAX_int32;
			for (const FSRAssemblyAreaFaceBridge& Bridge : Bridges)
			{
				const FSRPlanetSurfaceGridCellId& CellId = bUseStartFace ? Bridge.StartFaceCellId : Bridge.EndFaceCellId;
				const int32 Distance = FMath::Abs(GetAssemblyAreaBridgeAxisCoord(CellId, bUseY) - TargetCoord);
				if (Distance < BestDistance)
				{
					BestBridge = &Bridge;
					BestDistance = Distance;
				}
			}

			return BestBridge;
		}
	}

	bool FSRAssemblyAreaSelection::IsSelectionDragActive() const
	{
		return bSelectionDragActive;
	}

	bool FSRAssemblyAreaSelection::IsDeletionDragActive() const
	{
		return bDeletionDragActive;
	}

	void FSRAssemblyAreaSelection::BeginSelectionDrag(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& StartCellId)
	{
		bSelectionDragActive = true;
		SelectionSurfaceGrid = SurfaceGrid;
		SelectionStartCellId = StartCellId;
		LastSelectionTargetCellId = FSRPlanetSurfaceGridCellId();
		bHasSelectionStartCell = true;
		bHasLastSelectionTargetCell = false;
		SelectionCellIds.Reset();
	}

	void FSRAssemblyAreaSelection::EndSelectionDrag()
	{
		bSelectionDragActive = false;
	}

	void FSRAssemblyAreaSelection::ClearSelection()
	{
		bSelectionDragActive = false;
		SelectionSurfaceGrid = nullptr;
		SelectionStartCellId = FSRPlanetSurfaceGridCellId();
		LastSelectionTargetCellId = FSRPlanetSurfaceGridCellId();
		bHasSelectionStartCell = false;
		bHasLastSelectionTargetCell = false;
		SelectionCellIds.Reset();
	}

	bool FSRAssemblyAreaSelection::HasSelectionStartCell() const
	{
		return bHasSelectionStartCell;
	}

	bool FSRAssemblyAreaSelection::HasSelectionCells() const
	{
		return !SelectionCellIds.IsEmpty();
	}

	bool FSRAssemblyAreaSelection::IsLastSelectionTargetCell(const FSRPlanetSurfaceGridCellId& CellId) const
	{
		return bHasLastSelectionTargetCell && LastSelectionTargetCellId == CellId;
	}

	void FSRAssemblyAreaSelection::SetSelectionCells(
		TArray<FSRPlanetSurfaceGridCellId>&& CellIds,
		const FSRPlanetSurfaceGridCellId& TargetCellId)
	{
		SelectionCellIds = MoveTemp(CellIds);
		LastSelectionTargetCellId = TargetCellId;
		bHasLastSelectionTargetCell = true;
	}

	USRPlanetSurfaceGrid* FSRAssemblyAreaSelection::GetSelectionSurfaceGrid() const
	{
		return SelectionSurfaceGrid.Get();
	}

	const FSRPlanetSurfaceGridCellId& FSRAssemblyAreaSelection::GetSelectionStartCellId() const
	{
		return SelectionStartCellId;
	}

	const TArray<FSRPlanetSurfaceGridCellId>& FSRAssemblyAreaSelection::GetSelectionCellIds() const
	{
		return SelectionCellIds;
	}

	bool FSRAssemblyAreaSelection::ResolveSelectionCenterCellId(FSRPlanetSurfaceGridCellId& OutCenterCellId) const
	{
		OutCenterCellId = FSRPlanetSurfaceGridCellId();
		if (SelectionCellIds.IsEmpty())
		{
			return false;
		}

		const ESRCubeSphereFace Face = SelectionCellIds[0].Face;
		int32 MinCellX = SelectionCellIds[0].CellX;
		int32 MaxCellX = SelectionCellIds[0].CellX;
		int32 MinCellY = SelectionCellIds[0].CellY;
		int32 MaxCellY = SelectionCellIds[0].CellY;
		for (const FSRPlanetSurfaceGridCellId& CellId : SelectionCellIds)
		{
			if (CellId.Face != Face)
			{
				return false;
			}

			MinCellX = FMath::Min(MinCellX, CellId.CellX);
			MaxCellX = FMath::Max(MaxCellX, CellId.CellX);
			MinCellY = FMath::Min(MinCellY, CellId.CellY);
			MaxCellY = FMath::Max(MaxCellY, CellId.CellY);
		}

		OutCenterCellId.Face = Face;
		OutCenterCellId.CellX = MinCellX + (MaxCellX - MinCellX) / 2;
		OutCenterCellId.CellY = MinCellY + (MaxCellY - MinCellY) / 2;
		return true;
	}

	void FSRAssemblyAreaSelection::BeginDeletionDrag(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& StartCellId)
	{
		bDeletionDragActive = true;
		DeletionSurfaceGrid = SurfaceGrid;
		DeletionStartCellId = StartCellId;
		LastDeletionTargetCellId = FSRPlanetSurfaceGridCellId();
		bHasDeletionStartCell = true;
		bHasLastDeletionTargetCell = false;
		DeletionCellIds.Reset();
	}

	void FSRAssemblyAreaSelection::EndDeletionDrag()
	{
		bDeletionDragActive = false;
	}

	void FSRAssemblyAreaSelection::ClearDeletion()
	{
		bDeletionDragActive = false;
		DeletionSurfaceGrid = nullptr;
		DeletionStartCellId = FSRPlanetSurfaceGridCellId();
		LastDeletionTargetCellId = FSRPlanetSurfaceGridCellId();
		bHasDeletionStartCell = false;
		bHasLastDeletionTargetCell = false;
		DeletionCellIds.Reset();
	}

	bool FSRAssemblyAreaSelection::HasDeletionStartCell() const
	{
		return bHasDeletionStartCell;
	}

	bool FSRAssemblyAreaSelection::HasDeletionCells() const
	{
		return !DeletionCellIds.IsEmpty();
	}

	bool FSRAssemblyAreaSelection::IsLastDeletionTargetCell(const FSRPlanetSurfaceGridCellId& CellId) const
	{
		return bHasLastDeletionTargetCell && LastDeletionTargetCellId == CellId;
	}

	void FSRAssemblyAreaSelection::SetDeletionCells(
		TArray<FSRPlanetSurfaceGridCellId>&& CellIds,
		const FSRPlanetSurfaceGridCellId& TargetCellId)
	{
		DeletionCellIds = MoveTemp(CellIds);
		LastDeletionTargetCellId = TargetCellId;
		bHasLastDeletionTargetCell = true;
	}

	USRPlanetSurfaceGrid* FSRAssemblyAreaSelection::GetDeletionSurfaceGrid() const
	{
		return DeletionSurfaceGrid.Get();
	}

	const FSRPlanetSurfaceGridCellId& FSRAssemblyAreaSelection::GetDeletionStartCellId() const
	{
		return DeletionStartCellId;
	}

	const TArray<FSRPlanetSurfaceGridCellId>& FSRAssemblyAreaSelection::GetDeletionCellIds() const
	{
		return DeletionCellIds;
	}

	bool FSRAssemblyAreaSelection::BuildCellIds(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& StartCellId,
		const FSRPlanetSurfaceGridCellId& EndCellId,
		TArray<FSRPlanetSurfaceGridCellId>& OutCellIds) const
	{
		OutCellIds.Reset();
		if (!IsValid(SurfaceGrid))
		{
			return false;
		}

		if (StartCellId.Face == EndCellId.Face)
		{
			AppendAssemblyAreaRectCellIds(SurfaceGrid, StartCellId, EndCellId, OutCellIds);
			return !OutCellIds.IsEmpty();
		}

		TArray<FSRAssemblyAreaFaceBridge> Bridges;
		CollectAssemblyAreaFaceBridges(SurfaceGrid, StartCellId.Face, EndCellId.Face, Bridges);
		if (Bridges.IsEmpty())
		{
			return false;
		}

		const bool bStartEdgeXConstant = AreAssemblyAreaBridgeCoordsConstant(Bridges, true, true);
		const bool bStartEdgeYConstant = AreAssemblyAreaBridgeCoordsConstant(Bridges, true, false);
		const bool bEndEdgeXConstant = AreAssemblyAreaBridgeCoordsConstant(Bridges, false, true);
		const bool bEndEdgeYConstant = AreAssemblyAreaBridgeCoordsConstant(Bridges, false, false);
		if ((!bStartEdgeXConstant && !bStartEdgeYConstant) || (!bEndEdgeXConstant && !bEndEdgeYConstant))
		{
			return false;
		}

		const bool bStartBridgeAxisUseY = bStartEdgeXConstant;
		const bool bEndBridgeAxisUseY = bEndEdgeXConstant;
		const int32 StartAxisCoord = GetAssemblyAreaBridgeAxisCoord(StartCellId, bStartBridgeAxisUseY);
		const int32 EndAxisCoord = GetAssemblyAreaBridgeAxisCoord(EndCellId, bEndBridgeAxisUseY);

		const FSRAssemblyAreaFaceBridge* BridgeForStartAxis = FindClosestAssemblyAreaBridgeByAxisCoord(
			Bridges,
			true,
			bStartBridgeAxisUseY,
			StartAxisCoord);
		const FSRAssemblyAreaFaceBridge* BridgeForEndAxis = FindClosestAssemblyAreaBridgeByAxisCoord(
			Bridges,
			false,
			bEndBridgeAxisUseY,
			EndAxisCoord);
		if (!BridgeForStartAxis || !BridgeForEndAxis)
		{
			return false;
		}

		AppendAssemblyAreaRectCellIds(SurfaceGrid, StartCellId, BridgeForEndAxis->StartFaceCellId, OutCellIds);
		AppendAssemblyAreaRectCellIds(SurfaceGrid, BridgeForStartAxis->EndFaceCellId, EndCellId, OutCellIds);
		return !OutCellIds.IsEmpty();
	}
}
