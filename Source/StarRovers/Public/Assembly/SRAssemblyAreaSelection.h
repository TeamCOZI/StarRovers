#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"

class USRPlanetSurfaceGrid;

namespace StarRovers::Assembly
{
	class STARROVERS_API FSRAssemblyAreaSelection
	{
	public:
		bool IsSelectionDragActive() const;
		bool IsDeletionDragActive() const;

		void BeginSelectionDrag(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& StartCellId);
		void EndSelectionDrag();
		void ClearSelection();
		void ClearSelectionPreview();
		bool HasSelectionStartCell() const;
		bool HasSelectionCells() const;
		bool IsLastSelectionTargetCell(const FSRPlanetSurfaceGridCellId& CellId) const;
		void SetSelectionCells(TArray<FSRPlanetSurfaceGridCellId>&& CellIds, const FSRPlanetSurfaceGridCellId& TargetCellId);
		bool UpdateSelectionPreview(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& TargetCellId);
		USRPlanetSurfaceGrid* GetSelectionSurfaceGrid() const;
		const FSRPlanetSurfaceGridCellId& GetSelectionStartCellId() const;
		const TArray<FSRPlanetSurfaceGridCellId>& GetSelectionCellIds() const;
		bool ResolveSelectionCenterCellId(FSRPlanetSurfaceGridCellId& OutCenterCellId) const;

		void BeginDeletionDrag(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& StartCellId);
		void EndDeletionDrag();
		void ClearDeletion();
		void ClearDeletionPreview();
		bool HasDeletionStartCell() const;
		bool HasDeletionCells() const;
		bool IsLastDeletionTargetCell(const FSRPlanetSurfaceGridCellId& CellId) const;
		void SetDeletionCells(TArray<FSRPlanetSurfaceGridCellId>&& CellIds, const FSRPlanetSurfaceGridCellId& TargetCellId);
		bool UpdateDeletionPreview(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& TargetCellId);
		USRPlanetSurfaceGrid* GetDeletionSurfaceGrid() const;
		const FSRPlanetSurfaceGridCellId& GetDeletionStartCellId() const;
		const TArray<FSRPlanetSurfaceGridCellId>& GetDeletionCellIds() const;
		bool DeleteCells(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const TArray<FSRPlanetSurfaceGridCellId>& CellIds) const;

		bool BuildCellIds(
			USRPlanetSurfaceGrid* SurfaceGrid,
			const FSRPlanetSurfaceGridCellId& StartCellId,
			const FSRPlanetSurfaceGridCellId& EndCellId,
			TArray<FSRPlanetSurfaceGridCellId>& OutCellIds) const;

	private:
		bool bSelectionDragActive = false;
		TWeakObjectPtr<USRPlanetSurfaceGrid> SelectionSurfaceGrid;
		FSRPlanetSurfaceGridCellId SelectionStartCellId;
		FSRPlanetSurfaceGridCellId LastSelectionTargetCellId;
		bool bHasSelectionStartCell = false;
		bool bHasLastSelectionTargetCell = false;
		TArray<FSRPlanetSurfaceGridCellId> SelectionCellIds;

		bool bDeletionDragActive = false;
		TWeakObjectPtr<USRPlanetSurfaceGrid> DeletionSurfaceGrid;
		FSRPlanetSurfaceGridCellId DeletionStartCellId;
		FSRPlanetSurfaceGridCellId LastDeletionTargetCellId;
		bool bHasDeletionStartCell = false;
		bool bHasLastDeletionTargetCell = false;
		TArray<FSRPlanetSurfaceGridCellId> DeletionCellIds;
	};
}
