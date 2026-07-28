#include "Automation/SRStellarFuelFabricator.h"

#include "Automation/SRResourceInstanceOperations.h"
#include "Automation/SRResourceSystemContent.h"
#include "Simulation/SRSimulationSettings.h"

namespace
{
	FSRStellarFuelFabricationResultV2 MakeFailure(
		ESRStellarFuelFabricationOutcomeV2 Outcome,
		const FString& FailureReason)
	{
		FSRStellarFuelFabricationResultV2 Result;
		Result.Outcome = Outcome;
		Result.FailureReason = FailureReason;
		return Result;
	}

	bool IsBonusFinite(const FSRStellarFuelHandBonusV2& Bonus)
	{
		return FMath::IsFinite(Bonus.B) && FMath::IsFinite(Bonus.C);
	}

	bool ValidateRules(const FSRStellarFuelFabricationRulesV2& Rules, FString& OutFailureReason)
	{
		OutFailureReason.Reset();
		if (Rules.OutputResourceId.IsNone())
		{
			OutFailureReason = TEXT("Stellar Fuel output ResourceId is missing.");
			return false;
		}
		if (!FMath::IsFinite(Rules.BaseEnergyA)
			|| !FMath::IsFinite(Rules.TwinSealEnergyB)
			|| !FMath::IsFinite(Rules.TopologySealEnergyB)
			|| !FMath::IsFinite(Rules.PrismaticCatalystC)
			|| !IsBonusFinite(Rules.OnePairBonus)
			|| !IsBonusFinite(Rules.TwoPairBonus)
			|| !IsBonusFinite(Rules.ThreeOfAKindBonus)
			|| !IsBonusFinite(Rules.FiveGradeSequenceBonus)
			|| !IsBonusFinite(Rules.FullHouseBonus)
			|| !IsBonusFinite(Rules.FourOfAKindBonus))
		{
			OutFailureReason = TEXT("Stellar Fuel formula values must be finite.");
			return false;
		}
		return true;
	}

	int32 BuildFabricatorCardKey(ESRResourceSpectrum Spectrum, int32 Grade)
	{
		return (static_cast<int32>(Spectrum) << 8) | (Grade & 0xFF);
	}

	bool IsSupportedCardFamily(ESRResourceFamily Family)
	{
		switch (Family)
		{
		case ESRResourceFamily::Metal:
		case ESRResourceFamily::Crystal:
		case ESRResourceFamily::Organic:
		case ESRResourceFamily::Plasma:
		case ESRResourceFamily::Void:
			return true;
		case ESRResourceFamily::None:
		default:
			return false;
		}
	}

	bool IsSupportedCardSpectrum(ESRResourceSpectrum Spectrum)
	{
		switch (Spectrum)
		{
		case ESRResourceSpectrum::Red:
		case ESRResourceSpectrum::Green:
		case ESRResourceSpectrum::Blue:
		case ESRResourceSpectrum::Yellow:
			return true;
		case ESRResourceSpectrum::None:
		default:
			return false;
		}
	}

	ESRStellarFuelHandV2 ResolveHand(const TMap<int32, int32>& UniqueCardKeysPerGrade)
	{
		int32 PairGradeCount = 0;
		bool bHasThree = false;
		bool bHasFour = false;
		bool bHasFiveGradeSequence = UniqueCardKeysPerGrade.Num()
			== StarRovers::StellarFuel::RequiredCardCount;
		for (int32 Grade = StarRovers::Resources::MinimumGrade;
			Grade <= StarRovers::Resources::MaximumGrade;
			++Grade)
		{
			const int32 Count = UniqueCardKeysPerGrade.FindRef(Grade);
			bHasFiveGradeSequence &= Count > 0;
			PairGradeCount += Count >= 2 ? 1 : 0;
			bHasThree |= Count >= 3;
			bHasFour |= Count >= 4;
		}

		if (bHasFour)
		{
			return ESRStellarFuelHandV2::FourOfAKind;
		}
		if (bHasThree && PairGradeCount >= 2)
		{
			return ESRStellarFuelHandV2::FullHouse;
		}
		if (bHasFiveGradeSequence)
		{
			return ESRStellarFuelHandV2::FiveGradeSequence;
		}
		if (bHasThree)
		{
			return ESRStellarFuelHandV2::ThreeOfAKind;
		}
		if (PairGradeCount >= 2)
		{
			return ESRStellarFuelHandV2::TwoPair;
		}
		if (PairGradeCount == 1)
		{
			return ESRStellarFuelHandV2::OnePair;
		}
		return ESRStellarFuelHandV2::Unranked;
	}

