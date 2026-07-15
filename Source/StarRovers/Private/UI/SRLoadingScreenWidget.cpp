#include "UI/SRLoadingScreenWidget.h"

#include "Utility/SRLog.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateColor.h"

TSharedRef<SWidget> USRLoadingScreenWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}

	BuildLoadingScreenWidgetTree();
	return Super::RebuildWidget();
}

void USRLoadingScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildLoadingScreenWidgetTree();
	RefreshLoadingScreenText();
}

void USRLoadingScreenWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	BuildLoadingScreenWidgetTree();
	RefreshLoadingScreenText();
}

FReply USRLoadingScreenWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (IsVisible())
	{
		const FVector2D ScreenPosition = InMouseEvent.GetScreenSpacePosition();
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: LoadingScreen NativeOnMouseButtonDown handled Mouse=(%.1f, %.1f)"),
			ScreenPosition.X,
			ScreenPosition.Y);
		return FReply::Handled();
	}

	return IsVisible() ? FReply::Handled() : Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply USRLoadingScreenWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (IsVisible())
	{
		const FVector2D ScreenPosition = InMouseEvent.GetScreenSpacePosition();
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: LoadingScreen NativeOnMouseButtonUp handled Mouse=(%.1f, %.1f)"),
			ScreenPosition.X,
			ScreenPosition.Y);
		return FReply::Handled();
	}

	return IsVisible() ? FReply::Handled() : Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply USRLoadingScreenWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return IsVisible() ? FReply::Handled() : Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

void USRLoadingScreenWidget::SetLoadingProgress(float InProgress, const FText& InStatusText)
{
	LoadingProgress = FMath::Clamp(InProgress, 0.0f, 1.0f);
	LoadingStatusText = InStatusText;
	RefreshLoadingScreenText();
}

void USRLoadingScreenWidget::BuildLoadingScreenWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* RootCanvasPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("LoadingScreenCanvasPanel"));
	WidgetTree->RootWidget = RootCanvasPanel;

	BackgroundBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("LoadingScreenBackground"));
	BackgroundBorder->SetBrushColor(FLinearColor(0.005f, 0.007f, 0.011f, 1.0f));
	BackgroundBorder->SetPadding(FMargin(48.0f));

	if (UCanvasPanelSlot* BackgroundSlot = RootCanvasPanel->AddChildToCanvas(BackgroundBorder))
	{
		BackgroundSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		BackgroundSlot->SetOffsets(FMargin(0.0f));
	}

	UVerticalBox* ContentVerticalBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LoadingScreenContent"));
	if (UCanvasPanelSlot* ContentSlot = RootCanvasPanel->AddChildToCanvas(ContentVerticalBox))
	{
		ContentSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		ContentSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		ContentSlot->SetPosition(FVector2D::ZeroVector);
		ContentSlot->SetAutoSize(true);
	}

	TitleTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LoadingScreenTitle"));
	if (TitleTextBlock)
	{
		FSlateFontInfo TitleFont = TitleTextBlock->GetFont();
		TitleFont.Size = 36;
		TitleTextBlock->SetFont(TitleFont);
		TitleTextBlock->SetJustification(ETextJustify::Center);
		TitleTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.96f, 1.0f, 1.0f)));
		TitleTextBlock->SetAutoWrapText(true);

		if (UVerticalBoxSlot* TitleSlot = ContentVerticalBox->AddChildToVerticalBox(TitleTextBlock))
		{
			TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
			TitleSlot->SetHorizontalAlignment(HAlign_Center);
		}
	}

	StatusTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LoadingScreenStatus"));
	if (StatusTextBlock)
	{
		FSlateFontInfo StatusFont = StatusTextBlock->GetFont();
		StatusFont.Size = 18;
		StatusTextBlock->SetFont(StatusFont);
		StatusTextBlock->SetJustification(ETextJustify::Center);
		StatusTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(0.68f, 0.76f, 0.82f, 1.0f)));
		StatusTextBlock->SetAutoWrapText(true);

		if (UVerticalBoxSlot* StatusSlot = ContentVerticalBox->AddChildToVerticalBox(StatusTextBlock))
		{
			StatusSlot->SetHorizontalAlignment(HAlign_Center);
		}
	}

	ProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("LoadingScreenProgressBar"));
	if (ProgressBar)
	{
		ProgressBar->SetPercent(0.0f);
		ProgressBar->SetFillColorAndOpacity(FLinearColor(0.16f, 0.72f, 0.95f, 1.0f));

		if (UVerticalBoxSlot* ProgressSlot = ContentVerticalBox->AddChildToVerticalBox(ProgressBar))
		{
			ProgressSlot->SetPadding(FMargin(0.0f, 22.0f, 0.0f, 8.0f));
			ProgressSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	}

	ProgressTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LoadingScreenProgressText"));
	if (ProgressTextBlock)
	{
		FSlateFontInfo ProgressFont = ProgressTextBlock->GetFont();
		ProgressFont.Size = 14;
		ProgressTextBlock->SetFont(ProgressFont);
		ProgressTextBlock->SetJustification(ETextJustify::Center);
		ProgressTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(0.74f, 0.84f, 0.90f, 1.0f)));

		if (UVerticalBoxSlot* ProgressTextSlot = ContentVerticalBox->AddChildToVerticalBox(ProgressTextBlock))
		{
			ProgressTextSlot->SetHorizontalAlignment(HAlign_Center);
		}
	}

	RefreshLoadingScreenText();
}

void USRLoadingScreenWidget::RefreshLoadingScreenText()
{
	if (TitleTextBlock)
	{
		TitleTextBlock->SetText(NSLOCTEXT("StarRoversLoadingScreen", "Title", "Generating Star System"));
	}

	if (StatusTextBlock)
	{
		StatusTextBlock->SetText(LoadingStatusText.IsEmpty()
			? NSLOCTEXT("StarRoversLoadingScreen", "Status", "Preparing planets...")
			: LoadingStatusText);
	}

	if (ProgressBar)
	{
		ProgressBar->SetPercent(LoadingProgress);
	}

	if (ProgressTextBlock)
	{
		ProgressTextBlock->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(LoadingProgress * 100.0f))));
	}
}
