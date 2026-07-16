#pragma once

#include "CoreMinimal.h"
#include "Automation/SRResourceDataAsset.h"
#include "Engine/DataAsset.h"
#include "SRFacilityDataAsset.generated.h"

UENUM(BlueprintType)
enum class ESRFacilityKind : uint8
{
	Standard = 0 UMETA(DisplayName = "Standard"),
	Hub = 1 UMETA(DisplayName = "Hub"),
};

UENUM(BlueprintType)
enum class ESRFacilityRarity : uint8
{
	Basic = 0 UMETA(DisplayName = "Basic"),
	Advanced = 1 UMETA(DisplayName = "Advanced"),
	HighTech = 2 UMETA(DisplayName = "HighTech"),
	Innovation = 3 UMETA(DisplayName = "Innovation"),
	Starting = 4 UMETA(DisplayName = "Starting"),
};

UENUM(BlueprintType)
enum class ESRFacilityOperationKind : uint8
{
	Process = 0 UMETA(DisplayName = "Process"),
	Synthesize = 1 UMETA(DisplayName = "Synthesize"),
	Mine = 3 UMETA(DisplayName = "Mine"),
};

UENUM(BlueprintType)
enum class ESRFacilityTemperatureState : uint8
{
	Frozen UMETA(DisplayName = "Frozen"),
	Cold UMETA(DisplayName = "Cold"),
	Normal UMETA(DisplayName = "Normal"),
	Hot UMETA(DisplayName = "Hot"),
	Overheated UMETA(DisplayName = "Overheated"),
};

UENUM(BlueprintType)
enum class ESRFacilityPortKind : uint8
{
	Input UMETA(DisplayName = "Input"),
	Output UMETA(DisplayName = "Output"),
};

UENUM(BlueprintType)
enum class ESRFacilityEffectKind : uint8
{
	AdjustEnergy UMETA(DisplayName = "AdjustEnergy"),
	AdjustProcessLimit UMETA(DisplayName = "AdjustProcessLimit"),
	RemoveResource UMETA(DisplayName = "RemoveResource"),
	AttachTag UMETA(DisplayName = "AttachTag"),
	ProduceWaste UMETA(DisplayName = "ProduceWaste"),
	AdjustCellTemperature UMETA(DisplayName = "AdjustCellTemperature"),
	InvertHeat UMETA(DisplayName = "InvertHeat"),
	InvertTagEffects UMETA(DisplayName = "InvertTagEffects"),
	DuplicateInputResource UMETA(DisplayName = "DuplicateInputResource"),
	OverrideProcessTemperature UMETA(DisplayName = "OverrideProcessTemperature"),
	TriggerTagEffect UMETA(DisplayName = "TriggerTagEffect"),
	AdjustProcessTime UMETA(DisplayName = "AdjustProcessTime"),
	RemoveTag UMETA(DisplayName = "RemoveTag"),
	MultiplyEnergyByConsumedProcessLimit UMETA(DisplayName = "MultiplyEnergyByConsumedProcessLimit"),
	ChangeResourceType UMETA(DisplayName = "ChangeResourceType"),
};

UENUM(BlueprintType)
enum class ESRFacilityAttachTagSource : uint8
{
	SpecificTag UMETA(DisplayName = "SpecificTag"),
	LastAttachedTag UMETA(DisplayName = "LastAttachedTag"),
};

UENUM(BlueprintType)
enum class ESRFacilityEffectTagTarget : uint8
{
	SpecificTag UMETA(DisplayName = "SpecificTag"),
	LastAttachedTag UMETA(DisplayName = "LastAttachedTag"),
	AllTags UMETA(DisplayName = "AllTags"),
};

UENUM(BlueprintType)
enum class ESRFacilityEnergyAdjustmentValueSource : uint8
{
	FixedValue UMETA(DisplayName = "FixedValue"),
	RemainingProcessLimit UMETA(DisplayName = "RemainingProcessLimit"),
	TagStackCount UMETA(DisplayName = "TagStackCount"),
	EnergyChangeCount UMETA(DisplayName = "EnergyChangeCount"),
	TagEffectEnergyChangeAmount UMETA(DisplayName = "TagEffectEnergyChangeAmount"),
};

