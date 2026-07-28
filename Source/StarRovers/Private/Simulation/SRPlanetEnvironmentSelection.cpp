#include "Simulation/SRPlanetEnvironmentSelection.h"

#include "Celestial/SRPlanetDataAsset.h"
#include "Surface/SRPlanetTerrainProfileDataAsset.h"

namespace
{
	const USRPlanetDataAsset* DrawWeightedPlanet(
		const TArray<const USRPlanetDataAsset*>& Candidates,
		FRandomStream& RandomStream)
	{
		double TotalWeight = 0.0;
		for (const USRPlanetDataAsset* Candidate : Candidates)
		{
			if (IsValid(Candidate))
			{
				TotalWeight += FMath::Max(0.0f, Candidate->GenerationWeight);
			}
		}
		if (TotalWeight <= UE_SMALL_NUMBER)
		{
			return nullptr;
		}

		const double Roll = RandomStream.FRand() * TotalWeight;
		double AccumulatedWeight = 0.0;
		const USRPlanetDataAsset* LastValidCandidate = nullptr;
		for (const USRPlanetDataAsset* Candidate : Candidates)
		{
			if (!IsValid(Candidate) || Candidate->GenerationWeight <= UE_SMALL_NUMBER)
			{
				continue;
			}

			LastValidCandidate = Candidate;
			AccumulatedWeight += Candidate->GenerationWeight;
			if (Roll < AccumulatedWeight)
			{
				return Candidate;
			}
		}
		return LastValidCandidate;
	}

	void BuildWeightedPermutation(
		const TArray<const USRPlanetDataAsset*>& Candidates,
		FRandomStream& RandomStream,
		TArray<const USRPlanetDataAsset*>& OutPermutation)
	{
		TArray<const USRPlanetDataAsset*> Remaining = Candidates;
		OutPermutation.Reset(Candidates.Num());
		while (!Remaining.IsEmpty())
		{
			const USRPlanetDataAsset* Selected = DrawWeightedPlanet(Remaining, RandomStream);
			if (!IsValid(Selected))
			{
				break;
			}
			OutPermutation.Add(Selected);
			Remaining.RemoveSingleSwap(Selected, EAllowShrinking::No);
		}
	}

	bool CoversEveryRequiredRule(
		const TSet<FName>& CoveredRuleIds,
		const TSet<FName>& RequiredRuleIds)
	{
		for (const FName RequiredRuleId : RequiredRuleIds)
		{
			if (!CoveredRuleIds.Contains(RequiredRuleId))
			{
				return false;
			}
		}
		return true;
	}

	int32 CountNewCoverage(
		const TSet<FName>& CandidateRuleIds,
		const TSet<FName>& CoveredRuleIds,
		const TSet<FName>& RequiredRuleIds)
	{
		int32 Result = 0;
		for (const FName RuleId : CandidateRuleIds)
		{
			Result += RequiredRuleIds.Contains(RuleId) && !CoveredRuleIds.Contains(RuleId) ? 1 : 0;
		}
		return Result;
	}

	void AppendRequiredCoverage(
		const TSet<FName>& CandidateRuleIds,
		const TSet<FName>& RequiredRuleIds,
		TSet<FName>& InOutCoveredRuleIds)
	{
		for (const FName RuleId : CandidateRuleIds)
		{
			if (RequiredRuleIds.Contains(RuleId))
			{
				InOutCoveredRuleIds.Add(RuleId);
			}
		}
	}

	void BuildSelectionReport(
		const TArray<const USRPlanetDataAsset*>& SelectedPlanets,
		const TSet<FName>& RequiredRuleIds,
		FSRPlanetEnvironmentSelectionReport& OutReport)
	{
		OutReport = FSRPlanetEnvironmentSelectionReport();
		OutReport.RequiredResourceRuleIds = RequiredRuleIds;
		for (const USRPlanetDataAsset* Planet : SelectedPlanets)
		{
			TSet<FName> AvailableRuleIds;
			FSRPlanetEnvironmentSelector::GetEnabledResourceRuleIds(Planet, AvailableRuleIds);
			AppendRequiredCoverage(
				AvailableRuleIds,
				RequiredRuleIds,
				OutReport.CoveredResourceRuleIds);
		}
		for (const FName RequiredRuleId : RequiredRuleIds)
		{
			if (!OutReport.CoveredResourceRuleIds.Contains(RequiredRuleId))
			{
				OutReport.MissingResourceRuleIds.Add(RequiredRuleId);
			}
		}
		OutReport.bResourceCoverageSatisfied = OutReport.MissingResourceRuleIds.IsEmpty();
	}
}

void FSRPlanetEnvironmentSelector::GetEnabledResourceRuleIds(
	const USRPlanetDataAsset* Planet,
	TSet<FName>& OutResourceRuleIds)
{
	OutResourceRuleIds.Reset();
	TArray<FSRPlanetResourceRuleAvailability> Availability;
	GetEnabledResourceRuleAvailability(Planet, Availability);
	for (const FSRPlanetResourceRuleAvailability& Rule : Availability)
	{
		OutResourceRuleIds.Add(Rule.RuleId);
	}
}

