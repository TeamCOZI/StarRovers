#include "UI/SRAugmentChoiceWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/SlateColor.h"

void USRAugmentChoiceButtonAction::Initialize(USRAugmentChoiceWidget* InOwnerWidget, int32 InChoiceIndex)
{
	OwnerWidget = InOwnerWidget;
	ChoiceIndex = InChoiceIndex;
}

void USRAugmentChoiceButtonAction::HandleClicked()
{
	if (IsValid(OwnerWidget))
	{
		OwnerWidget->DispatchChoiceSelected(ChoiceIndex);
	}
}

TSharedRef<SWidget> USRAugmentChoiceWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}

	BuildAugmentChoiceWidgetTree();
	return Super::RebuildWidget();
}

void USRAugmentChoiceWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!WidgetTree || !WidgetTree->RootWidget)
	{
		BuildAugmentChoiceWidgetTree();
	}
	else
	{
		CacheAugmentChoiceWidgetTree();
		if (CycleTextBlock)
		{
			CycleTextBlock->SetText(FormatCycleText());
		}
		RebuildChoiceButtons();
	}
}

void USRAugmentChoiceWidget::SetAugmentChoices(const TArray<FSRAugmentChoice>& NewChoices, int32 NewCycleIndex)
{
	Choices = NewChoices;
	CycleIndex = NewCycleIndex;

	if (!WidgetTree || !WidgetTree->RootWidget)
	{
		BuildAugmentChoiceWidgetTree();
	}
	else
	{
		if (CycleTextBlock)
		{
			CycleTextBlock->SetText(FormatCycleText());
		}
		RebuildChoiceButtons();
	}
}

void USRAugmentChoiceWidget::ClearAugmentChoices()
{
	Choices.Reset();
	CycleIndex = 0;
	RebuildChoiceButtons();
}

TArray<FSRAugmentChoice> USRAugmentChoiceWidget::GetAugmentChoices() const
{
	return Choices;
}

int32 USRAugmentChoiceWidget::GetCycleIndex() const
{
	return CycleIndex;
}

void USRAugmentChoiceWidget::DispatchChoiceSelected(int32 ChoiceIndex)
{
	if (!Choices.IsValidIndex(ChoiceIndex))
	{
		return;
	}

	USRAugmentSubsystem* AugmentSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USRAugmentSubsystem>() : nullptr;
	if (!AugmentSubsystem)
	{
		return;
	}

	if (AugmentSubsystem->SelectAugmentChoiceByIndex(ChoiceIndex))
	{
		ClearAugmentChoices();
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void USRAugmentChoiceWidget::BuildAugmentChoiceWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}

	if (WidgetTree->RootWidget)
	{
		CacheAugmentChoiceWidgetTree();
		if (CycleTextBlock)
		{
			CycleTextBlock->SetText(FormatCycleText());
		}
		RebuildChoiceButtons();
		return;
	}

	ChoiceActions.Reset();

	UCanvasPanel* RootCanvasPanel = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("AugmentChoiceCanvasPanel"));
	WidgetTree->RootWidget = RootCanvasPanel;
	RootCanvasPanel->SetVisibility(ESlateVisibility::Visible);

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("AugmentChoicePanelBorder"));
	PanelBorder->SetBrushColor(PanelColor);
	PanelBorder->SetPadding(FMargin(20.0f, 18.0f));

	if (UCanvasPanelSlot* PanelCanvasSlot = RootCanvasPanel->AddChildToCanvas(PanelBorder))
	{
		PanelCanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		PanelCanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelCanvasSlot->SetAutoSize(false);
		PanelCanvasSlot->SetSize(FVector2D(FMath::Max(240.0f, PanelWidth), FMath::Max(240.0f, PanelHeight)));
	}

	PanelVerticalBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("AugmentChoicePanelVerticalBox"));
	PanelBorder->SetContent(PanelVerticalBox);

	TitleTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("AugmentChoiceTitleTextBlock"));
	TitleTextBlock->SetText(TitleText);
	TitleTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(0.93f, 0.96f, 0.98f, 1.0f)));
	TitleTextBlock->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* TitleSlot = PanelVerticalBox->AddChildToVerticalBox(TitleTextBlock))
	{
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	}

	SubtitleTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("AugmentChoiceSubtitleTextBlock"));
	SubtitleTextBlock->SetText(SubtitleText);
	SubtitleTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.80f, 0.84f, 1.0f)));
	SubtitleTextBlock->SetJustification(ETextJustify::Center);
	SubtitleTextBlock->SetAutoWrapText(true);
	if (UVerticalBoxSlot* SubtitleSlot = PanelVerticalBox->AddChildToVerticalBox(SubtitleTextBlock))
	{
		SubtitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}

	CycleTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("AugmentChoiceCycleTextBlock"));
	CycleTextBlock->SetText(FormatCycleText());
	CycleTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(0.62f, 0.72f, 0.78f, 1.0f)));
	CycleTextBlock->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* CycleSlot = PanelVerticalBox->AddChildToVerticalBox(CycleTextBlock))
	{
		CycleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	}

	ChoicesHorizontalBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		TEXT("AugmentChoiceChoicesHorizontalBox"));
	if (UVerticalBoxSlot* ChoicesSlot = PanelVerticalBox->AddChildToVerticalBox(ChoicesHorizontalBox))
	{
		ChoicesSlot->SetPadding(FMargin(0.0f));
		ChoicesSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	RebuildChoiceButtons();
}

