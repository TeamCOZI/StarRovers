#pragma once

#include "CoreMinimal.h"
#include "UI/SRCelestialBodyFocusInfo.h"

class AActor;
class USRCelestialBodyFocusInfoWidget;

class FSRPlayerControllerFocusInfoState
{
public:
	static bool SetHoveredSurfaceCellInfo(
		AActor* SelectedActor,
		FSRCelestialBodyFocusInfo& SelectedActorFocusInfo,
		bool bHasHoveredSurfaceCell,
		const FSRPlanetSurfaceGridCellInfo& HoveredSurfaceCellInfo);

	static bool SetSelectedSurfaceStructureInfo(
		AActor* SelectedActor,
		FSRCelestialBodyFocusInfo& SelectedActorFocusInfo,
		bool bHasSelectedSurfaceStructure,
		const FSRFocusedSurfaceStructureInfo& SelectedSurfaceStructureInfo);

	static void RebuildSelectedActorSurfaceStructureInfo(
		AActor* SelectedActor,
		FSRCelestialBodyFocusInfo& SelectedActorFocusInfo,
		const FSRFocusedSurfaceStructureInfo& SelectedSurfaceStructureInfo);

	static void ApplyToFocusInfoWidget(
		const FSRCelestialBodyFocusInfo& SelectedActorFocusInfo,
		bool bAssemblyModeActive,
		USRCelestialBodyFocusInfoWidget* FocusInfoWidget,
		bool bShowWidget);

private:
	static bool EnsureFocusInfo(AActor* SelectedActor, FSRCelestialBodyFocusInfo& SelectedActorFocusInfo);
};
