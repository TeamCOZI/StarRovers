#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityDataAsset.h"
#include "SRResourceSystemContent.generated.h"

UENUM(BlueprintType)
enum class ESRProcessTagContentV2 : uint8
{
	None UMETA(DisplayName = "None"),
	Overtone UMETA(DisplayName = "Overtone"),
	Reclamation UMETA(DisplayName = "Reclamation"),
	Crosslink UMETA(DisplayName = "Crosslink"),
	LandingCharge UMETA(DisplayName = "Landing Charge"),
	PilgrimCharge UMETA(DisplayName = "Pilgrim Charge"),
};

UENUM(BlueprintType)
enum class ESRFuelImprintContentV2 : uint8
{
	None UMETA(DisplayName = "None"),
	TwinSeal UMETA(DisplayName = "Twin Seal"),
	ConvergenceSeal UMETA(DisplayName = "Convergence Seal"),
	FoundrySeal UMETA(DisplayName = "Foundry Seal"),
	PilgrimSeal UMETA(DisplayName = "Pilgrim Seal"),
	PrismaticCatalyst UMETA(DisplayName = "Prismatic Catalyst"),
};

UENUM(BlueprintType)
enum class ESRProcessTagTriggerV2 : uint8
{
	PositiveFamilyStateActivated UMETA(DisplayName = "Positive Family State Activated"),
	NegativeFamilyStateCleared UMETA(DisplayName = "Negative Family State Cleared"),
	ProcessArchetypeChanged UMETA(DisplayName = "Process Archetype Changed"),
	FirstEnergyChangeAfterImport UMETA(DisplayName = "First Energy Change After Import"),
	FirstValidProcessOutsideOrigin UMETA(DisplayName = "First Valid Process Outside Origin"),
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRProcessTagDefinitionV2
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Content")
	FName TagId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Content")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Content")
	ESRProcessTagTriggerV2 Trigger = ESRProcessTagTriggerV2::PositiveFamilyStateActivated;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Content")
	double EnergyDelta = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Content")
	int32 TriggerCount = 1;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRFuelImprintDefinitionV2
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Content")
	FName ImprintId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Content")
	FText DisplayName;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRReferenceResourceDefinitionV2
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Content")
	ESRResourceContentPresetV2 Preset = ESRResourceContentPresetV2::Custom;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Content")
	FName ResourceId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Content")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Content")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Content")
	ESRResourceFamily Family = ESRResourceFamily::None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Content")
	double SeedEnergy = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Content")
	ESRResourceSpectrum Spectrum = ESRResourceSpectrum::None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Content")
	int32 Grade = StarRovers::Resources::MinimumGrade;

	/** Finite units authored into every natural deposit of this resource. */
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Content", meta = (ClampMin = "1"))
	int32 DepositTotalAmount = 1;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRUtilityResourceDefinitionV2
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Content")
	ESRResourceContentPresetV2 Preset = ESRResourceContentPresetV2::Custom;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Content")
	FName ResourceId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Content")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Content")
	FText Description;

	/** Zero means this utility is fabricated and has no natural deposit. */
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Resource V2|Content", meta = (ClampMin = "0"))
	int32 DepositTotalAmount = 0;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRFacilityContentDefinitionV2
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility V2|Content")
	ESRFacilityContentPresetV2 Preset = ESRFacilityContentPresetV2::Custom;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility V2|Content")
	FName ContentId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility V2|Content")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility V2|Content")
	ESRFacilityOperationKind OperationKind = ESRFacilityOperationKind::Process;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility V2|Content")
	ESRFacilityProcessRoleV2 ProcessRole = ESRFacilityProcessRoleV2::FamilyProcess;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility V2|Content")
	ESRFacilityLineRoleV2 LineRole = ESRFacilityLineRoleV2::None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility V2|Content")
	ESRFacilitySynthesisRoleV2 SynthesisRole = ESRFacilitySynthesisRoleV2::None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility V2|Content")
	FName ProcessArchetype = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility V2|Content")
	ESRResourceFamily AcceptedFamily = ESRResourceFamily::None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility V2|Content")
	ESRResourceFamilyAction FamilyAction = ESRResourceFamilyAction::None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility V2|Content")
	double FacilityEnergyDelta = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility V2|Content")
	float CycleSeconds = 1.0f;

	// The example Line temperature. Runtime Family processing still reads the
	// actual Cell temperature so environment and inter-body routing remain relevant.
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility V2|Content")
	ESRFacilityTemperatureState ReferenceTemperature = ESRFacilityTemperatureState::Normal;

	// Capacity demand while this facility is actively processing. Idle facilities
	// never reserve Operational Capacity.
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility V2|Content")
	int32 OperationalLoad = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility V2|Content")
	ESROperationalPriorityV2 DefaultOperationalPriority = ESROperationalPriorityV2::Normal;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility V2|Content")
	FName DefaultPayloadId = NAME_None;
};

