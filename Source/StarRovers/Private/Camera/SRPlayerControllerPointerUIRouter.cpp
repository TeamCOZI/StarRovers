#include "SRPlayerControllerPointerUIRouter.h"

#include "Utility/SRLog.h"
#include "UI/SRAugmentChoiceWidget.h"
#include "UI/SRCelestialBodyFocusInfoWidget.h"
#include "UI/SRCelestialBodyOverviewWidget.h"
#include "UI/SRFacilityControlWidget.h"
#include "UI/SRStructureSelectionWidget.h"
#include "UI/SRTimeControlWidget.h"

bool FSRPlayerControllerPointerUIRouter::RouteLeftClick(
	const TArray<ESRPlayerUILayer>& WidgetLayerOrder,
	float MouseX,
	float MouseY,
	bool bHasMousePosition,
	USRFacilityControlWidget* FacilityControlWidget,
	USRCelestialBodyFocusInfoWidget* FocusInfoWidget,
	USRCelestialBodyOverviewWidget* OverviewWidget,
	USRTimeControlWidget* TimeControlWidget,
	USRAugmentChoiceWidget* AugmentChoiceWidget,
	USRStructureSelectionWidget* StructureSelectionWidget)
{
	const bool bOverFacilityControl = IsValid(FacilityControlWidget) && FacilityControlWidget->IsPointerOverControlPanel();
	const bool bOverFocusInfo = IsValid(FocusInfoWidget) && FocusInfoWidget->IsPointerOverFocusInfoUI();
	const bool bOverOverview = IsValid(OverviewWidget) && OverviewWidget->IsPointerOverOverviewUI();
	const bool bOverTimeControl = IsValid(TimeControlWidget) && TimeControlWidget->IsPointerOverTimeControlPanel();
	const bool bOverAugmentChoice = IsValid(AugmentChoiceWidget) && AugmentChoiceWidget->IsVisible();
	const bool bOverStructureSelection = IsValid(StructureSelectionWidget) && StructureSelectionWidget->IsPointerOverStructureSelectionPanel();
	const bool bOverBlockingUI = bOverFacilityControl
		|| bOverFocusInfo
		|| bOverOverview
		|| bOverTimeControl
		|| bOverAugmentChoice
		|| bOverStructureSelection;
	ESRPlayerUILayer TopBlockingUILayer = ESRPlayerUILayer::FocusInfo;
	const TCHAR* TopBlockingUIName = TEXT("None");
	int32 TopBlockingUIZOrder = MIN_int32;

	const auto ConsiderBlockingUILayer =
		[&WidgetLayerOrder, &TopBlockingUILayer, &TopBlockingUIName, &TopBlockingUIZOrder](bool bIsPointerOverLayer, ESRPlayerUILayer Layer, const TCHAR* LayerName)
		{
			if (!bIsPointerOverLayer)
			{
				return;
			}

			const int32 LayerZOrder = StarRovers::PlayerControllerUI::ResolveWidgetLayerZOrder(WidgetLayerOrder, Layer);
			if (LayerZOrder >= TopBlockingUIZOrder)
			{
				TopBlockingUILayer = Layer;
				TopBlockingUIName = LayerName;
				TopBlockingUIZOrder = LayerZOrder;
			}
		};

	ConsiderBlockingUILayer(bOverFacilityControl, ESRPlayerUILayer::FacilityControl, TEXT("FacilityControl"));
	ConsiderBlockingUILayer(bOverFocusInfo, ESRPlayerUILayer::FocusInfo, TEXT("FocusInfo"));
	ConsiderBlockingUILayer(bOverOverview, ESRPlayerUILayer::Overview, TEXT("Overview"));
	ConsiderBlockingUILayer(bOverTimeControl, ESRPlayerUILayer::TimeControl, TEXT("TimeControl"));
	ConsiderBlockingUILayer(bOverAugmentChoice, ESRPlayerUILayer::AugmentChoice, TEXT("AugmentChoice"));
	ConsiderBlockingUILayer(bOverStructureSelection, ESRPlayerUILayer::StructureSelection, TEXT("StructureSelection"));

	SR_LOG(Camera, LogTemp, Log, TEXT("SR UI Click Trace: PlayerController LeftClick Mouse=(%.1f, %.1f) HasMouse=%s FacilityControl=%s FocusInfo=%s Overview=%s TimeControl=%s AugmentChoice=%s StructureSelection=%s TopBlockingUI=%s TopZOrder=%d"),
		MouseX,
		MouseY,
		bHasMousePosition ? TEXT("true") : TEXT("false"),
		bOverFacilityControl ? TEXT("true") : TEXT("false"),
		bOverFocusInfo ? TEXT("true") : TEXT("false"),
		bOverOverview ? TEXT("true") : TEXT("false"),
		bOverTimeControl ? TEXT("true") : TEXT("false"),
		bOverAugmentChoice ? TEXT("true") : TEXT("false"),
		bOverStructureSelection ? TEXT("true") : TEXT("false"),
		TopBlockingUIName,
		TopBlockingUIZOrder);

	if (!bOverBlockingUI)
	{
		return false;
	}

	if (TopBlockingUILayer == ESRPlayerUILayer::FacilityControl
		&& FacilityControlWidget
		&& FacilityControlWidget->TryHandleFacilityControlPointerClick())
	{
		SR_LOG(Camera, LogTemp, Log, TEXT("SR UI Click Trace: PlayerController LeftClick handled by top FacilityControl UI."));
		return true;
	}

	if (TopBlockingUILayer == ESRPlayerUILayer::StructureSelection
		&& StructureSelectionWidget
		&& StructureSelectionWidget->TryHandleStructureSelectionPointerClick())
	{
		SR_LOG(Camera, LogTemp, Log, TEXT("SR UI Click Trace: PlayerController LeftClick handled by top StructureSelection UI."));
		return true;
	}

	SR_LOG(Camera, LogTemp, Log, TEXT("SR UI Click Trace: PlayerController LeftClick blocked by top UI hit test: %s."), TopBlockingUIName);
	return true;
}
