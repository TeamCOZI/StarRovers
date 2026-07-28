#pragma once

#include "CoreMinimal.h"
#include "Camera/SRPlayerControllerWidgetLayers.h"

class USRAugmentChoiceWidget;
class USRCelestialBodyFocusInfoWidget;
class USRCelestialBodyOverviewWidget;
class USRFacilityControlWidget;
class USRFocusedHubShortcutWidget;
class USRGameOverWidget;
class USRStructureSelectionWidget;
class USRTimeControlWidget;

class FSRPlayerControllerPointerUIRouter
{
public:
	static bool RouteLeftClick(
		const TArray<ESRPlayerUILayer>& WidgetLayerOrder,
		float MouseX,
		float MouseY,
		bool bHasMousePosition,
		USRFacilityControlWidget* FacilityControlWidget,
		USRFocusedHubShortcutWidget* FocusedHubShortcutWidget,
		USRCelestialBodyFocusInfoWidget* FocusInfoWidget,
		USRCelestialBodyOverviewWidget* OverviewWidget,
		USRTimeControlWidget* TimeControlWidget,
		USRAugmentChoiceWidget* AugmentChoiceWidget,
		USRStructureSelectionWidget* StructureSelectionWidget,
		USRGameOverWidget* GameOverWidget);
};
