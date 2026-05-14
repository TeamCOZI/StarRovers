#pragma once

#include "CoreMinimal.h"
#include "SRCelestialBodyFocusInfo.generated.h"

class AActor;

USTRUCT(BlueprintType)
struct STARROVERS_API FSRCelestialBodyFocusInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus")
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus")
	bool bCanConstruct = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus")
	bool bHasSurfaceGrid = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus")
	bool bIsValid = false;
};
