#include "Pattern/SRPatternRoutingFilter.h"

bool FSRPatternRoutingFilter::IsCanonical() const
{
	if (!RequiredPattern.IsCanonical() || !RequiredMask.IsCanonical())
	{
		return false;
	}

	switch (MatchMode)
	{
	case ESRPatternRoutingMatchMode::AnyPattern:
		return true;
	case ESRPatternRoutingMatchMode::ExactPattern:
		return !RequiredPattern.IsEmpty();
	case ESRPatternRoutingMatchMode::MaskedPattern:
		return RequiredMask.HasAnyActiveCell();
	default:
		return false;
	}
}

void FSRPatternRoutingFilter::Normalize()
{
	RequiredPattern.Normalize();
	RequiredMask.Normalize();
	switch (MatchMode)
	{
	case ESRPatternRoutingMatchMode::AnyPattern:
	case ESRPatternRoutingMatchMode::ExactPattern:
	case ESRPatternRoutingMatchMode::MaskedPattern:
		break;
	default:
		MatchMode = ESRPatternRoutingMatchMode::AnyPattern;
		break;
	}
}

bool StarRovers::PatternRouting::IsValidPatternPayload(
	const FSRResourceInstance& ResourceInstance)
{
	return !ResourceInstance.ResourceId.IsNone()
		&& ResourceInstance.StackCount > 0
		&& ResourceInstance.Pattern.IsCanonical()
		&& !ResourceInstance.Pattern.IsEmpty();
}

bool StarRovers::PatternRouting::IsEmptyPatternPayload(
	const FSRResourceInstance& ResourceInstance)
{
	return ResourceInstance.ResourceInstanceId.IsNone()
		&& !IsValid(ResourceInstance.ResourceDataAsset.Get())
		&& ResourceInstance.ResourceId.IsNone()
		&& ResourceInstance.Pattern.IsCanonical()
		&& ResourceInstance.Pattern.IsEmpty()
		&& ResourceInstance.SourcePatternId.IsNone()
		&& ResourceInstance.SourcePatternSeed == 0;
}

bool StarRovers::PatternRouting::IsValidOrEmptyPatternPayload(
	const FSRResourceInstance& ResourceInstance)
{
	return IsValidPatternPayload(ResourceInstance)
		|| IsEmptyPatternPayload(ResourceInstance);
}

bool StarRovers::PatternRouting::MatchesRoutingFilter(
	const FSRResourceInstance& ResourceInstance,
	const FSRPatternRoutingFilter& RoutingFilter)
{
	if (!IsValidPatternPayload(ResourceInstance)
		|| !RoutingFilter.IsCanonical()
		|| (!RoutingFilter.ResourceId.IsNone()
			&& ResourceInstance.ResourceId != RoutingFilter.ResourceId))
	{
		return false;
	}

	switch (RoutingFilter.MatchMode)
	{
	case ESRPatternRoutingMatchMode::AnyPattern:
		return true;
	case ESRPatternRoutingMatchMode::ExactPattern:
		return ResourceInstance.Pattern == RoutingFilter.RequiredPattern;
	case ESRPatternRoutingMatchMode::MaskedPattern:
		for (int32 CellIndex = 0; CellIndex < StarRovers::Pattern::CellCount; ++CellIndex)
		{
			if (RoutingFilter.RequiredMask.ActiveCells[CellIndex]
				&& ResourceInstance.Pattern.Cells[CellIndex]
					!= RoutingFilter.RequiredPattern.Cells[CellIndex])
			{
				return false;
			}
		}
		return true;
	default:
		return false;
	}
}
