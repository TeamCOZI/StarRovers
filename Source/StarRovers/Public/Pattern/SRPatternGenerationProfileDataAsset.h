#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Pattern/SRPatternGenerationValidator.h"
#include "Pattern/SRStellarPatternContract.h"
#include "SRPatternGenerationProfileDataAsset.generated.h"

class USRFacilityDataAsset;

UCLASS(BlueprintType)
class STARROVERS_API USRPatternGenerationProfileDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	USRPatternGenerationProfileDataAsset();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "CandidateStellarContracts", ToolTip = "Contracts are shuffled deterministically by the Run seed and generated-system signature. The first solvable candidate becomes the active stellar contract."))
	TArray<FSRStellarPatternContract> CandidateStellarContracts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "AvailableFacilityDataAssets"))
	TArray<TObjectPtr<USRFacilityDataAsset>> AvailableFacilityDataAssets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "MaxOperationDepth", ClampMin = "0", ClampMax = "8"))
	int32 MaxOperationDepth = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "MaxReachableStates", ClampMin = "1", ClampMax = "8192"))
	int32 MaxReachableStates = 2048;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "MaxValidationSourcesPerResourcePerBody", ClampMin = "1", ClampMax = "8", ToolTip = "Maximum generated deposits sampled for each resource family on each body by the existential reachability validator. Runtime deposits are not removed or changed."))
	int32 MaxValidationSourcesPerResourcePerBody = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "RequireInterBodyTransfer"))
	bool bRequireInterBodyTransfer = false;
};
