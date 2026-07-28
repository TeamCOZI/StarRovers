#pragma once

#include "Assembly/SRStructureBuildCatalog.h"
#include "CoreMinimal.h"
#include "SRStructureBuildDockModel.generated.h"

/**
 * Primary navigation axis for the construction dock.
 *
 * All is the complete catalog, Shared contains structures without a Family
 * restriction, and the remaining entries are resource-line workspaces.
 */
UENUM(BlueprintType)
enum class ESRStructureBuildFamilyFilter : uint8
{
	All UMETA(DisplayName = "All"),
	Metal UMETA(DisplayName = "Metal"),
	Crystal UMETA(DisplayName = "Crystal"),
	Organic UMETA(DisplayName = "Organic"),
	Plasma UMETA(DisplayName = "Plasma"),
	Void UMETA(DisplayName = "Void"),
	Shared UMETA(DisplayName = "Shared"),
};

/** Runtime summary used to render one Family tab without re-reading assets. */
USTRUCT(BlueprintType)
struct STARROVERS_API FSRStructureBuildFamilyTab
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Build Dock")
	ESRStructureBuildFamilyFilter Filter = ESRStructureBuildFamilyFilter::All;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Build Dock")
	FText Label;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Build Dock")
	ESRResourceFamily ResourceFamily = ESRResourceFamily::None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Build Dock")
	int32 TotalOptionCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly|Build Dock")
	int32 SelectableOptionCount = 0;

	bool HasOptions() const
	{
		return TotalOptionCount > 0;
	}
};

/**
 * Pure Family-first filtering and deterministic ordering for Build Dock UI.
 * Unlock evaluation remains in FSRStructureBuildCatalogBuilder.
 */
class STARROVERS_API FSRStructureBuildDockModel final
{
public:
	static void BuildFamilyTabs(
		const TArray<FSRStructureBuildOption>& BuildOptions,
		TArray<FSRStructureBuildFamilyTab>& OutTabs);

	static void QueryOptions(
		const TArray<FSRStructureBuildOption>& BuildOptions,
		ESRStructureBuildFamilyFilter FamilyFilter,
		bool bIncludeSharedWorkflowOptions,
		TArray<FName>& OutStructureIds);

	static bool MatchesFamilyFilter(
		const FSRStructureBuildOption& BuildOption,
		ESRStructureBuildFamilyFilter FamilyFilter,
		bool bIncludeSharedWorkflowOptions);

	static ESRStructureBuildFamilyFilter ResolvePreferredFilter(
		const FSRStructureBuildOption& BuildOption);

	/** Resolves one stable, selectable option for Guidance and Build Dock recommendation badges. */
	static FName FindRecommendedOptionId(
		const TArray<FSRStructureBuildOption>& BuildOptions,
		ESRStructureBuildRole Role,
		ESRResourceFamily PreferredFamily = ESRResourceFamily::None);

	static ESRResourceFamily ResolveResourceFamily(
		ESRStructureBuildFamilyFilter FamilyFilter);

	static FText GetFilterLabel(ESRStructureBuildFamilyFilter FamilyFilter);

	static bool IsSharedWorkflowRole(ESRStructureBuildRole Role);
};
