#pragma once

#include "CoreMinimal.h"
#include "Automation/SRResourceDataAsset.h"
#include "Automation/SROperationalCapacityTypes.h"
#include "Automation/SRStellarFuelTypes.h"
#include "Engine/DataAsset.h"
#include "SRFacilityDataAsset.generated.h"

namespace StarRovers::Facilities
{
	inline constexpr int32 LegacyFacilityDefinitionVersion = 1;
	inline constexpr int32 CurrentFacilityDefinitionVersion = 2;
}

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
enum class ESRFacilityProcessRoleV2 : uint8
{
	FamilyProcess UMETA(DisplayName = "Family Process"),
	ApplyProcessTag UMETA(DisplayName = "Apply Process Tag"),
	ApplyFuelImprint UMETA(DisplayName = "Apply Fuel Imprint"),
	ClearProcessTag UMETA(DisplayName = "Clear Process Tag"),
};

/**
 * The glanceable job a processing facility performs inside a Family line.
 * This is presentation and balance metadata; Family State authority remains in
 * the processing Kernel.
 */
UENUM(BlueprintType)
enum class ESRFacilityLineRoleV2 : uint8
{
	None UMETA(DisplayName = "None"),
	UniversalBridge UMETA(DisplayName = "Universal Bridge"),
	Primer UMETA(DisplayName = "Primer"),
	Payoff UMETA(DisplayName = "Payoff"),
	Repeater UMETA(DisplayName = "Repeater"),
	Recovery UMETA(DisplayName = "Recovery"),
	Burst UMETA(DisplayName = "Burst"),
	Stabilizer UMETA(DisplayName = "Stabilizer"),
	Sacrifice UMETA(DisplayName = "Sacrifice"),
};

UENUM(BlueprintType)
enum class ESRFacilitySynthesisRoleV2 : uint8
{
	None UMETA(DisplayName = "None"),
	StellarFuelFabricator UMETA(DisplayName = "Stellar Fuel Fabricator"),
	IndustrialSupplyFabricator UMETA(DisplayName = "Industrial Supply Fabricator"),
	ServiceCore UMETA(DisplayName = "Service Core"),
	FleetBerth UMETA(DisplayName = "Fleet Berth"),
};

// Resource V2 reference content. Mining and later infrastructure systems keep
// their dedicated migration phases.
UENUM(BlueprintType)
enum class ESRFacilityContentPresetV2 : uint8
{
	Custom UMETA(DisplayName = "Custom"),
	PulseProcessor UMETA(DisplayName = "Pulse Processor"),
	CompressionMill UMETA(DisplayName = "Compression Mill"),
	TagImprinter UMETA(DisplayName = "Tag Imprinter"),
	FuelImprinter UMETA(DisplayName = "Fuel Imprinter"),
	TagScrubber UMETA(DisplayName = "Tag Scrubber"),
	InductionForge UMETA(DisplayName = "Induction Forge"),
	CryoPress UMETA(DisplayName = "Cryo Press"),
	ResonanceMill UMETA(DisplayName = "Resonance Mill"),
	FacetShifter UMETA(DisplayName = "Facet Shifter"),
	GrowthVat UMETA(DisplayName = "Growth Vat"),
	EnzymeLoom UMETA(DisplayName = "Enzyme Loom"),
	SporePress UMETA(DisplayName = "Spore Press"),
	ArcAmplifier UMETA(DisplayName = "Arc Amplifier"),
	GroundingCoil UMETA(DisplayName = "Grounding Coil"),
	NullSink UMETA(DisplayName = "Null Sink"),
	EchoChamber UMETA(DisplayName = "Echo Chamber"),
	StellarFuelFabricator UMETA(DisplayName = "Stellar Fuel Fabricator"),
	SupplyFabricator UMETA(DisplayName = "Supply Fabricator"),
	ServiceCore UMETA(DisplayName = "Service Core"),
	FleetBerth UMETA(DisplayName = "Fleet Berth"),
	// Appended to preserve serialized values of the existing presets.
	AnnealingChamber UMETA(DisplayName = "Annealing Chamber"),
};

