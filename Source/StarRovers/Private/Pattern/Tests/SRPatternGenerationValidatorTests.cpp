#if WITH_DEV_AUTOMATION_TESTS

#include "Pattern/SRPatternGenerationValidator.h"

#include "Misc/AutomationTest.h"

namespace StarRovers::PatternGenerationTests
{
	FSRPatternTransformOperatorSpec MakeRowTransform(
		int32 Row,
		ESRPatternDirection Direction)
	{
		FSRPatternTransformOperatorSpec TransformOperator;
		TransformOperator.SelectionMask.Reset(false);
		for (int32 Column = 0; Column < StarRovers::Pattern::GridSize; ++Column)
		{
			TransformOperator.SelectionMask.SetCellActive(Row, Column, true);
		}
		TransformOperator.Direction = Direction;
		TransformOperator.OrganicGrowthsPerComponent = 0;
		return TransformOperator;
	}

	FSRPatternGenerationGoal MakeExactGoal(const FSRPattern& RequiredPattern)
	{
		FSRPatternGenerationGoal Goal;
		Goal.RequiredPattern = RequiredPattern;
		Goal.RequiredMask.Reset(true);
		return Goal;
	}

	FSRPatternGenerationSourceSpec MakeSource(
		const TCHAR* SourceId,
		const TCHAR* BodyId,
		const FSRPattern& Pattern)
	{
		FSRPatternGenerationSourceSpec Source;
		Source.SourceId = FName(SourceId);
		Source.BodyId = FName(BodyId);
		Source.Pattern = Pattern;
		return Source;
	}

