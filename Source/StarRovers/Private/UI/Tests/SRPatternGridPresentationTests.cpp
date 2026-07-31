#if WITH_DEV_AUTOMATION_TESTS

#include "UI/SRPatternGridWidget.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRPatternGridPresentationTest,
	"StarRovers.UI.PatternGrid.Presentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRPatternGridPresentationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FSRPattern Pattern;
	Pattern.SetGlyph(0, 0, ESRGlyphType::Metal);
	Pattern.SetGlyph(2, 2, ESRGlyphType::Organic);
	Pattern.SetGlyph(4, 4, ESRGlyphType::Plasma);
	FSRPatternMask Mask(false);
	Mask.SetCellActive(0, 0, true);
	Mask.SetCellActive(2, 2, true);

	TArray<FSRPatternGridCellPresentation> Cells;
	FString FailureReason;
	TestTrue(
		TEXT("A canonical Pattern and mask produce presentation cells."),
		FSRPatternGridPresentation::BuildCells(Pattern, &Mask, Cells, FailureReason));
	TestTrue(TEXT("A successful presentation has no failure reason."), FailureReason.IsEmpty());
	TestEqual(TEXT("The presentation always has 25 row-major cells."), Cells.Num(), StarRovers::Pattern::CellCount);
	TestEqual(TEXT("The first cell keeps its glyph."), Cells[0].Glyph, ESRGlyphType::Metal);
	TestTrue(TEXT("The first cell is inside the demand mask."), Cells[0].bMaskActive);
	TestEqual(TEXT("The center cell maps to row two."), Cells[12].Row, 2);
	TestEqual(TEXT("The center cell maps to column two."), Cells[12].Column, 2);
	TestEqual(TEXT("The center cell keeps its glyph."), Cells[12].Glyph, ESRGlyphType::Organic);
	TestFalse(TEXT("A cell outside the mask is visibly inactive."), Cells[24].bMaskActive);

	FSRPattern InvalidPattern;
	InvalidPattern.Cells.Pop();
	const TArray<FSRPatternGridCellPresentation> PreviousCells = Cells;
	TestFalse(
		TEXT("A malformed Pattern is rejected."),
		FSRPatternGridPresentation::BuildCells(InvalidPattern, nullptr, Cells, FailureReason));
	TestTrue(TEXT("A rejected presentation exposes a reason."), !FailureReason.IsEmpty());
	TestTrue(TEXT("A rejected presentation exposes no partial cells."), Cells.IsEmpty());
	TestEqual(TEXT("The prior successful snapshot remains independently intact."), PreviousCells.Num(), StarRovers::Pattern::CellCount);
	return true;
}

#endif
