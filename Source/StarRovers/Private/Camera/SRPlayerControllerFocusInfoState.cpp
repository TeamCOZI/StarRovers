#include "SRPlayerControllerFocusInfoState.h"

#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Surface/SRPlanetSurfaceGrid.h"
#include "UI/SRCelestialBodyFocusInfoWidget.h"

bool FSRPlayerControllerFocusInfoState::SetHoveredSurfaceCellInfo(
	AActor* SelectedActor,
	FSRCelestialBodyFocusInfo& SelectedActorFocusInfo,
	bool bHasHoveredSurfaceCell,
	const FSRPlanetSurfaceGridCellInfo& HoveredSurfaceCellInfo)
{
	if (!EnsureFocusInfo(SelectedActor, SelectedActorFocusInfo))
	{
		return false;
	}

	SelectedActorFocusInfo.bHasHoveredSurfaceCell = bHasHoveredSurfaceCell;
	SelectedActorFocusInfo.HoveredSurfaceCellInfo = bHasHoveredSurfaceCell
		? HoveredSurfaceCellInfo
		: FSRPlanetSurfaceGridCellInfo();
	SelectedActorFocusInfo.HoveredSurfaceGridPatchCellIds.Reset();
	if (bHasHoveredSurfaceCell)
	{
		if (USRPlanetSurfaceGrid* SurfaceGrid = USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(SelectedActor))
		{
			SurfaceGrid->GetInteractionGridPatchCellIds(
				HoveredSurfaceCellInfo.CellId,
				SelectedActorFocusInfo.HoveredSurfaceGridPatchCellIds);
		}
	}

	return true;
}

bool FSRPlayerControllerFocusInfoState::SetSelectedSurfaceStructureInfo(
	AActor* SelectedActor,
	FSRCelestialBodyFocusInfo& SelectedActorFocusInfo,
	bool bHasSelectedSurfaceStructure,
	const FSRFocusedSurfaceStructureInfo& SelectedSurfaceStructureInfo)
{
	if (!EnsureFocusInfo(SelectedActor, SelectedActorFocusInfo))
	{
		return false;
	}

	SelectedActorFocusInfo.bHasSelectedSurfaceStructure = bHasSelectedSurfaceStructure;
	SelectedActorFocusInfo.SelectedSurfaceStructureInfo = bHasSelectedSurfaceStructure
		? SelectedSurfaceStructureInfo
		: FSRFocusedSurfaceStructureInfo();
	return true;
}

void FSRPlayerControllerFocusInfoState::RebuildSelectedActorSurfaceStructureInfo(
	AActor* SelectedActor,
	FSRCelestialBodyFocusInfo& SelectedActorFocusInfo,
	const FSRFocusedSurfaceStructureInfo& SelectedSurfaceStructureInfo)
{
	SelectedActorFocusInfo = USRCelestialBodyRuntimeLibrary::BuildCelestialBodyFocusInfo(SelectedActor);
	SelectedActorFocusInfo.bHasSelectedSurfaceStructure = SelectedSurfaceStructureInfo.bIsValid;
	SelectedActorFocusInfo.SelectedSurfaceStructureInfo = SelectedSurfaceStructureInfo.bIsValid
		? SelectedSurfaceStructureInfo
		: FSRFocusedSurfaceStructureInfo();
}

void FSRPlayerControllerFocusInfoState::ApplyToFocusInfoWidget(
	const FSRCelestialBodyFocusInfo& SelectedActorFocusInfo,
	bool bAssemblyModeActive,
	USRCelestialBodyFocusInfoWidget* FocusInfoWidget,
	bool bShowWidget)
{
	if (!FocusInfoWidget)
	{
		return;
	}

	FocusInfoWidget->SetFocusInfo(SelectedActorFocusInfo);
	FocusInfoWidget->SetAssemblyModeActive(bAssemblyModeActive);
	if (bShowWidget)
	{
		FocusInfoWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

bool FSRPlayerControllerFocusInfoState::EnsureFocusInfo(
	AActor* SelectedActor,
	FSRCelestialBodyFocusInfo& SelectedActorFocusInfo)
{
	if (!SelectedActorFocusInfo.bIsValid)
	{
		SelectedActorFocusInfo = USRCelestialBodyRuntimeLibrary::BuildCelestialBodyFocusInfo(SelectedActor);
	}

	return SelectedActorFocusInfo.bIsValid;
}
