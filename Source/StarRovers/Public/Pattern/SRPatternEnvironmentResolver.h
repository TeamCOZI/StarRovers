#pragma once

#include "CoreMinimal.h"
#include "Pattern/SRPatternResolver.h"
#include "SRPatternEnvironmentResolver.generated.h"

UENUM(BlueprintType)
enum class ESRPatternEnvironmentEffectKind : uint8
{
	DirectionalPull = 0 UMETA(DisplayName = "DirectionalPull"),
	ContinuousDrift = 1 UMETA(DisplayName = "ContinuousDrift"),
	OrganicBloom = 2 UMETA(DisplayName = "OrganicBloom"),
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPatternEnvironmentEffectSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Environment", meta = (DisplayName = "EffectKind"))
	ESRPatternEnvironmentEffectKind EffectKind = ESRPatternEnvironmentEffectKind::DirectionalPull;

	// Empty means every occupied glyph. A playable glyph limits movement effects to that glyph type.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Environment", meta = (
		DisplayName = "AffectedGlyph",
		EditCondition = "EffectKind != ESRPatternEnvironmentEffectKind::OrganicBloom",
		EditConditionHides))
	ESRGlyphType AffectedGlyph = ESRGlyphType::Empty;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Environment", meta = (DisplayName = "Direction"))
	ESRPatternDirection Direction = ESRPatternDirection::Down;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Environment", meta = (
		DisplayName = "Distance",
		ClampMin = "1",
		ClampMax = "4",
		EditCondition = "EffectKind == ESRPatternEnvironmentEffectKind::DirectionalPull",
		EditConditionHides))
	int32 Distance = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Environment", meta = (
		DisplayName = "MaxDriftSteps",
		ClampMin = "1",
		ClampMax = "4",
		EditCondition = "EffectKind == ESRPatternEnvironmentEffectKind::ContinuousDrift",
		EditConditionHides))
	int32 MaxDriftSteps = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Environment", meta = (
		DisplayName = "OrganicGrowthsPerComponent",
		ClampMin = "1",
		ClampMax = "4",
		EditCondition = "EffectKind == ESRPatternEnvironmentEffectKind::OrganicBloom",
		EditConditionHides))
	int32 OrganicGrowthsPerComponent = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Environment", meta = (
		DisplayName = "FluidSidePreference",
		EditCondition = "EffectKind != ESRPatternEnvironmentEffectKind::OrganicBloom",
		EditConditionHides))
	ESRPatternFluidSidePreference FluidSidePreference = ESRPatternFluidSidePreference::ClockwiseFirst;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPatternEnvironmentSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Environment", meta = (DisplayName = "EnvironmentId"))
	FName EnvironmentId = FName(TEXT("Neutral"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Environment", meta = (DisplayName = "Effects", TitleProperty = "EffectKind"))
	TArray<FSRPatternEnvironmentEffectSpec> Effects;

	bool IsCanonical() const;
	void Normalize();
};

UENUM(BlueprintType)
enum class ESRPatternEnvironmentResolveFailure : uint8
{
	None = 0 UMETA(DisplayName = "None"),
	InvalidInputPattern = 1 UMETA(DisplayName = "InvalidInputPattern"),
	InvalidEnvironmentSpec = 2 UMETA(DisplayName = "InvalidEnvironmentSpec"),
	ResolverFailure = 3 UMETA(DisplayName = "ResolverFailure"),
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPatternEnvironmentResolveResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|Environment", meta = (DisplayName = "Succeeded"))
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|Environment", meta = (DisplayName = "Failure"))
	ESRPatternEnvironmentResolveFailure Failure = ESRPatternEnvironmentResolveFailure::None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|Environment", meta = (DisplayName = "OutputPattern"))
	FSRPattern OutputPattern;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|Environment", meta = (DisplayName = "TraceEvents"))
	TArray<FSRPatternTraceEvent> TraceEvents;
};

class STARROVERS_API FSRPatternEnvironmentResolver final
{
public:
	static constexpr int32 MaxEnvironmentEffects = 4;
	static constexpr int32 MaxDirectionalDistance = StarRovers::Pattern::GridSize - 1;
	static constexpr int32 MaxContinuousDriftSteps = StarRovers::Pattern::GridSize - 1;

	static bool IsValidEffectSpec(const FSRPatternEnvironmentEffectSpec& EffectSpec);
	static bool IsValidEnvironmentSpec(const FSRPatternEnvironmentSpec& EnvironmentSpec);

	static FSRPatternEnvironmentResolveResult Resolve(
		const FSRPattern& InputPattern,
		const FSRPatternEnvironmentSpec& EnvironmentSpec);
};
