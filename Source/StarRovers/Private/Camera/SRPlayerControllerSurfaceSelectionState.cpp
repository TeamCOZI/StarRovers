#include "SRPlayerControllerSurfaceSelectionState.h"

#include "Assembly/SRAssemblyComponent.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Surface/SRPlanetSurfaceGrid.h"

void FSRPlayerControllerSurfaceSelectionState::ClearSelectedActorSurfacePreview(AActor* SelectedActor)
{
	if (USRPlanetSurfaceGrid* PreviousSurfaceGrid = USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(SelectedActor))
	{
		PreviousSurfaceGrid->ClearSelectedCell();
		PreviousSurfaceGrid->ClearSelectedFootprintCells();
		PreviousSurfaceGrid->ClearOccupiedPreviewCells();
		PreviousSurfaceGrid->ClearFacilityPortPreviewCells();
	}
}

void FSRPlayerControllerSurfaceSelectionState::ClearFocusedActorHover(
	AActor* FocusedActor,
	USRAssemblyComponent* AssemblyComponent)
{
	if (!IsValid(FocusedActor))
	{
		if (AssemblyComponent)
		{
			AssemblyComponent->ClearSurfaceHover();
		}
		return;
	}

	if (USRPlanetSurfaceGrid* FocusedSurfaceGrid = USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(FocusedActor))
	{
		FocusedSurfaceGrid->ClearHoveredCell();
		FocusedSurfaceGrid->ClearHoverGridHighlightCells();
	}
}
