#include "UI/SRGameOverWidget.h"

#include "Utility/SRLog.h"
#include "UI/SRUILayoutPolicy.h"
#include "UI/SRStellarRunResultPresentation.h"
#include "Blueprint/WidgetTree.h"
#include "Celestial/SRStar.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateColor.h"

namespace
{
	UTextBlock* ConstructGameOverTextBlock(UWidgetTree* WidgetTree, const FName& Name, int32 FontSize, const FLinearColor& Color)
	{
		if (!WidgetTree)
		{
			return nullptr;
		}

		UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		if (!TextBlock)
		{
			return nullptr;
		}

		FSlateFontInfo FontInfo = TextBlock->GetFont();
		FontInfo.Size = FontSize;
		TextBlock->SetFont(FontInfo);
		TextBlock->SetJustification(ETextJustify::Center);
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		TextBlock->SetAutoWrapText(true);
		return TextBlock;
	}
}

TSharedRef<SWidget> USRGameOverWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}

	BuildGameOverWidgetTree();
	return Super::RebuildWidget();
}

void USRGameOverWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildGameOverWidgetTree();
	RefreshGameOverText();
}

void USRGameOverWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	BuildGameOverWidgetTree();
	RefreshGameOverText();
}

FReply USRGameOverWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (IsVisible())
	{
		const FVector2D ScreenPosition = InMouseEvent.GetScreenSpacePosition();
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: GameOver NativeOnMouseButtonDown handled Mouse=(%.1f, %.1f)"),
			ScreenPosition.X,
			ScreenPosition.Y);
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply USRGameOverWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (IsVisible())
	{
		const FVector2D ScreenPosition = InMouseEvent.GetScreenSpacePosition();
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: GameOver NativeOnMouseButtonUp handled Mouse=(%.1f, %.1f)"),
			ScreenPosition.X,
			ScreenPosition.Y);
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply USRGameOverWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return IsVisible() ? FReply::Handled() : Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

void USRGameOverWidget::SetGameOverStar(ASRStar* Star)
{
	GameOverStar = Star;
	RefreshGameOverText();
}

void USRGameOverWidget::BuildGameOverWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* RootCanvasPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("GameOverCanvasPanel"));
	WidgetTree->RootWidget = RootCanvasPanel;

	BackgroundBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GameOverBackground"));
	BackgroundBorder->SetBrushColor(FLinearColor(0.010f, 0.006f, 0.008f, 0.92f));
	BackgroundBorder->SetPadding(FMargin(48.0f));
	if (UCanvasPanelSlot* BackgroundSlot = RootCanvasPanel->AddChildToCanvas(BackgroundBorder))
	{
		BackgroundSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		BackgroundSlot->SetOffsets(FMargin(0.0f));
	}

	UScaleBox* ContentScaleBox = WidgetTree->ConstructWidget<UScaleBox>(
		UScaleBox::StaticClass(),
		TEXT("GameOverContentScaleBox"));
	ContentScaleBox->SetStretch(EStretch::ScaleToFit);
	ContentScaleBox->SetStretchDirection(EStretchDirection::DownOnly);
	if (UCanvasPanelSlot* ContentScaleSlot = RootCanvasPanel->AddChildToCanvas(ContentScaleBox))
	{
		ContentScaleSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		ContentScaleSlot->SetOffsets(FMargin(FSRUILayoutPolicy::DefaultSafeMargin));
	}

	USizeBox* ContentSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("GameOverContentSizeBox"));
	ContentSizeBox->SetWidthOverride(620.0f);
	if (UScaleBoxSlot* ContentSlot = Cast<UScaleBoxSlot>(ContentScaleBox->AddChild(ContentSizeBox)))
	{
		ContentSlot->SetHorizontalAlignment(HAlign_Center);
		ContentSlot->SetVerticalAlignment(VAlign_Center);
	}

	UVerticalBox* ContentVerticalBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("GameOverContent"));
	ContentSizeBox->AddChild(ContentVerticalBox);

	TitleTextBlock = ConstructGameOverTextBlock(WidgetTree, TEXT("GameOverTitle"), 56, FLinearColor(1.0f, 0.30f, 0.20f, 1.0f));
	if (TitleTextBlock)
	{
		if (UVerticalBoxSlot* TitleSlot = ContentVerticalBox->AddChildToVerticalBox(TitleTextBlock))
		{
			TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
			TitleSlot->SetHorizontalAlignment(HAlign_Center);
		}
	}

	SubtitleTextBlock = ConstructGameOverTextBlock(WidgetTree, TEXT("GameOverSubtitle"), 24, FLinearColor(0.96f, 0.90f, 0.82f, 1.0f));
	if (SubtitleTextBlock)
	{
		if (UVerticalBoxSlot* SubtitleSlot = ContentVerticalBox->AddChildToVerticalBox(SubtitleTextBlock))
		{
			SubtitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));
			SubtitleSlot->SetHorizontalAlignment(HAlign_Center);
		}
	}

	DetailTextBlock = ConstructGameOverTextBlock(WidgetTree, TEXT("GameOverDetail"), 16, FLinearColor(0.74f, 0.78f, 0.82f, 1.0f));
	if (DetailTextBlock)
	{
		if (UVerticalBoxSlot* DetailSlot = ContentVerticalBox->AddChildToVerticalBox(DetailTextBlock))
		{
			DetailSlot->SetHorizontalAlignment(HAlign_Center);
		}
	}

	RefreshGameOverText();
}

void USRGameOverWidget::RefreshGameOverText()
{
	FSRStellarRunResultSnapshot Snapshot;
	const ASRStar* Star = GameOverStar.Get();
	if (IsValid(Star))
	{
		const FSRStellarFuelState FuelState = Star->GetStellarFuelState();
		Snapshot.bHasStar = true;
		Snapshot.RunProgress = FuelState.RunProgress;
		Snapshot.StoredFuel = FuelState.StoredFuel;
		Snapshot.ReferenceFuel = FuelState.InitialStageFuel;
	}
	const FSRStellarRunResultPresentation Presentation =
		FSRStellarRunResultPresentationBuilder::Build(Snapshot);

	if (BackgroundBorder)
	{
		BackgroundBorder->SetBrushColor(Presentation.BackgroundColor);
	}
	if (TitleTextBlock)
	{
		TitleTextBlock->SetText(Presentation.TitleText);
		TitleTextBlock->SetColorAndOpacity(FSlateColor(Presentation.TitleColor));
	}

	if (SubtitleTextBlock)
	{
		SubtitleTextBlock->SetText(Presentation.SubtitleText);
		SubtitleTextBlock->SetColorAndOpacity(FSlateColor(Presentation.SubtitleColor));
	}

	if (DetailTextBlock)
	{
		DetailTextBlock->SetText(Presentation.DetailText);
		DetailTextBlock->SetColorAndOpacity(FSlateColor(Presentation.DetailColor));
	}
}
