#include "SRPlayerControllerSurfaceSelectionState.h"

#include "Assembly/SRAssemblyComponent.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "GameFramework/Actor.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"

void FSRPlayerControllerSurfaceSelectionState::ClearSelectedActorSurfacePreview(AActor* SelectedActor)
{
	USRPlanetSurfaceGrid* PreviousSurfaceGrid = USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(SelectedActor);
	if (PreviousSurfaceGrid)
	{
		PreviousSurfaceGrid->ClearSelectedCell();
		PreviousSurfaceGrid->ClearSelectedFootprintCells();
		PreviousSurfaceGrid->ClearOccupiedPreviewCells();
		PreviousSurfaceGrid->ClearFacilityPortPreviewCells();
	}

	if (IsValid(SelectedActor))
	{
		if (USRStructureInstanceManagerComponent* StructureManager =
			SelectedActor->FindComponentByClass<USRStructureInstanceManagerComponent>())
		{
			StructureManager->ClearMiningResourceDepositHighlights(PreviousSurfaceGrid);
		}
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
