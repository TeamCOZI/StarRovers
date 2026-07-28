#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SRResourceDataAsset.generated.h"

class USRResourceDataAsset;

namespace StarRovers::Resources
{
	inline constexpr int32 LegacyResourceSchemaVersion = 1;
	inline constexpr int32 InitialResourceV2SchemaVersion = 2;
	inline constexpr int32 CurrentResourceSchemaVersion = 3;
	inline constexpr int32 LegacyResourceDefinitionVersion = 1;
	inline constexpr int32 CurrentResourceDefinitionVersion = 2;
	inline constexpr int32 MinimumGrade = 1;
	inline constexpr int32 MaximumGrade = 5;
}

UENUM(BlueprintType)
enum class ESRResourceClass : uint8
{
	Unknown UMETA(DisplayName = "Unknown"),
	Card UMETA(DisplayName = "Card"),
	Utility UMETA(DisplayName = "Utility"),
	Waste UMETA(DisplayName = "Waste"),
	StellarFuel UMETA(DisplayName = "Stellar Fuel"),
};

UENUM(BlueprintType)
enum class ESRResourceFamily : uint8
{
	None UMETA(DisplayName = "None"),
	Metal UMETA(DisplayName = "Metal"),
	Crystal UMETA(DisplayName = "Crystal"),
	Organic UMETA(DisplayName = "Organic"),
	Plasma UMETA(DisplayName = "Plasma"),
	Void UMETA(DisplayName = "Void"),
};

// Authored reference cards from the Resource V2 vertical slice. Custom keeps existing
// assets opt-in and prevents PostLoad from silently rewriting Legacy definitions.
UENUM(BlueprintType)
enum class ESRResourceContentPresetV2 : uint8
{
	Custom UMETA(DisplayName = "Custom"),
	HeliosIron UMETA(DisplayName = "Helios Iron"),
	EchoQuartz UMETA(DisplayName = "Echo Quartz"),
	VerdantSpore UMETA(DisplayName = "Verdant Spore"),
	AuroraPlasma UMETA(DisplayName = "Aurora Plasma"),
	NullPearl UMETA(DisplayName = "Null Pearl"),
	CommonOre UMETA(DisplayName = "Common Ore"),
	BiomassFeedstock UMETA(DisplayName = "Biomass Feedstock"),
	IndustrialSupply UMETA(DisplayName = "Industrial Supply"),
};

UENUM(BlueprintType)
enum class ESRResourceSpectrum : uint8
{
	None UMETA(DisplayName = "None"),
	Red UMETA(DisplayName = "Red"),
	Green UMETA(DisplayName = "Green"),
	Blue UMETA(DisplayName = "Blue"),
	Yellow UMETA(DisplayName = "Yellow"),
};

// Values are bit indices for ActiveFamilyStateFlags, not mask values.
UENUM(BlueprintType, meta = (Bitflags))
enum class ESRResourceFamilyState : uint8
{
	Tempered UMETA(DisplayName = "Tempered"),
	Fatigued UMETA(DisplayName = "Fatigued"),
	Resonant UMETA(DisplayName = "Resonant"),
	Fractured UMETA(DisplayName = "Fractured"),
	Matured UMETA(DisplayName = "Matured"),
	Depleted UMETA(DisplayName = "Depleted"),
	Energized UMETA(DisplayName = "Energized"),
	Overloaded UMETA(DisplayName = "Overloaded"),
	Echoing UMETA(DisplayName = "Echoing"),
	Collapsed UMETA(DisplayName = "Collapsed"),
};

UENUM(BlueprintType)
enum class ESRResourceProcessTemperatureState : uint8
{
	None UMETA(DisplayName = "None"),
	Frozen UMETA(DisplayName = "Frozen"),
	Cold UMETA(DisplayName = "Cold"),
	Normal UMETA(DisplayName = "Normal"),
	Hot UMETA(DisplayName = "Hot"),
	Overheated UMETA(DisplayName = "Overheated"),
};

UENUM(BlueprintType)
enum class ESRResourceFamilyAction : uint8
{
	None UMETA(DisplayName = "None"),
	Growth UMETA(DisplayName = "Growth"),
	Amplification UMETA(DisplayName = "Amplification"),
	Discharge UMETA(DisplayName = "Discharge"),
	VoidSacrifice UMETA(DisplayName = "Void Sacrifice"),
	EnergyGain UMETA(DisplayName = "Energy Gain"),
	// Appended to preserve serialized values of the existing actions.
	Anneal UMETA(DisplayName = "Anneal"),
};

