#if WITH_DEV_AUTOMATION_TESTS

#include "Pattern/SRStellarPatternContract.h"

#include "Misc/AutomationTest.h"

namespace StarRovers::StellarContractTests
{
	FSRStellarPatternContract MakeContract()
	{
		FSRStellarPatternContract Contract;
		Contract.ContractId = FName(TEXT("TestContract"));
		Contract.RequiredPattern.Reset();
		Contract.RequiredPattern.SetGlyph(2, 2, ESRGlyphType::Metal);
		Contract.RequiredMask.Reset(false);
		Contract.RequiredMask.SetCellActive(2, 2, true);
		Contract.BaseScorePerPattern = 10;
		Contract.RequiredScorePerCycle = 20;
		Contract.RequiredScoreGrowthPerCycle = 5;
		Contract.StellarHealthMaximum = 30.0;
		Contract.StartingStellarHealth = 30.0;
		Contract.InitialStellarHealthDecreasePerSecond = 2.0;
		Contract.StellarHealthDecreaseMultiplierPerPeriod = 1.5;
		Contract.StellarHealthRestoredPerPatternScore = 0.5;
		Contract.bAllowOverlappingBonusHands = false;
		Contract.BonusRules.Reset();
		return Contract;
	}

	FSRPatternHandRule MakeSameGlyphRule(
		const TCHAR* RuleId,
		int32 BonusScore,
		int32 MaximumMatches,
		std::initializer_list<FIntPoint> Cells)
	{
		FSRPatternHandRule Rule;
		Rule.RuleId = FName(RuleId);
		Rule.DisplayName = FText::FromString(FString(RuleId));
		Rule.RuleKind = ESRPatternHandRuleKind::SameGlyphShape;
		Rule.Rarity = ESRPatternHandRarity::Common;
		Rule.TransformPolicy = ESRPatternHandTransformPolicy::RotateAndTranslate;
		Rule.BonusScore = BonusScore;
		Rule.MaximumMatches = MaximumMatches;
		Rule.ShapeMask.Reset(false);
		for (const FIntPoint& Cell : Cells)
		{
			Rule.ShapeMask.SetCellActive(Cell.X, Cell.Y, true);
		}
		return Rule;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStellarContractDemandMaskTest,
	"StarRovers.Pattern.StellarContract.DemandMask",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStellarContractDemandMaskTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FSRStellarPatternContract Contract = StarRovers::StellarContractTests::MakeContract();
	Contract.BaseScorePerPattern = 7;
	Contract.RequiredMask.SetCellActive(2, 3, true);

	FSRPattern MatchingPattern;
	MatchingPattern.SetGlyph(2, 2, ESRGlyphType::Metal);
	MatchingPattern.SetGlyph(0, 0, ESRGlyphType::Plasma);
	const FSRStellarPatternScoreResult MatchingResult =
		FSRStellarPatternContractResolver::ScorePattern(MatchingPattern, 3, Contract);
	TestTrue(TEXT("Cells outside the demand Mask do not affect qualification."), MatchingResult.bMatchesDemand);
	TestEqual(TEXT("A homogeneous stack scores each identical Pattern."), MatchingResult.TotalScore, int64(21));

	FSRPattern MismatchingPattern = MatchingPattern;
	MismatchingPattern.SetGlyph(2, 3, ESRGlyphType::Organic);
	const FSRStellarPatternScoreResult MismatchingResult =
		FSRStellarPatternContractResolver::ScorePattern(MismatchingPattern, 3, Contract);
	TestFalse(TEXT("An active demand cell that explicitly requires Empty rejects an occupied cell."), MismatchingResult.bMatchesDemand);
	TestEqual(TEXT("A rejected Pattern never contributes partial score."), MismatchingResult.TotalScore, int64(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStellarContractNonOverlappingHandsTest,
	"StarRovers.Pattern.StellarContract.NonOverlappingHands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStellarContractNonOverlappingHandsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FSRStellarPatternContract Contract = StarRovers::StellarContractTests::MakeContract();
	FSRPatternHandRule LineRule = StarRovers::StellarContractTests::MakeSameGlyphRule(
		TEXT("Line"),
		5,
		5,
		{ FIntPoint(0, 0), FIntPoint(0, 1), FIntPoint(0, 2), FIntPoint(0, 3), FIntPoint(0, 4) });
	FSRPatternHandRule CrossRule = StarRovers::StellarContractTests::MakeSameGlyphRule(
		TEXT("Cross"),
		14,
		1,
		{ FIntPoint(0, 1), FIntPoint(1, 0), FIntPoint(1, 1), FIntPoint(1, 2), FIntPoint(2, 1) });
	CrossRule.Rarity = ESRPatternHandRarity::Rare;
	Contract.BonusRules = { LineRule, CrossRule };

	FSRPattern CrossPattern;
	for (int32 Index = 0; Index < StarRovers::Pattern::GridSize; ++Index)
	{
		CrossPattern.SetGlyph(2, Index, ESRGlyphType::Metal);
		CrossPattern.SetGlyph(Index, 2, ESRGlyphType::Metal);
	}

	const FSRStellarPatternScoreResult NonOverlappingResult =
		FSRStellarPatternContractResolver::ScorePattern(CrossPattern, 1, Contract);
	TestEqual(TEXT("The higher-value Cross claims its cells before overlapping Lines."), NonOverlappingResult.HandMatches.Num(), 1);
	TestTrue(
		TEXT("The selected hand is the Rare Cross."),
		NonOverlappingResult.HandMatches.Num() == 1
			&& NonOverlappingResult.HandMatches[0].RuleId == FName(TEXT("Cross")));
	TestEqual(TEXT("Non-overlapping scoring prevents nested-hand score multiplication."), NonOverlappingResult.TotalScore, int64(24));

	Contract.bAllowOverlappingBonusHands = true;
	const FSRStellarPatternScoreResult OverlappingResult =
		FSRStellarPatternContractResolver::ScorePattern(CrossPattern, 1, Contract);
	TestEqual(TEXT("An explicitly permissive contract can score the Cross and both Lines."), OverlappingResult.HandMatches.Num(), 3);
	TestEqual(TEXT("Overlapping hand scoring remains data controlled."), OverlappingResult.TotalScore, int64(34));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStellarContractExactTransformedHandTest,
	"StarRovers.Pattern.StellarContract.ExactTransformedHand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStellarContractExactTransformedHandTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FSRStellarPatternContract Contract = StarRovers::StellarContractTests::MakeContract();
	FSRPatternHandRule ExactRule;
	ExactRule.RuleId = FName(TEXT("RotatedSequence"));
	ExactRule.RuleKind = ESRPatternHandRuleKind::ExactGlyphShape;
	ExactRule.Rarity = ESRPatternHandRarity::Epic;
	ExactRule.TransformPolicy = ESRPatternHandTransformPolicy::RotateReflectAndTranslate;
	ExactRule.BonusScore = 20;
	ExactRule.MaximumMatches = 1;
	ExactRule.ShapeMask.Reset(false);
	ExactRule.ShapeMask.SetCellActive(0, 0, true);
	ExactRule.ShapeMask.SetCellActive(1, 0, true);
	ExactRule.ShapeMask.SetCellActive(1, 1, true);
	ExactRule.RequiredPattern.Reset();
	ExactRule.RequiredPattern.SetGlyph(0, 0, ESRGlyphType::Metal);
	ExactRule.RequiredPattern.SetGlyph(1, 0, ESRGlyphType::Organic);
	ExactRule.RequiredPattern.SetGlyph(1, 1, ESRGlyphType::Crystal);
	Contract.BonusRules.Add(ExactRule);

	FSRPattern RotatedPattern;
	RotatedPattern.SetGlyph(2, 2, ESRGlyphType::Metal);
	// A translated 90-degree copy of Metal/Organic/Crystal occupies the upper left.
	RotatedPattern.SetGlyph(0, 1, ESRGlyphType::Metal);
	RotatedPattern.SetGlyph(0, 0, ESRGlyphType::Organic);
	RotatedPattern.SetGlyph(1, 0, ESRGlyphType::Crystal);

	const FSRStellarPatternScoreResult Result =
		FSRStellarPatternContractResolver::ScorePattern(RotatedPattern, 1, Contract);
	TestTrue(TEXT("A translated 90-degree exact glyph sequence is found deterministically."), Result.bMatchesDemand && Result.HandMatches.Num() == 1);
	TestEqual(TEXT("The exact transformed hand awards its configured Epic score."), Result.TotalScore, int64(30));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStellarContractCycleSettlementTest,
	"StarRovers.Pattern.StellarContract.CycleSettlement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStellarContractCycleSettlementTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FSRStellarPatternContract Contract = StarRovers::StellarContractTests::MakeContract();
	const FSRStellarContractCycleSettlement DeficitSettlement =
		FSRStellarPatternContractResolver::SettleCycle(Contract, 0, 12);
	TestTrue(TEXT("A valid contract settles deterministically."), DeficitSettlement.bValid);
	TestEqual(TEXT("Cycle zero uses the base requirement."), DeficitSettlement.RequiredScore, int64(20));
	TestEqual(TEXT("Missing score is explicit."), DeficitSettlement.MissingScore, int64(8));
	TestFalse(TEXT("Missing score does not meet the score target."), DeficitSettlement.bRequirementMet);

	const FSRStellarContractCycleSettlement SurplusSettlement =
		FSRStellarPatternContractResolver::SettleCycle(Contract, 1, 35);
	TestEqual(TEXT("Linear requirement growth is cycle-indexed."), SurplusSettlement.RequiredScore, int64(25));
	TestEqual(TEXT("Surplus score is retained as score telemetry."), SurplusSettlement.SurplusScore, int64(10));
	TestTrue(TEXT("Surplus score meets the score target."), SurplusSettlement.bRequirementMet);

	TestTrue(
		TEXT("Period zero uses the configured initial per-second health decrease."),
		FMath::IsNearlyEqual(
			FSRStellarPatternContractResolver::GetStellarHealthDecreasePerSecondForPeriod(Contract, 0),
			2.0));
	TestTrue(
		TEXT("The health decrease multiplier compounds once per Period."),
		FMath::IsNearlyEqual(
			FSRStellarPatternContractResolver::GetStellarHealthDecreasePerSecondForPeriod(Contract, 2),
			4.5));
	TestTrue(
		TEXT("Every accepted Pattern score point restores its configured health value."),
		FMath::IsNearlyEqual(
			FSRStellarPatternContractResolver::GetStellarHealthRestorationForScore(Contract, 10),
			5.0));
	return true;
}

#endif
