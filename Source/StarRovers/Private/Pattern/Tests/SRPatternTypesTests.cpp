#if WITH_DEV_AUTOMATION_TESTS

#include "Pattern/SRPatternTypes.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRPatternDefaultStateTest,
	"StarRovers.Pattern.Foundation.DefaultState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRPatternDefaultStateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FSRPattern Pattern;
	TestTrue(TEXT("A default pattern is canonical."), Pattern.IsCanonical());
	TestEqual(TEXT("A pattern always has 25 cells."), Pattern.Cells.Num(), StarRovers::Pattern::CellCount);
	TestEqual(TEXT("A default pattern has no occupied cells."), Pattern.GetOccupiedCellCount(), 0);
	TestTrue(TEXT("A default pattern is empty."), Pattern.IsEmpty());
	TestEqual(TEXT("A default pattern has five compact rows."), Pattern.ToCompactString(), FString(TEXT("...../...../...../...../.....")));

	const FSRPattern FilledPattern(ESRGlyphType::Metal);
	TestEqual(TEXT("A filled pattern has 25 occupied cells."), FilledPattern.GetOccupiedCellCount(), StarRovers::Pattern::CellCount);
	TestEqual(TEXT("A filled pattern counts its glyphs."), FilledPattern.CountGlyph(ESRGlyphType::Metal), StarRovers::Pattern::CellCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRPatternCoordinateTest,
	"StarRovers.Pattern.Foundation.Coordinates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRPatternCoordinateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	int32 Index = INDEX_NONE;
	TestTrue(TEXT("The bottom-right coordinate is valid."), StarRovers::Pattern::TryCoordinateToIndex(4, 4, Index));
	TestEqual(TEXT("Coordinates use row-major indexing."), Index, 24);
	TestFalse(TEXT("A row outside the board is rejected."), StarRovers::Pattern::TryCoordinateToIndex(5, 0, Index));
	TestEqual(TEXT("A rejected coordinate clears the output index."), Index, INDEX_NONE);

	int32 Row = INDEX_NONE;
	int32 Column = INDEX_NONE;
	TestTrue(TEXT("Index 17 can be converted to a coordinate."), StarRovers::Pattern::TryIndexToCoordinate(17, Row, Column));
	TestEqual(TEXT("Index 17 resolves to row 3."), Row, 3);
	TestEqual(TEXT("Index 17 resolves to column 2."), Column, 2);
	TestFalse(TEXT("Index 25 is rejected."), StarRovers::Pattern::TryIndexToCoordinate(25, Row, Column));

	int32 RowDelta = 0;
	int32 ColumnDelta = 0;
	TestTrue(TEXT("Up has a valid direction delta."), StarRovers::Pattern::TryGetDirectionDelta(ESRPatternDirection::Up, RowDelta, ColumnDelta));
	TestEqual(TEXT("Up decreases the row."), RowDelta, -1);
	TestEqual(TEXT("Up does not change the column."), ColumnDelta, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRPatternMutationAndNormalizationTest,
	"StarRovers.Pattern.Foundation.MutationAndNormalization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRPatternMutationAndNormalizationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FSRPattern Pattern;
	TestTrue(TEXT("A valid glyph can be written."), Pattern.SetGlyph(2, 3, ESRGlyphType::Crystal));
	TestTrue(TEXT("The written glyph can be read."), Pattern.GetGlyph(2, 3) == ESRGlyphType::Crystal);
	TestFalse(TEXT("An invalid coordinate cannot be written."), Pattern.SetGlyph(-1, 0, ESRGlyphType::Metal));
	TestFalse(TEXT("An invalid enum value cannot be written."), Pattern.SetGlyph(0, 0, static_cast<ESRGlyphType>(255)));

	Pattern.Cells.SetNum(3);
	Pattern.Cells[0] = ESRGlyphType::Metal;
	Pattern.Cells[1] = static_cast<ESRGlyphType>(255);
	TestFalse(TEXT("A truncated pattern is not canonical."), Pattern.IsCanonical());
	TestTrue(TEXT("Normalization reports a repair."), Pattern.Normalize());
	TestTrue(TEXT("A normalized pattern is canonical."), Pattern.IsCanonical());
	TestEqual(TEXT("Normalization restores 25 cells."), Pattern.Cells.Num(), StarRovers::Pattern::CellCount);
	TestTrue(TEXT("Normalization preserves valid cells."), Pattern.Cells[0] == ESRGlyphType::Metal);
	TestTrue(TEXT("Normalization replaces invalid glyphs with Empty."), Pattern.Cells[1] == ESRGlyphType::Empty);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRPatternIdentityAndMaskTest,
	"StarRovers.Pattern.Foundation.IdentityAndMask",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRPatternIdentityAndMaskTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FSRPattern First;
	FSRPattern Second;
	First.SetGlyph(1, 1, ESRGlyphType::Organic);
	Second.SetGlyph(1, 1, ESRGlyphType::Organic);
	TestTrue(TEXT("Patterns with identical cells compare equal."), First == Second);
	TestEqual(TEXT("Equal patterns have equal stable hashes."), First.GetStableHash(), Second.GetStableHash());

	Second.SetGlyph(1, 2, ESRGlyphType::Organic);
	TestTrue(TEXT("Changing one cell changes pattern identity."), First != Second);

	FSRPatternMask Mask;
	TestTrue(TEXT("A default mask is canonical."), Mask.IsCanonical());
	TestFalse(TEXT("A default mask has no active cells."), Mask.HasAnyActiveCell());
	TestTrue(TEXT("A mask cell can be enabled."), Mask.SetCellActive(2, 2, true));
	TestTrue(TEXT("The enabled mask cell is active."), Mask.IsCellActive(2, 2));
	TestEqual(TEXT("The mask counts enabled cells."), Mask.GetActiveCellCount(), 1);

	Mask.Reset(true);
	TestTrue(TEXT("A filled mask reports all cells active."), Mask.AreAllCellsActive());
	return true;
}

#endif