UENUM(BlueprintType)
enum class ESRResourceSlotLifecycle : uint8
{
	Empty UMETA(DisplayName = "Empty"),
	Primed UMETA(DisplayName = "Primed"),
	Spent UMETA(DisplayName = "Spent"),
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRResourceProcessTagSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Process Tag")
	FName TagId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Process Tag")
	ESRResourceSlotLifecycle Lifecycle = ESRResourceSlotLifecycle::Empty;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Process Tag", meta = (ClampMin = "0"))
	int32 RemainingTriggers = 0;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRResourceFuelImprintSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Fuel Imprint")
	FName ImprintId = NAME_None;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRResourceProcessingMemory
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Processing Memory")
	FName LastProcessArchetype = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Processing Memory")
	ESRResourceProcessTemperatureState LastTemperature = ESRResourceProcessTemperatureState::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Processing Memory")
	ESRResourceFamilyAction LastFamilyAction = ESRResourceFamilyAction::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Processing Memory", meta = (ClampMin = "0"))
	int32 ConsecutiveSameArchetypeCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Processing Memory", meta = (ClampMin = "0"))
	int32 ConsecutiveSameFamilyActionCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Processing Memory", meta = (ClampMin = "0"))
	int32 GeneralProcessesSinceReset = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Processing Memory", meta = (ClampMin = "0"))
	double StoredFamilyMagnitude = 0.0;

	// Used by Landing Charge to distinguish a new import from an import that has
	// already received an Energy-changing Family process.
	UPROPERTY()
	int32 TransitCountAtLastEnergyChange = 0;

	// Diagnostic counters are intentionally hidden from the normal resource card UI.
	UPROPERTY()
	int32 ProcessCount = 0;

	UPROPERTY()
	int32 EnergyChangeCount = 0;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRResourceLogisticsMetadata
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Logistics")
	FName OriginBodyId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Logistics")
	FName LastProcessedBodyId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Logistics")
	FName LastTransitSourceBodyId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Logistics")
	FName LastTransitDestinationBodyId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Logistics", meta = (ClampMin = "0"))
	int32 TransitCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2|Logistics")
	bool bHasBeenProcessedOutsideOrigin = false;
};

UENUM(BlueprintType)
enum class ESRResourceProcessTag : uint8
{
	Responsive = 0 UMETA(DisplayName = "HeatResponsive"),
	HalfLife = 2 UMETA(DisplayName = "HalfLife"),
	Volatile = 3 UMETA(DisplayName = "Volatile"),
	Supercooled = 5 UMETA(DisplayName = "Supercooled"),
	HyperReactive = 6 UMETA(DisplayName = "HyperReactive"),
	Charge = 7 UMETA(DisplayName = "Charge"),
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRResourceTagStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource", meta = (DisplayName = "Tag"))
	ESRResourceProcessTag Tag = ESRResourceProcessTag::Responsive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource", meta = (DisplayName = "StackCount", ClampMin = "1"))
	int32 StackCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource", meta = (DisplayName = "RemainingCycles", ClampMin = "0"))
	int32 RemainingCycles = 0;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRResourceInstance
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Resource V2|Migration")
	int32 ResourceSchemaVersion = StarRovers::Resources::CurrentResourceSchemaVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource", meta = (DisplayName = "ResourceInstanceId"))
	FName ResourceInstanceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource", meta = (DisplayName = "ResourceDataAsset"))
	TObjectPtr<USRResourceDataAsset> ResourceDataAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource", meta = (DisplayName = "ResourceId"))
	FName ResourceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource|Legacy", meta = (
		DisplayName = "EnergyValue (Legacy)",
		AdvancedDisplay,
		ToolTip = "Legacy runtime energy. Resource V2 uses Current Energy; this field remains active while the Legacy ruleset is selected."))
	double EnergyValue = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource|Legacy", meta = (
		DisplayName = "RemainingProcessLimit (Legacy)",
		ClampMin = "0",
		AdvancedDisplay))
	int32 RemainingProcessLimit = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource|Legacy", meta = (
		DisplayName = "ProcessCount (Legacy)",
		ClampMin = "0",
		AdvancedDisplay))
	int32 ProcessCount = 0;

	// Internal stack used by facility effects. This is intentionally not exposed to Blueprint/UI.
	UPROPERTY()
	int32 EnergyChangeCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource|Legacy", meta = (
		DisplayName = "Tags (Legacy)",
		AdvancedDisplay,
		ToolTip = "Legacy multi-tag array. Resource V2 uses one Process Tag slot and one Fuel Imprint slot."))
	TArray<FSRResourceTagStack> Tags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2")
	ESRResourceClass ResourceClass = ESRResourceClass::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2")
	ESRResourceFamily Family = ESRResourceFamily::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2", meta = (DisplayName = "Current Energy"))
	double CurrentEnergy = 0.0;

	// Captured once when the resource is created or migrated. Refinement rules
	// must not change when a Data Asset is rebalanced later in the Run.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Resource V2", meta = (DisplayName = "Seed Energy Snapshot", AdvancedDisplay))
	double SeedEnergySnapshot = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Resource V2", meta = (DisplayName = "Has Seed Energy Snapshot", AdvancedDisplay))
	bool bHasSeedEnergySnapshot = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2")
	ESRResourceSpectrum Spectrum = ESRResourceSpectrum::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2", meta = (
		ClampMin = "1",
		ClampMax = "5",
		UIMin = "1",
		UIMax = "5"))
	int32 Grade = StarRovers::Resources::MinimumGrade;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2", meta = (
		Bitmask,
		BitmaskEnum = "/Script/StarRovers.ESRResourceFamilyState"))
	int32 ActiveFamilyStateFlags = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2")
	FSRResourceProcessTagSlot ProcessTagSlot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2")
	FSRResourceFuelImprintSlot FuelImprintSlot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2")
	FSRResourceProcessingMemory ProcessingMemory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource V2")
	FSRResourceLogisticsMetadata LogisticsMetadata;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Resource", meta = (DisplayName = "StackCount", ClampMin = "1"))
	int32 StackCount = 1;
};

