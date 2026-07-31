#pragma once

#include "CoreMinimal.h"
#include "Pattern/SRPatternResolver.h"
#include "SRPatternFacilityResolver.generated.h"

UENUM(BlueprintType)
enum class ESRPatternFacilityResolveFailure : uint8
{
	None = 0 UMETA(DisplayName = "None"),
	InvalidInputPattern = 1 UMETA(DisplayName = "InvalidInputPattern"),
	InvalidTransformSpec = 2 UMETA(DisplayName = "InvalidTransformSpec"),
	InvalidSeparationMask = 3 UMETA(DisplayName = "InvalidSeparationMask"),
	EmptySeparationOutput = 4 UMETA(DisplayName = "EmptySeparationOutput"),
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPatternTransformOperatorSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Facility", meta = (DisplayName = "SelectionMask"))
	FSRPatternMask SelectionMask;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Facility", meta = (DisplayName = "Direction"))
	ESRPatternDirection Direction = ESRPatternDirection::Right;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Facility", meta = (DisplayName = "FluidSidePreference"))
	ESRPatternFluidSidePreference FluidSidePreference = ESRPatternFluidSidePreference::ClockwiseFirst;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Facility", meta = (DisplayName = "OrganicGrowthsPerComponent", ClampMin = "0", ClampMax = "4"))
	int32 OrganicGrowthsPerComponent = 1;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPatternSeparationOperatorSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Facility", meta = (DisplayName = "PrimaryOutputMask"))
	FSRPatternMask PrimaryOutputMask;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPatternFacilityResolveResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|Facility", meta = (DisplayName = "Succeeded"))
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|Facility", meta = (DisplayName = "Failure"))
	ESRPatternFacilityResolveFailure Failure = ESRPatternFacilityResolveFailure::None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|Facility", meta = (DisplayName = "OutputPatterns"))
	TArray<FSRPattern> OutputPatterns;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|Facility", meta = (DisplayName = "TraceEvents"))
	TArray<FSRPatternTraceEvent> TraceEvents;
};

class STARROVERS_API FSRPatternFacilityResolver final
{
public:
	static bool IsValidTransformOperatorSpec(const FSRPatternTransformOperatorSpec& OperatorSpec);
	static bool IsValidSeparationOperatorSpec(const FSRPatternSeparationOperatorSpec& OperatorSpec);

	static FSRPatternFacilityResolveResult ResolveTransform(
		const FSRPattern& InputPattern,
		const FSRPatternTransformOperatorSpec& OperatorSpec);

	static FSRPatternFacilityResolveResult ResolveSynthesis(
		const FSRPattern& BasePattern,
		const FSRPattern& OverlayPattern);

	static FSRPatternFacilityResolveResult ResolveSeparation(
		const FSRPattern& InputPattern,
		const FSRPatternSeparationOperatorSpec& OperatorSpec);
};
