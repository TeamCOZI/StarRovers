#include "UI/SRUIComponents.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/SlateColor.h"

USRThemedCardWidget::USRThemedCardWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (const USRUIThemeSettings* Theme = USRUIThemeLibrary::GetThemeSettings())
	{
		CardPadding = Theme->CardPadding;
	}
	RefreshTheme();
}

void USRThemedCardWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	RefreshTheme();
}

void USRThemedCardWidget::SetVisualState(ESRUIVisualState NewVisualState)
{
	if (VisualState == NewVisualState)
	{
		return;
	}
	VisualState = NewVisualState;
	RefreshTheme();
}

ESRUIVisualState USRThemedCardWidget::GetVisualState() const
{
	return VisualState;
}

void USRThemedCardWidget::SetCardPadding(const FMargin& NewCardPadding)
{
	CardPadding = NewCardPadding;
	RefreshTheme();
}

FSRUIStatePalette USRThemedCardWidget::GetResolvedPalette() const
{
	return USRUIThemeLibrary::ResolveStatePalette(VisualState);
}

void USRThemedCardWidget::RefreshTheme()
{
	USRUIThemeLibrary::ApplyCardStyle(this, VisualState, CardPadding);
}

TSharedRef<SWidget> USRStatusBadgeWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void USRStatusBadgeWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildWidgetTree();
	RefreshPresentation();
}

void USRStatusBadgeWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	BuildWidgetTree();
	RefreshPresentation();
}

void USRStatusBadgeWidget::SetBadge(FText NewLabel, ESRUIVisualState NewVisualState)
{
	Label = MoveTemp(NewLabel);
	VisualState = NewVisualState;
	RefreshPresentation();
}

void USRStatusBadgeWidget::SetLabel(FText NewLabel)
{
	Label = MoveTemp(NewLabel);
	RefreshPresentation();
}

void USRStatusBadgeWidget::SetVisualState(ESRUIVisualState NewVisualState)
{
	VisualState = NewVisualState;
	RefreshPresentation();
}

FText USRStatusBadgeWidget::GetLabel() const
{
	return Label;
}

ESRUIVisualState USRStatusBadgeWidget::GetVisualState() const
{
	return VisualState;
}

void USRStatusBadgeWidget::BuildWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}
	if (WidgetTree->RootWidget)
	{
		CacheWidgetTree();
		RefreshPresentation();
		return;
	}

	RootCard = WidgetTree->ConstructWidget<USRThemedCardWidget>(
		USRThemedCardWidget::StaticClass(),
		TEXT("StatusBadgeRootCard"));
	WidgetTree->RootWidget = RootCard;

	UHorizontalBox* ContentBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		TEXT("StatusBadgeContentBox"));
	RootCard->SetContent(ContentBox);

	IndicatorSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		TEXT("StatusBadgeIndicatorSizeBox"));
	IndicatorSizeBox->SetWidthOverride(6.0f);
	IndicatorSizeBox->SetHeightOverride(6.0f);
	IndicatorBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("StatusBadgeIndicatorBorder"));
	IndicatorBorder->SetPadding(FMargin(0.0f));
	IndicatorSizeBox->AddChild(IndicatorBorder);
	if (UHorizontalBoxSlot* IndicatorSlot = ContentBox->AddChildToHorizontalBox(IndicatorSizeBox))
	{
		IndicatorSlot->SetPadding(FMargin(0.0f, 0.0f, USRUIThemeLibrary::ResolveSpacing(1), 0.0f));
		IndicatorSlot->SetVerticalAlignment(VAlign_Center);
	}

	LabelTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("StatusBadgeLabelTextBlock"));
	LabelTextBlock->SetAutoWrapText(false);
	if (UHorizontalBoxSlot* LabelSlot = ContentBox->AddChildToHorizontalBox(LabelTextBlock))
	{
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}

	RefreshPresentation();
}

void USRStatusBadgeWidget::CacheWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}
	RootCard = Cast<USRThemedCardWidget>(WidgetTree->FindWidget(TEXT("StatusBadgeRootCard")));
	IndicatorSizeBox = Cast<USizeBox>(WidgetTree->FindWidget(TEXT("StatusBadgeIndicatorSizeBox")));
	IndicatorBorder = Cast<UBorder>(WidgetTree->FindWidget(TEXT("StatusBadgeIndicatorBorder")));
	LabelTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("StatusBadgeLabelTextBlock")));
}

void USRStatusBadgeWidget::RefreshPresentation()
{
	const FSRUIStatePalette Palette = USRUIThemeLibrary::ResolveStatePalette(VisualState);
	if (RootCard)
	{
		RootCard->SetVisualState(VisualState);
		if (const USRUIThemeSettings* Theme = USRUIThemeLibrary::GetThemeSettings())
		{
			RootCard->SetCardPadding(Theme->BadgePadding);
		}
	}
	if (IndicatorSizeBox)
	{
		IndicatorSizeBox->SetVisibility(bShowIndicator
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}
	if (IndicatorBorder)
	{
		IndicatorBorder->SetBrushColor(Palette.AccentColor);
	}
	if (LabelTextBlock)
	{
		LabelTextBlock->SetText(Label);
		USRUIThemeLibrary::ApplyTextStyle(
			LabelTextBlock,
			ESRUITextStyle::Caption,
			VisualState,
			false);
		LabelTextBlock->SetColorAndOpacity(FSlateColor(Palette.PrimaryTextColor));
	}
}

TSharedRef<SWidget> USRInfoCardWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void USRInfoCardWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildWidgetTree();
	RefreshPresentation();
}

void USRInfoCardWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	BuildWidgetTree();
	RefreshPresentation();
}

void USRInfoCardWidget::SetCardData(
	FText NewTitle,
	FText NewValue,
	FText NewDetail,
	ESRUIVisualState NewVisualState)
{
	TitleText = MoveTemp(NewTitle);
	ValueText = MoveTemp(NewValue);
	DetailText = MoveTemp(NewDetail);
	VisualState = NewVisualState;
	RefreshPresentation();
}

void USRInfoCardWidget::SetStatus(
	FText NewStatusText,
	ESRUIVisualState NewStatusState,
	bool bVisible)
{
	StatusText = MoveTemp(NewStatusText);
	StatusState = NewStatusState;
	bShowStatus = bVisible;
	RefreshPresentation();
}

void USRInfoCardWidget::ClearStatus()
{
	StatusText = FText::GetEmpty();
	bShowStatus = false;
	RefreshPresentation();
}

FText USRInfoCardWidget::GetTitleText() const
{
	return TitleText;
}

FText USRInfoCardWidget::GetValueText() const
{
	return ValueText;
}

FText USRInfoCardWidget::GetDetailText() const
{
	return DetailText;
}

void USRInfoCardWidget::BuildWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}
	if (WidgetTree->RootWidget)
	{
		CacheWidgetTree();
		RefreshPresentation();
		return;
	}

	RootCard = WidgetTree->ConstructWidget<USRThemedCardWidget>(
		USRThemedCardWidget::StaticClass(),
		TEXT("InfoCardRoot"));
	WidgetTree->RootWidget = RootCard;

	UVerticalBox* ContentBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("InfoCardContentBox"));
	RootCard->SetContent(ContentBox);

	UHorizontalBox* HeaderBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		TEXT("InfoCardHeaderBox"));
	if (UVerticalBoxSlot* HeaderSlot = ContentBox->AddChildToVerticalBox(HeaderBox))
	{
		HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, USRUIThemeLibrary::ResolveSpacing(1)));
	}

	TitleTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("InfoCardTitleTextBlock"));
	TitleTextBlock->SetAutoWrapText(true);
	if (UHorizontalBoxSlot* TitleSlot = HeaderBox->AddChildToHorizontalBox(TitleTextBlock))
	{
		TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TitleSlot->SetVerticalAlignment(VAlign_Center);
	}

	StatusBadge = WidgetTree->ConstructWidget<USRStatusBadgeWidget>(
		USRStatusBadgeWidget::StaticClass(),
		TEXT("InfoCardStatusBadge"));
	if (UHorizontalBoxSlot* BadgeSlot = HeaderBox->AddChildToHorizontalBox(StatusBadge))
	{
		BadgeSlot->SetPadding(FMargin(USRUIThemeLibrary::ResolveSpacing(2), 0.0f, 0.0f, 0.0f));
		BadgeSlot->SetVerticalAlignment(VAlign_Center);
	}

	ValueTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("InfoCardValueTextBlock"));
	ValueTextBlock->SetAutoWrapText(false);
	if (UVerticalBoxSlot* ValueSlot = ContentBox->AddChildToVerticalBox(ValueTextBlock))
	{
		ValueSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, USRUIThemeLibrary::ResolveSpacing(1)));
	}

	DetailTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("InfoCardDetailTextBlock"));
	DetailTextBlock->SetAutoWrapText(true);
	ContentBox->AddChildToVerticalBox(DetailTextBlock);

	RefreshPresentation();
}

void USRInfoCardWidget::CacheWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}
	RootCard = Cast<USRThemedCardWidget>(WidgetTree->FindWidget(TEXT("InfoCardRoot")));
	TitleTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("InfoCardTitleTextBlock")));
	ValueTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("InfoCardValueTextBlock")));
	DetailTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("InfoCardDetailTextBlock")));
	StatusBadge = Cast<USRStatusBadgeWidget>(WidgetTree->FindWidget(TEXT("InfoCardStatusBadge")));
}

void USRInfoCardWidget::RefreshPresentation()
{
	if (RootCard)
	{
		RootCard->SetVisualState(VisualState);
	}
	if (TitleTextBlock)
	{
		TitleTextBlock->SetText(TitleText);
		USRUIThemeLibrary::ApplyTextStyle(TitleTextBlock, ESRUITextStyle::Heading, VisualState);
	}
	if (ValueTextBlock)
	{
		ValueTextBlock->SetText(ValueText);
		USRUIThemeLibrary::ApplyTextStyle(ValueTextBlock, ESRUITextStyle::Metric, VisualState, true);
	}
	if (DetailTextBlock)
	{
		DetailTextBlock->SetText(DetailText);
		USRUIThemeLibrary::ApplyTextStyle(DetailTextBlock, ESRUITextStyle::Body, VisualState);
		DetailTextBlock->SetVisibility(DetailText.IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::HitTestInvisible);
	}
	if (StatusBadge)
	{
		StatusBadge->SetBadge(StatusText, StatusState);
		StatusBadge->SetVisibility(bShowStatus && !StatusText.IsEmpty()
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}
}