UCLASS(BlueprintType)
class STARROVERS_API USRResourceDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	USRResourceDataAsset();

	UFUNCTION(BlueprintPure, Category = "StarRovers|Resource")
	FSRResourceInstance BuildDefaultInstance() const;

	// Explicit authoring action. Selecting a preset alone never mutates an asset.
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "StarRovers|Resource V2|Authoring")
	void ApplyResourceV2Preset();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity", meta = (DisplayName = "ResourceId"))
	FName ResourceId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity", meta = (DisplayName = "DisplayName"))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Identity", meta = (DisplayName = "Description"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Resource V2|Migration", meta = (
		DisplayName = "Resource Definition Version",
		ClampMin = "1",
		ClampMax = "2",
		ToolTip = "Existing assets remain version 1. Set newly-authored Resource V2 assets to version 2."))
	int32 ResourceDefinitionVersion = StarRovers::Resources::LegacyResourceDefinitionVersion;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Resource V2|Authoring", meta = (
		DisplayName = "Resource V2 Preset",
		ToolTip = "Choose a reference Card or Utility resource, then run Apply Resource V2 Preset. Custom leaves all fields unchanged."))
	ESRResourceContentPresetV2 ResourceV2Preset = ESRResourceContentPresetV2::Custom;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Resource V2", meta = (
		EditCondition = "ResourceDefinitionVersion >= 2",
		EditConditionHides))
	ESRResourceClass ResourceClass = ESRResourceClass::Card;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Resource V2", meta = (
		EditCondition = "ResourceDefinitionVersion >= 2",
		EditConditionHides))
	ESRResourceFamily Family = ESRResourceFamily::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Resource V2", meta = (
		DisplayName = "Seed Energy",
		EditCondition = "ResourceDefinitionVersion >= 2",
		EditConditionHides))
	double SeedEnergy = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Resource V2", meta = (
		DisplayName = "Native Spectrum",
		EditCondition = "ResourceDefinitionVersion >= 2 && ResourceClass == ESRResourceClass::Card",
		EditConditionHides))
	ESRResourceSpectrum NativeSpectrum = ESRResourceSpectrum::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Resource V2", meta = (
		DisplayName = "Native Grade",
		ClampMin = "1",
		ClampMax = "5",
		UIMin = "1",
		UIMax = "5",
		EditCondition = "ResourceDefinitionVersion >= 2 && ResourceClass == ESRResourceClass::Card",
		EditConditionHides))
	int32 NativeGrade = StarRovers::Resources::MinimumGrade;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Resource|Legacy", meta = (
		DisplayName = "BaseEnergyValue (Legacy)",
		AdvancedDisplay))
	double BaseEnergyValue = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Resource|Legacy", meta = (
		DisplayName = "BaseProcessLimit (Legacy)",
		ClampMin = "0",
		AdvancedDisplay))
	int32 BaseProcessLimit = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Resource|Legacy", meta = (
		DisplayName = "DefaultTags (Legacy)",
		AdvancedDisplay))
	TArray<FSRResourceTagStack> DefaultTags;
};
