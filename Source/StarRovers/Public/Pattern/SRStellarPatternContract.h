#pragma once

#include "CoreMinimal.h"
#include "Pattern/SRPatternTypes.h"
#include "SRStellarPatternContract.generated.h"

UENUM(BlueprintType)
enum class ESRPatternHandRuleKind : uint8
{
	SameGlyphShape = 0 UMETA(DisplayName = "Same Glyph Shape"),
	ExactGlyphShape = 1 UMETA(DisplayName = "Exact Glyph Shape"),
};

UENUM(BlueprintType)
enum class ESRPatternHandTransformPolicy : uint8
{
	Fixed = 0 UMETA(DisplayName = "Fixed"),
	Translate = 1 UMETA(DisplayName = "Translate"),
	RotateAndTranslate = 2 UMETA(DisplayName = "Rotate and Translate"),
	RotateReflectAndTranslate = 3 UMETA(DisplayName = "Rotate, Reflect, and Translate"),
};

UENUM(BlueprintType)
enum class ESRPatternHandRarity : uint8
{
	Common = 0 UMETA(DisplayName = "Common"),
	Uncommon = 1 UMETA(DisplayName = "Uncommon"),
	Rare = 2 UMETA(DisplayName = "Rare"),
	Epic = 3 UMETA(DisplayName = "Epic"),
	Legendary = 4 UMETA(DisplayName = "Legendary"),
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPatternHandRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Hand", meta = (DisplayName = "RuleId"))
	FName RuleId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Hand", meta = (DisplayName = "DisplayName"))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Hand", meta = (DisplayName = "RuleKind"))
	ESRPatternHandRuleKind RuleKind = ESRPatternHandRuleKind::SameGlyphShape;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Hand", meta = (DisplayName = "Rarity"))
	ESRPatternHandRarity Rarity = ESRPatternHandRarity::Common;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Hand", meta = (DisplayName = "ShapeMask"))
	FSRPatternMask ShapeMask;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Hand", meta = (DisplayName = "RequiredPattern", EditCondition = "RuleKind == ESRPatternHandRuleKind::ExactGlyphShape"))
	FSRPattern RequiredPattern;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Hand", meta = (DisplayName = "TransformPolicy"))
	ESRPatternHandTransformPolicy TransformPolicy = ESRPatternHandTransformPolicy::Fixed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Hand", meta = (DisplayName = "BonusScore", ClampMin = "1"))
	int32 BonusScore = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Pattern|Hand", meta = (DisplayName = "MaximumMatches", ClampMin = "1", ClampMax = "25"))
	int32 MaximumMatches = 1;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRStellarPatternContract
{
	GENERATED_BODY()

	FSRStellarPatternContract();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Contract", meta = (DisplayName = "ContractId"))
	FName ContractId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Contract", meta = (DisplayName = "RequiredPattern"))
	FSRPattern RequiredPattern;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Contract", meta = (DisplayName = "RequiredMask"))
	FSRPatternMask RequiredMask;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Contract|Score", meta = (DisplayName = "BaseScorePerPattern", ClampMin = "1"))
	int32 BaseScorePerPattern = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Contract|Score", meta = (DisplayName = "RequiredScorePerCycle", ClampMin = "1"))
	int32 RequiredScorePerCycle = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Contract|Score", meta = (DisplayName = "RequiredScoreGrowthPerCycle", ClampMin = "0"))
	int32 RequiredScoreGrowthPerCycle = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Contract|Health", meta = (DisplayName = "StellarHealthMaximum", ClampMin = "0.01"))
	double StellarHealthMaximum = 1000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Contract|Health", meta = (DisplayName = "StartingStellarHealth", ClampMin = "0.0"))
	double StartingStellarHealth = 1000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Contract|Health", meta = (DisplayName = "InitialStellarHealthDecreasePerSecond", ClampMin = "0.0", ToolTip = "Stellar health removed for each simulated second during Period 0."))
	double InitialStellarHealthDecreasePerSecond = 0.25;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Contract|Health", meta = (DisplayName = "StellarHealthDecreaseMultiplierPerPeriod", ClampMin = "1.0", ToolTip = "Multiplies the per-second stellar-health decrease whenever a shared simulation Period ends."))
	double StellarHealthDecreaseMultiplierPerPeriod = 1.05;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Contract|Health", meta = (DisplayName = "StellarHealthRestoredPerPatternScore", ClampMin = "0.0", ToolTip = "Immediate stellar health restored for each score point in an accepted Pattern cargo."))
	double StellarHealthRestoredPerPatternScore = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Contract|Hands", meta = (DisplayName = "AllowOverlappingBonusHands"))
	bool bAllowOverlappingBonusHands = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Stellar Contract|Hands", meta = (DisplayName = "BonusRules"))
	TArray<FSRPatternHandRule> BonusRules;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPatternHandMatch
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|Hand", meta = (DisplayName = "RuleId"))
	FName RuleId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|Hand", meta = (DisplayName = "DisplayName"))
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|Hand", meta = (DisplayName = "Rarity"))
	ESRPatternHandRarity Rarity = ESRPatternHandRarity::Common;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|Hand", meta = (DisplayName = "MatchedGlyph"))
	ESRGlyphType MatchedGlyph = ESRGlyphType::Empty;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|Hand", meta = (DisplayName = "MatchedCells"))
	FSRPatternMask MatchedCells;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|Hand", meta = (DisplayName = "BonusScore"))
	int32 BonusScore = 0;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRStellarPatternScoreResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Contract", meta = (DisplayName = "ContractValid"))
	bool bContractValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Contract", meta = (DisplayName = "PatternValid"))
	bool bPatternValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Contract", meta = (DisplayName = "MatchesDemand"))
	bool bMatchesDemand = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Contract", meta = (DisplayName = "StackCount"))
	int32 StackCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Contract", meta = (DisplayName = "BaseScorePerPattern"))
	int32 BaseScorePerPattern = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Contract", meta = (DisplayName = "BonusScorePerPattern"))
	int64 BonusScorePerPattern = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Contract", meta = (DisplayName = "ScorePerPattern"))
	int64 ScorePerPattern = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Contract", meta = (DisplayName = "TotalScore"))
	int64 TotalScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Contract", meta = (DisplayName = "HandMatches"))
	TArray<FSRPatternHandMatch> HandMatches;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Contract", meta = (DisplayName = "FailureReason"))
	FString FailureReason;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRStellarContractCycleSettlement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Contract", meta = (DisplayName = "Valid"))
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Contract", meta = (DisplayName = "CycleIndex"))
	int32 CycleIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Contract", meta = (DisplayName = "RequiredScore"))
	int64 RequiredScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Contract", meta = (DisplayName = "SubmittedScore"))
	int64 SubmittedScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Contract", meta = (DisplayName = "MissingScore"))
	int64 MissingScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Contract", meta = (DisplayName = "SurplusScore"))
	int64 SurplusScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Contract", meta = (DisplayName = "RequirementMet"))
	bool bRequirementMet = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Stellar Contract", meta = (DisplayName = "FailureReason"))
	FString FailureReason;
};