void USRAugmentChoiceWidget::CacheAugmentChoiceWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}

	PanelBorder = Cast<UBorder>(WidgetTree->FindWidget(TEXT("AugmentChoicePanelBorder")));
	PanelVerticalBox = Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("AugmentChoicePanelVerticalBox")));
	TitleTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("AugmentChoiceTitleTextBlock")));
	SubtitleTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("AugmentChoiceSubtitleTextBlock")));
	CycleTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("AugmentChoiceCycleTextBlock")));
	ChoicesHorizontalBox = Cast<UHorizontalBox>(WidgetTree->FindWidget(TEXT("AugmentChoiceChoicesHorizontalBox")));

	if (TitleTextBlock)
	{
		TitleTextBlock->SetText(TitleText);
	}
	if (SubtitleTextBlock)
	{
		SubtitleTextBlock->SetText(SubtitleText);
	}
}

void USRAugmentChoiceWidget::RebuildChoiceButtons()
{
	if (!ChoicesHorizontalBox || !WidgetTree)
	{
		return;
	}

	ChoicesHorizontalBox->ClearChildren();
	ChoiceActions.Reset();

	for (int32 ChoiceIndex = 0; ChoiceIndex < Choices.Num(); ++ChoiceIndex)
	{
		const FSRAugmentChoice& Choice = Choices[ChoiceIndex];

		USizeBox* ChoiceSizeBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			FName(*FString::Printf(TEXT("AugmentChoiceSizeBox%d"), ChoiceIndex + 1)));
		ChoiceSizeBox->SetHeightOverride(FMath::Max(56.0f, ChoiceButtonHeight));

		UButton* ChoiceButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(),
			FName(*FString::Printf(TEXT("AugmentChoiceButton%d"), ChoiceIndex + 1)));
		ChoiceButton->SetBackgroundColor(ChoiceButtonColor);

		USRAugmentChoiceButtonAction* ChoiceAction = NewObject<USRAugmentChoiceButtonAction>(this);
		ChoiceAction->Initialize(this, ChoiceIndex);
		ChoiceActions.Add(ChoiceAction);
		ChoiceButton->OnClicked.AddDynamic(ChoiceAction, &USRAugmentChoiceButtonAction::HandleClicked);

		UVerticalBox* ChoiceContentBox = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(),
			FName(*FString::Printf(TEXT("AugmentChoiceContentBox%d"), ChoiceIndex + 1)));

		UTextBlock* NameTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			FName(*FString::Printf(TEXT("AugmentChoiceNameTextBlock%d"), ChoiceIndex + 1)));
		NameTextBlock->SetText(Choice.DisplayName.IsEmpty() ? FText::FromName(Choice.StructureId) : Choice.DisplayName);
		NameTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(0.94f, 0.98f, 1.0f, 1.0f)));
		NameTextBlock->SetAutoWrapText(true);
		if (UVerticalBoxSlot* NameSlot = ChoiceContentBox->AddChildToVerticalBox(NameTextBlock))
		{
			NameSlot->SetPadding(FMargin(12.0f, 8.0f, 12.0f, 2.0f));
		}

		UTextBlock* RarityTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			FName(*FString::Printf(TEXT("AugmentChoiceRarityTextBlock%d"), ChoiceIndex + 1)));
		RarityTextBlock->SetText(FormatRarityText(Choice.Rarity));
		RarityTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(0.56f, 0.80f, 0.82f, 1.0f)));
		if (UVerticalBoxSlot* RaritySlot = ChoiceContentBox->AddChildToVerticalBox(RarityTextBlock))
		{
			RaritySlot->SetPadding(FMargin(12.0f, 0.0f, 12.0f, 2.0f));
		}

		UTextBlock* DescriptionTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			FName(*FString::Printf(TEXT("AugmentChoiceDescriptionTextBlock%d"), ChoiceIndex + 1)));
		DescriptionTextBlock->SetText(Choice.Description);
		DescriptionTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.78f, 0.82f, 1.0f)));
		DescriptionTextBlock->SetAutoWrapText(true);
		if (UVerticalBoxSlot* DescriptionSlot = ChoiceContentBox->AddChildToVerticalBox(DescriptionTextBlock))
		{
			DescriptionSlot->SetPadding(FMargin(12.0f, 0.0f, 12.0f, 8.0f));
		}

		ChoiceButton->AddChild(ChoiceContentBox);
		ChoiceSizeBox->AddChild(ChoiceButton);
		if (UHorizontalBoxSlot* ChoiceSlot = ChoicesHorizontalBox->AddChildToHorizontalBox(ChoiceSizeBox))
		{
			ChoiceSlot->SetPadding(FMargin(ChoiceIndex == 0 ? 0.0f : 8.0f, 0.0f, 0.0f, 0.0f));
			ChoiceSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}
}

FText USRAugmentChoiceWidget::FormatCycleText() const
{
	return CycleIndex > 0
		? FText::Format(FTextFormat(NSLOCTEXT("StarRoversAugmentChoice", "CycleTextFormat", "Cycle {0}")), FText::AsNumber(CycleIndex))
		: FText::GetEmpty();
}

FText USRAugmentChoiceWidget::FormatRarityText(ESRFacilityRarity Rarity) const
{
	switch (Rarity)
	{
	case ESRFacilityRarity::Basic:
		return NSLOCTEXT("StarRoversAugmentChoice", "BasicRarity", "Basic");
	case ESRFacilityRarity::Advanced:
		return NSLOCTEXT("StarRoversAugmentChoice", "AdvancedRarity", "Advanced");
	case ESRFacilityRarity::HighTech:
		return NSLOCTEXT("StarRoversAugmentChoice", "HighTechRarity", "High Tech");
	case ESRFacilityRarity::Innovation:
		return NSLOCTEXT("StarRoversAugmentChoice", "InnovationRarity", "Innovation");
	case ESRFacilityRarity::Starting:
		return NSLOCTEXT("StarRoversAugmentChoice", "StartingRarity", "Starting");
	default:
		return FText::GetEmpty();
	}
}
