#include "Assembly/SRStructureBuildDockModel.h"

namespace
{
	const TArray<ESRStructureBuildFamilyFilter>& GetOrderedFamilyFilters()
	{
		static const TArray<ESRStructureBuildFamilyFilter> Filters = {
			ESRStructureBuildFamilyFilter::All,
			ESRStructureBuildFamilyFilter::Metal,
			ESRStructureBuildFamilyFilter::Crystal,
			ESRStructureBuildFamilyFilter::Organic,
			ESRStructureBuildFamilyFilter::Plasma,
			ESRStructureBuildFamilyFilter::Void,
			ESRStructureBuildFamilyFilter::Shared,
		};
		return Filters;
	}

	int32 GetRoleSortRank(ESRStructureBuildRole Role)
	{
		switch (Role)
		{
		case ESRStructureBuildRole::Extraction: return 0;
		case ESRStructureBuildRole::Logistics: return 1;
		case ESRStructureBuildRole::FamilyProcessing: return 2;
		case ESRStructureBuildRole::TagProcessing: return 3;
		case ESRStructureBuildRole::FuelImprinting: return 4;
		case ESRStructureBuildRole::Synthesis: return 5;
		case ESRStructureBuildRole::StellarFuelFabrication: return 6;
		case ESRStructureBuildRole::Infrastructure: return 7;
		case ESRStructureBuildRole::Hub: return 8;
		case ESRStructureBuildRole::General:
		default: return 9;
		}
	}

	int32 GetRaritySortRank(ESRFacilityRarity Rarity)
	{
		switch (Rarity)
		{
		case ESRFacilityRarity::Starting: return 0;
		case ESRFacilityRarity::Basic: return 1;
		case ESRFacilityRarity::Advanced: return 2;
		case ESRFacilityRarity::HighTech: return 3;
		case ESRFacilityRarity::Innovation: return 4;
		default: return 5;
		}
	}

	FString ResolveStableLabel(const FSRStructureBuildOption& BuildOption)
	{
		return BuildOption.DisplayName.IsEmpty()
			? BuildOption.StructureId.ToString()
			: BuildOption.DisplayName.ToString();
	}
}

void FSRStructureBuildDockModel::BuildFamilyTabs(
	const TArray<FSRStructureBuildOption>& BuildOptions,
	TArray<FSRStructureBuildFamilyTab>& OutTabs)
{
	OutTabs.Reset();
	OutTabs.Reserve(GetOrderedFamilyFilters().Num());

	for (const ESRStructureBuildFamilyFilter Filter : GetOrderedFamilyFilters())
	{
		FSRStructureBuildFamilyTab& Tab = OutTabs.AddDefaulted_GetRef();
		Tab.Filter = Filter;
		Tab.Label = GetFilterLabel(Filter);
		Tab.ResourceFamily = ResolveResourceFamily(Filter);

		for (const FSRStructureBuildOption& BuildOption : BuildOptions)
		{
			const bool bOwnedByTab = Filter == ESRStructureBuildFamilyFilter::All
				|| (Filter == ESRStructureBuildFamilyFilter::Shared
					&& BuildOption.ResourceFamily == ESRResourceFamily::None)
				|| (Filter != ESRStructureBuildFamilyFilter::Shared
					&& Filter != ESRStructureBuildFamilyFilter::All
					&& BuildOption.ResourceFamily == Tab.ResourceFamily);
			if (BuildOption.StructureId.IsNone() || !bOwnedByTab)
			{
				continue;
			}

			++Tab.TotalOptionCount;
			if (BuildOption.IsSelectable())
			{
				++Tab.SelectableOptionCount;
			}
		}
	}
}

