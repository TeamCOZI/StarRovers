#pragma once

#include "CoreMinimal.h"
#include "SRStellarEvolutionTypes.generated.h"

UENUM(BlueprintType)
enum class ESRStellarEvolutionStage : uint8
{
	MainSequence UMETA(DisplayName = "Main Sequence"),
	RedGiant UMETA(DisplayName = "Red Giant"),
	Supernova UMETA(DisplayName = "Supernova")
};
