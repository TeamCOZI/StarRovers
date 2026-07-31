#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Pattern/SRPatternEnvironmentResolver.h"
#include "SRPatternEnvironmentDataAsset.generated.h"

UCLASS(BlueprintType)
class STARROVERS_API USRPatternEnvironmentDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	USRPatternEnvironmentDataAsset();

	UFUNCTION(BlueprintPure, Category = "StarRovers|Pattern Environment")
	FSRPatternEnvironmentSpec BuildEnvironmentSpec() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Pattern Environment")
	bool IsEnvironmentValid() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity", meta = (DisplayName = "EnvironmentId"))
	FName EnvironmentId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity", meta = (DisplayName = "DisplayName"))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity", meta = (DisplayName = "Description"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Pattern Environment", meta = (DisplayName = "Effects"))
	TArray<FSRPatternEnvironmentEffectSpec> Effects;
};
