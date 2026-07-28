#include "Simulation/SRResourceEconomySoak.h"

#include "Automation/SRResourceSystemContent.h"
#include "Celestial/SRPlanetDataAsset.h"
#include "Simulation/SRPlanetEnvironmentSelection.h"

namespace
{
	FSRRunBalanceScenario MakePressureScenario()
	{
		FSRRunBalanceScenario Scenario;
		Scenario.ScenarioId = TEXT("ResourceEconomySeedSoak");
		Scenario.DurationSeconds = 2100.0;
		Scenario.OutputSampleIntervalSeconds = 10.0;
		Scenario.SecondsPerCycle = 60.0;
		Scenario.StartingStoredFuel = 20000.0;
		Scenario.DemandCurve = ESRRunBalanceDemandCurve::StellarPressureV2;
		Scenario.DemandCurveV2.InitialDemandPerSecond = 50.0;
		Scenario.DemandCurveV2.GraceCycleCount = 2;
		Scenario.DemandCurveV2.DemandIncreasePerCycle = 5.0;
		Scenario.DemandCurveV2.MaximumDemandPerSecond = 100.0;
		Scenario.PressureRulesV2.FuelReserveCapacity = 20000.0;
		Scenario.PressureRulesV2.RedGiantEmergencyReserveRatio = 1.0;
		return Scenario;
	}

	FString EscapeCsv(FString Value)
	{
		Value.ReplaceInline(TEXT("\""), TEXT("\"\""));
		return FString::Printf(TEXT("\"%s\""), *Value);
	}

	void AddFailure(FString& InOutFailureReason, const FString& Failure)
	{
		if (Failure.IsEmpty())
		{
			return;
		}
		if (!InOutFailureReason.IsEmpty())
		{
			InOutFailureReason += TEXT(" | ");
		}
		InOutFailureReason += Failure;
	}

	TSet<FName> BuildReferenceCardRuleIds()
	{
		TArray<FSRReferenceResourceDefinitionV2> Definitions;
		FSRResourceSystemContent::GetAllReferenceResourceDefinitions(Definitions);
		TSet<FName> Result;
		for (const FSRReferenceResourceDefinitionV2& Definition : Definitions)
		{
			Result.Add(FName(*FString::Printf(TEXT("ResourceV2.%s"), *Definition.ResourceId.ToString())));
		}
		return Result;
	}

	void FinalizeEmptyRanges(FSRResourceEconomySoakReport& Report)
	{
		if (Report.MinimumObservedPlanetCount == MAX_int32)
		{
			Report.MinimumObservedPlanetCount = 0;
		}
		if (Report.MinimumObservedUniqueEnvironmentCount == MAX_int32)
		{
			Report.MinimumObservedUniqueEnvironmentCount = 0;
		}
		if (Report.MinimumObservedGuaranteedCompleteFronts == MAX_int32)
		{
			Report.MinimumObservedGuaranteedCompleteFronts = 0;
		}
		if (Report.MinimumObservedPotentialCompleteFronts == MAX_int32)
		{
			Report.MinimumObservedPotentialCompleteFronts = 0;
		}
		for (TPair<FName, FSRResourceEconomyRuleSoakRange>& Pair : Report.RuleRanges)
		{
			if (Pair.Value.MinimumSourcePlanetCount == MAX_int32)
			{
				Pair.Value.MinimumSourcePlanetCount = 0;
			}
			if (Pair.Value.MinimumGuaranteedDepositCount == MAX_int32)
			{
				Pair.Value.MinimumGuaranteedDepositCount = 0;
			}
			if (Pair.Value.MinimumPotentialDepositCount == MAX_int32)
			{
				Pair.Value.MinimumPotentialDepositCount = 0;
			}
		}
	}
}