void FSRPlanetEnvironmentSelector::GetEnabledResourceRuleAvailability(
	const USRPlanetDataAsset* Planet,
	TArray<FSRPlanetResourceRuleAvailability>& OutAvailability)
{
	OutAvailability.Reset();
	if (!IsValid(Planet) || !IsValid(Planet->TerrainProfileDataAsset.Get()))
	{
		return;
	}

	for (const FSRProfileNaturalStructureSpawnRule& Rule
		: Planet->TerrainProfileDataAsset->ProfileNaturalStructureSpawnRules)
	{
		if (Rule.RuleId.IsNone()
			|| !Rule.RuleId.ToString().StartsWith(TEXT("ResourceV2."), ESearchCase::CaseSensitive))
		{
			continue;
		}

		const FSRNaturalStructureSpawnRuleOverride* Override =
			Planet->ProfileNaturalStructureSpawnRuleOverrides.FindByPredicate(
				[RuleId = Rule.RuleId](const FSRNaturalStructureSpawnRuleOverride& Candidate)
				{
					return Candidate.RuleId == RuleId;
				});
		const bool bEnabled = Override ? Override->bEnabled : Rule.bEnabled;
		const float SpawnChance = Override ? Override->SpawnChancePerCell : Rule.SpawnChancePerCell;
		const int32 MinimumGuaranteedCount = Override
			? Override->MinimumGuaranteedCount
			: Rule.MinimumGuaranteedCount;
		if (bEnabled && (SpawnChance > UE_SMALL_NUMBER || MinimumGuaranteedCount > 0))
		{
			FSRPlanetResourceRuleAvailability& EffectiveRule =
				OutAvailability.AddDefaulted_GetRef();
			EffectiveRule.RuleId = Rule.RuleId;
			EffectiveRule.SpawnChancePerCell = FMath::Clamp(SpawnChance, 0.0f, 1.0f);
			EffectiveRule.MinimumGuaranteedCount = FMath::Max(0, MinimumGuaranteedCount);
			EffectiveRule.MaximumCount = FMath::Max(
				0,
				Override ? Override->MaxCount : Rule.MaxCount);
		}
	}
	OutAvailability.Sort(
		[](const FSRPlanetResourceRuleAvailability& Left,
			const FSRPlanetResourceRuleAvailability& Right)
		{
			return Left.RuleId.LexicalLess(Right.RuleId);
		});
}

void FSRPlanetEnvironmentSelector::Select(
	const TArray<const USRPlanetDataAsset*>& CandidatePlanets,
	int32 RequestedPlanetCount,
	int32 MinimumUniquePlanetTypes,
	FRandomStream& RandomStream,
	TArray<const USRPlanetDataAsset*>& OutSelectedPlanets)
{
	FSRPlanetEnvironmentSelectionReport IgnoredReport;
	SelectWithResourceCoverage(
		CandidatePlanets,
		RequestedPlanetCount,
		MinimumUniquePlanetTypes,
		{},
		RandomStream,
		OutSelectedPlanets,
		IgnoredReport);
}