	FSRStellarFuelHandBonusV2 ResolveHandBonus(
		ESRStellarFuelHandV2 Hand,
		const FSRStellarFuelFabricationRulesV2& Rules)
	{
		switch (Hand)
		{
		case ESRStellarFuelHandV2::OnePair: return Rules.OnePairBonus;
		case ESRStellarFuelHandV2::TwoPair: return Rules.TwoPairBonus;
		case ESRStellarFuelHandV2::ThreeOfAKind: return Rules.ThreeOfAKindBonus;
		case ESRStellarFuelHandV2::FiveGradeSequence: return Rules.FiveGradeSequenceBonus;
		case ESRStellarFuelHandV2::FullHouse: return Rules.FullHouseBonus;
		case ESRStellarFuelHandV2::FourOfAKind: return Rules.FourOfAKindBonus;
		case ESRStellarFuelHandV2::Unranked:
		default: return FSRStellarFuelHandBonusV2();
		}
	}

	bool HasValidConvergenceTopology(const TArray<FSRResourceInstance>& Cards)
	{
		TSet<FName> ExportedOrigins;
		for (const FSRResourceInstance& Card : Cards)
		{
			const FSRResourceLogisticsMetadata& Metadata = Card.LogisticsMetadata;
			if (!Metadata.OriginBodyId.IsNone()
				&& Metadata.LastProcessedBodyId == Metadata.OriginBodyId
				&& Metadata.LastTransitSourceBodyId == Metadata.OriginBodyId
				&& Metadata.TransitCount > 0)
			{
				ExportedOrigins.Add(Metadata.OriginBodyId);
			}
		}
		return ExportedOrigins.Num() >= 3;
	}

	bool HasValidFoundryTopology(
		const TArray<FSRResourceInstance>& Cards,
		FName FabricatorBodyId)
	{
		if (FabricatorBodyId.IsNone())
		{
			return false;
		}
		for (const FSRResourceInstance& Card : Cards)
		{
			if (Card.LogisticsMetadata.LastProcessedBodyId != FabricatorBodyId)
			{
				return false;
			}
		}
		return true;
	}

	bool HasValidPilgrimTopology(const TArray<FSRResourceInstance>& Cards)
	{
		for (const FSRResourceInstance& Card : Cards)
		{
			if (Card.LogisticsMetadata.OriginBodyId.IsNone()
				|| !Card.LogisticsMetadata.bHasBeenProcessedOutsideOrigin)
			{
				return false;
			}
		}
		return true;
	}

	FString BuildCardKeyList(const TArray<FSRStellarFuelCardContributionV2>& Cards)
	{
		TArray<FString> Keys;
		Keys.Reserve(Cards.Num());
		const UEnum* SpectrumEnum = StaticEnum<ESRResourceSpectrum>();
		for (const FSRStellarFuelCardContributionV2& Card : Cards)
		{
			const FString SpectrumName = SpectrumEnum
				? SpectrumEnum->GetNameStringByValue(static_cast<int64>(Card.Spectrum))
				: TEXT("?");
			Keys.Add(FString::Printf(TEXT("%s%d"), *SpectrumName.Left(1), Card.Grade));
		}
		return FString::Join(Keys, TEXT(", "));
	}
}

bool FSRStellarFuelFabricator::IsResourceV2RulesetActive()
{
	const USRSimulationSettings* Settings = GetDefault<USRSimulationSettings>();
	return IsValid(Settings)
		&& Settings->ResourceRulesetVersion == ESRResourceRulesetVersion::ResourceV2;
}