FString FSRResourceEconomySoakReport::BuildSummaryString() const
{
	return FString::Printf(
		TEXT("Seeds=%d Passed=%d Failed=%d Planets=%d-%d Unique=%d-%d Fronts(G/P)=%d-%d/%d-%d RawMine=%.0fs LineFed=%.0fs Basic=%d@%.0fs Expansion=%d@%.0fs Deterministic=%s"),
		SeedResults.Num(),
		PassedSeedCount,
		FailedSeedCount,
		MinimumObservedPlanetCount,
		MaximumObservedPlanetCount,
		MinimumObservedUniqueEnvironmentCount,
		MaximumObservedUniqueEnvironmentCount,
		MinimumObservedGuaranteedCompleteFronts,
		MaximumObservedGuaranteedCompleteFronts,
		MinimumObservedPotentialCompleteFronts,
		MaximumObservedPotentialCompleteFronts,
		RawSingleDepositMiningSeconds,
		LineFedSingleFrontSeconds,
		static_cast<int32>(BasicLineResult.Outcome),
		BasicLineResult.SimulatedUntilSeconds,
		static_cast<int32>(DistributedExpansionResult.Outcome),
		DistributedExpansionResult.CompletionSeconds,
		bDeterministicReplayPassed ? TEXT("true") : TEXT("false"));
}

FString FSRResourceEconomySoakReport::BuildCsv() const
{
	FString Csv = TEXT("Seed,Passed,PlanetCount,UniqueEnvironmentCount,Coverage,GuaranteedCompleteFronts,PotentialCompleteFronts,GuaranteedFuelBatches,PotentialFuelBatches,Environments,Availability,Failure\n");
	for (const FSRResourceEconomySeedSoakResult& SeedResult : SeedResults)
	{
		TArray<FString> Availability;
		for (const TPair<FName, FSRResourceEconomyRuleSeedAvailability>& Pair : SeedResult.AvailabilityByRuleId)
		{
			Availability.Add(FString::Printf(
				TEXT("%s:%d/%d/%d"),
				*Pair.Key.ToString(),
				Pair.Value.SourcePlanetCount,
				Pair.Value.GuaranteedDepositCount,
				Pair.Value.PotentialDepositCount));
		}
		Availability.Sort();
		Csv += FString::Printf(
			TEXT("%d,%s,%d,%d,%s,%d,%d,%lld,%lld,%s,%s,%s\n"),
			SeedResult.Seed,
			SeedResult.bPassed ? TEXT("true") : TEXT("false"),
			SeedResult.PlanetCount,
			SeedResult.UniqueEnvironmentCount,
			SeedResult.bResourceCoverageSatisfied ? TEXT("true") : TEXT("false"),
			SeedResult.GuaranteedCompleteFronts,
			SeedResult.PotentialCompleteFronts,
			SeedResult.GuaranteedFuelBatchCount,
			SeedResult.PotentialFuelBatchCount,
			*EscapeCsv(FString::Join(SeedResult.EnvironmentNames, TEXT("|"))),
			*EscapeCsv(FString::Join(Availability, TEXT("|"))),
			*EscapeCsv(SeedResult.FailureReason));
	}
	return Csv;
}

