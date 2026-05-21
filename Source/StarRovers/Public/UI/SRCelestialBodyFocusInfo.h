#pragma once

#include "CoreMinimal.h"
#include "SRCelestialBodyFocusInfo.generated.h"

class AActor;

USTRUCT(BlueprintType)
struct STARROVERS_API FSRCelestialBodyFocusInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "Actor"))
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "DisplayName"))
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "bCanConstruct"))
	bool bCanConstruct = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "bHasSurfaceGrid"))
	bool bHasSurfaceGrid = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Focus", meta = (DisplayName = "bIsValid"))
	bool bIsValid = false;
};