void FSRPlanetEnvironmentSelector::SelectWithResourceCoverage(
	const TArray<const USRPlanetDataAsset*>& CandidatePlanets,
	int32 RequestedPlanetCount,
	int32 MinimumUniquePlanetTypes,
	const TArray<FName>& RequiredResourceRuleIds,
	FRandomStream& RandomStream,
	TArray<const USRPlanetDataAsset*>& OutSelectedPlanets,
	FSRPlanetEnvironmentSelectionReport& OutReport)
{
	OutSelectedPlanets.Reset();
	OutReport = FSRPlanetEnvironmentSelectionReport();
	TSet<FName> RequiredRuleIdSet;
	for (const FName RuleId : RequiredResourceRuleIds)
	{
		if (!RuleId.IsNone())
		{
			RequiredRuleIdSet.Add(RuleId);
		}
	}
	if (RequestedPlanetCount <= 0)
	{
		BuildSelectionReport(OutSelectedPlanets, RequiredRuleIdSet, OutReport);
		return;
	}

	TArray<const USRPlanetDataAsset*> ValidCandidates;
	TSet<const USRPlanetDataAsset*> SeenCandidates;
	for (const USRPlanetDataAsset* Candidate : CandidatePlanets)
	{
		if (!IsValid(Candidate)
			|| Candidate->GenerationWeight <= UE_SMALL_NUMBER
			|| SeenCandidates.Contains(Candidate))
		{
			continue;
		}
		SeenCandidates.Add(Candidate);
		ValidCandidates.Add(Candidate);
	}
	if (ValidCandidates.IsEmpty())
	{
		BuildSelectionReport(OutSelectedPlanets, RequiredRuleIdSet, OutReport);
		return;
	}

	TMap<const USRPlanetDataAsset*, TSet<FName>> RuleIdsByCandidate;
	for (const USRPlanetDataAsset* Candidate : ValidCandidates)
	{
		GetEnabledResourceRuleIds(Candidate, RuleIdsByCandidate.Add(Candidate));
	}

	TArray<const USRPlanetDataAsset*> WeightedCandidateOrder;
	BuildWeightedPermutation(ValidCandidates, RandomStream, WeightedCandidateOrder);
	const int32 MaximumUniqueSelectionCount = FMath::Min(RequestedPlanetCount, ValidCandidates.Num());
	TArray<const USRPlanetDataAsset*> CoverageSubset;
	if (!RequiredRuleIdSet.IsEmpty())
	{
		for (int32 MaximumSubsetSize = 1;
			MaximumSubsetSize <= MaximumUniqueSelectionCount && CoverageSubset.IsEmpty();
			++MaximumSubsetSize)
		{
			TArray<const USRPlanetDataAsset*> CurrentSubset;
			TSet<FName> CurrentCoverage;
			TFunction<bool(int32)> SearchCoverageSubset;
			SearchCoverageSubset = [&](int32 CandidateIndex)
			{
				if (CoversEveryRequiredRule(CurrentCoverage, RequiredRuleIdSet))
				{
					CoverageSubset = CurrentSubset;
					return true;
				}
				if (CandidateIndex >= WeightedCandidateOrder.Num()
					|| CurrentSubset.Num() >= MaximumSubsetSize
					|| CurrentSubset.Num() + WeightedCandidateOrder.Num() - CandidateIndex < 1)
				{
					return false;
				}

				const USRPlanetDataAsset* Candidate = WeightedCandidateOrder[CandidateIndex];
				const TSet<FName>& CandidateRuleIds = RuleIdsByCandidate.FindChecked(Candidate);
				if (CountNewCoverage(CandidateRuleIds, CurrentCoverage, RequiredRuleIdSet) > 0)
				{
					const TSet<FName> CoverageBeforeCandidate = CurrentCoverage;
					CurrentSubset.Add(Candidate);
					AppendRequiredCoverage(CandidateRuleIds, RequiredRuleIdSet, CurrentCoverage);
					if (SearchCoverageSubset(CandidateIndex + 1))
					{
						return true;
					}
					CurrentSubset.Pop(EAllowShrinking::No);
					CurrentCoverage = CoverageBeforeCandidate;
				}
				return SearchCoverageSubset(CandidateIndex + 1);
			};
			SearchCoverageSubset(0);
		}
	}

	TArray<const USRPlanetDataAsset*> SelectedUniquePlanets = CoverageSubset;
	TArray<const USRPlanetDataAsset*> RemainingUniqueCandidates = ValidCandidates;
	TSet<FName> SelectedCoverage;
	for (const USRPlanetDataAsset* Selected : SelectedUniquePlanets)
	{
		RemainingUniqueCandidates.RemoveSingleSwap(Selected, EAllowShrinking::No);
		AppendRequiredCoverage(
			RuleIdsByCandidate.FindChecked(Selected),
			RequiredRuleIdSet,
			SelectedCoverage);
	}

	const int32 UniqueSelectionTarget = FMath::Min3(
		RequestedPlanetCount,
		FMath::Max(FMath::Max(0, MinimumUniquePlanetTypes), SelectedUniquePlanets.Num()),
		ValidCandidates.Num());
	while (SelectedUniquePlanets.Num() < UniqueSelectionTarget && !RemainingUniqueCandidates.IsEmpty())
	{
		int32 BestNewCoverage = INDEX_NONE;
		TArray<const USRPlanetDataAsset*> BestCandidates;
		for (const USRPlanetDataAsset* Candidate : RemainingUniqueCandidates)
		{
			const int32 NewCoverage = CountNewCoverage(
				RuleIdsByCandidate.FindChecked(Candidate),
				SelectedCoverage,
				RequiredRuleIdSet);
			if (NewCoverage > BestNewCoverage)
			{
				BestNewCoverage = NewCoverage;
				BestCandidates.Reset();
				BestCandidates.Add(Candidate);
			}
			else if (NewCoverage == BestNewCoverage)
			{
				BestCandidates.Add(Candidate);
			}
		}

		const USRPlanetDataAsset* Selected = DrawWeightedPlanet(BestCandidates, RandomStream);
		if (!IsValid(Selected))
		{
			break;
		}
		SelectedUniquePlanets.Add(Selected);
		RemainingUniqueCandidates.RemoveSingleSwap(Selected, EAllowShrinking::No);
		AppendRequiredCoverage(
			RuleIdsByCandidate.FindChecked(Selected),
			RequiredRuleIdSet,
			SelectedCoverage);
	}

	for (int32 Index = SelectedUniquePlanets.Num() - 1; Index > 0; --Index)
	{
		SelectedUniquePlanets.Swap(Index, RandomStream.RandRange(0, Index));
	}
	OutSelectedPlanets = SelectedUniquePlanets;
	while (OutSelectedPlanets.Num() < RequestedPlanetCount)
	{
		const USRPlanetDataAsset* Selected = DrawWeightedPlanet(ValidCandidates, RandomStream);
		if (!IsValid(Selected))
		{
			break;
		}
		OutSelectedPlanets.Add(Selected);
	}

	BuildSelectionReport(OutSelectedPlanets, RequiredRuleIdSet, OutReport);
}
