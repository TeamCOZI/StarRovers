#include "Camera/SRPlayerControllerWidgetLayers.h"

namespace
{
    constexpr ESRPlayerUILayer DefaultWidgetLayerOrder[] =
    {
        ESRPlayerUILayer::FocusInfo,
        ESRPlayerUILayer::Overview,
        ESRPlayerUILayer::TimeControl,
        ESRPlayerUILayer::AugmentChoice,
        ESRPlayerUILayer::StructureSelection,
        ESRPlayerUILayer::HubShortcut,
        ESRPlayerUILayer::FacilityControl,
        ESRPlayerUILayer::GameOver
    };

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
        AddUniqueWidgetLayer(ResolvedLayerOrder, ConfiguredLayer);
    }

    for (const ESRPlayerUILayer DefaultLayer : DefaultWidgetLayerOrder)
    {
        AddUniqueWidgetLayer(ResolvedLayerOrder, DefaultLayer);
    }

    const int32 LayerIndex = ResolvedLayerOrder.IndexOfByKey(WidgetLayer);
    return LayerIndex != INDEX_NONE ? LayerIndex : ResolvedLayerOrder.Num();
}
