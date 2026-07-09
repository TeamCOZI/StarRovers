#include "Assembly/SRAssemblyStructureDragPathBuilder.h"

#include "Assembly/SRAssemblyPlacementDragState.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace StarRovers::Assembly
{
	bool FSRAssemblyStructureDragPathBuilder::BuildQueuedCellIds(
		const FSRAssemblyPlacementDragState& PlacementDrag,
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& TargetCellId,
		TArray<FSRPlanetSurfaceGridCellId>& OutCellIds)
	{
		OutCellIds.Reset();
		if (!IsValid(SurfaceGrid))
		{
			return false;
		}

		TArray<FSRPlanetSurfaceGridCellId> CandidateCellIds;
		if (PlacementDrag.HasLastStructurePlacementDragCell(SurfaceGrid))
		{
			AppendGridLineCellIds(PlacementDrag.LastStructurePlacementDragCellId, TargetCellId, CandidateCellIds);
		}
		else
		{
			CandidateCellIds.Add(TargetCellId);
		}

		for (const FSRPlanetSurfaceGridCellId& CandidateCellId : CandidateCellIds)
		{
			if (PlacementDrag.IsLastStructurePlacementDragCell(SurfaceGrid, CandidateCellId))
			{
				continue;
			}

			OutCellIds.Add(CandidateCellId);
		}

		return true;
	}

	void FSRAssemblyStructureDragPathBuilder::AppendGridLineCellIds(
		const FSRPlanetSurfaceGridCellId& StartCellId,
		const FSRPlanetSurfaceGridCellId& EndCellId,
		TArray<FSRPlanetSurfaceGridCellId>& OutCellIds)
	{
		if (StartCellId.Face != EndCellId.Face)
		{
			OutCellIds.Add(EndCellId);
			return;
		}

		const int32 DeltaX = FMath::Abs(EndCellId.CellX - StartCellId.CellX);
		const int32 DeltaY = FMath::Abs(EndCellId.CellY - StartCellId.CellY);
		const int32 StepX = StartCellId.CellX < EndCellId.CellX ? 1 : -1;
		const int32 StepY = StartCellId.CellY < EndCellId.CellY ? 1 : -1;

		int32 CurrentX = StartCellId.CellX;
		int32 CurrentY = StartCellId.CellY;
		int32 Error = DeltaX - DeltaY;

		while (true)
		{
			FSRPlanetSurfaceGridCellId CellId;
			CellId.Face = StartCellId.Face;
			CellId.CellX = CurrentX;
			CellId.CellY = CurrentY;
			OutCellIds.Add(CellId);

			if (CurrentX == EndCellId.CellX && CurrentY == EndCellId.CellY)
			{
				break;
			}

			const int32 Error2 = Error * 2;
			if (Error2 > -DeltaY)
			{
				Error -= DeltaY;
				CurrentX += StepX;
			}
			if (Error2 < DeltaX)
			{
				Error += DeltaX;
				CurrentY += StepY;
			}
		}
	}
}