UENUM(BlueprintType)
enum class ESRFacilityEffectKind : uint8
{
	// The C++ identifier is retained for existing serialized Data Assets. Runtime semantics are AdjustCatalyst.
	AdjustEnergy UMETA(DisplayName = "AdjustCatalyst"),
	AdjustProcessLimit UMETA(DisplayName = "AdjustProcessLimit"),
	RemoveResource UMETA(DisplayName = "RemoveResource"),
	AttachTag UMETA(DisplayName = "AttachTag"),
	ProduceWaste UMETA(DisplayName = "ProduceWaste"),
	AdjustCellTemperature UMETA(DisplayName = "AdjustCellTemperature"),
	InvertHeat UMETA(DisplayName = "InvertHeat"),
	InvertTagEffects UMETA(DisplayName = "InvertCatalyst"),
	DoubleTagEffects UMETA(DisplayName = "DoubleCatalyst"),
	DuplicateInputResource UMETA(DisplayName = "DuplicateInputResource"),
	OverrideProcessTemperature UMETA(DisplayName = "OverrideProcessTemperature"),
	TriggerTagEffect UMETA(DisplayName = "TriggerTagEffect"),
	AdjustProcessTime UMETA(DisplayName = "AdjustProcessTime"),
	RemoveTag UMETA(DisplayName = "RemoveTag"),
	ChangeResourceType UMETA(DisplayName = "ChangeResourceType"),
	TransferTagsToWaste UMETA(DisplayName = "TransferTagsToWaste"),
};

UENUM(BlueprintType)
enum class ESRFacilityAttachTagSource : uint8
{
	SpecificTag UMETA(DisplayName = "SpecificTag"),
	LastAttachedTag UMETA(DisplayName = "LastAttachedTag"),
	MissingTags UMETA(DisplayName = "MissingTags"),
	AttachedTags UMETA(DisplayName = "AttachedTags"),
};

UENUM(BlueprintType)
enum class ESRFacilityEffectTagTarget : uint8
{
	SpecificTag UMETA(DisplayName = "SpecificTag"),
	LastAttachedTag UMETA(DisplayName = "LastAttachedTag"),
	AllTags UMETA(DisplayName = "AttachedTags"),
};

UENUM(BlueprintType)
enum class ESRFacilityRemoveTagAmountMode : uint8
{
	All UMETA(DisplayName = "All"),
	Count UMETA(DisplayName = "Count"),
};

UENUM(BlueprintType)
enum class ESRFacilityEnergyAdjustmentValueSource : uint8
{
	FixedValue UMETA(DisplayName = "FixedValue"),
	RemainingProcessLimit UMETA(DisplayName = "RemainingProcessLimit"),
	TagStackCount UMETA(DisplayName = "TagStackCount"),
	EnergyChangeCount UMETA(DisplayName = "EnergyChangeCount"),
	TagEffectEnergyChangeAmount UMETA(DisplayName = "TagEffectEnergyChangeAmount"),
	ProcessCount UMETA(DisplayName = "ProcessCount"),
	TagKindCount UMETA(DisplayName = "TagKindCount"),
};

UENUM(BlueprintType)
enum class ESRFacilityTagStackCountTarget : uint8
{
	SpecificTag UMETA(DisplayName = "SpecificTag"),
	All UMETA(DisplayName = "All"),
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
	Multiply UMETA(DisplayName = "Multiply"),
};

UENUM(BlueprintType)
enum class ESRFacilityProcessTimeAdjustmentMode : uint8
{
	AddSeconds UMETA(DisplayName = "AddSeconds"),
	Multiply UMETA(DisplayName = "Multiply"),
};

