#pragma once

#include "CoreMinimal.h"
#include "SRPlayerControllerWidgetLayers.generated.h"

UENUM(BlueprintType)
enum class ESRPlayerUILayer : uint8
{
    FocusInfo UMETA(DisplayName = "Focus Info"),
    Overview UMETA(DisplayName = "Overview"),
    TimeControl UMETA(DisplayName = "Time Control"),
    AugmentChoice UMETA(DisplayName = "Augment Choice"),
    StructureSelection UMETA(DisplayName = "Structure Selection"),
    HubShortcut UMETA(DisplayName = "Hub Shortcut"),
    FacilityControl UMETA(DisplayName = "Facility Control"),
    GameOver UMETA(DisplayName = "Game Over")
};

namespace StarRovers::PlayerControllerUI
{
    STARROVERS_API TArray<ESRPlayerUILayer> MakeDefaultWidgetLayerOrder();
    STARROVERS_API int32 ResolveWidgetLayerZOrder(const TArray<ESRPlayerUILayer>& ConfiguredLayerOrder, ESRPlayerUILayer WidgetLayer);
}
