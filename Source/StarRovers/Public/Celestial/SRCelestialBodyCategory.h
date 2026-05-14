#pragma once

#include "CoreMinimal.h"
#include "SRCelestialBodyCategory.generated.h"

UENUM(BlueprintType)
enum class ESRCelestialBodyCategory : uint8
{
	Unknown UMETA(DisplayName = "Unknown"),
	Star UMETA(DisplayName = "Star"),
	Planet UMETA(DisplayName = "Planet"),
	Moon UMETA(DisplayName = "Moon")
};