void FSRStructureBuildDockModel::QueryOptions(
	const TArray<FSRStructureBuildOption>& BuildOptions,
	ESRStructureBuildFamilyFilter FamilyFilter,
	bool bIncludeSharedWorkflowOptions,
	TArray<FName>& OutStructureIds)
{
	TArray<const FSRStructureBuildOption*> MatchingOptions;
	MatchingOptions.Reserve(BuildOptions.Num());
	for (const FSRStructureBuildOption& BuildOption : BuildOptions)
	{
		if (!BuildOption.StructureId.IsNone()
			&& MatchesFamilyFilter(BuildOption, FamilyFilter, bIncludeSharedWorkflowOptions))
		{
			MatchingOptions.Add(&BuildOption);
		}
	}

	MatchingOptions.StableSort([](const FSRStructureBuildOption& Left, const FSRStructureBuildOption& Right)
	{
		const int32 LeftRoleRank = GetRoleSortRank(Left.Role);
		const int32 RightRoleRank = GetRoleSortRank(Right.Role);
		if (LeftRoleRank != RightRoleRank)
		{
			return LeftRoleRank < RightRoleRank;
		}

		const bool bLeftSelectable = Left.IsSelectable();
		const bool bRightSelectable = Right.IsSelectable();
		if (bLeftSelectable != bRightSelectable)
		{
			return bLeftSelectable;
		}

		const int32 LeftRarityRank = GetRaritySortRank(Left.Rarity);
		const int32 RightRarityRank = GetRaritySortRank(Right.Rarity);
		if (LeftRarityRank != RightRarityRank)
		{
			return LeftRarityRank < RightRarityRank;
		}

		const int32 LabelComparison = ResolveStableLabel(Left).Compare(
			ResolveStableLabel(Right),
			ESearchCase::IgnoreCase);
		return LabelComparison != 0
			? LabelComparison < 0
			: Left.StructureId.LexicalLess(Right.StructureId);
	});

	OutStructureIds.Reset(MatchingOptions.Num());
	for (const FSRStructureBuildOption* BuildOption : MatchingOptions)
	{
		OutStructureIds.Add(BuildOption->StructureId);
	}
}

bool FSRStructureBuildDockModel::MatchesFamilyFilter(
	const FSRStructureBuildOption& BuildOption,
	ESRStructureBuildFamilyFilter FamilyFilter,
	bool bIncludeSharedWorkflowOptions)
{
	if (BuildOption.StructureId.IsNone())
	{
		return false;
	}

	if (FamilyFilter == ESRStructureBuildFamilyFilter::All)
	{
		return true;
	}

	if (FamilyFilter == ESRStructureBuildFamilyFilter::Shared)
	{
		return BuildOption.ResourceFamily == ESRResourceFamily::None;
	}

	const ESRResourceFamily RequestedFamily = ResolveResourceFamily(FamilyFilter);
	if (BuildOption.ResourceFamily == RequestedFamily)
	{
		return true;
	}

	return bIncludeSharedWorkflowOptions
		&& BuildOption.ResourceFamily == ESRResourceFamily::None
		&& IsSharedWorkflowRole(BuildOption.Role);
}

ESRStructureBuildFamilyFilter FSRStructureBuildDockModel::ResolvePreferredFilter(
	const FSRStructureBuildOption& BuildOption)
{
	switch (BuildOption.ResourceFamily)
	{
	case ESRResourceFamily::Metal: return ESRStructureBuildFamilyFilter::Metal;
	case ESRResourceFamily::Crystal: return ESRStructureBuildFamilyFilter::Crystal;
	case ESRResourceFamily::Organic: return ESRStructureBuildFamilyFilter::Organic;
	case ESRResourceFamily::Plasma: return ESRStructureBuildFamilyFilter::Plasma;
	case ESRResourceFamily::Void: return ESRStructureBuildFamilyFilter::Void;
	case ESRResourceFamily::None:
	default: return ESRStructureBuildFamilyFilter::Shared;
	}
}