UENUM(BlueprintType)
enum class ESRFacilityProcessTimeAdjustmentValueSource : uint8
{
	FixedValue UMETA(DisplayName = "FixedValue"),
	TagStackCount UMETA(DisplayName = "TagStackCount"),
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
		EditCondition = "(EffectKind == ESRFacilityEffectKind::AdjustEnergy && EnergyValueSource == ESRFacilityEnergyAdjustmentValueSource::FixedValue) || EffectKind == ESRFacilityEffectKind::AdjustProcessLimit || (EffectKind == ESRFacilityEffectKind::AdjustProcessTime && ProcessTimeValueSource == ESRFacilityProcessTimeAdjustmentValueSource::FixedValue)",
		EditConditionHides))
	double Value = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (
		DisplayName = "Count",
		ClampMin = "1",
		EditCondition = "(EffectKind == ESRFacilityEffectKind::AttachTag && (AttachTagSource == ESRFacilityAttachTagSource::SpecificTag || AttachTagSource == ESRFacilityAttachTagSource::LastAttachedTag)) || EffectKind == ESRFacilityEffectKind::ProduceWaste || EffectKind == ESRFacilityEffectKind::DuplicateInputResource || (EffectKind == ESRFacilityEffectKind::RemoveTag && RemoveTagAmountMode == ESRFacilityRemoveTagAmountMode::Count)",
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
		DisplayName = "ProcessTimeValueSource",
		EditCondition = "EffectKind == ESRFacilityEffectKind::AdjustProcessTime",
		EditConditionHides))
	ESRFacilityProcessTimeAdjustmentValueSource ProcessTimeValueSource = ESRFacilityProcessTimeAdjustmentValueSource::FixedValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (
		DisplayName = "TagStackCountTarget",
		EditCondition = "(EffectKind == ESRFacilityEffectKind::AdjustEnergy && EnergyValueSource == ESRFacilityEnergyAdjustmentValueSource::TagStackCount) || (EffectKind == ESRFacilityEffectKind::AdjustProcessTime && ProcessTimeValueSource == ESRFacilityProcessTimeAdjustmentValueSource::TagStackCount)",
		EditConditionHides))
	ESRFacilityTagStackCountTarget TagStackCountTarget = ESRFacilityTagStackCountTarget::SpecificTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (
		DisplayName = "ResourceTag",
		EditCondition = "(EffectKind == ESRFacilityEffectKind::AttachTag && AttachTagSource == ESRFacilityAttachTagSource::SpecificTag) || (EffectKind == ESRFacilityEffectKind::AdjustEnergy && EnergyValueSource == ESRFacilityEnergyAdjustmentValueSource::TagStackCount && TagStackCountTarget == ESRFacilityTagStackCountTarget::SpecificTag) || (EffectKind == ESRFacilityEffectKind::AdjustProcessTime && ProcessTimeValueSource == ESRFacilityProcessTimeAdjustmentValueSource::TagStackCount && TagStackCountTarget == ESRFacilityTagStackCountTarget::SpecificTag) || ((EffectKind == ESRFacilityEffectKind::TriggerTagEffect || EffectKind == ESRFacilityEffectKind::RemoveTag || EffectKind == ESRFacilityEffectKind::TransferTagsToWaste) && TagTarget == ESRFacilityEffectTagTarget::SpecificTag)",
		EditConditionHides))
	ESRResourceProcessTag ResourceTag = ESRResourceProcessTag::Responsive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (
		DisplayName = "ProducedWasteResource",
		EditCondition = "EffectKind == ESRFacilityEffectKind::ProduceWaste || EffectKind == ESRFacilityEffectKind::TransferTagsToWaste",
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
		EditCondition = "EffectKind == ESRFacilityEffectKind::TriggerTagEffect || EffectKind == ESRFacilityEffectKind::RemoveTag || EffectKind == ESRFacilityEffectKind::TransferTagsToWaste",
		EditConditionHides))
	ESRFacilityEffectTagTarget TagTarget = ESRFacilityEffectTagTarget::SpecificTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (
		DisplayName = "RemoveTagAmountMode",
		EditCondition = "EffectKind == ESRFacilityEffectKind::RemoveTag",
		EditConditionHides))
	ESRFacilityRemoveTagAmountMode RemoveTagAmountMode = ESRFacilityRemoveTagAmountMode::All;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (
		DisplayName = "CatalystValueSource",
		EditCondition = "EffectKind == ESRFacilityEffectKind::AdjustEnergy",
		EditConditionHides))
	ESRFacilityEnergyAdjustmentValueSource EnergyValueSource = ESRFacilityEnergyAdjustmentValueSource::FixedValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (
		DisplayName = "CatalystValueMultiplier",
		EditCondition = "EffectKind == ESRFacilityEffectKind::AdjustEnergy && (EnergyValueSource == ESRFacilityEnergyAdjustmentValueSource::RemainingProcessLimit || EnergyValueSource == ESRFacilityEnergyAdjustmentValueSource::TagStackCount || EnergyValueSource == ESRFacilityEnergyAdjustmentValueSource::EnergyChangeCount || EnergyValueSource == ESRFacilityEnergyAdjustmentValueSource::TagEffectEnergyChangeAmount || EnergyValueSource == ESRFacilityEnergyAdjustmentValueSource::ProcessCount || EnergyValueSource == ESRFacilityEnergyAdjustmentValueSource::TagKindCount)",
		EditConditionHides))
	double EnergyValueMultiplier = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility", meta = (
		DisplayName = "CatalystAdjustmentMode",
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

