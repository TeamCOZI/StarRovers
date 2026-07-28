#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityDataAsset.h"
#include "Structure/SRStructureDataAsset.h"
#include "SRStructureBuildCatalog.generated.h"

class USRAugmentSubsystem;

UENUM(BlueprintType)
enum class ESRStructureBuildAvailability : uint8
{
	Available UMETA(DisplayName = "Available"),
	LockedByAugment UMETA(DisplayName = "Locked by Augment"),
	ConstructionDisabled UMETA(DisplayName = "Construction Disabled"),
};

UENUM(BlueprintType)
enum class ESRStructureBuildBlockReason : uint8
{
	None UMETA(DisplayName = "None"),
	RequiresAugment UMETA(DisplayName = "Requires Augment"),
	ConstructionDisabled UMETA(DisplayName = "Construction Disabled"),
};

UENUM(BlueprintType)
enum class ESRStructureBuildRole : uint8
{
	General UMETA(DisplayName = "General"),
	Logistics UMETA(DisplayName = "Logistics"),
	Extraction UMETA(DisplayName = "Extraction"),
	FamilyProcessing UMETA(DisplayName = "Family Processing"),
	TagProcessing UMETA(DisplayName = "Tag Processing"),
	FuelImprinting UMETA(DisplayName = "Fuel Imprinting"),
	Synthesis UMETA(DisplayName = "Synthesis"),
	StellarFuelFabrication UMETA(DisplayName = "Stellar Fuel Fabrication"),
	Infrastructure UMETA(DisplayName = "Infrastructure"),
	Hub UMETA(DisplayName = "Hub"),
};

/**
 * Stable, UI-facing description of one configured construction option.
 *
 * bEnabled is retained for existing Blueprint compatibility. New code should
 * use IsSelectable(), Availability, and BlockReason so a locked entry can stay
 * visible without becoming buildable.
 */
USTRUCT(BlueprintType)
struct STARROVERS_API FSRStructureBuildOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly", meta = (DisplayName = "StructureId"))
	FName StructureId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly", meta = (DisplayName = "DisplayName"))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly", meta = (DisplayName = "Description"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly", meta = (DisplayName = "StructureDataAsset"))
	TObjectPtr<USRStructureDataAsset> StructureDataAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly", meta = (DisplayName = "bEnabled"))
	bool bEnabled = true;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Catalog")
	bool bUnlocked = true;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Catalog")
	ESRStructureBuildAvailability Availability = ESRStructureBuildAvailability::Available;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Catalog")
	ESRStructureBuildBlockReason BlockReason = ESRStructureBuildBlockReason::None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Catalog")
	FText BlockReasonText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Catalog")
	FText UnlockHintText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Catalog")
	ESRStructureBuildRole Role = ESRStructureBuildRole::General;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Catalog")
	ESRResourceFamily ResourceFamily = ESRResourceFamily::None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Catalog")
	ESRStructureBuildKind BuildKind = ESRStructureBuildKind::Structure;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Catalog")
	ESRFacilityRarity Rarity = ESRFacilityRarity::Starting;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Catalog")
	FName FacilityContentId = NAME_None;

	/** Cached Facility V2 contract used by Build Dock diagrams without re-reading assets every frame. */
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Catalog|Transform")
	ESRFacilityOperationKind OperationKind = ESRFacilityOperationKind::Process;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Catalog|Transform")
	ESRFacilityProcessRoleV2 ProcessRole = ESRFacilityProcessRoleV2::FamilyProcess;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Catalog|Transform")
	ESRFacilityLineRoleV2 LineRole = ESRFacilityLineRoleV2::None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Catalog|Transform")
	FName ProcessArchetype = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Catalog|Transform")
	ESRResourceFamilyAction FamilyAction = ESRResourceFamilyAction::None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Catalog|Transform")
	double FacilityEnergyDelta = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Catalog|Transform")
	FName ProcessTagId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Catalog|Transform")
	FName FuelImprintId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Catalog|Transform")
	ESRFacilitySynthesisRoleV2 SynthesisRole = ESRFacilitySynthesisRoleV2::None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Catalog")
	int32 FootprintCellsX = 1;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Catalog")
	int32 FootprintCellsY = 1;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Catalog")
	int32 OperationalLoad = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Catalog")
	ESROperationalPriorityV2 OperationalPriority = ESROperationalPriorityV2::Normal;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Catalog")
	float BaseProcessSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Catalog")
	int32 InputPortCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Catalog")
	int32 OutputPortCount = 0;

	bool IsSelectable() const
	{
		return bEnabled
			&& bUnlocked
			&& Availability == ESRStructureBuildAvailability::Available
			&& BlockReason == ESRStructureBuildBlockReason::None
			&& IsValid(StructureDataAsset.Get())
			&& !StructureId.IsNone();
	}
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRStructureBuildCatalog
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Catalog")
	TArray<FSRStructureBuildOption> BuildOptions;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Catalog|Diagnostics")
	int32 ConfiguredAssetCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Catalog|Diagnostics")
	int32 ExcludedInvalidAssetCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Catalog|Diagnostics")
	int32 ExcludedNaturalDepositCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Catalog|Diagnostics")
	int32 ExcludedDuplicateIdCount = 0;
};

/** Builds a deterministic catalog while keeping unlock evaluation outside UI widgets. */
class STARROVERS_API FSRStructureBuildCatalogBuilder final
{
public:
	static void BuildCatalog(
		const TArray<USRStructureDataAsset*>& ConfiguredStructureDataAssets,
		const USRAugmentSubsystem* AugmentSubsystem,
		FSRStructureBuildCatalog& OutCatalog);

	// Public for deterministic automation tests and editor validation tools.
	static bool TryBuildOption(
		USRStructureDataAsset* StructureDataAsset,
		bool bUnlocked,
		FSRStructureBuildOption& OutBuildOption);

	static ESRStructureBuildRole ResolveRole(const FSRStructureData& StructureData);
	static ESRResourceFamily ResolveResourceFamily(const FSRStructureData& StructureData);
	static FText BuildStatusText(const FSRStructureBuildOption& BuildOption);
	static FText BuildToolTipText(const FSRStructureBuildOption& BuildOption);
};
