#include "Logistics/SRSpaceLogisticsTypes.h"

bool StarRovers::SpaceLogistics::PatternSave::IsSupportedVersion(int32 Version)
{
	return Version == LegacyResourceFilterVersion
		|| Version == CurrentVersion;
}

FSRPatternRoutingFilter StarRovers::SpaceLogistics::PatternSave::ResolveRouteCargoFilter(
	int32 Version,
	const FSRPatternRoutingFilter& PatternFilter,
	FName LegacyCargoResourceId)
{
	if (Version == LegacyResourceFilterVersion)
	{
		FSRPatternRoutingFilter MigratedFilter;
		MigratedFilter.ResourceId = LegacyCargoResourceId;
		return MigratedFilter;
	}

	return PatternFilter;
}