bool FSRStellarFuelFabricator::ShouldRouteThroughResourceV2(
	const USRFacilityDataAsset* FacilityDataAsset)
{
	return IsResourceV2RulesetActive()
		&& IsValid(FacilityDataAsset)
		&& FacilityDataAsset->FacilityKind == ESRFacilityKind::Standard
		&& FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Synthesize
		&& FacilityDataAsset->ResourceV2Synthesis.SynthesisRole
			== ESRFacilitySynthesisRoleV2::StellarFuelFabricator;
}

bool FSRStellarFuelFabricator::ValidateFacilityDefinition(
	const USRFacilityDataAsset* FacilityDataAsset,
	FString& OutFailureReason)
{
	OutFailureReason.Reset();
	if (!IsValid(FacilityDataAsset))
	{
		OutFailureReason = TEXT("Facility Data Asset is invalid.");
		return false;
	}
	if (FacilityDataAsset->FacilityDefinitionVersion
		!= StarRovers::Facilities::CurrentFacilityDefinitionVersion)
	{
		OutFailureReason = TEXT("Stellar Fuel Fabricator requires the current Facility definition version.");
		return false;
	}
	if (FacilityDataAsset->FacilityKind != ESRFacilityKind::Standard
		|| FacilityDataAsset->OperationKind != ESRFacilityOperationKind::Synthesize
		|| FacilityDataAsset->ResourceV2Synthesis.SynthesisRole
			!= ESRFacilitySynthesisRoleV2::StellarFuelFabricator)
	{
		OutFailureReason = TEXT("Facility is not configured as a Resource V2 Stellar Fuel Fabricator.");
		return false;
	}
	if (FacilityDataAsset->InputInventory.SlotCount != StarRovers::StellarFuel::RequiredCardCount
		|| FacilityDataAsset->OutputInventory.SlotCount < 1)
	{
		OutFailureReason = TEXT("Stellar Fuel Fabricator requires exactly five input slots and at least one output slot.");
		return false;
	}
	return ValidateRules(
		FacilityDataAsset->ResourceV2Synthesis.StellarFuelRules,
		OutFailureReason);
}

bool FSRStellarFuelFabricator::ValidateInputCard(
	const FSRResourceInstance& InputCard,
	FString& OutFailureReason,
	ESRStellarFuelFabricationOutcomeV2* OutFailureOutcome)
{
	OutFailureReason.Reset();
	auto Fail = [&OutFailureReason, OutFailureOutcome](
		ESRStellarFuelFabricationOutcomeV2 Outcome,
		const TCHAR* FailureReason)
	{
		OutFailureReason = FailureReason;
		if (OutFailureOutcome)
		{
			*OutFailureOutcome = Outcome;
		}
		return false;
	};

	if (InputCard.ResourceId.IsNone()
		|| InputCard.ResourceClass != ESRResourceClass::Card
		|| !IsSupportedCardFamily(InputCard.Family)
		|| !IsSupportedCardSpectrum(InputCard.Spectrum)
		|| InputCard.Grade < StarRovers::Resources::MinimumGrade
		|| InputCard.Grade > StarRovers::Resources::MaximumGrade
		|| InputCard.StackCount <= 0)
	{
		return Fail(
			ESRStellarFuelFabricationOutcomeV2::InvalidCard,
			TEXT("Resource is not a valid Family Card."));
	}
	if (InputCard.ResourceSchemaVersion != StarRovers::Resources::CurrentResourceSchemaVersion)
	{
		return Fail(
			ESRStellarFuelFabricationOutcomeV2::UnsupportedSchema,
			TEXT("Card must use the current Resource schema."));
	}
	if (!FMath::IsFinite(InputCard.CurrentEnergy) || InputCard.CurrentEnergy < 0.0)
	{
		return Fail(
			ESRStellarFuelFabricationOutcomeV2::InvalidEnergy,
			TEXT("Card has invalid Current Energy."));
	}
	if (!InputCard.FuelImprintSlot.ImprintId.IsNone())
	{
		FSRFuelImprintDefinitionV2 ImprintDefinition;
		if (!FSRResourceSystemContent::TryGetFuelImprintDefinition(
			InputCard.FuelImprintSlot.ImprintId,
			ImprintDefinition))
		{
			return Fail(
				ESRStellarFuelFabricationOutcomeV2::InvalidFuelImprint,
				TEXT("Card has an unknown Fuel Imprint."));
		}
	}

	if (OutFailureOutcome)
	{
		*OutFailureOutcome = ESRStellarFuelFabricationOutcomeV2::Success;
	}
	return true;
}

