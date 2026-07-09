#include "Assembly/SRAssemblySurfaceHoverUpdater.h"

#include "Assembly/SRAssemblySurfaceCursorQuery.h"
#include "Assembly/SRAssemblySurfaceState.h"
#include "Camera/SRPlayerController.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	FString FormatSurfaceHoverCellId(const FSRPlanetSurfaceGridCellId& CellId)
	{
		return FString::Printf(
			TEXT("Face=%d X=%d Y=%d"),
			static_cast<int32>(CellId.Face),
			CellId.CellX,
			CellId.CellY);
	}
}

StarRovers::Assembly::ESRAssemblySurfaceHoverUpdateResult StarRovers::Assembly::FSRAssemblySurfaceHoverUpdater::Update(
	ASRPlayerController* PlayerController,
	bool bAssemblyModeActive,
	USRPlanetSurfaceGrid*& HoveredSurfaceGrid,
	FSRAssemblySurfaceState& SurfaceState,
	FSRAssemblySurfaceHoverUpdate& OutUpdate)
{
	OutUpdate = FSRAssemblySurfaceHoverUpdate();

	if (!bAssemblyModeActive || !PlayerController)
	{
		return ESRAssemblySurfaceHoverUpdateResult::ClearHover;
	}

	if (PlayerController->IsPointerOverBlockingUI())
	{
		return ESRAssemblySurfaceHoverUpdateResult::ClearHoverPreview;
	}

	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* SurfaceGrid = nullptr;
	if (!FSRAssemblySurfaceCursorQuery::TryGetFocusedSurfaceGrid(PlayerController, FocusedActor, SurfaceGrid))
	{
		return ESRAssemblySurfaceHoverUpdateResult::ClearHover;
	}

	SurfaceGrid->SetHoveredInteractionGridPatchVisible(IsValid(PlayerController->GetSelectedStructureDataAsset()));

	FVector2D CurrentMousePosition = FVector2D::ZeroVector;
	const bool bHasMousePosition = PlayerController->GetMousePosition(CurrentMousePosition.X, CurrentMousePosition.Y);
	if (bHasMousePosition
		&& SurfaceState.bHasLastHoveredSampleMousePosition
		&& SurfaceState.LastHoveredSampleSurfaceGrid == SurfaceGrid
		&& HoveredSurfaceGrid == SurfaceGrid
		&& SurfaceGrid->HasHoveredCell()
		&& FVector2D::Distance(CurrentMousePosition, SurfaceState.LastHoveredSampleMousePosition) <= 0.5f)
	{
		FSRPlanetSurfaceGridCell CachedHoveredCell;
		if (SurfaceState.bHasLastPublishedHoveredCellInfo
			&& SurfaceGrid->GetHoveredCell(CachedHoveredCell)
			&& !(CachedHoveredCell.CellId == SurfaceState.LastPublishedHoveredCellId))
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[SR SurfaceHover] Result=CacheMismatch GridHovered={%s} PublishedHovered={%s} MouseDelta=%.3f"),
				*FormatSurfaceHoverCellId(CachedHoveredCell.CellId),
				*FormatSurfaceHoverCellId(SurfaceState.LastPublishedHoveredCellId),
				FVector2D::Distance(CurrentMousePosition, SurfaceState.LastHoveredSampleMousePosition));
		}
		return ESRAssemblySurfaceHoverUpdateResult::NoChange;
	}

	FSRPlanetSurfaceGridCell HoveredCell;
	FVector HoverHitLocation = FVector::ZeroVector;
	if (!FSRAssemblySurfaceCursorQuery::TryProjectCursorToSurfaceCell(PlayerController, SurfaceGrid, HoveredCell, HoverHitLocation))
	{
		return ESRAssemblySurfaceHoverUpdateResult::ClearHover;
	}

	if (HoveredSurfaceGrid && HoveredSurfaceGrid != SurfaceGrid)
	{
		HoveredSurfaceGrid->ClearHoveredCell();
	}

	HoveredSurfaceGrid = SurfaceGrid;
	FSRPlanetSurfaceGridCell PreviousHoveredCell;
	const bool bHadPreviousHoveredCell = SurfaceGrid->GetHoveredCell(PreviousHoveredCell);
	HoveredSurfaceGrid->SetHoveredCell(HoveredCell.CellId);
	if (bHadPreviousHoveredCell && PreviousHoveredCell.CellId.Face != HoveredCell.CellId.Face)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[SR SurfaceHover] Result=FaceTransition Previous={%s} Current={%s} Hit=%s"),
			*FormatSurfaceHoverCellId(PreviousHoveredCell.CellId),
			*FormatSurfaceHoverCellId(HoveredCell.CellId),
			*HoverHitLocation.ToCompactString());
	}
	if (bHasMousePosition)
	{
		SurfaceState.LastHoveredSampleSurfaceGrid = SurfaceGrid;
		SurfaceState.LastHoveredSampleMousePosition = CurrentMousePosition;
		SurfaceState.bHasLastHoveredSampleMousePosition = true;
	}

	OutUpdate.SurfaceGrid = SurfaceGrid;
	OutUpdate.HoveredCell = HoveredCell;
	return ESRAssemblySurfaceHoverUpdateResult::Updated;
}
