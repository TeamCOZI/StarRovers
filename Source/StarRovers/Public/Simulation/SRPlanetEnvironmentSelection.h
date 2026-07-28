#pragma once

#include "CoreMinimal.h"

class USRPlanetDataAsset;

struct STARROVERS_API FSRPlanetResourceRuleAvailability
{
	FName RuleId = NAME_None;
	float SpawnChancePerCell = 0.0f;
	int32 MinimumGuaranteedCount = 0;
	int32 MaximumCount = 0;
};

struct STARROVERS_API FSRPlanetEnvironmentSelectionReport
{
	TSet<FName> RequiredResourceRuleIds;
	TSet<FName> CoveredResourceRuleIds;
	TSet<FName> MissingResourceRuleIds;
	bool bResourceCoverageSatisfied = true;
};

/** Deterministic weighted selection with an optional minimum number of unique environments. */
class STARROVERS_API FSRPlanetEnvironmentSelector final
{
public:
	static void Select(
		const TArray<const USRPlanetDataAsset*>& CandidatePlanets,
		int32 RequestedPlanetCount,
		int32 MinimumUniquePlanetTypes,
		FRandomStream& RandomStream,
		TArray<const USRPlanetDataAsset*>& OutSelectedPlanets);

	/**
	 * Selects a deterministic weighted planet set, but first finds a distinct
	 * environment subset that covers every required effective Resource V2 rule.
	 * If the catalog cannot satisfy the contract, a best-effort selection is
	 * still returned and the missing rules are exposed through OutReport.
	 */
	static void SelectWithResourceCoverage(
		const TArray<const USRPlanetDataAsset*>& CandidatePlanets,
		int32 RequestedPlanetCount,
		int32 MinimumUniquePlanetTypes,
		const TArray<FName>& RequiredResourceRuleIds,
		FRandomStream& RandomStream,
		TArray<const USRPlanetDataAsset*>& OutSelectedPlanets,
		FSRPlanetEnvironmentSelectionReport& OutReport);

	/** Returns the effective enabled ResourceV2.* rules after planet overrides. */
	static void GetEnabledResourceRuleIds(
		const USRPlanetDataAsset* Planet,
		TSet<FName>& OutResourceRuleIds);

	/** Returns the effective finite spawn envelope used by generation and soak tests. */
	static void GetEnabledResourceRuleAvailability(
		const USRPlanetDataAsset* Planet,
		TArray<FSRPlanetResourceRuleAvailability>& OutAvailability);
};