FSRStellarFuelFabricationResultV2 FSRStellarFuelFabricator::EvaluateCards(
	const TArray<FSRResourceInstance>& InputCards,
	const FSRStellarFuelFabricationRulesV2& Rules,
	FName FabricatorBodyId)
{
	FString RulesFailure;
	if (!ValidateRules(Rules, RulesFailure))
	{
		return MakeFailure(ESRStellarFuelFabricationOutcomeV2::InvalidRules, RulesFailure);
	}
	if (InputCards.Num() != StarRovers::StellarFuel::RequiredCardCount)
	{
		return MakeFailure(
			ESRStellarFuelFabricationOutcomeV2::WrongCardCount,
			TEXT("Stellar Fuel Fabricator requires exactly five Card inputs."));
	}

	FSRStellarFuelFabricationResultV2 Result;
	Result.Outcome = ESRStellarFuelFabricationOutcomeV2::Success;
	Result.CardContributions.Reserve(InputCards.Num());
	TMap<int32, TArray<int32>> InputIndicesPerCardKey;
	TMap<int32, int32> UniqueCardKeysPerGrade;
	TSet<ESRResourceSpectrum> PresentSpectra;
	TMap<FName, int32> ImprintCounts;

	for (int32 InputIndex = 0; InputIndex < InputCards.Num(); ++InputIndex)
	{
		const FSRResourceInstance& Card = InputCards[InputIndex];
		FString CardFailureReason;
		ESRStellarFuelFabricationOutcomeV2 CardFailureOutcome =
			ESRStellarFuelFabricationOutcomeV2::InvalidCard;
		if (!ValidateInputCard(Card, CardFailureReason, &CardFailureOutcome))
		{
			return MakeFailure(
				CardFailureOutcome,
				FString::Printf(TEXT("Input %d: %s"), InputIndex, *CardFailureReason));
		}
		if (!Card.FuelImprintSlot.ImprintId.IsNone())
		{
			++ImprintCounts.FindOrAdd(Card.FuelImprintSlot.ImprintId);
		}

		FSRStellarFuelCardContributionV2& Contribution = Result.CardContributions.AddDefaulted_GetRef();
		Contribution.InputIndex = InputIndex;
		Contribution.ResourceId = Card.ResourceId;
		Contribution.Spectrum = Card.Spectrum;
		Contribution.Grade = Card.Grade;
		Contribution.CurrentEnergy = Card.CurrentEnergy;
		Contribution.FuelImprintId = Card.FuelImprintSlot.ImprintId;
		const int32 CardKey = BuildFabricatorCardKey(Card.Spectrum, Card.Grade);
		TArray<int32>& CardKeyInputIndices = InputIndicesPerCardKey.FindOrAdd(CardKey);
		Contribution.bUniqueCardKey = CardKeyInputIndices.IsEmpty();
		if (Contribution.bUniqueCardKey)
		{
			++UniqueCardKeysPerGrade.FindOrAdd(Card.Grade);
			PresentSpectra.Add(Card.Spectrum);
		}
		CardKeyInputIndices.Add(InputIndex);
		Result.InputEnergySum += Card.CurrentEnergy;
		if (!FMath::IsFinite(Result.InputEnergySum))
		{
			return MakeFailure(
				ESRStellarFuelFabricationOutcomeV2::NonFiniteResult,
				TEXT("Input Energy sum overflowed to a non-finite value."));
		}
	}

	Result.UniqueCardKeyCount = InputIndicesPerCardKey.Num();
	Result.Hand = ResolveHand(UniqueCardKeysPerGrade);
	const FSRStellarFuelHandBonusV2 HandBonus = ResolveHandBonus(Result.Hand, Rules);
	Result.HandEnergyB = HandBonus.B;
	Result.HandCatalystC = HandBonus.C;

	const FName TwinSealId = FSRResourceSystemContent::GetFuelImprintId(ESRFuelImprintContentV2::TwinSeal);
	for (const TPair<int32, TArray<int32>>& CardKeyPair : InputIndicesPerCardKey)
	{
		const TArray<int32>& CardIndices = CardKeyPair.Value;
		if (CardIndices.IsEmpty())
		{
			continue;
		}
		const int32 Grade = InputCards[CardIndices[0]].Grade;
		if (UniqueCardKeysPerGrade.FindRef(Grade) < 2)
		{
			continue;
		}
		for (const int32 InputIndex : CardIndices)
		{
			if (InputCards[InputIndex].FuelImprintSlot.ImprintId == TwinSealId)
			{
				Result.CardContributions[InputIndex].bTwinSealContributed = true;
				++Result.EffectiveTwinSealCount;
				break;
			}
		}
	}
	Result.TwinSealEnergyB = static_cast<double>(Result.EffectiveTwinSealCount)
		* Rules.TwinSealEnergyB;

	const FName ConvergenceSealId = FSRResourceSystemContent::GetFuelImprintId(ESRFuelImprintContentV2::ConvergenceSeal);
	const FName FoundrySealId = FSRResourceSystemContent::GetFuelImprintId(ESRFuelImprintContentV2::FoundrySeal);
	const FName PilgrimSealId = FSRResourceSystemContent::GetFuelImprintId(ESRFuelImprintContentV2::PilgrimSeal);
	int32 EligibleTopologyImprintCount = 0;
	auto AddSatisfiedTopology = [&Result, &ImprintCounts, &EligibleTopologyImprintCount](FName ImprintId, bool bCondition)
	{
		const int32 Count = ImprintCounts.FindRef(ImprintId);
		if (bCondition && Count > 0)
		{
			Result.SatisfiedTopologySealIds.Add(ImprintId);
			EligibleTopologyImprintCount += Count;
		}
	};
	AddSatisfiedTopology(ConvergenceSealId, HasValidConvergenceTopology(InputCards));
	AddSatisfiedTopology(FoundrySealId, HasValidFoundryTopology(InputCards, FabricatorBodyId));
	AddSatisfiedTopology(PilgrimSealId, HasValidPilgrimTopology(InputCards));
	if (!Result.SatisfiedTopologySealIds.IsEmpty())
	{
		// Stable priority avoids input-order dependence. Current prototype values are equal.
		Result.AppliedTopologySealId = Result.SatisfiedTopologySealIds[0];
		Result.TopologySealEnergyB = Rules.TopologySealEnergyB;
		Result.SuppressedTopologySealCount = FMath::Max(0, EligibleTopologyImprintCount - 1);
	}

	const FName PrismaticCatalystId = FSRResourceSystemContent::GetFuelImprintId(
		ESRFuelImprintContentV2::PrismaticCatalyst);
	const int32 PrismaticCatalystCount = ImprintCounts.FindRef(PrismaticCatalystId);
	Result.bPrismaticSpectrumConditionMet = PresentSpectra.Num() == 4;
	if (Result.bPrismaticSpectrumConditionMet && PrismaticCatalystCount > 0)
	{
		Result.EffectivePrismaticCatalystCount = 1;
		Result.SuppressedPrismaticCatalystCount = FMath::Max(0, PrismaticCatalystCount - 1);
		Result.PrismaticCatalystC = Rules.PrismaticCatalystC;
	}

	Result.FormulaA = Rules.BaseEnergyA;
	Result.FormulaB = Result.InputEnergySum
		+ Result.HandEnergyB
		+ Result.TwinSealEnergyB
		+ Result.TopologySealEnergyB;
	Result.FormulaC = 1.0
		+ Result.HandCatalystC
		+ Result.PrismaticCatalystC;
	Result.UnclampedFuelEnergy = Result.FormulaA + (Result.FormulaB * Result.FormulaC);
	if (!FMath::IsFinite(Result.FormulaB)
		|| !FMath::IsFinite(Result.FormulaC)
		|| !FMath::IsFinite(Result.UnclampedFuelEnergy))
	{
		return MakeFailure(
			ESRStellarFuelFabricationOutcomeV2::NonFiniteResult,
			TEXT("Final Stellar Fuel formula overflowed to a non-finite value."));
	}
	Result.FuelEnergy = Rules.bClampFinalEnergyAtZero
		? FMath::Max(0.0, Result.UnclampedFuelEnergy)
		: Result.UnclampedFuelEnergy;
	Result.ClampEnergyDelta = Result.FuelEnergy - Result.UnclampedFuelEnergy;

	Result.OutputFuel.ResourceSchemaVersion = StarRovers::Resources::CurrentResourceSchemaVersion;
	Result.OutputFuel.ResourceId = Rules.OutputResourceId;
	Result.OutputFuel.ResourceClass = ESRResourceClass::StellarFuel;
	Result.OutputFuel.Family = ESRResourceFamily::None;
	Result.OutputFuel.CurrentEnergy = Result.FuelEnergy;
	Result.OutputFuel.Spectrum = ESRResourceSpectrum::None;
	Result.OutputFuel.Grade = StarRovers::Resources::MinimumGrade;
	Result.OutputFuel.StackCount = 1;
	StarRovers::Resources::InitializeResourceOrigin(Result.OutputFuel, FabricatorBodyId);
	StarRovers::Resources::SynchronizeResourceV2RuntimeStateToLegacy(Result.OutputFuel);
	return Result;
}

