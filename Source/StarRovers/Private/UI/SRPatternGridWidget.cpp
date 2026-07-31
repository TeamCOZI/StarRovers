#include "UI/SRPatternGridWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Styling/SlateColor.h"

bool FSRPatternGridPresentation::BuildCells(
	const FSRPattern& Pattern,
	const FSRPatternMask* OptionalMask,
	TArray<FSRPatternGridCellPresentation>& OutCells,
	FString& OutFailureReason)
{
	OutCells.Reset();
	OutFailureReason.Reset();
	if (!Pattern.IsCanonical())
	{
		OutFailureReason = TEXT("Pattern UI requires exactly 25 canonical glyph cells.");
		return false;
	}
	if (OptionalMask && !OptionalMask->IsCanonical())
	{
		OutFailureReason = TEXT("Pattern UI mask requires exactly 25 canonical cells.");
		return false;
	}

	TArray<FSRPatternGridCellPresentation> Cells;
	Cells.Reserve(StarRovers::Pattern::CellCount);
	for (int32 Index = 0; Index < StarRovers::Pattern::CellCount; ++Index)
	{
		int32 Row = 0;
		int32 Column = 0;
		if (!StarRovers::Pattern::TryIndexToCoordinate(Index, Row, Column))
		{
			OutFailureReason = TEXT("Pattern UI could not resolve a canonical cell coordinate.");
			return false;
		}

		FSRPatternGridCellPresentation& Cell = Cells.AddDefaulted_GetRef();
		Cell.Row = Row;
		Cell.Column = Column;
		Cell.Glyph = Pattern.Cells[Index];
		Cell.bMaskActive = !OptionalMask || OptionalMask->ActiveCells[Index];
		Cell.GlyphLabel = GetGlyphLabel(Cell.Glyph);
		Cell.FillColor = GetGlyphColor(Cell.Glyph);
		Cell.TextColor = Cell.Glyph == ESRGlyphType::Empty
			? FLinearColor(0.36f, 0.42f, 0.46f, 1.0f)
			: FLinearColor(0.96f, 0.98f, 1.0f, 1.0f);
		if (!Cell.bMaskActive)
		{
			Cell.FillColor = FLinearColor(
				Cell.FillColor.R * 0.22f,
				Cell.FillColor.G * 0.22f,
				Cell.FillColor.B * 0.22f,
				0.55f);
			Cell.TextColor.A = 0.30f;
		}
	}

	OutCells = MoveTemp(Cells);
	return true;
}

FText FSRPatternGridPresentation::GetGlyphLabel(ESRGlyphType Glyph)
{
	switch (Glyph)
	{
	case ESRGlyphType::Empty:
		return NSLOCTEXT("StarRoversPatternGrid", "EmptyGlyph", "·");
	case ESRGlyphType::Metal:
		return NSLOCTEXT("StarRoversPatternGrid", "MetalGlyph", "M");
	case ESRGlyphType::Organic:
		return NSLOCTEXT("StarRoversPatternGrid", "OrganicGlyph", "O");
	case ESRGlyphType::Crystal:
		return NSLOCTEXT("StarRoversPatternGrid", "CrystalGlyph", "C");
	case ESRGlyphType::Fluid:
		return NSLOCTEXT("StarRoversPatternGrid", "FluidGlyph", "F");
	case ESRGlyphType::Plasma:
		return NSLOCTEXT("StarRoversPatternGrid", "PlasmaGlyph", "P");
	default:
		return NSLOCTEXT("StarRoversPatternGrid", "InvalidGlyph", "?");
	}
}

FLinearColor FSRPatternGridPresentation::GetGlyphColor(ESRGlyphType Glyph)
{
	switch (Glyph)
	{
	case ESRGlyphType::Metal:
		return FLinearColor(0.34f, 0.39f, 0.45f, 1.0f);
	case ESRGlyphType::Organic:
		return FLinearColor(0.12f, 0.46f, 0.24f, 1.0f);
	case ESRGlyphType::Crystal:
		return FLinearColor(0.12f, 0.46f, 0.58f, 1.0f);
	case ESRGlyphType::Fluid:
		return FLinearColor(0.10f, 0.27f, 0.62f, 1.0f);
	case ESRGlyphType::Plasma:
		return FLinearColor(0.58f, 0.13f, 0.50f, 1.0f);
	case ESRGlyphType::Empty:
	default:
		return FLinearColor(0.035f, 0.045f, 0.055f, 1.0f);
	}
}

TSharedRef<SWidget> USRPatternGridWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}
	BuildPatternGrid();
	return Super::RebuildWidget();
}

void USRPatternGridWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	BuildPatternGrid();
}

