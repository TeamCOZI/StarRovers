#pragma once

#include "CoreMinimal.h"
#include "Pattern/SRPatternEnvironmentResolver.h"
#include "Pattern/SRPatternFacilityResolver.h"
#include "SRPatternGenerationValidator.generated.h"

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPatternGenerationSourceSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "SourceId"))
	FName SourceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "BodyId"))
	FName BodyId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "Pattern"))
	FSRPattern Pattern;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPatternGenerationBodySpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "BodyId"))
	FName BodyId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "Environment"))
	FSRPatternEnvironmentSpec Environment;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "TransformOperators"))
	TArray<FSRPatternTransformOperatorSpec> TransformOperators;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "SeparationOperators"))
	TArray<FSRPatternSeparationOperatorSpec> SeparationOperators;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "AllowSynthesis"))
	bool bAllowSynthesis = false;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPatternGenerationGoal
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "RequiredPattern"))
	FSRPattern RequiredPattern;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "RequiredMask"))
	FSRPatternMask RequiredMask;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPatternGenerationValidationRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "Sources"))
	TArray<FSRPatternGenerationSourceSpec> Sources;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "Bodies"))
	TArray<FSRPatternGenerationBodySpec> Bodies;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "Goal"))
	FSRPatternGenerationGoal Goal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "MaxOperationDepth", ClampMin = "0", ClampMax = "8"))
	int32 MaxOperationDepth = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "MaxReachableStates", ClampMin = "1", ClampMax = "8192"))
	int32 MaxReachableStates = 2048;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "RequireInterBodyTransfer"))
	bool bRequireInterBodyTransfer = false;
};

UENUM(BlueprintType)
enum class ESRPatternGenerationValidationFailure : uint8
{
	None = 0 UMETA(DisplayName = "None"),
	InvalidRequest = 1 UMETA(DisplayName = "InvalidRequest"),
	GoalUnreachable = 2 UMETA(DisplayName = "GoalUnreachable"),
	InterBodyTransferNotRequired = 3 UMETA(DisplayName = "InterBodyTransferNotRequired"),
	StateLimitExceeded = 4 UMETA(DisplayName = "StateLimitExceeded"),
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPatternGenerationValidationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "ValidationPerformed"))
	bool bValidationPerformed = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "Solvable"))
	bool bSolvable = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "GoalReachable"))
	bool bGoalReachable = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "RequiresInterBodyTransfer"))
	bool bRequiresInterBodyTransfer = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "Failure"))
	ESRPatternGenerationValidationFailure Failure = ESRPatternGenerationValidationFailure::None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "MinimumOperationDepth"))
	int32 MinimumOperationDepth = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "GoalBodyId"))
	FName GoalBodyId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "ExploredStateCount"))
	int32 ExploredStateCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "SelectedContractId"))
	FName SelectedContractId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "AttemptedContractCount"))
	int32 AttemptedContractCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|Generation", meta = (DisplayName = "CandidateContractCount"))
	int32 CandidateContractCount = 0;
};

namespace StarRovers::PatternGeneration
{
	/**
	 * Builds a reproducible permutation for this generated Run. A different Run
	 * seed changes candidate priority while the system signature keeps selection
	 * tied to the resources that were actually generated.
	 */
	STARROVERS_API void BuildSeededCandidateOrder(
		int32 CandidateCount,
		int32 RuntimeGenerationSeed,
		uint32 GeneratedSystemSignature,
		TArray<int32>& OutCandidateOrder);
}

class STARROVERS_API FSRPatternGenerationValidator final
{
public:
	static constexpr int32 MaxOperationDepth = 8;
	static constexpr int32 MaxReachableStates = 8192;
	static constexpr int32 MaxOperatorsPerBody = 16;

	static bool DoesPatternMatchGoal(
		const FSRPattern& Pattern,
		const FSRPatternGenerationGoal& Goal);

	static FSRPatternGenerationValidationResult Validate(
		const FSRPatternGenerationValidationRequest& Request);
};
