#pragma once

#include "Assembly/SRAssemblyStructurePlacementPreview.h"
#include "Assembly/SRStructureBuildCatalog.h"
#include "CoreMinimal.h"
#include "UI/SRUITheme.h"
#include "SRStructureBuildPresentation.generated.h"

/** Compact, asset-free presentation consumed by one Build Dock card. */
USTRUCT(BlueprintType)
struct STARROVERS_API FSRStructureBuildCardPresentation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	FName StructureId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	FText RoleText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	FText MetadataText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	FText AvailabilityText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	ESRResourceFamily ResourceFamily = ESRResourceFamily::None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	ESRStructureBuildAvailability Availability = ESRStructureBuildAvailability::Available;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	bool bSelectable = false;
};

/** Expanded static information shown above the Build Dock cards. */
USTRUCT(BlueprintType)
struct STARROVERS_API FSRStructureBuildDetailPresentation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	FName StructureId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	FText Title;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	FText ClassificationText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	FText SpecificationText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	FText AvailabilityText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	FText AvailabilityDetailText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	ESRResourceFamily ResourceFamily = ESRResourceFamily::None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	ESRStructureBuildAvailability Availability = ESRStructureBuildAvailability::Available;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	bool bSelectable = false;
};

/** Actionable replacement for blank Build Dock catalog/filter states. */
USTRUCT(BlueprintType)
struct STARROVERS_API FSRStructureBuildEmptyStatePresentation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	bool bVisible = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	FText Title;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	FText ClassificationText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	FText DetailText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	FText ActionText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	FText BadgeText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	ESRUIVisualState VisualState = ESRUIVisualState::Neutral;
};

/** Current objective reduced to the single Build Dock entry that advances it. */
USTRUCT(BlueprintType)
struct STARROVERS_API FSRStructureBuildRecommendationContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	bool bActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	FName RecommendedStructureId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	int32 CurrentStep = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	int32 TotalSteps = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	FText ObjectiveText;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRStructureBuildRecommendationPresentation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	bool bVisible = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	FText BadgeText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	FText ReasonText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	ESRUIVisualState VisualState = ESRUIVisualState::Warning;
};

/** Static, truthful input -> operation -> output contract for a construction option. */
USTRUCT(BlueprintType)
struct STARROVERS_API FSRStructureBuildFlowPresentation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	bool bVisible = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	FText InputText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	FText ProcessText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	FText OutputText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	FText EffectText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	FText ToolTipText;
};

/** Three glanceable placement metrics backed by the authoritative world preview. */
USTRUCT(BlueprintType)
struct STARROVERS_API FSRStructureBuildPlacementPresentation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	FText TargetText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	FText FootprintText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	FText CapacityText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	FText DetailText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	ESRUIVisualState TargetVisualState = ESRUIVisualState::Info;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	ESRUIVisualState FootprintVisualState = ESRUIVisualState::Neutral;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Build Dock")
	ESRUIVisualState CapacityVisualState = ESRUIVisualState::Neutral;
};

/** Centralized labels and formatting for cards, details, tests, and Blueprint tooling. */
class STARROVERS_API FSRStructureBuildPresentationBuilder final
{
public:
	static FSRStructureBuildCardPresentation BuildCard(const FSRStructureBuildOption& BuildOption);
	static FSRStructureBuildDetailPresentation BuildDetail(const FSRStructureBuildOption& BuildOption);
	static FSRStructureBuildEmptyStatePresentation BuildEmptyState(
		int32 TotalOptionCount,
		int32 VisibleOptionCount,
		int32 SelectableVisibleOptionCount);
	static FSRStructureBuildRecommendationPresentation BuildRecommendation(
		const FSRStructureBuildOption& BuildOption,
		const FSRStructureBuildRecommendationContext& Context);
	static FSRStructureBuildFlowPresentation BuildFlow(const FSRStructureBuildOption& BuildOption);
	static FSRStructureBuildPlacementPresentation BuildPlacement(
		const FSRStructureBuildOption& BuildOption,
		const FSRStructurePlacementPreview* LivePreview);

	static FText GetRoleLabel(ESRStructureBuildRole Role);
	static FText GetFamilyLabel(ESRResourceFamily ResourceFamily);
	static FText GetRarityLabel(ESRFacilityRarity Rarity);
	static FText GetPriorityLabel(ESROperationalPriorityV2 Priority);
	static FText GetAvailabilityLabel(ESRStructureBuildAvailability Availability);
};
