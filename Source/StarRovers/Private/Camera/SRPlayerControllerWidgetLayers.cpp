#include "Camera/SRPlayerControllerWidgetLayers.h"

namespace
{
    constexpr ESRPlayerUILayer DefaultWidgetLayerOrder[] =
    {
        ESRPlayerUILayer::FocusInfo,
        ESRPlayerUILayer::Overview,
        ESRPlayerUILayer::TimeControl,
        ESRPlayerUILayer::StructureSelection,
        ESRPlayerUILayer::HubShortcut,
        ESRPlayerUILayer::FacilityControl,
        ESRPlayerUILayer::AugmentChoice,
        ESRPlayerUILayer::GameOver
    };

	bool IsFixedModalLayer(ESRPlayerUILayer WidgetLayer)
	{
		return WidgetLayer == ESRPlayerUILayer::AugmentChoice
			|| WidgetLayer == ESRPlayerUILayer::GameOver;
	}

    void AddUniqueWidgetLayer(TArray<ESRPlayerUILayer>& OutLayers, ESRPlayerUILayer WidgetLayer)
    {
        if (!OutLayers.Contains(WidgetLayer))
        {
            OutLayers.Add(WidgetLayer);
        }
    }
}

TArray<ESRPlayerUILayer> StarRovers::PlayerControllerUI::MakeDefaultWidgetLayerOrder()
{
    TArray<ESRPlayerUILayer> ResolvedLayerOrder;
    ResolvedLayerOrder.Reserve(UE_ARRAY_COUNT(DefaultWidgetLayerOrder));

    for (const ESRPlayerUILayer DefaultLayer : DefaultWidgetLayerOrder)
    {
        ResolvedLayerOrder.Add(DefaultLayer);
    }

    return ResolvedLayerOrder;
}

int32 StarRovers::PlayerControllerUI::ResolveWidgetLayerZOrder(const TArray<ESRPlayerUILayer>& ConfiguredLayerOrder, ESRPlayerUILayer WidgetLayer)
{
    TArray<ESRPlayerUILayer> ResolvedLayerOrder;
    ResolvedLayerOrder.Reserve(UE_ARRAY_COUNT(DefaultWidgetLayerOrder));

    for (const ESRPlayerUILayer ConfiguredLayer : ConfiguredLayerOrder)
    {
		if (!IsFixedModalLayer(ConfiguredLayer))
		{
			AddUniqueWidgetLayer(ResolvedLayerOrder, ConfiguredLayer);
		}
    }

    for (const ESRPlayerUILayer DefaultLayer : DefaultWidgetLayerOrder)
    {
		if (!IsFixedModalLayer(DefaultLayer))
		{
			AddUniqueWidgetLayer(ResolvedLayerOrder, DefaultLayer);
		}
    }

	// These are behavioral modals, not merely visual customization layers.
	// Keep choices above every gameplay panel and defeat above everything,
	// including Blueprint defaults saved with an older layer order.
	AddUniqueWidgetLayer(ResolvedLayerOrder, ESRPlayerUILayer::AugmentChoice);
	AddUniqueWidgetLayer(ResolvedLayerOrder, ESRPlayerUILayer::GameOver);

    const int32 LayerIndex = ResolvedLayerOrder.IndexOfByKey(WidgetLayer);
    return LayerIndex != INDEX_NONE ? LayerIndex : ResolvedLayerOrder.Num();
}
