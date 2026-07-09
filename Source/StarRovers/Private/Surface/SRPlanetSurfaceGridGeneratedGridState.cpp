#include "SRPlanetSurfaceGridGeneratedGridState.h"

#include "SRPlanetSurfaceGridOwnerBody.h"

namespace SurfaceGridOwnerBody = StarRovers::SurfaceGridOwnerBody;

namespace
{
	bool TryInferFaceResolutionFromCells(const TArray<FSRPlanetSurfaceGridCell>& Cells, int32& OutFaceResolution)
	{
		TMap<ESRCubeSphereFace, int32> CellCountByFace;
		for (const FSRPlanetSurfaceGridCell& Cell : Cells)
		{
			CellCountByFace.FindOrAdd(Cell.CellId.Face)++;
		}

		for (const TPair<ESRCubeSphereFace, int32>& CellCountPair : CellCountByFace)
		{
			OutFaceResolution = FMath::Max(1, FMath::RoundToInt(FMath::Sqrt(static_cast<float>(CellCountPair.Value))));
			return true;
		}

		return false;
	}
}

void StarRovers::SurfaceGridGeneratedGridState::AssignGeneratedCells(
	TArray<FSRPlanetSurfaceGridCell>& TargetCells,
	TArray<FSRPlanetSurfaceGridCell>&& NewCells,
	int32& FaceResolution,
	bool& bUsingGeneratedGridCells)
{
	TargetCells = MoveTemp(NewCells);
	bUsingGeneratedGridCells = true;

	int32 InferredFaceResolution = FaceResolution;
	if (TryInferFaceResolutionFromCells(TargetCells, InferredFaceResolution))
	{
		FaceResolution = InferredFaceResolution;
	}
}

bool StarRovers::SurfaceGridGeneratedGridState::TryLoadOwnerCachedCells(
	const AActor* Owner,
	TArray<FSRPlanetSurfaceGridCell>& TargetCells,
	bool& bUsingGeneratedGridCells)
{
	if (!SurfaceGridOwnerBody::GetCachedSurfaceGridCells(Owner, TargetCells))
	{
		return false;
	}

	bUsingGeneratedGridCells = true;
	return true;
}

void StarRovers::SurfaceGridGeneratedGridState::ResetGeneratedGridInteractionState(
	bool& bHasHoveredCell,
	FSRPlanetSurfaceGridCellId& HoveredCellId,
	bool& bHasSelectedCell,
	FSRPlanetSurfaceGridCellId& SelectedCellId,
	TArray<FSRPlanetSurfaceGridCellId>& InputPortPreviewCellIds,
	TArray<FSRPlanetSurfaceGridCellId>& OutputPortPreviewCellIds,
	TArray<FSRPlanetSurfaceGridCellId>& DeletionPreviewCellIds,
	TArray<FSRPlanetSurfaceGridCellId>& InvalidPreviewCellIds)
{
	bHasHoveredCell = false;
	HoveredCellId = FSRPlanetSurfaceGridCellId();
	bHasSelectedCell = false;
	SelectedCellId = FSRPlanetSurfaceGridCellId();
	InputPortPreviewCellIds.Reset();
	OutputPortPreviewCellIds.Reset();
	DeletionPreviewCellIds.Reset();
	InvalidPreviewCellIds.Reset();
}
