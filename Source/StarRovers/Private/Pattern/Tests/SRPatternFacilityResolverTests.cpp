#if WITH_DEV_AUTOMATION_TESTS

#include "Pattern/SRPatternFacilityResolver.h"

#include "Misc/AutomationTest.h"

namespace StarRovers::PatternFacilityTests
{
	FSRPatternTransformOperatorSpec MakeSingleCellTransform(
		int32 Row,
		int32 Column,
		ESRPatternDirection Direction)
	{
		FSRPatternTransformOperatorSpec OperatorSpec;
		OperatorSpec.SelectionMask.Reset(false);
		OperatorSpec.SelectionMask.SetCellActive(Row, Column, true);
		OperatorSpec.Direction = Direction;
		OperatorSpec.OrganicGrowthsPerComponent = 0;
		return OperatorSpec;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRPatternFacilityTransformTest,
	"StarRovers.Pattern.Facility.Transform",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRPatternFacilityTransformTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FSRPattern InputPattern;
	InputPattern.SetGlyph(2, 0, ESRGlyphType::Metal);
	const FSRPatternTransformOperatorSpec OperatorSpec =
		StarRovers::PatternFacilityTests::MakeSingleCellTransform(2, 0, ESRPatternDirection::Right);

	const FSRPatternFacilityResolveResult Result = FSRPatternFacilityResolver::ResolveTransform(
		InputPattern,
		OperatorSpec);
	TestTrue(TEXT("Transform succeeds for a canonical input and operator."), Result.bSucceeded);
	TestEqual(TEXT("Transform creates exactly one output."), Result.OutputPatterns.Num(), 1);
	if (Result.OutputPatterns.Num() == 1)
	{
		TestTrue(
			TEXT("A Transform facility always applies a fixed one-cell movement command."),
			Result.OutputPatterns[0].GetGlyph(2, 1) == ESRGlyphType::Metal);
		TestTrue(
			TEXT("The input Pattern is immutable."),
			InputPattern.GetGlyph(2, 0) == ESRGlyphType::Metal);
	}
	for (const FSRPatternTraceEvent& TraceEvent : Result.TraceEvents)
	{
		TestEqual(TEXT("Every Transform trace targets output zero."), TraceEvent.OutputIndex, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRPatternFacilitySynthesisTest,
	"StarRovers.Pattern.Facility.Synthesis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRPatternFacilitySynthesisTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FSRPattern BasePattern;
	BasePattern.SetGlyph(0, 0, ESRGlyphType::Metal);
	BasePattern.SetGlyph(1, 1, ESRGlyphType::Organic);
	FSRPattern OverlayPattern;
	OverlayPattern.SetGlyph(0, 0, ESRGlyphType::Organic);
	OverlayPattern.SetGlyph(1, 1, ESRGlyphType::Metal);
	OverlayPattern.SetGlyph(2, 2, ESRGlyphType::Plasma);

	const FSRPatternFacilityResolveResult Result = FSRPatternFacilityResolver::ResolveSynthesis(
		BasePattern,
		OverlayPattern);
	TestTrue(TEXT("Synthesis succeeds for two canonical inputs."), Result.bSucceeded);
	TestEqual(TEXT("Synthesis creates exactly one output."), Result.OutputPatterns.Num(), 1);
	if (Result.OutputPatterns.Num() == 1)
	{
		const FSRPattern& OutputPattern = Result.OutputPatterns[0];
		TestTrue(TEXT("A losing overlay glyph leaves the base glyph intact."), OutputPattern.GetGlyph(0, 0) == ESRGlyphType::Metal);
		TestTrue(TEXT("A winning overlay glyph replaces the base glyph."), OutputPattern.GetGlyph(1, 1) == ESRGlyphType::Metal);
		TestTrue(TEXT("An overlay glyph copies into an empty base cell."), OutputPattern.GetGlyph(2, 2) == ESRGlyphType::Plasma);
		TestEqual(TEXT("Synthesis does not create unrelated occupied cells."), OutputPattern.GetOccupiedCellCount(), 3);
	}
	TestEqual(TEXT("Every occupied overlap emits one collision trace."), Result.TraceEvents.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRPatternFacilitySeparationTest,
	"StarRovers.Pattern.Facility.Separation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRPatternFacilitySeparationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FSRPattern InputPattern;
	InputPattern.SetGlyph(0, 0, ESRGlyphType::Metal);
	InputPattern.SetGlyph(1, 3, ESRGlyphType::Crystal);
	InputPattern.SetGlyph(4, 4, ESRGlyphType::Organic);
	FSRPatternSeparationOperatorSpec OperatorSpec;
	OperatorSpec.PrimaryOutputMask.Reset(false);
	OperatorSpec.PrimaryOutputMask.SetCellActive(0, 0, true);
	OperatorSpec.PrimaryOutputMask.SetCellActive(1, 3, true);

	const FSRPatternFacilityResolveResult Result = FSRPatternFacilityResolver::ResolveSeparation(
		InputPattern,
		OperatorSpec);
	TestTrue(TEXT("Separation succeeds when both partitions contain a glyph."), Result.bSucceeded);
	TestEqual(TEXT("Separation creates exactly two outputs."), Result.OutputPatterns.Num(), 2);
	if (Result.OutputPatterns.Num() == 2)
	{
		const FSRPattern& PrimaryOutput = Result.OutputPatterns[0];
		const FSRPattern& SecondaryOutput = Result.OutputPatterns[1];
		TestEqual(
			TEXT("Separation preserves the total occupied-cell count."),
			PrimaryOutput.GetOccupiedCellCount() + SecondaryOutput.GetOccupiedCellCount(),
			InputPattern.GetOccupiedCellCount());
		for (int32 CellIndex = 0; CellIndex < StarRovers::Pattern::CellCount; ++CellIndex)
		{
			const bool bPrimaryOccupied = PrimaryOutput.Cells[CellIndex] != ESRGlyphType::Empty;
			const bool bSecondaryOccupied = SecondaryOutput.Cells[CellIndex] != ESRGlyphType::Empty;
			TestFalse(TEXT("An occupied glyph is never duplicated across Separation outputs."), bPrimaryOccupied && bSecondaryOccupied);
			const ESRGlyphType RecombinedGlyph = bPrimaryOccupied
				? PrimaryOutput.Cells[CellIndex]
				: SecondaryOutput.Cells[CellIndex];
			TestTrue(TEXT("The two outputs exactly recombine to the input."), RecombinedGlyph == InputPattern.Cells[CellIndex]);
		}
	}

	FSRPatternSeparationOperatorSpec InvalidOperatorSpec = OperatorSpec;
	InvalidOperatorSpec.PrimaryOutputMask.ActiveCells.Pop();
	const FSRPatternFacilityResolveResult InvalidResult = FSRPatternFacilityResolver::ResolveSeparation(
		InputPattern,
		InvalidOperatorSpec);
	TestFalse(TEXT("A non-canonical mask fails atomically."), InvalidResult.bSucceeded);
	TestTrue(
		TEXT("A non-canonical mask reports the precise failure."),
		InvalidResult.Failure == ESRPatternFacilityResolveFailure::InvalidSeparationMask);
	TestTrue(TEXT("A failed Separation produces no partial output."), InvalidResult.OutputPatterns.IsEmpty());
	TestTrue(TEXT("A failed Separation produces no partial trace."), InvalidResult.TraceEvents.IsEmpty());
	return true;
}

#endif