// A small projection keeps the Pattern contract resolver independent from the
// run-progression subsystem while letting runtime, preview, and balance tests use
// exactly the same scoring and stellar-health calculations.
USTRUCT(BlueprintType)
struct STARROVERS_API FSRStellarPatternContractModifiers
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "StarRovers|Stellar Contract|Modifier")
	double BaseScoreMultiplier = 1.0;

	UPROPERTY(BlueprintReadWrite, Category = "StarRovers|Stellar Contract|Modifier")
	double BonusScoreMultiplier = 1.0;

	UPROPERTY(BlueprintReadWrite, Category = "StarRovers|Stellar Contract|Modifier")
	double RequiredScoreMultiplier = 1.0;

	UPROPERTY(BlueprintReadWrite, Category = "StarRovers|Stellar Contract|Modifier")
	double HealthDamageMultiplier = 1.0;

	UPROPERTY(BlueprintReadWrite, Category = "StarRovers|Stellar Contract|Modifier")
	double HealthRecoveryMultiplier = 1.0;
};

class STARROVERS_API FSRStellarPatternContractResolver final
{
public:
	static bool ValidateContract(
		const FSRStellarPatternContract& Contract,
		FString& OutFailureReason);

	static bool DoesPatternMatchDemand(
		const FSRPattern& Pattern,
		const FSRStellarPatternContract& Contract);

	static int64 GetRequiredScoreForCycle(
		const FSRStellarPatternContract& Contract,
		int32 CycleIndex,
		const FSRStellarPatternContractModifiers& Modifiers = FSRStellarPatternContractModifiers());

	static double GetStellarHealthDecreasePerSecondForPeriod(
		const FSRStellarPatternContract& Contract,
		int32 PeriodIndex,
		const FSRStellarPatternContractModifiers& Modifiers = FSRStellarPatternContractModifiers());

	static double GetStellarHealthRestorationForScore(
		const FSRStellarPatternContract& Contract,
		int64 PatternScore,
		const FSRStellarPatternContractModifiers& Modifiers = FSRStellarPatternContractModifiers());

	static FSRStellarPatternScoreResult ScorePattern(
		const FSRPattern& Pattern,
		int32 StackCount,
		const FSRStellarPatternContract& Contract,
		const FSRStellarPatternContractModifiers& Modifiers = FSRStellarPatternContractModifiers());

	static FSRStellarContractCycleSettlement SettleCycle(
		const FSRStellarPatternContract& Contract,
		int32 CycleIndex,
		int64 SubmittedScore,
		const FSRStellarPatternContractModifiers& Modifiers = FSRStellarPatternContractModifiers());
};