USTRUCT(BlueprintType)
struct STARROVERS_API FSRFacilityProcessDefinitionV2
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility V2|Process")
	ESRFacilityProcessRoleV2 ProcessRole = ESRFacilityProcessRoleV2::FamilyProcess;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility V2|Process", meta = (
		EditCondition = "ProcessRole == ESRFacilityProcessRoleV2::FamilyProcess",
		EditConditionHides))
	ESRFacilityLineRoleV2 LineRole = ESRFacilityLineRoleV2::None;

	// Stable mechanical identity. Repeating the same archetype drives Family history such as Metal Fatigue.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility V2|Process")
	FName ProcessArchetype = NAME_None;

	// None means that any Card Family may enter. Family-specific facilities set this explicitly.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility V2|Process")
	ESRResourceFamily AcceptedFamily = ESRResourceFamily::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility V2|Process")
	ESRResourceFamilyAction FamilyAction = ESRResourceFamilyAction::None;

	// Normal processing is additive. Multipliers are reserved for the Stellar Fuel Fabricator.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility V2|Process", meta = (DisplayName = "Facility Energy Delta"))
	double FacilityEnergyDelta = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility V2|Process", meta = (
		EditCondition = "ProcessRole == ESRFacilityProcessRoleV2::ApplyProcessTag",
		EditConditionHides))
	FName ProcessTagId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility V2|Process", meta = (
		EditCondition = "ProcessRole == ESRFacilityProcessRoleV2::ApplyFuelImprint",
		EditConditionHides))
	FName FuelImprintId = NAME_None;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRFacilitySynthesisDefinitionV2
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility V2|Synthesis")
	ESRFacilitySynthesisRoleV2 SynthesisRole = ESRFacilitySynthesisRoleV2::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Facility V2|Synthesis", meta = (
		EditCondition = "SynthesisRole == ESRFacilitySynthesisRoleV2::StellarFuelFabricator",
		EditConditionHides))
	FSRStellarFuelFabricationRulesV2 StellarFuelRules;
};

UCLASS(BlueprintType)
class STARROVERS_API USRFacilityDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	USRFacilityDataAsset();

	virtual void PostLoad() override;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Facility V2")
	bool UsesResourceV2Definition() const;

	// Explicit authoring action. Selecting a preset alone never mutates an asset.
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "StarRovers|Facility V2|Authoring")
	void ApplyResourceV2Preset();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "FacilityKind"))
	ESRFacilityKind FacilityKind = ESRFacilityKind::Standard;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "Rarity", EditCondition = "FacilityKind == ESRFacilityKind::Standard", EditConditionHides))
	ESRFacilityRarity Rarity = ESRFacilityRarity::Basic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "OperationKind", EditCondition = "FacilityKind == ESRFacilityKind::Standard", EditConditionHides))
	ESRFacilityOperationKind OperationKind = ESRFacilityOperationKind::Process;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "BaseProcessSeconds", ClampMin = "0.01"))
	float BaseProcessSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility V2|Operational Capacity", meta = (
		DisplayName = "Operational Load",
		ClampMin = "0",
		EditCondition = "FacilityDefinitionVersion >= 2",
		EditConditionHides))
	int32 OperationalLoad = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility V2|Operational Capacity", meta = (
		DisplayName = "Default Operational Priority",
		EditCondition = "FacilityDefinitionVersion >= 2",
		EditConditionHides))
	ESROperationalPriorityV2 DefaultOperationalPriority = ESROperationalPriorityV2::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility V2|Migration", meta = (
		DisplayName = "Facility Definition Version",
		ClampMin = "1",
		ClampMax = "2",
		ToolTip = "Existing assets remain version 1. Set a migrated Resource V2 processing facility to version 2."))
	int32 FacilityDefinitionVersion = StarRovers::Facilities::LegacyFacilityDefinitionVersion;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility V2|Authoring", meta = (
		DisplayName = "Facility V2 Preset",
		ToolTip = "Choose a Resource V2 processing, synthesis, or infrastructure preset, then run Apply Resource V2 Preset. Custom leaves all fields unchanged."))
	ESRFacilityContentPresetV2 ResourceV2Preset = ESRFacilityContentPresetV2::Custom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility V2|Authoring")
	FName ResourceV2ContentId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility V2", meta = (
		DisplayName = "Resource V2 Process",
		EditCondition = "FacilityDefinitionVersion >= 2 && FacilityKind == ESRFacilityKind::Standard && OperationKind == ESRFacilityOperationKind::Process",
		EditConditionHides))
	FSRFacilityProcessDefinitionV2 ResourceV2Process;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Facility V2", meta = (
		DisplayName = "Resource V2 Synthesis",
		EditCondition = "FacilityDefinitionVersion >= 2 && FacilityKind == ESRFacilityKind::Standard && OperationKind == ESRFacilityOperationKind::Synthesize",
		EditConditionHides))
	FSRFacilitySynthesisDefinitionV2 ResourceV2Synthesis;

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
