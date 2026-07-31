#pragma once

#include "CoreMinimal.h"
#include "Pattern/SRPatternTypes.h"
#include "SRRunModifierTypes.generated.h"

UENUM(BlueprintType)
enum class ESRRunModifierSourceKind : uint8
{
	Technology = 0 UMETA(DisplayName = "Technology"),
	Augment = 1 UMETA(DisplayName = "Augment"),
	Trial = 2 UMETA(DisplayName = "Trial"),
};

UENUM(BlueprintType)
enum class ESRRunModifierFacilityScope : uint8
{
	Any = 0 UMETA(DisplayName = "Any Facility"),
	Transform = 1 UMETA(DisplayName = "Transform"),
	Synthesis = 2 UMETA(DisplayName = "Synthesis"),
	Separation = 3 UMETA(DisplayName = "Separation"),
	Mining = 4 UMETA(DisplayName = "Mining"),
};

UENUM(BlueprintType)
enum class ESRRunModifierEffectKind : uint8
{
	FacilityProcessTimeMultiplier = 0 UMETA(DisplayName = "Facility Process Time Multiplier"),
	TransformOrganicGrowthDelta = 1 UMETA(DisplayName = "Transform Organic Growth Delta"),
	EnvironmentIntensityDelta = 2 UMETA(DisplayName = "Environment Intensity Delta"),
	StellarBaseScoreMultiplier = 3 UMETA(DisplayName = "Stellar Base Score Multiplier"),
	StellarBonusScoreMultiplier = 4 UMETA(DisplayName = "Stellar Bonus Score Multiplier"),
	StellarRequiredScoreMultiplier = 5 UMETA(DisplayName = "Stellar Required Score Multiplier"),
	StellarHealthDamageMultiplier = 6 UMETA(DisplayName = "Stellar Health Damage Multiplier"),
	StellarHealthRecoveryMultiplier = 7 UMETA(DisplayName = "Stellar Health Recovery Multiplier"),
	LogisticsTravelTimeMultiplier = 8 UMETA(DisplayName = "Logistics Travel Time Multiplier"),
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRRunModifierEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Modifier", meta = (DisplayName = "EffectId"))
	FName EffectId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Modifier", meta = (DisplayName = "EffectKind"))
	ESRRunModifierEffectKind EffectKind = ESRRunModifierEffectKind::FacilityProcessTimeMultiplier;

	// Multiplier effects use this value as a factor (0.8 = 20% faster/less).
	// Delta effects require an integer value in the inclusive range [-4, 4].
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Modifier", meta = (DisplayName = "Magnitude"))
	double Magnitude = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Modifier|Condition", meta = (DisplayName = "FacilityScope"))
	ESRRunModifierFacilityScope FacilityScope = ESRRunModifierFacilityScope::Any;

	// Empty is a wildcard. A playable glyph applies only when that glyph is the
	// deterministic dominant glyph of the current facility input.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Modifier|Condition", meta = (DisplayName = "AffectedGlyph"))
	ESRGlyphType AffectedGlyph = ESRGlyphType::Empty;

	// None is a wildcard. A named contract limits stellar effects to that contract.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Modifier|Condition", meta = (DisplayName = "ContractId"))
	FName ContractId = NAME_None;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRRunModifierSource
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Modifier", meta = (DisplayName = "SourceId"))
	FName SourceId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Modifier", meta = (DisplayName = "SourceKind"))
	ESRRunModifierSourceKind SourceKind = ESRRunModifierSourceKind::Technology;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Modifier", meta = (DisplayName = "Priority"))
	int32 Priority = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Modifier", meta = (DisplayName = "StackCount"))
	int32 StackCount = 1;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Modifier", meta = (DisplayName = "Effects"))
	TArray<FSRRunModifierEffect> Effects;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRRunModifierContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Modifier", meta = (DisplayName = "Revision"))
	int32 Revision = 0;

	// Always stored in canonical Technology -> Augment -> Trial order, then by
	// priority and source ID. Facility batches snapshot this complete context.
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Modifier", meta = (DisplayName = "ActiveSources"))
	TArray<FSRRunModifierSource> ActiveSources;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRRunModifierQuery
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Modifier", meta = (DisplayName = "FacilityScope"))
	ESRRunModifierFacilityScope FacilityScope = ESRRunModifierFacilityScope::Any;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Modifier", meta = (DisplayName = "DominantGlyph"))
	ESRGlyphType DominantGlyph = ESRGlyphType::Empty;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Run Modifier", meta = (DisplayName = "ContractId"))
	FName ContractId = NAME_None;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRResolvedRunModifiers
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Modifier")
	double FacilityProcessTimeMultiplier = 1.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Modifier")
	int32 TransformOrganicGrowthDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Modifier")
	int32 EnvironmentIntensityDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Modifier")
	double StellarBaseScoreMultiplier = 1.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Modifier")
	double StellarBonusScoreMultiplier = 1.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Modifier")
	double StellarRequiredScoreMultiplier = 1.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Modifier")
	double StellarHealthDamageMultiplier = 1.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Modifier")
	double StellarHealthRecoveryMultiplier = 1.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Run Modifier")
	double LogisticsTravelTimeMultiplier = 1.0;
};

class STARROVERS_API FSRRunModifierResolver final
{
public:
	static constexpr int32 MaximumSourceStacks = 16;
	static constexpr double MinimumGeneralMultiplier = 0.25;
	static constexpr double MaximumGeneralMultiplier = 4.0;
	static constexpr double MinimumRequiredScoreMultiplier = 0.5;
	static constexpr double MaximumRequiredScoreMultiplier = 3.0;
	static constexpr double MinimumLogisticsTravelTimeMultiplier = 0.5;
	static constexpr double MaximumLogisticsTravelTimeMultiplier = 2.0;
	static constexpr int32 MinimumDelta = -4;
	static constexpr int32 MaximumDelta = 4;

	static bool IsMultiplierEffect(ESRRunModifierEffectKind EffectKind);
	static bool ValidateEffect(const FSRRunModifierEffect& Effect, FString& OutFailureReason);
	static bool ValidateSource(const FSRRunModifierSource& Source, FString& OutFailureReason);

	static bool BuildContext(
		const TArray<FSRRunModifierSource>& Sources,
		int32 Revision,
		FSRRunModifierContext& OutContext,
		FString& OutFailureReason);

	static FSRResolvedRunModifiers Resolve(
		const FSRRunModifierContext& Context,
		const FSRRunModifierQuery& Query = FSRRunModifierQuery());
};