void USRPatternGridWidget::SetPattern(const FSRPattern& NewPattern)
{
	DisplayPattern = NewPattern;
	bUseMask = false;
	RefreshPatternGrid();
}

void USRPatternGridWidget::SetPatternAndMask(const FSRPattern& NewPattern, const FSRPatternMask& NewMask)
{
	DisplayPattern = NewPattern;
	DisplayMask = NewMask;
	bUseMask = true;
	RefreshPatternGrid();
}

void USRPatternGridWidget::ClearPattern()
{
	DisplayPattern.Reset();
	DisplayMask.Reset(false);
	bUseMask = false;
	RefreshPatternGrid();
}

void USRPatternGridWidget::SetCellSize(float NewCellSize)
{
	CellSize = FMath::Clamp(NewCellSize, 10.0f, 80.0f);
	BuildPatternGrid();
}

FSRPattern USRPatternGridWidget::GetPattern() const
{
	return DisplayPattern;
}

FSRPatternMask USRPatternGridWidget::GetPatternMask() const
{
	return DisplayMask;
}

bool USRPatternGridWidget::IsUsingPatternMask() const
{
	return bUseMask;
}

void USRPatternGridWidget::BuildPatternGrid()
{
	if (!WidgetTree)
	{
		return;
	}

	GridPanel = Cast<UGridPanel>(WidgetTree->RootWidget);
	if (!GridPanel)
	{
		GridPanel = WidgetTree->ConstructWidget<UGridPanel>(UGridPanel::StaticClass(), TEXT("PatternGridPanel"));
		WidgetTree->RootWidget = GridPanel;
	}
	GridPanel->ClearChildren();
	CellFrames.Reset();
	CellFills.Reset();
	CellLabels.Reset();

	for (int32 Index = 0; Index < StarRovers::Pattern::CellCount; ++Index)
	{
		int32 Row = 0;
		int32 Column = 0;
		StarRovers::Pattern::TryIndexToCoordinate(Index, Row, Column);

		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			FName(*FString::Printf(TEXT("PatternCellFrame_%02d"), Index)));
		Frame->SetPadding(FMargin(1.0f));

		UBorder* Fill = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			FName(*FString::Printf(TEXT("PatternCellFill_%02d"), Index)));
		Fill->SetPadding(FMargin(0.0f));
		Fill->SetHorizontalAlignment(HAlign_Center);
		Fill->SetVerticalAlignment(VAlign_Center);

		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			FName(*FString::Printf(TEXT("PatternCellLabel_%02d"), Index)));
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = FMath::Clamp(FMath::RoundToInt(CellSize * 0.48f), 7, 28);
		Label->SetFont(Font);
		Label->SetJustification(ETextJustify::Center);

		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			FName(*FString::Printf(TEXT("PatternCellSize_%02d"), Index)));
		SizeBox->SetWidthOverride(CellSize);
		SizeBox->SetHeightOverride(CellSize);
		Fill->SetContent(Label);
		Frame->SetContent(Fill);
		SizeBox->SetContent(Frame);

		if (UGridSlot* GridCellSlot = GridPanel->AddChildToGrid(SizeBox, Row, Column))
		{
			GridCellSlot->SetPadding(FMargin(1.0f));
			GridCellSlot->SetHorizontalAlignment(HAlign_Center);
			GridCellSlot->SetVerticalAlignment(VAlign_Center);
		}
		CellFrames.Add(Frame);
		CellFills.Add(Fill);
		CellLabels.Add(Label);
	}

	RefreshPatternGrid();
}

void USRPatternGridWidget::RefreshPatternGrid()
{
	if (CellFrames.Num() != StarRovers::Pattern::CellCount
		|| CellFills.Num() != StarRovers::Pattern::CellCount
		|| CellLabels.Num() != StarRovers::Pattern::CellCount)
	{
		return;
	}

	TArray<FSRPatternGridCellPresentation> Cells;
	FString FailureReason;
	const FSRPatternMask* Mask = bUseMask ? &DisplayMask : nullptr;
	if (!FSRPatternGridPresentation::BuildCells(DisplayPattern, Mask, Cells, FailureReason))
	{
		return;
	}

	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FSRPatternGridCellPresentation& Cell = Cells[Index];
		CellFrames[Index]->SetBrushColor(Cell.bMaskActive ? ActiveCellFrameColor : InactiveCellFrameColor);
		CellFills[Index]->SetBrushColor(Cell.FillColor);
		CellLabels[Index]->SetText(Cell.GlyphLabel);
		CellLabels[Index]->SetColorAndOpacity(FSlateColor(Cell.TextColor));
	}
}