FName FSRStructureBuildDockModel::FindRecommendedOptionId(
	const TArray<FSRStructureBuildOption>& BuildOptions,
	ESRStructureBuildRole Role,
	ESRResourceFamily PreferredFamily)
{
	TArray<FName> OrderedOptionIds;
	QueryOptions(
		BuildOptions,
		ESRStructureBuildFamilyFilter::All,
		true,
		OrderedOptionIds);

	for (const FName StructureId : OrderedOptionIds)
	{
		const FSRStructureBuildOption* BuildOption = BuildOptions.FindByPredicate(
			[StructureId](const FSRStructureBuildOption& Candidate)
			{
				return Candidate.StructureId == StructureId;
			});
		if (!BuildOption || BuildOption->Role != Role || !BuildOption->IsSelectable())
		{
			continue;
		}
		if (PreferredFamily != ESRResourceFamily::None
			&& Role == ESRStructureBuildRole::FamilyProcessing
			&& BuildOption->ResourceFamily != PreferredFamily)
		{
			continue;
		}
		return StructureId;
	}

	return NAME_None;
}

ESRResourceFamily FSRStructureBuildDockModel::ResolveResourceFamily(
	ESRStructureBuildFamilyFilter FamilyFilter)
{
	switch (FamilyFilter)
	{
	case ESRStructureBuildFamilyFilter::Metal: return ESRResourceFamily::Metal;
	case ESRStructureBuildFamilyFilter::Crystal: return ESRResourceFamily::Crystal;
	case ESRStructureBuildFamilyFilter::Organic: return ESRResourceFamily::Organic;
	case ESRStructureBuildFamilyFilter::Plasma: return ESRResourceFamily::Plasma;
	case ESRStructureBuildFamilyFilter::Void: return ESRResourceFamily::Void;
	case ESRStructureBuildFamilyFilter::All:
	case ESRStructureBuildFamilyFilter::Shared:
	default: return ESRResourceFamily::None;
	}
}

FText FSRStructureBuildDockModel::GetFilterLabel(ESRStructureBuildFamilyFilter FamilyFilter)
{
	switch (FamilyFilter)
	{
	case ESRStructureBuildFamilyFilter::All:
		return NSLOCTEXT("StarRoversBuildDock", "AllFamilyFilter", "ALL");
	case ESRStructureBuildFamilyFilter::Metal:
		return NSLOCTEXT("StarRoversBuildDock", "MetalFamilyFilter", "METAL");
	case ESRStructureBuildFamilyFilter::Crystal:
		return NSLOCTEXT("StarRoversBuildDock", "CrystalFamilyFilter", "CRYSTAL");
	case ESRStructureBuildFamilyFilter::Organic:
		return NSLOCTEXT("StarRoversBuildDock", "OrganicFamilyFilter", "ORGANIC");
	case ESRStructureBuildFamilyFilter::Plasma:
		return NSLOCTEXT("StarRoversBuildDock", "PlasmaFamilyFilter", "PLASMA");
	case ESRStructureBuildFamilyFilter::Void:
		return NSLOCTEXT("StarRoversBuildDock", "VoidFamilyFilter", "VOID");
	case ESRStructureBuildFamilyFilter::Shared:
	default:
		return NSLOCTEXT("StarRoversBuildDock", "SharedFamilyFilter", "SHARED");
	}
}

bool FSRStructureBuildDockModel::IsSharedWorkflowRole(ESRStructureBuildRole Role)
{
	switch (Role)
	{
	case ESRStructureBuildRole::Logistics:
	case ESRStructureBuildRole::Extraction:
	case ESRStructureBuildRole::FamilyProcessing:
	case ESRStructureBuildRole::TagProcessing:
	case ESRStructureBuildRole::FuelImprinting:
		return true;
	case ESRStructureBuildRole::General:
	case ESRStructureBuildRole::Synthesis:
	case ESRStructureBuildRole::StellarFuelFabrication:
	case ESRStructureBuildRole::Infrastructure:
	case ESRStructureBuildRole::Hub:
	default:
		return false;
	}
}
