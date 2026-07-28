#pragma once

#include "CoreMinimal.h"
#include "Automation/SRResourceDataAsset.h"
#include "SRResourceProcessingKernel.generated.h"

UENUM(BlueprintType)
enum class ESRResourceProcessOutcome : uint8
{
	Success UMETA(DisplayName = "Success"),
	InvalidResource UMETA(DisplayName = "Invalid Resource"),
	UnsupportedSchema UMETA(DisplayName = "Unsupported Schema"),
	UnsupportedResourceClass UMETA(DisplayName = "Unsupported Resource Class"),
	TerminalResource UMETA(DisplayName = "Terminal Resource"),
	MissingFamily UMETA(DisplayName = "Missing Family"),
	MissingProcessArchetype UMETA(DisplayName = "Missing Process Archetype"),
	InvalidTemperature UMETA(DisplayName = "Invalid Temperature"),
	InvalidFamilyAction UMETA(DisplayName = "Invalid Family Action"),
	InvalidProcessTag UMETA(DisplayName = "Invalid Process Tag"),
	InvalidEnergy UMETA(DisplayName = "Invalid Energy"),
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRResourceProcessSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Processing")
	FName ProcessArchetype = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Processing")
	ESRResourceProcessTemperatureState Temperature = ESRResourceProcessTemperatureState::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Processing")
	ESRResourceFamilyAction FamilyAction = ESRResourceFamilyAction::None;

	/**
	 * Family-specific facilities and conditioned holds set this true. A
	 * universal Bridge still advances negative Family pressure, but cannot
	 * activate or cash out the positive Family merit.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Processing")
	bool bIsFamilySpecialist = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Processing", meta = (DisplayName = "Facility Energy Delta"))
	double FacilityEnergyDelta = 0.0;

	// Optional for a pure preview. Runtime callers provide the stable celestial body id.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Processing")
	FName ProcessingBodyId = NAME_None;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRResourceProcessingRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Processing")
	bool bClampCurrentEnergyAtZero = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Processing|Metal", meta = (
		DisplayName = "Metal Work Strain Threshold",
		ClampMin = "1",
		ToolTip = "The Metal operation count at which Fatigued activates. The serialized field name is retained for compatibility."))
	int32 MetalFatiguedRepeatThreshold = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Processing|Metal", meta = (ClampMin = "0"))
	double MetalTemperedEnergyBonus = 5.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Processing|Metal", meta = (ClampMin = "0"))
	double MetalFatiguedEnergyPenalty = 8.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Processing|Crystal", meta = (ClampMin = "1"))
	int32 CrystalResonantRepeatThreshold = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Processing|Crystal", meta = (ClampMin = "1"))
	int32 CrystalFracturedRepeatThreshold = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Processing|Crystal", meta = (ClampMin = "0"))
	double CrystalResonantEnergyBonus = 4.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Processing|Crystal", meta = (ClampMin = "0"))
	double CrystalFracturedEnergyPenalty = 10.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Processing|Organic", meta = (ClampMin = "1"))
	int32 OrganicDepletedProcessThreshold = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Processing|Organic", meta = (ClampMin = "0"))
	double OrganicMaturedEnergyBonus = 6.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Processing|Organic", meta = (ClampMin = "0"))
	double OrganicDepletedEnergyPenalty = 7.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Processing|Plasma", meta = (ClampMin = "1"))
	int32 PlasmaOverloadedAmplificationThreshold = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Processing|Plasma", meta = (ClampMin = "0"))
	double PlasmaEnergizedEnergyBonus = 5.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Processing|Plasma", meta = (ClampMin = "0"))
	double PlasmaOverloadedEnergyPenalty = 10.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Processing|Void", meta = (ClampMin = "1"))
	int32 VoidCollapsedGainThreshold = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Processing|Void", meta = (ClampMin = "0"))
	double VoidEchoEnergyMultiplier = 2.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Processing|Void", meta = (ClampMin = "0"))
	double VoidMaximumEchoEnergyBonus = 8.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Processing|Void", meta = (ClampMin = "0"))
	double VoidCollapsedEnergyPenalty = 8.0;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRResourceProcessResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Processing")
	ESRResourceProcessOutcome Outcome = ESRResourceProcessOutcome::InvalidResource;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Processing")
	FString FailureReason;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Processing")
	FSRResourceInstance OutputResource;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Processing")
	ESRResourceFamilyAction EffectiveFamilyAction = ESRResourceFamilyAction::None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Processing")
	double InputEnergy = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Processing")
	double RequestedFacilityEnergyDelta = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Processing")
	double FacilityEnergyDelta = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Processing")
	double FamilyEnergyDelta = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Processing")
	double ProcessTagEnergyDelta = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Processing")
	FName EvaluatedProcessTagId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Processing")
	bool bProcessTagTriggered = false;

	// Landing Charge observes an Energy change before its own additive bonus.
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Processing")
	bool bPreTagEnergyChanged = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Processing")
	double ClampEnergyDelta = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Processing")
	double UnclampedOutputEnergy = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Processing")
	double OutputEnergy = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Processing")
	double AppliedEnergyDelta = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Processing", meta = (
		Bitmask,
		BitmaskEnum = "/Script/StarRovers.ESRResourceFamilyState"))
	int32 ActivatedFamilyStateFlags = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Processing", meta = (
		Bitmask,
		BitmaskEnum = "/Script/StarRovers.ESRResourceFamilyState"))
	int32 ClearedFamilyStateFlags = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Processing")
	bool bProcessArchetypeChanged = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Processing")
	bool bFamilyActionChanged = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Processing")
	bool bPositiveFamilyStateActivated = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Processing")
	bool bNegativeFamilyStateCleared = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Processing")
	bool bEnergyClamped = false;

	bool IsSuccess() const
	{
		return Outcome == ESRResourceProcessOutcome::Success;
	}
};

class STARROVERS_API FSRResourceProcessingKernel final
{
public:
	// Pure, deterministic and preview-safe: InputResource is never modified.
	static FSRResourceProcessResult Evaluate(
		const FSRResourceInstance& InputResource,
		const FSRResourceProcessSpec& ProcessSpec,
		const FSRResourceProcessingRules& Rules = FSRResourceProcessingRules());
};