FSRResourceEconomySoakReport FSRResourceEconomySoakModel::Run(
	const TArray<const USRPlanetDataAsset*>& PlanetCandidates,
	const TArray<FName>& RequiredResourceRuleIds,
	const FSRResourceEconomySoakRules& Rules)
{
	FSRResourceEconomySoakReport Report;
	Report.Rules = Rules;
	Report.Rules.SeedCount = FMath::Max(1, Rules.SeedCount);
	Report.Rules.MinimumPlanetCount = FMath::Max(1, Rules.MinimumPlanetCount);
	Report.Rules.MaximumPlanetCount = FMath::Max(
		Report.Rules.MinimumPlanetCount,
		Rules.MaximumPlanetCount);
	Report.Rules.MinimumUniquePlanetTypes = FMath::Max(0, Rules.MinimumUniquePlanetTypes);
	Report.Rules.ExpectedResourceTypesPerPlanet = FMath::Max(1, Rules.ExpectedResourceTypesPerPlanet);
	Report.Rules.MinimumGuaranteedCompleteFronts = FMath::Max(1, Rules.MinimumGuaranteedCompleteFronts);
	Report.Rules.MinerCycleSeconds = FMath::Max(0.01, Rules.MinerCycleSeconds);
	Report.EconomyContract = FSRFiniteResourceEconomyModel::BuildReferenceContract();
	if (!Report.EconomyContract.bIsValid)
	{
		Report.FailureReason = Report.EconomyContract.FailureReason;
		FinalizeEmptyRanges(Report);
		return Report;
	}

	const TSet<FName> CardRuleIds = BuildReferenceCardRuleIds();
	if (CardRuleIds.Num() != Report.EconomyContract.RequiredCardTypeCount)
	{
		Report.FailureReason = TEXT("Reference Card rules do not match the finite economy contract.");
		FinalizeEmptyRanges(Report);
		return Report;
	}

	for (const FName RuleId : RequiredResourceRuleIds)
	{
		if (!RuleId.IsNone())
		{
			Report.RuleRanges.FindOrAdd(RuleId);
		}
	}

	Report.SeedResults.Reserve(Report.Rules.SeedCount);
	for (int32 SeedOffset = 0; SeedOffset < Report.Rules.SeedCount; ++SeedOffset)
	{
		FSRResourceEconomySeedSoakResult& SeedResult = Report.SeedResults.AddDefaulted_GetRef();
		SeedResult.Seed = Report.Rules.FirstSeed + SeedOffset;
		FRandomStream RandomStream(SeedResult.Seed);
		SeedResult.PlanetCount = RandomStream.RandRange(
			Report.Rules.MinimumPlanetCount,
			Report.Rules.MaximumPlanetCount);

		TArray<const USRPlanetDataAsset*> SelectedPlanets;
		FSRPlanetEnvironmentSelectionReport SelectionReport;
		FSRPlanetEnvironmentSelector::SelectWithResourceCoverage(
			PlanetCandidates,
			SeedResult.PlanetCount,
			Report.Rules.MinimumUniquePlanetTypes,
			RequiredResourceRuleIds,
			RandomStream,
			SelectedPlanets,
			SelectionReport);
		SeedResult.bResourceCoverageSatisfied = SelectionReport.bResourceCoverageSatisfied;
		if (SelectedPlanets.Num() != SeedResult.PlanetCount)
		{
			AddFailure(SeedResult.FailureReason, TEXT("Selector did not fill the requested planet count."));
		}
		if (!SeedResult.bResourceCoverageSatisfied)
		{
			AddFailure(SeedResult.FailureReason, TEXT("Required system resource coverage is incomplete."));
		}

		TSet<const USRPlanetDataAsset*> UniqueEnvironments;
		for (const USRPlanetDataAsset* Planet : SelectedPlanets)
		{
			if (!IsValid(Planet))
			{
				AddFailure(SeedResult.FailureReason, TEXT("Selection contains an invalid planet asset."));
				continue;
			}
			UniqueEnvironments.Add(Planet);
			const FString EnvironmentName = Planet->VariableName.IsEmpty()
				? Planet->GetName()
				: Planet->VariableName.ToString();
			SeedResult.EnvironmentNames.Add(EnvironmentName);

			TArray<FSRPlanetResourceRuleAvailability> PlanetAvailability;
			FSRPlanetEnvironmentSelector::GetEnabledResourceRuleAvailability(
				Planet,
				PlanetAvailability);
			if (PlanetAvailability.Num() != Report.Rules.ExpectedResourceTypesPerPlanet)
			{
				AddFailure(
					SeedResult.FailureReason,
					FString::Printf(
						TEXT("%s exposes %d resource types instead of %d."),
						*EnvironmentName,
						PlanetAvailability.Num(),
						Report.Rules.ExpectedResourceTypesPerPlanet));
			}
			for (const FSRPlanetResourceRuleAvailability& Availability : PlanetAvailability)
			{
				FSRResourceEconomyRuleSeedAvailability& RuleAvailability =
					SeedResult.AvailabilityByRuleId.FindOrAdd(Availability.RuleId);
				++RuleAvailability.SourcePlanetCount;
				RuleAvailability.GuaranteedDepositCount += Availability.MinimumGuaranteedCount;
				RuleAvailability.PotentialDepositCount += Availability.MaximumCount;
				if (Availability.SpawnChancePerCell <= UE_SMALL_NUMBER
					|| Availability.MinimumGuaranteedCount <= 0
					|| Availability.MaximumCount < Availability.MinimumGuaranteedCount)
				{
					AddFailure(
						SeedResult.FailureReason,
						FString::Printf(
							TEXT("%s has an invalid finite spawn envelope for %s (%d-%d, chance %.6f)."),
							*EnvironmentName,
							*Availability.RuleId.ToString(),
							Availability.MinimumGuaranteedCount,
							Availability.MaximumCount,
							Availability.SpawnChancePerCell));
				}
			}
		}

		SeedResult.EnvironmentNames.Sort();
		SeedResult.UniqueEnvironmentCount = UniqueEnvironments.Num();
		for (const USRPlanetDataAsset* UniqueEnvironment : UniqueEnvironments)
		{
			const FString EnvironmentName = UniqueEnvironment->VariableName.IsEmpty()
				? UniqueEnvironment->GetName()
				: UniqueEnvironment->VariableName.ToString();
			++Report.EnvironmentAppearanceCount.FindOrAdd(EnvironmentName);
		}
		if (SeedResult.UniqueEnvironmentCount < FMath::Min(
			Report.Rules.MinimumUniquePlanetTypes,
			SeedResult.PlanetCount))
		{
			AddFailure(SeedResult.FailureReason, TEXT("Minimum environment diversity was not satisfied."));
		}

		SeedResult.GuaranteedCompleteFronts = MAX_int32;
		SeedResult.PotentialCompleteFronts = MAX_int32;
		for (const FName CardRuleId : CardRuleIds)
		{
			const FSRResourceEconomyRuleSeedAvailability* CardAvailability =
				SeedResult.AvailabilityByRuleId.Find(CardRuleId);
			if (!CardAvailability)
			{
				SeedResult.GuaranteedCompleteFronts = 0;
				SeedResult.PotentialCompleteFronts = 0;
				AddFailure(
					SeedResult.FailureReason,
					FString::Printf(TEXT("Missing Card source: %s."), *CardRuleId.ToString()));
				continue;
			}
			SeedResult.GuaranteedCompleteFronts = FMath::Min(
				SeedResult.GuaranteedCompleteFronts,
				CardAvailability->GuaranteedDepositCount);
			SeedResult.PotentialCompleteFronts = FMath::Min(
				SeedResult.PotentialCompleteFronts,
				CardAvailability->PotentialDepositCount);
		}
		if (SeedResult.GuaranteedCompleteFronts == MAX_int32)
		{
			SeedResult.GuaranteedCompleteFronts = 0;
		}
		if (SeedResult.PotentialCompleteFronts == MAX_int32)
		{
			SeedResult.PotentialCompleteFronts = 0;
		}
		SeedResult.GuaranteedFuelBatchCount =
			static_cast<int64>(SeedResult.GuaranteedCompleteFronts)
			* static_cast<int64>(Report.EconomyContract.BatchesPerCardDepositSet);
		SeedResult.PotentialFuelBatchCount =
			static_cast<int64>(SeedResult.PotentialCompleteFronts)
			* static_cast<int64>(Report.EconomyContract.BatchesPerCardDepositSet);
		if (SeedResult.GuaranteedCompleteFronts
			< Report.Rules.MinimumGuaranteedCompleteFronts)
		{
			AddFailure(
				SeedResult.FailureReason,
				FString::Printf(
					TEXT("Only %d complete Card fronts are guaranteed; %d required."),
					SeedResult.GuaranteedCompleteFronts,
					Report.Rules.MinimumGuaranteedCompleteFronts));
		}

		for (const FName RequiredRuleId : RequiredResourceRuleIds)
		{
			const FSRResourceEconomyRuleSeedAvailability Availability =
				SeedResult.AvailabilityByRuleId.FindRef(RequiredRuleId);
			FSRResourceEconomyRuleSoakRange& Range = Report.RuleRanges.FindOrAdd(RequiredRuleId);
			Range.MinimumSourcePlanetCount = FMath::Min(
				Range.MinimumSourcePlanetCount, Availability.SourcePlanetCount);
			Range.MaximumSourcePlanetCount = FMath::Max(
				Range.MaximumSourcePlanetCount, Availability.SourcePlanetCount);
			Range.MinimumGuaranteedDepositCount = FMath::Min(
				Range.MinimumGuaranteedDepositCount, Availability.GuaranteedDepositCount);
			Range.MaximumGuaranteedDepositCount = FMath::Max(
				Range.MaximumGuaranteedDepositCount, Availability.GuaranteedDepositCount);
			Range.MinimumPotentialDepositCount = FMath::Min(
				Range.MinimumPotentialDepositCount, Availability.PotentialDepositCount);
			Range.MaximumPotentialDepositCount = FMath::Max(
				Range.MaximumPotentialDepositCount, Availability.PotentialDepositCount);
		}

		SeedResult.bPassed = SeedResult.FailureReason.IsEmpty();
		if (SeedResult.bPassed)
		{
			++Report.PassedSeedCount;
		}
		else
		{
			++Report.FailedSeedCount;
		}
		Report.MinimumObservedPlanetCount = FMath::Min(
			Report.MinimumObservedPlanetCount, SeedResult.PlanetCount);
		Report.MaximumObservedPlanetCount = FMath::Max(
			Report.MaximumObservedPlanetCount, SeedResult.PlanetCount);
		Report.MinimumObservedUniqueEnvironmentCount = FMath::Min(
			Report.MinimumObservedUniqueEnvironmentCount, SeedResult.UniqueEnvironmentCount);
		Report.MaximumObservedUniqueEnvironmentCount = FMath::Max(
			Report.MaximumObservedUniqueEnvironmentCount, SeedResult.UniqueEnvironmentCount);
		Report.MinimumObservedGuaranteedCompleteFronts = FMath::Min(
			Report.MinimumObservedGuaranteedCompleteFronts, SeedResult.GuaranteedCompleteFronts);
		Report.MaximumObservedGuaranteedCompleteFronts = FMath::Max(
			Report.MaximumObservedGuaranteedCompleteFronts, SeedResult.GuaranteedCompleteFronts);
		Report.MinimumObservedPotentialCompleteFronts = FMath::Min(
			Report.MinimumObservedPotentialCompleteFronts, SeedResult.PotentialCompleteFronts);
		Report.MaximumObservedPotentialCompleteFronts = FMath::Max(
			Report.MaximumObservedPotentialCompleteFronts, SeedResult.PotentialCompleteFronts);
	}

	FString StageFailure;
	FSRRunBalanceSupplyStage BasicStage;
	if (!FSRFiniteResourceEconomyModel::BuildReferenceSupplyStage(
		false,
		1,
		Report.Rules.BasicLineStartSeconds,
		Report.Rules.BasicTransitDelaySeconds,
		BasicStage,
		StageFailure))
	{
		AddFailure(Report.FailureReason, StageFailure);
	}
	else
	{
		FSRRunBalanceScenario BasicScenario = MakePressureScenario();
		BasicScenario.SupplyStages.Add(BasicStage);
		Report.BasicLineResult = FSRRunBalanceSimulator::Simulate(BasicScenario);

		FSRRunBalanceSupplyStage ExpansionStage;
		if (!FSRFiniteResourceEconomyModel::BuildReferenceSupplyStage(
			true,
			1,
			Report.Rules.ExpansionLineStartSeconds,
			Report.Rules.DistributedTransitDelaySeconds,
			ExpansionStage,
			StageFailure))
		{
			AddFailure(Report.FailureReason, StageFailure);
		}
		else
		{
			FSRRunBalanceScenario ExpansionScenario = MakePressureScenario();
			ExpansionScenario.SupplyStages.Add(BasicStage);
			ExpansionScenario.SupplyStages.Add(ExpansionStage);
			Report.DistributedExpansionResult =
				FSRRunBalanceSimulator::Simulate(ExpansionScenario);
		}
	}

	Report.RawSingleDepositMiningSeconds =
		Report.EconomyContract.BatchesPerCardDepositSet
		* Report.Rules.MinerCycleSeconds;
	Report.LineFedSingleFrontSeconds =
		Report.EconomyContract.BatchesPerCardDepositSet
		* Report.EconomyContract.FabricationCycleSeconds;
	const double ExpectedBasicExhaustionSeconds =
		Report.Rules.BasicLineStartSeconds
		+ Report.Rules.BasicTransitDelaySeconds
		+ (Report.EconomyContract.BatchesPerCardDepositSet - 1)
			* Report.EconomyContract.FabricationCycleSeconds;
	if (Report.BasicLineResult.Outcome == ESRStellarRunOutcome::Victory
		|| Report.BasicLineResult.SupplyDeliveryCount
			!= Report.EconomyContract.BatchesPerCardDepositSet
		|| !FMath::IsNearlyEqual(
			Report.BasicLineResult.FirstSupplyExhaustionSeconds,
			ExpectedBasicExhaustionSeconds,
			0.5))
	{
		AddFailure(Report.FailureReason, TEXT("The one-front finite recovery envelope drifted."));
	}
	if (Report.DistributedExpansionResult.Outcome != ESRStellarRunOutcome::Victory
		|| Report.DistributedExpansionResult.CompletionSeconds < 1500.0
		|| Report.DistributedExpansionResult.CompletionSeconds > 2100.0)
	{
		AddFailure(Report.FailureReason, TEXT("Delayed distributed expansion misses the 25-35 minute victory target."));
	}

	FinalizeEmptyRanges(Report);
	Report.bDeterministicReplayPassed = true;
	for (int32 SeedIndex = 0; SeedIndex < Report.SeedResults.Num(); ++SeedIndex)
	{
		FRandomStream ReplayRandomStream(Report.SeedResults[SeedIndex].Seed);
		const int32 ReplayPlanetCount = ReplayRandomStream.RandRange(
			Report.Rules.MinimumPlanetCount,
			Report.Rules.MaximumPlanetCount);
		TArray<const USRPlanetDataAsset*> ReplaySelection;
		FSRPlanetEnvironmentSelectionReport ReplaySelectionReport;
		FSRPlanetEnvironmentSelector::SelectWithResourceCoverage(
			PlanetCandidates,
			ReplayPlanetCount,
			Report.Rules.MinimumUniquePlanetTypes,
			RequiredResourceRuleIds,
			ReplayRandomStream,
			ReplaySelection,
			ReplaySelectionReport);
		TArray<FString> ReplayNames;
		for (const USRPlanetDataAsset* Planet : ReplaySelection)
		{
			ReplayNames.Add(IsValid(Planet) && !Planet->VariableName.IsEmpty()
				? Planet->VariableName.ToString()
				: IsValid(Planet) ? Planet->GetName() : TEXT("Invalid"));
		}
		ReplayNames.Sort();
		if (ReplayPlanetCount != Report.SeedResults[SeedIndex].PlanetCount
			|| ReplayNames != Report.SeedResults[SeedIndex].EnvironmentNames
			|| ReplaySelectionReport.bResourceCoverageSatisfied
				!= Report.SeedResults[SeedIndex].bResourceCoverageSatisfied)
		{
			Report.bDeterministicReplayPassed = false;
			AddFailure(
				Report.FailureReason,
				FString::Printf(
					TEXT("Deterministic portfolio replay failed for seed %d."),
					Report.SeedResults[SeedIndex].Seed));
			break;
		}
	}

	if (Report.FailedSeedCount > 0)
	{
		AddFailure(
			Report.FailureReason,
			FString::Printf(TEXT("%d sampled seeds violated the authored portfolio contract."), Report.FailedSeedCount));
	}
	const int32 MinimumAppearanceCount = FMath::Max(1, Report.Rules.SeedCount / 4);
	for (const USRPlanetDataAsset* Candidate : PlanetCandidates)
	{
		if (!IsValid(Candidate) || Candidate->GenerationWeight <= UE_SMALL_NUMBER)
		{
			continue;
		}
		const FString EnvironmentName = Candidate->VariableName.IsEmpty()
			? Candidate->GetName()
			: Candidate->VariableName.ToString();
		if (Report.EnvironmentAppearanceCount.FindRef(EnvironmentName) < MinimumAppearanceCount)
		{
			AddFailure(
				Report.FailureReason,
				FString::Printf(
					TEXT("%s appears in fewer than 25%% of sampled systems."),
					*EnvironmentName));
		}
	}

	Report.bPassed = Report.FailureReason.IsEmpty();
	return Report;
}
