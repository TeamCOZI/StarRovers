#include "Camera/SRPlayerControllerUiTypes.h"

namespace
{
    constexpr ESRPlayerUiLayer DefaultWidgetLayerOrder[] =
    {
        ESRPlayerUiLayer::FocusInfo,
        ESRPlayerUiLayer::Overview,
        ESRPlayerUiLayer::TimeControl,
        ESRPlayerUiLayer::StructureSelection,
        ESRPlayerUiLayer::FacilityControl
    };

    void AddUniqueWidgetLayer(TArray<ESRPlayerUiLayer>& OutLayers, ESRPlayerUiLayer WidgetLayer)
    {
        if (!OutLayers.Contains(WidgetLayer))
        {
            OutLayers.Add(WidgetLayer);
        }
    }
}

TArray<ESRPlayerUiLayer> StarRovers::PlayerControllerUI::MakeDefaultWidgetLayerOrder()
{
    TArray<ESRPlayerUiLayer> ResolvedLayerOrder;
    ResolvedLayerOrder.Reserve(UE_ARRAY_COUNT(DefaultWidgetLayerOrder));

    for (const ESRPlayerUiLayer DefaultLayer : DefaultWidgetLayerOrder)
    {
        ResolvedLayerOrder.Add(DefaultLayer);
    }

    return ResolvedLayerOrder;
}

int32 StarRovers::PlayerControllerUI::ResolveWidgetLayerZOrder(const TArray<ESRPlayerUiLayer>& ConfiguredLayerOrder, ESRPlayerUiLayer WidgetLayer)
{
    TArray<ESRPlayerUiLayer> ResolvedLayerOrder;
    ResolvedLayerOrder.Reserve(UE_ARRAY_COUNT(DefaultWidgetLayerOrder));

    for (const ESRPlayerUiLayer ConfiguredLayer : ConfiguredLayerOrder)
    {
        AddUniqueWidgetLayer(ResolvedLayerOrder, ConfiguredLayer);
    }

    for (const ESRPlayerUiLayer DefaultLayer : DefaultWidgetLayerOrder)
    {
        AddUniqueWidgetLayer(ResolvedLayerOrder, DefaultLayer);
    }

    const int32 LayerIndex = ResolvedLayerOrder.IndexOfByKey(WidgetLayer);
    return LayerIndex != INDEX_NONE ? LayerIndex : ResolvedLayerOrder.Num();
}