UENUM(BlueprintType)
enum class ESRFacilityEnergyAdjustmentMode : uint8
{
	Add = 0 UMETA(DisplayName = "Add"),
	Multiply = 1 UMETA(DisplayName = "Multiply"),
	Subtract = 2 UMETA(DisplayName = "Subtract"),
};

UENUM(BlueprintType)
enum class ESRFacilityProcessLimitAdjustmentMode : uint8
{
	AddValue UMETA(DisplayName = "AddValue"),
	SetValue UMETA(DisplayName = "SetValue"),
};

UENUM(BlueprintType)
enum class ESRFacilityProcessTimeAdjustmentMode : uint8
{
	AddSeconds UMETA(DisplayName = "AddSeconds"),
	Multiply UMETA(DisplayName = "Multiply"),
};

UENUM(BlueprintType)
enum class ESRFacilityEffectConditionKind : uint8
{
	EnergyAtLeast = 0 UMETA(DisplayName = "EnergyAtLeast"),
	EnergyAtMost = 1 UMETA(DisplayName = "EnergyAtMost"),
	EnergyIncreased = 2 UMETA(DisplayName = "EnergyIncreased"),
	EnergyDecreased = 3 UMETA(DisplayName = "EnergyDecreased"),
	Tag = 4 UMETA(DisplayName = "Tag"),
	TemperatureState = 5 UMETA(DisplayName = "TemperatureState"),
	ProcessCountEquals = 6 UMETA(DisplayName = "ProcessCountAtLeast"),
	PrimeEnergy = 7 UMETA(DisplayName = "PrimeEnergy"),
	EnergyGreaterThan = 8 UMETA(DisplayName = "EnergyGreaterThan"),
	EnergyLessThan = 9 UMETA(DisplayName = "EnergyLessThan"),
};

UENUM(BlueprintType)
enum class ESRFacilityTagConditionMode : uint8
{
	HasTag UMETA(DisplayName = "HasTag"),
	MissingTag UMETA(DisplayName = "MissingTag"),
	StackCountAtLeast UMETA(DisplayName = "StackCountAtLeast"),
};

UENUM(BlueprintType)
enum class ESRFacilityTagConditionTarget : uint8
{
	SpecificTag UMETA(DisplayName = "SpecificTag"),
	AllTags UMETA(DisplayName = "AllTags"),
};