FSRStellarFuelFabricationResultV2 FSRStellarFuelFabricator::Evaluate(
	const FSRFacilityInstance& FacilityInstance,
	const TArray<FSRResourceInstance>& InputCards,
	FName FabricatorBodyId)
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	FString DefinitionFailure;
	if (!ValidateFacilityDefinition(FacilityDataAsset, DefinitionFailure))
	{
		return MakeFailure(ESRStellarFuelFabricationOutcomeV2::InvalidRules, DefinitionFailure);
	}
	return EvaluateCards(
		InputCards,
		FacilityDataAsset->ResourceV2Synthesis.StellarFuelRules,
		FabricatorBodyId);
}

FString FSRStellarFuelFabricator::BuildPreviewSummary(
	const FSRStellarFuelFabricationResultV2& Result)
{
	if (!Result.IsSuccess())
	{
		return FString::Printf(
			TEXT("Stellar Fuel Fabricator V2 unavailable\n%s"),
			Result.FailureReason.IsEmpty() ? TEXT("Unknown failure") : *Result.FailureReason);
	}

	const UEnum* HandEnum = StaticEnum<ESRStellarFuelHandV2>();
	const FString HandName = HandEnum
		? HandEnum->GetDisplayNameTextByValue(static_cast<int64>(Result.Hand)).ToString()
		: TEXT("Unknown");
	return FString::Printf(
		TEXT("Stellar Fuel Fabricator V2\n")
		TEXT("Cards: %s | Unique Keys: %d/5\n")
		TEXT("Hand: %s (B %+.1f, C %+.1f)\n")
		TEXT("B = %.1f Input %+.1f Hand %+.1f Twin %+.1f Topology = %.1f\n")
		TEXT("C = 1 %+.1f Hand %+.1f Prismatic = %.1f\n")
		TEXT("Topology: %s | Twin: %d | Catalyst: %d\n")
		TEXT("Final: %.1f + %.1f * %.1f %+.1f Clamp = %.1f"),
		*BuildCardKeyList(Result.CardContributions),
		Result.UniqueCardKeyCount,
		*HandName,
		Result.HandEnergyB,
		Result.HandCatalystC,
		Result.InputEnergySum,
		Result.HandEnergyB,
		Result.TwinSealEnergyB,
		Result.TopologySealEnergyB,
		Result.FormulaB,
		Result.HandCatalystC,
		Result.PrismaticCatalystC,
		Result.FormulaC,
		Result.AppliedTopologySealId.IsNone() ? TEXT("None") : *Result.AppliedTopologySealId.ToString(),
		Result.EffectiveTwinSealCount,
		Result.EffectivePrismaticCatalystCount,
		Result.FormulaA,
		Result.FormulaB,
		Result.FormulaC,
		Result.ClampEnergyDelta,
		Result.FuelEnergy);
}
