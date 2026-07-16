#include "Assembly/SRAssemblySurfaceHoverUpdater.h"

#include "Utility/SRLog.h"
#include "Assembly/SRAssemblySurfaceFocusInfoBuilder.h"
#include "Assembly/SRAssemblySurfaceCursorQuery.h"
#include "Assembly/SRAssemblySurfaceState.h"
#include "Camera/SRPlayerController.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
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

	void AppendUniqueCellId(
		const FSRPlanetSurfaceGridCellId& CellId,
		TArray<FSRPlanetSurfaceGridCellId>& OutCellIds,
		TSet<FSRPlanetSurfaceGridCellId>& CellIdSet)
	{
		const int32 PreviousCount = CellIdSet.Num();
		CellIdSet.Add(CellId);
		if (CellIdSet.Num() != PreviousCount)
		{
			OutCellIds.Add(CellId);
		}
	}

	void AppendOccupantIfCellOccupied(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& CellId,
		TSet<FName>& OutOccupantIds)
	{
		FSRPlanetSurfaceGridCell Cell;
		if (IsValid(SurfaceGrid)
			&& SurfaceGrid->GetCellById(CellId, Cell)
			&& Cell.bOccupied
			&& !Cell.OccupantId.IsNone())
		{
			OutOccupantIds.Add(Cell.OccupantId);
		}
	}

	void AppendAdjacentOccupantsForCell(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& CellId,
		TSet<FName>& OutOccupantIds)
	{
		if (!IsValid(SurfaceGrid))
		{
			return;
		}

		FSRPlanetSurfaceGridCellNeighbors Neighbors;
		if (!SurfaceGrid->GetCellNeighbors(CellId, Neighbors))
		{
			return;
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
			AppendOccupantIfCellOccupied(SurfaceGrid, NeighborCellId, OutOccupantIds);
		}
	}

	void AppendPlacedStructurePortConnectionCellsInPatch(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlacedStructureInstance& PlacedStructure,
		const TSet<FSRPlanetSurfaceGridCellId>& PatchCellIdSet,
		TArray<FSRPlanetSurfaceGridCellId>& OutInputPortCellIds,
		TArray<FSRPlanetSurfaceGridCellId>& OutOutputPortCellIds,
		TSet<FSRPlanetSurfaceGridCellId>& InputPortCellIdSet,
		TSet<FSRPlanetSurfaceGridCellId>& OutputPortCellIdSet)
	{
		USRStructureDataAsset* StructureDataAsset = PlacedStructure.StructureDataAsset.Get();
		if (!IsValid(SurfaceGrid)
			|| PlacedStructure.bNaturalStructure
			|| !IsValid(StructureDataAsset)
			|| PlacedStructure.FootprintCellIds.IsEmpty())
		{
			return;
		}

		const FSRStructureData StructureData = StructureDataAsset->BuildData();
		if (StructureData.BuildKind != ESRStructureBuildKind::Structure
			|| (StructureData.InputPorts.IsEmpty() && StructureData.OutputPorts.IsEmpty()))
		{
			return;
		}

		FSRFocusedSurfaceStructureInfo StructureInfo;
		StructureInfo.bIsValid = true;
		StructureInfo.OccupantId = PlacedStructure.OccupantId;
		StructureInfo.StructureId = PlacedStructure.StructureId;
		StructureInfo.OriginCellId = PlacedStructure.OriginCellId;
		StructureInfo.FootprintCellIds = PlacedStructure.FootprintCellIds;
		StructureInfo.StructureDataAsset = StructureDataAsset;
		StructureInfo.BuildKind = StructureData.BuildKind;
		StructureInfo.bNaturalStructure = PlacedStructure.bNaturalStructure;

		TArray<FSRFocusedFacilityPortInfo> FacilityPorts;
		StarRovers::Assembly::FSRAssemblySurfaceFocusInfoBuilder::BuildFocusedFacilityPortInfo(
			SurfaceGrid,
			StructureInfo,
			StructureData,
			StarRovers::Structure::GetRotatedFootprintCellsX(StructureData, PlacedStructure.PlacementRotationSteps),
			PlacedStructure.PlacementRotationSteps,
			FacilityPorts);

		TArray<FSRPlanetSurfaceGridCellId> StructureInputPortCellIds;
		TArray<FSRPlanetSurfaceGridCellId> StructureOutputPortCellIds;
		StarRovers::Assembly::FSRAssemblySurfaceFocusInfoBuilder::GatherFacilityPortPreviewCells(
			FacilityPorts,
			StructureInputPortCellIds,
			StructureOutputPortCellIds);

		for (const FSRPlanetSurfaceGridCellId& InputPortCellId : StructureInputPortCellIds)
		{
			if (PatchCellIdSet.Contains(InputPortCellId))
			{
				AppendUniqueCellId(InputPortCellId, OutInputPortCellIds, InputPortCellIdSet);
			}
		}

		for (const FSRPlanetSurfaceGridCellId& OutputPortCellId : StructureOutputPortCellIds)
		{
			if (PatchCellIdSet.Contains(OutputPortCellId))
			{
				AppendUniqueCellId(OutputPortCellId, OutOutputPortCellIds, OutputPortCellIdSet);
			}
		}
	}

	void RefreshHoverGridHighlights(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& HoveredCellId,
		bool bShowHoverGridHighlights)
	{
		if (!IsValid(SurfaceGrid) || !bShowHoverGridHighlights)
		{
			if (IsValid(SurfaceGrid))
			{
				SurfaceGrid->ClearHoverGridHighlightCells();
			}
			return;
		}

		TArray<FSRPlanetSurfaceGridCellId> PatchCellIds;
		if (!SurfaceGrid->GetInteractionGridPatchCellIds(HoveredCellId, PatchCellIds))
		{
			SurfaceGrid->ClearHoverGridHighlightCells();
			return;
		}

		TArray<FSRPlanetSurfaceGridCellId> OccupiedCellIds;
		TArray<FSRPlanetSurfaceGridCellId> InputPortCellIds;
		TArray<FSRPlanetSurfaceGridCellId> OutputPortCellIds;
		TSet<FSRPlanetSurfaceGridCellId> PatchCellIdSet;
		TSet<FSRPlanetSurfaceGridCellId> OccupiedCellIdSet;
		TSet<FSRPlanetSurfaceGridCellId> InputPortCellIdSet;
		TSet<FSRPlanetSurfaceGridCellId> OutputPortCellIdSet;
		TSet<FName> CandidatePortOccupantIds;

		PatchCellIdSet.Reserve(PatchCellIds.Num());
		OccupiedCellIdSet.Reserve(PatchCellIds.Num());
		InputPortCellIdSet.Reserve(PatchCellIds.Num());
		OutputPortCellIdSet.Reserve(PatchCellIds.Num());
		CandidatePortOccupantIds.Reserve(PatchCellIds.Num());
		for (const FSRPlanetSurfaceGridCellId& PatchCellId : PatchCellIds)
		{
			PatchCellIdSet.Add(PatchCellId);

			FSRPlanetSurfaceGridCell PatchCell;
			if (SurfaceGrid->GetCellById(PatchCellId, PatchCell)
				&& PatchCell.bOccupied
				&& !PatchCell.OccupantId.IsNone())
			{
				AppendUniqueCellId(PatchCellId, OccupiedCellIds, OccupiedCellIdSet);
				CandidatePortOccupantIds.Add(PatchCell.OccupantId);
			}

			AppendAdjacentOccupantsForCell(SurfaceGrid, PatchCellId, CandidatePortOccupantIds);
		}

		AActor* SurfaceOwner = SurfaceGrid->GetOwner();
		USRStructureInstanceManagerComponent* StructureInstanceManager = IsValid(SurfaceOwner)
			? SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>()
			: nullptr;
		if (IsValid(StructureInstanceManager))
		{
			for (const FName CandidatePortOccupantId : CandidatePortOccupantIds)
			{
				FSRPlacedStructureInstance PlacedStructure;
				if (StructureInstanceManager->GetPlacedStructure(CandidatePortOccupantId, PlacedStructure))
				{
					AppendPlacedStructurePortConnectionCellsInPatch(
						SurfaceGrid,
						PlacedStructure,
						PatchCellIdSet,
						InputPortCellIds,
						OutputPortCellIds,
						InputPortCellIdSet,
						OutputPortCellIdSet);
				}
			}
		}

		SurfaceGrid->SetHoverGridHighlightCells(OccupiedCellIds, InputPortCellIds, OutputPortCellIds);
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

	const bool bShowHoveredInteractionGridPatch = IsValid(PlayerController->GetSelectedStructureDataAsset());
	SurfaceGrid->SetHoveredInteractionGridPatchVisible(bShowHoveredInteractionGridPatch);

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
		if (!SurfaceGrid->GetHoveredCell(CachedHoveredCell))
		{
			SurfaceGrid->ClearHoverGridHighlightCells();
			return ESRAssemblySurfaceHoverUpdateResult::NoChange;
		}

		if (SurfaceState.bHasLastPublishedHoveredCellInfo
			&& !(CachedHoveredCell.CellId == SurfaceState.LastPublishedHoveredCellId))
		{
			SR_LOG(Assembly, LogTemp,
				Warning,
				TEXT("[SR SurfaceHover] Result=CacheMismatch GridHovered={%s} PublishedHovered={%s} MouseDelta=%.3f"),
				*FormatSurfaceHoverCellId(CachedHoveredCell.CellId),
				*FormatSurfaceHoverCellId(SurfaceState.LastPublishedHoveredCellId),
				FVector2D::Distance(CurrentMousePosition, SurfaceState.LastHoveredSampleMousePosition));
		}
		RefreshHoverGridHighlights(SurfaceGrid, CachedHoveredCell.CellId, bShowHoveredInteractionGridPatch);
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
		HoveredSurfaceGrid->ClearHoverGridHighlightCells();
	}

	HoveredSurfaceGrid = SurfaceGrid;
	FSRPlanetSurfaceGridCell PreviousHoveredCell;
	const bool bHadPreviousHoveredCell = SurfaceGrid->GetHoveredCell(PreviousHoveredCell);
	HoveredSurfaceGrid->SetHoveredCell(HoveredCell.CellId);
	if (bHadPreviousHoveredCell && PreviousHoveredCell.CellId.Face != HoveredCell.CellId.Face)
	{
		SR_LOG(Assembly, LogTemp,
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
	RefreshHoverGridHighlights(SurfaceGrid, HoveredCell.CellId, bShowHoveredInteractionGridPatch);
	return ESRAssemblySurfaceHoverUpdateResult::Updated;
}