UENUM(BlueprintType)
enum class ESRFacilityConditionLogic : uint8
{
	And UMETA(DisplayName = "And"),
	Or UMETA(DisplayName = "Or"),
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRFacilityEffectConditionSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Condition", meta = (DisplayName = "ConditionKind"))
	ESRFacilityEffectConditionKind ConditionKind = ESRFacilityEffectConditionKind::EnergyAtLeast;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Condition", meta = (
		DisplayName = "EnergyValue",
		EditCondition = "ConditionKind == ESRFacilityEffectConditionKind::EnergyAtLeast || ConditionKind == ESRFacilityEffectConditionKind::EnergyAtMost || ConditionKind == ESRFacilityEffectConditionKind::EnergyGreaterThan || ConditionKind == ESRFacilityEffectConditionKind::EnergyLessThan",
		EditConditionHides))
	double EnergyValue = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Condition", meta = (
		DisplayName = "TagMode",
		EditCondition = "ConditionKind == ESRFacilityEffectConditionKind::Tag",
		EditConditionHides))
	ESRFacilityTagConditionMode TagMode = ESRFacilityTagConditionMode::HasTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Condition", meta = (
		DisplayName = "TagTarget",
		EditCondition = "ConditionKind == ESRFacilityEffectConditionKind::Tag",
		EditConditionHides))
	ESRFacilityTagConditionTarget TagTarget = ESRFacilityTagConditionTarget::SpecificTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Condition", meta = (
		DisplayName = "ResourceTag",
		EditCondition = "ConditionKind == ESRFacilityEffectConditionKind::Tag && TagTarget == ESRFacilityTagConditionTarget::SpecificTag",
		EditConditionHides))
	ESRResourceProcessTag ResourceTag = ESRResourceProcessTag::Responsive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Condition", meta = (
		DisplayName = "TagStackCount",
		ClampMin = "1",
		EditCondition = "ConditionKind == ESRFacilityEffectConditionKind::Tag && TagMode == ESRFacilityTagConditionMode::StackCountAtLeast",
		EditConditionHides))
	int32 TagStackCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Condition", meta = (
		DisplayName = "TemperatureState",
		EditCondition = "ConditionKind == ESRFacilityEffectConditionKind::TemperatureState",
		EditConditionHides))
	ESRFacilityTemperatureState TemperatureState = ESRFacilityTemperatureState::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Condition", meta = (
		DisplayName = "ProcessCount",
		ClampMin = "0",
		EditCondition = "ConditionKind == ESRFacilityEffectConditionKind::ProcessCountEquals",
		EditConditionHides))
	int32 ProcessCount = 0;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRFacilityEffectConditionGroupSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Condition Group", meta = (DisplayName = "ConditionLogic"))
	ESRFacilityConditionLogic ConditionLogic = ESRFacilityConditionLogic::And;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Condition Group", meta = (DisplayName = "Conditions"))
	TArray<FSRFacilityEffectConditionSpec> Conditions;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRFacilityEffectSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (DisplayName = "EffectKind"))
	ESRFacilityEffectKind EffectKind = ESRFacilityEffectKind::AdjustEnergy;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (
		DisplayName = "Value",
		EditCondition = "(EffectKind == ESRFacilityEffectKind::AdjustEnergy && EnergyValueSource == ESRFacilityEnergyAdjustmentValueSource::FixedValue) || EffectKind == ESRFacilityEffectKind::AdjustProcessLimit || EffectKind == ESRFacilityEffectKind::AdjustProcessTime",
		EditConditionHides))
	double Value = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (
		DisplayName = "Count",
		ClampMin = "1",
		EditCondition = "EffectKind == ESRFacilityEffectKind::AttachTag || EffectKind == ESRFacilityEffectKind::ProduceWaste || EffectKind == ESRFacilityEffectKind::DuplicateInputResource",
		EditConditionHides))
	int32 Count = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (
		DisplayName = "TileRange",
		ClampMin = "1",
		EditCondition = "EffectKind == ESRFacilityEffectKind::AdjustCellTemperature",
		EditConditionHides))
	int32 TileRange = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (
		DisplayName = "TemperatureStepDelta",
		EditCondition = "EffectKind == ESRFacilityEffectKind::AdjustCellTemperature",
		EditConditionHides))
	int32 TemperatureStepDelta = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (
		DisplayName = "ResourceTag",
		EditCondition = "(EffectKind == ESRFacilityEffectKind::AttachTag && AttachTagSource == ESRFacilityAttachTagSource::SpecificTag) || (EffectKind == ESRFacilityEffectKind::AdjustEnergy && EnergyValueSource == ESRFacilityEnergyAdjustmentValueSource::TagStackCount) || ((EffectKind == ESRFacilityEffectKind::TriggerTagEffect || EffectKind == ESRFacilityEffectKind::RemoveTag) && TagTarget == ESRFacilityEffectTagTarget::SpecificTag)",
		EditConditionHides))
	ESRResourceProcessTag ResourceTag = ESRResourceProcessTag::Responsive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (
		DisplayName = "ProducedWasteResource",
		EditCondition = "EffectKind == ESRFacilityEffectKind::ProduceWaste",
		EditConditionHides))
	TObjectPtr<USRResourceDataAsset> ProducedResource = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (
		DisplayName = "TargetResource",
		EditCondition = "EffectKind == ESRFacilityEffectKind::ChangeResourceType",
		EditConditionHides))
	TObjectPtr<USRResourceDataAsset> TargetResource = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (
		DisplayName = "AttachTagSource",
		EditCondition = "EffectKind == ESRFacilityEffectKind::AttachTag",
		EditConditionHides))
	ESRFacilityAttachTagSource AttachTagSource = ESRFacilityAttachTagSource::SpecificTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (
		DisplayName = "TagTarget",
		EditCondition = "EffectKind == ESRFacilityEffectKind::TriggerTagEffect || EffectKind == ESRFacilityEffectKind::RemoveTag",
		EditConditionHides))
	ESRFacilityEffectTagTarget TagTarget = ESRFacilityEffectTagTarget::SpecificTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (
		DisplayName = "EnergyValueSource",
		EditCondition = "EffectKind == ESRFacilityEffectKind::AdjustEnergy",
		EditConditionHides))
	ESRFacilityEnergyAdjustmentValueSource EnergyValueSource = ESRFacilityEnergyAdjustmentValueSource::FixedValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (
		DisplayName = "EnergyAdjustmentMode",
		EditCondition = "EffectKind == ESRFacilityEffectKind::AdjustEnergy",
		EditConditionHides))
	ESRFacilityEnergyAdjustmentMode EnergyAdjustmentMode = ESRFacilityEnergyAdjustmentMode::Add;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (
		DisplayName = "ProcessLimitMode",
		EditCondition = "EffectKind == ESRFacilityEffectKind::AdjustProcessLimit",
		EditConditionHides))
	ESRFacilityProcessLimitAdjustmentMode ProcessLimitMode = ESRFacilityProcessLimitAdjustmentMode::AddValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (
		DisplayName = "ProcessTemperatureState",
		EditCondition = "EffectKind == ESRFacilityEffectKind::OverrideProcessTemperature",
		EditConditionHides))
	ESRFacilityTemperatureState ProcessTemperatureState = ESRFacilityTemperatureState::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (
		DisplayName = "ProcessTimeMode",
		EditCondition = "EffectKind == ESRFacilityEffectKind::AdjustProcessTime",
		EditConditionHides))
	ESRFacilityProcessTimeAdjustmentMode ProcessTimeMode = ESRFacilityProcessTimeAdjustmentMode::AddSeconds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (DisplayName = "Conditions"))
	TArray<FSRFacilityEffectConditionSpec> Conditions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (DisplayName = "ConditionGroupLogic"))
	ESRFacilityConditionLogic ConditionGroupLogic = ESRFacilityConditionLogic::Or;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (DisplayName = "ConditionGroups"))
	TArray<FSRFacilityEffectConditionGroupSpec> ConditionGroups;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRFacilityInventorySpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Inventory", meta = (DisplayName = "SlotCount", ClampMin = "0"))
	int32 SlotCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility|Inventory", meta = (DisplayName = "SlotCapacity", ClampMin = "1"))
	int32 SlotCapacity = 8;
};

UCLASS(BlueprintType)
class STARROVERS_API USRFacilityDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	USRFacilityDataAsset();

	virtual void PostLoad() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "FacilityKind"))
	ESRFacilityKind FacilityKind = ESRFacilityKind::Standard;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "Rarity", EditCondition = "FacilityKind == ESRFacilityKind::Standard", EditConditionHides))
	ESRFacilityRarity Rarity = ESRFacilityRarity::Basic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "OperationKind", EditCondition = "FacilityKind == ESRFacilityKind::Standard", EditConditionHides))
	ESRFacilityOperationKind OperationKind = ESRFacilityOperationKind::Process;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "BaseProcessSeconds", ClampMin = "0.01"))
	float BaseProcessSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility|Inventory", meta = (DisplayName = "InputInventory"))
	FSRFacilityInventorySpec InputInventory;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility|Inventory", meta = (DisplayName = "OutputInventory"))
	FSRFacilityInventorySpec OutputInventory;

	UPROPERTY()
	int32 InputCapacity = 8;

	UPROPERTY()
	int32 OutputCapacity = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "Effects"))
	TArray<FSRFacilityEffectSpec> Effects;
};