	FSRPatternGenerationBodySpec MakeNeutralBody(const TCHAR* BodyId)
	{
		FSRPatternGenerationBodySpec Body;
		Body.BodyId = FName(BodyId);
		return Body;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRPatternGenerationInterBodyReachabilityTest,
	"StarRovers.Pattern.Generation.InterBodyReachability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRPatternGenerationInterBodyReachabilityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FSRPattern SourcePattern;
	SourcePattern.SetGlyph(2, 0, ESRGlyphType::Metal);
	FSRPattern RequiredPattern;
	RequiredPattern.SetGlyph(2, 2, ESRGlyphType::Metal);
	FSRPatternGenerationValidationRequest Request;
	Request.Sources.Add(StarRovers::PatternGenerationTests::MakeSource(
		TEXT("IronSource"),
		TEXT("GravityBody"),
		SourcePattern));
	Request.Goal.RequiredPattern = RequiredPattern;
	Request.Goal.RequiredMask.Reset(false);
	Request.Goal.RequiredMask.SetCellActive(2, 2, true);
	Request.MaxOperationDepth = 2;
	Request.MaxReachableStates = 128;
	Request.bRequireInterBodyTransfer = true;

	FSRPatternGenerationBodySpec GravityBody =
		StarRovers::PatternGenerationTests::MakeNeutralBody(TEXT("GravityBody"));
	GravityBody.TransformOperators.Add(StarRovers::PatternGenerationTests::MakeRowTransform(
		2,
		ESRPatternDirection::Right));
	GravityBody.Environment.EnvironmentId = FName(TEXT("ReverseGravity"));
	FSRPatternEnvironmentEffectSpec ReversePull;
	ReversePull.EffectKind = ESRPatternEnvironmentEffectKind::DirectionalPull;
	ReversePull.Direction = ESRPatternDirection::Left;
	GravityBody.Environment.Effects.Add(ReversePull);
	Request.Bodies.Add(GravityBody);

	FSRPatternGenerationBodySpec NeutralBody =
		StarRovers::PatternGenerationTests::MakeNeutralBody(TEXT("NeutralBody"));
	NeutralBody.TransformOperators.Add(StarRovers::PatternGenerationTests::MakeRowTransform(
		2,
		ESRPatternDirection::Right));
	Request.Bodies.Add(NeutralBody);

	const FSRPatternGenerationValidationResult Result = FSRPatternGenerationValidator::Validate(Request);
	TestTrue(TEXT("The goal is solvable through the combined body network."), Result.bSolvable);
	TestTrue(TEXT("The validator proves that an inter-body transfer is required."), Result.bRequiresInterBodyTransfer);
	TestEqual(TEXT("The destination needs two facility operations."), Result.MinimumOperationDepth, 2);
	TestTrue(TEXT("The reachable goal is completed on the neutral body."), Result.GoalBodyId == FName(TEXT("NeutralBody")));

	Request.Bodies[0].Environment = FSRPatternEnvironmentSpec();
	const FSRPatternGenerationValidationResult LocallySolvableResult =
		FSRPatternGenerationValidator::Validate(Request);
	TestFalse(TEXT("A profile requiring transport rejects a locally solvable generation."), LocallySolvableResult.bSolvable);
	TestTrue(
		TEXT("The validator reports why the transport constraint was not satisfied."),
		LocallySolvableResult.Failure == ESRPatternGenerationValidationFailure::InterBodyTransferNotRequired);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRPatternGenerationSynthesisReachabilityTest,
	"StarRovers.Pattern.Generation.SynthesisReachability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRPatternGenerationSynthesisReachabilityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FSRPattern OrganicPattern;
	OrganicPattern.SetGlyph(0, 0, ESRGlyphType::Organic);
	FSRPattern PlasmaPattern;
	PlasmaPattern.SetGlyph(4, 4, ESRGlyphType::Plasma);
	FSRPattern CombinedPattern = OrganicPattern;
	CombinedPattern.SetGlyph(4, 4, ESRGlyphType::Plasma);

	FSRPatternGenerationValidationRequest Request;
	Request.Sources.Add(StarRovers::PatternGenerationTests::MakeSource(
		TEXT("OrganicSource"),
		TEXT("SynthesisBody"),
		OrganicPattern));
	Request.Sources.Add(StarRovers::PatternGenerationTests::MakeSource(
		TEXT("PlasmaSource"),
		TEXT("SynthesisBody"),
		PlasmaPattern));
	Request.Bodies.Add(StarRovers::PatternGenerationTests::MakeNeutralBody(TEXT("SynthesisBody")));
	Request.Bodies[0].bAllowSynthesis = true;
	Request.Goal = StarRovers::PatternGenerationTests::MakeExactGoal(CombinedPattern);
	Request.MaxOperationDepth = 1;
	Request.MaxReachableStates = 128;

	const FSRPatternGenerationValidationResult Result = FSRPatternGenerationValidator::Validate(Request);
	TestTrue(TEXT("Two source Patterns can satisfy an exact combined goal through Synthesis."), Result.bSolvable);
	TestFalse(TEXT("Same-body Synthesis does not require transport."), Result.bRequiresInterBodyTransfer);
	TestEqual(TEXT("The combined Pattern is reached in one Synthesis operation."), Result.MinimumOperationDepth, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRPatternGenerationSeparationReachabilityTest,
	"StarRovers.Pattern.Generation.SeparationReachability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRPatternGenerationSeparationReachabilityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FSRPattern MixedPattern;
	MixedPattern.SetGlyph(0, 0, ESRGlyphType::Metal);
	MixedPattern.SetGlyph(4, 4, ESRGlyphType::Organic);
	FSRPattern MetalOnlyPattern;
	MetalOnlyPattern.SetGlyph(0, 0, ESRGlyphType::Metal);

	FSRPatternGenerationValidationRequest Request;
	Request.Sources.Add(StarRovers::PatternGenerationTests::MakeSource(
		TEXT("MixedSource"),
		TEXT("SeparationBody"),
		MixedPattern));
	Request.Bodies.Add(StarRovers::PatternGenerationTests::MakeNeutralBody(TEXT("SeparationBody")));
	FSRPatternSeparationOperatorSpec SeparationOperator;
	SeparationOperator.PrimaryOutputMask.Reset(false);
	SeparationOperator.PrimaryOutputMask.SetCellActive(0, 0, true);
	Request.Bodies[0].SeparationOperators.Add(SeparationOperator);
	Request.Goal = StarRovers::PatternGenerationTests::MakeExactGoal(MetalOnlyPattern);
	Request.MaxOperationDepth = 1;
	Request.MaxReachableStates = 64;

	const FSRPatternGenerationValidationResult Result = FSRPatternGenerationValidator::Validate(Request);
	TestTrue(TEXT("An exact subset goal is reachable through non-duplicating Separation."), Result.bSolvable);
	TestEqual(TEXT("The separated Pattern is reached in one operation."), Result.MinimumOperationDepth, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRPatternGenerationSeededCandidateOrderTest,
	"StarRovers.Pattern.Generation.SeededCandidateOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRPatternGenerationSeededCandidateOrderTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	constexpr int32 CandidateCount = 16;
	constexpr uint32 SystemSignature = 0x91B72F4Du;
	TArray<int32> FirstOrder;
	TArray<int32> RepeatedOrder;
	StarRovers::PatternGeneration::BuildSeededCandidateOrder(
		CandidateCount,
		137,
		SystemSignature,
		FirstOrder);
	StarRovers::PatternGeneration::BuildSeededCandidateOrder(
		CandidateCount,
		137,
		SystemSignature,
		RepeatedOrder);

	TestTrue(TEXT("The same Run seed reproduces the same candidate order."), FirstOrder == RepeatedOrder);
	TestEqual(TEXT("Every candidate is included exactly once."), FirstOrder.Num(), CandidateCount);
	TSet<int32> UniqueIndices;
	for (const int32 CandidateIndex : FirstOrder)
	{
		UniqueIndices.Add(CandidateIndex);
	}
	TestEqual(TEXT("The candidate order is a permutation without duplicates."), UniqueIndices.Num(), CandidateCount);
	for (int32 CandidateIndex = 0; CandidateIndex < CandidateCount; ++CandidateIndex)
	{
		TestTrue(
			FString::Printf(TEXT("Candidate %d remains selectable."), CandidateIndex),
			UniqueIndices.Contains(CandidateIndex));
	}

	TSet<int32> FirstCandidateAcrossRuns;
	for (int32 RunSeed = 1; RunSeed <= 32; ++RunSeed)
	{
		TArray<int32> SeededOrder;
		StarRovers::PatternGeneration::BuildSeededCandidateOrder(
			CandidateCount,
			RunSeed,
			SystemSignature,
			SeededOrder);
		if (!SeededOrder.IsEmpty())
		{
			FirstCandidateAcrossRuns.Add(SeededOrder[0]);
		}
	}
	TestTrue(
		TEXT("Different Run seeds produce more than one first-choice contract."),
		FirstCandidateAcrossRuns.Num() > 1);
	return true;
}

#endif
