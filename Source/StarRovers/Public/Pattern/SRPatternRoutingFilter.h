#pragma once

#include "CoreMinimal.h"
#include "Automation/SRResourceDataAsset.h"
#include "SRPatternRoutingFilter.generated.h"

UENUM(BlueprintType)
enum class ESRPatternRoutingMatchMode : uint8
{
	AnyPattern = 0 UMETA(DisplayName = "Any Pattern"),
	ExactPattern = 1 UMETA(DisplayName = "Exact Pattern"),
	MaskedPattern = 2 UMETA(DisplayName = "Masked Pattern"),
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPatternRoutingFilter
{
	GENERATED_BODY()

	// None accepts every resource identity. Pattern matching is evaluated separately.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Routing", meta = (DisplayName = "ResourceId"))
	FName ResourceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Routing", meta = (DisplayName = "MatchMode"))
	ESRPatternRoutingMatchMode MatchMode = ESRPatternRoutingMatchMode::AnyPattern;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Routing", meta = (
		DisplayName = "RequiredPattern",
		EditCondition = "MatchMode != ESRPatternRoutingMatchMode::AnyPattern",
		EditConditionHides))
	FSRPattern RequiredPattern;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Routing", meta = (
		DisplayName = "RequiredMask",
		EditCondition = "MatchMode == ESRPatternRoutingMatchMode::MaskedPattern",
		EditConditionHides))
	FSRPatternMask RequiredMask;

	bool IsCanonical() const;
	void Normalize();
};

namespace StarRovers::PatternRouting
{
	// A transportable resource must have an identity, positive quantity, and a non-empty canonical Pattern.
	STARROVERS_API bool IsValidPatternPayload(const FSRResourceInstance& ResourceInstance);

	// Empty cargo is represented by a fully reset resource identity and an empty canonical Pattern.
	STARROVERS_API bool IsEmptyPatternPayload(const FSRResourceInstance& ResourceInstance);
	STARROVERS_API bool IsValidOrEmptyPatternPayload(const FSRResourceInstance& ResourceInstance);

	STARROVERS_API bool MatchesRoutingFilter(
		const FSRResourceInstance& ResourceInstance,
		const FSRPatternRoutingFilter& RoutingFilter);
}