class STARROVERS_API FSRResourceSystemContent final
{
public:
	static FName GetProcessTagId(ESRProcessTagContentV2 ProcessTag);
	static FName GetFuelImprintId(ESRFuelImprintContentV2 FuelImprint);
	static FName GetUtilityResourceId(ESRResourceContentPresetV2 UtilityPreset);

	static bool TryGetProcessTagDefinition(FName TagId, FSRProcessTagDefinitionV2& OutDefinition);
	static bool TryGetFuelImprintDefinition(FName ImprintId, FSRFuelImprintDefinitionV2& OutDefinition);
	static bool TryGetReferenceResourceDefinition(
		ESRResourceContentPresetV2 Preset,
		FSRReferenceResourceDefinitionV2& OutDefinition);
	static bool TryGetReferenceResourceDefinition(
		FName ResourceId,
		FSRReferenceResourceDefinitionV2& OutDefinition);
	static bool TryGetUtilityResourceDefinition(
		ESRResourceContentPresetV2 Preset,
		FSRUtilityResourceDefinitionV2& OutDefinition);
	/** Returns false for fabricated resources and unsupported presets. */
	static bool TryGetDepositTotalAmount(
		ESRResourceContentPresetV2 Preset,
		int32& OutDepositTotalAmount);
	static bool TryGetFacilityDefinition(
		ESRFacilityContentPresetV2 Preset,
		FSRFacilityContentDefinitionV2& OutDefinition);
	// Process Tags are deliberately a narrow, additive combo layer. Keeping the
	// authoring policy beside the catalog prevents a future content entry from
	// becoming a passive multiplier, a permanent loop, or a strictly better copy
	// of another Tag without failing validation.
	static bool ValidateProcessTagDefinition(
		const FSRProcessTagDefinitionV2& Definition,
		FString& OutFailureReason);
	static bool ValidateProcessTagCatalog(FString& OutFailureReason);

	static void GetAllProcessTagDefinitions(TArray<FSRProcessTagDefinitionV2>& OutDefinitions);
	static void GetAllFuelImprintDefinitions(TArray<FSRFuelImprintDefinitionV2>& OutDefinitions);
	static void GetAllReferenceResourceDefinitions(TArray<FSRReferenceResourceDefinitionV2>& OutDefinitions);
	static void GetAllUtilityResourceDefinitions(TArray<FSRUtilityResourceDefinitionV2>& OutDefinitions);
	static void GetAllFacilityDefinitions(TArray<FSRFacilityContentDefinitionV2>& OutDefinitions);

	static bool ApplyResourcePreset(USRResourceDataAsset& ResourceDataAsset, ESRResourceContentPresetV2 Preset);
	static bool ApplyFacilityPreset(USRFacilityDataAsset& FacilityDataAsset, ESRFacilityContentPresetV2 Preset);
	static bool MakeReferenceResourceInstance(
		ESRResourceContentPresetV2 Preset,
		FName OriginBodyId,
		FSRResourceInstance& OutResourceInstance);
	static bool MakeReferenceStellarFuelBatch(
		ESRStellarFuelReferenceTopologyV2 Topology,
		FName FabricatorBodyId,
		TArray<FSRResourceInstance>& OutCards);
};
