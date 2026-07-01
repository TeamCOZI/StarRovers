#pragma once

#include "CoreMinimal.h"
#include "SRPlayerControllerUiTypes.generated.h"

UENUM(BlueprintType)
enum class ESRPlayerUiLayer : uint8
{
    FocusInfo UMETA(DisplayName = "Focus Info"),
    Overview UMETA(DisplayName = "Overview"),
    TimeControl UMETA(DisplayName = "Time Control"),
    StructureSelection UMETA(DisplayName = "Structure Selection"),
    FacilityControl UMETA(DisplayName = "Facility Control")
};

namespace StarRovers::PlayerControllerUI
{
    STARROVERS_API TArray<ESRPlayerUiLayer> MakeDefaultWidgetLayerOrder();
    STARROVERS_API int32 ResolveWidgetLayerZOrder(const TArray<ESRPlayerUiLayer>& ConfiguredLayerOrder, ESRPlayerUiLayer WidgetLayer);
}
