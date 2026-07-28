#pragma once

#include "CoreMinimal.h"
#include "Simulation/SRFiniteResourceEconomy.h"

class USRPlanetDataAsset;

struct STARROVERS_API FSRResourceEconomySoakRules
{
	int32 FirstSeed = 1;
	int32 SeedCount = 512;
	int32 MinimumPlanetCount = 5;
	int32 MaximumPlanetCount = 7;
	int32 MinimumUniquePlanetTypes = 4;
	int32 ExpectedResourceTypesPerPlanet = 3;
	int32 MinimumGuaranteedCompleteFronts = 4;
	double MinerCycleSeconds = 4.0;
	double BasicLineStartSeconds = 300.0;
	double BasicTransitDelaySeconds = 30.0;
	// A remote expansion must dispatch before the 25-minute pressure boundary so
	// its transit-delayed first batch arrives at that boundary.
	double ExpansionLineStartSeconds = 1380.0;
	double DistributedTransitDelaySeconds = 120.0;
};

struct STARROVERS_API FSRResourceEconomyRuleSeedAvailability
{
	int32 SourcePlanetCount = 0;
	int32 GuaranteedDepositCount = 0;
	int32 PotentialDepositCount = 0;
};

struct STARROVERS_API FSRResourceEconomySeedSoakResult
{
	int32 Seed = 0;
	int32 PlanetCount = 0;
	int32 UniqueEnvironmentCount = 0;
	int32 GuaranteedCompleteFronts = 0;
	int32 PotentialCompleteFronts = 0;
	int64 GuaranteedFuelBatchCount = 0;
	int64 PotentialFuelBatchCount = 0;
	bool bResourceCoverageSatisfied = false;
	bool bPassed = false;
	TArray<FString> EnvironmentNames;
	TMap<FName, FSRResourceEconomyRuleSeedAvailability> AvailabilityByRuleId;
	FString FailureReason;
};

struct STARROVERS_API FSRResourceEconomyRuleSoakRange
{
	int32 MinimumSourcePlanetCount = MAX_int32;
	int32 MaximumSourcePlanetCount = 0;
	int32 MinimumGuaranteedDepositCount = MAX_int32;
	int32 MaximumGuaranteedDepositCount = 0;
	int32 MinimumPotentialDepositCount = MAX_int32;
	int32 MaximumPotentialDepositCount = 0;
};

struct STARROVERS_API FSRResourceEconomySoakReport
{
	FSRResourceEconomySoakRules Rules;
	FSRFiniteResourceEconomyContract EconomyContract;
	FSRRunBalanceResult BasicLineResult;
	FSRRunBalanceResult DistributedExpansionResult;
	TArray<FSRResourceEconomySeedSoakResult> SeedResults;
	TMap<FName, FSRResourceEconomyRuleSoakRange> RuleRanges;
	TMap<FString, int32> EnvironmentAppearanceCount;
	int32 PassedSeedCount = 0;
	int32 FailedSeedCount = 0;
	int32 MinimumObservedPlanetCount = MAX_int32;
	int32 MaximumObservedPlanetCount = 0;
	int32 MinimumObservedUniqueEnvironmentCount = MAX_int32;
	int32 MaximumObservedUniqueEnvironmentCount = 0;
	int32 MinimumObservedGuaranteedCompleteFronts = MAX_int32;
	int32 MaximumObservedGuaranteedCompleteFronts = 0;
	int32 MinimumObservedPotentialCompleteFronts = MAX_int32;
	int32 MaximumObservedPotentialCompleteFronts = 0;
	double RawSingleDepositMiningSeconds = 0.0;
	double LineFedSingleFrontSeconds = 0.0;
	bool bDeterministicReplayPassed = false;
	bool bPassed = false;
	FString FailureReason;

	FString BuildSummaryString() const;
	FString BuildCsv() const;
};

/**
 * Pure deterministic balance harness. It samples the authored planet portfolio
 * without constructing meshes, then evaluates the same finite-resource and
 * Stellar Pressure models used by runtime play.
 */
class STARROVERS_API FSRResourceEconomySoakModel final
{
public:
	static FSRResourceEconomySoakReport Run(
		const TArray<const USRPlanetDataAsset*>& PlanetCandidates,
		const TArray<FName>& RequiredResourceRuleIds,
		const FSRResourceEconomySoakRules& Rules = FSRResourceEconomySoakRules());
};
