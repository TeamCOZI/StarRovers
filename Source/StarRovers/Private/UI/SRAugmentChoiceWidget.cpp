#include "UI/SRAugmentChoiceWidget.h"

#include "UI/SRAugmentChoicePresentation.h"
#include "UI/SRUIComponents.h"
#include "UI/SRUILayoutPolicy.h"
#include "UI/SRUITheme.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/SlateColor.h"
#include "InputCoreTypes.h"

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

	ApplySharedUITheme();
	BuildAugmentChoiceWidgetTree();
	return Super::RebuildWidget();
}

void USRAugmentChoiceWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ApplySharedUITheme();
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
		RefreshHeaderBadges();
		RebuildChoiceButtons();
	}
	ApplySharedUITheme();
}

FReply USRAugmentChoiceWidget::NativeOnKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_LeftShoulder)
	{
		return NavigateChoiceFocus(-1) ? FReply::Handled() : FReply::Unhandled();
	}
	if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_RightShoulder)
	{
		return NavigateChoiceFocus(1) ? FReply::Handled() : FReply::Unhandled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void USRAugmentChoiceWidget::ApplySharedUITheme()
{
	if (!bUseSharedUITheme)
	{
		return;
	}

	const USRUIThemeSettings* Theme = USRUIThemeLibrary::GetThemeSettings();
	if (!IsValid(Theme))
	{
		return;
	}

	PanelColor = Theme->PanelColor;
	ChoiceButtonColor = USRUIThemeLibrary::ResolveStatePalette(ESRUIVisualState::Neutral).SurfaceColor;
	ChoiceButtonHoverColor = USRUIThemeLibrary::ResolveStatePalette(ESRUIVisualState::Hovered).SurfaceColor;
	if (PanelBorder)
	{
		PanelBorder->SetBrushColor(PanelColor);
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
		if (SubtitleTextBlock)
		{
			SubtitleTextBlock->SetText(ResolveSubtitleText());
		}
		if (CycleTextBlock)
		{
			CycleTextBlock->SetText(FormatCycleText());
		}
		RefreshHeaderBadges();
		RebuildChoiceButtons();
	}
}

void USRAugmentChoiceWidget::ClearAugmentChoices()
{
	Choices.Reset();
	CycleIndex = 0;
	RefreshHeaderBadges();
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

bool USRAugmentChoiceWidget::FocusDefaultChoice()
{
	for (int32 ChoiceIndex = 0; ChoiceIndex < ChoiceButtons.Num(); ++ChoiceIndex)
	{
		if (FocusChoiceByIndex(ChoiceIndex))
		{
			return true;
		}
	}
	return false;
}

bool USRAugmentChoiceWidget::NavigateChoiceFocus(int32 Direction)
{
	if (ChoiceButtons.IsEmpty() || Direction == 0)
	{
		return false;
	}

	const int32 StepDirection = FMath::Sign(Direction);
	const int32 StartIndex = ChoiceButtons.IsValidIndex(FocusedChoiceIndex)
		? FocusedChoiceIndex
		: (StepDirection > 0 ? -1 : 0);
	for (int32 Step = 1; Step <= ChoiceButtons.Num(); ++Step)
	{
		const int32 NextIndex = (StartIndex + StepDirection * Step + ChoiceButtons.Num() * 2)
			% ChoiceButtons.Num();
		if (FocusChoiceByIndex(NextIndex))
		{
			return true;
		}
	}
	return false;
}

int32 USRAugmentChoiceWidget::GetFocusedChoiceIndex() const
{
	return FocusedChoiceIndex;
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
		if (PanelScaleBox && PanelDesignSizeBox && PanelBorder)
		{
			if (CycleTextBlock)
			{
				CycleTextBlock->SetText(FormatCycleText());
			}
			RebuildChoiceButtons();
			return;
		}

		// Upgrade configured Blueprint trees authored before the responsive
		// modal contract by rebuilding the complete native hierarchy.
		WidgetTree->RootWidget = nullptr;
		ChoiceActions.Reset();
		ChoiceButtons.Reset();
		FocusedChoiceIndex = INDEX_NONE;
	}

	ChoiceActions.Reset();
	ChoiceButtons.Reset();
	FocusedChoiceIndex = INDEX_NONE;

	UCanvasPanel* RootCanvasPanel = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("AugmentChoiceCanvasPanel"));
	WidgetTree->RootWidget = RootCanvasPanel;
	RootCanvasPanel->SetVisibility(ESlateVisibility::Visible);

	PanelScaleBox = WidgetTree->ConstructWidget<UScaleBox>(
		UScaleBox::StaticClass(),
		TEXT("AugmentChoicePanelScaleBox"));
	PanelScaleBox->SetStretch(EStretch::ScaleToFit);
	PanelScaleBox->SetStretchDirection(EStretchDirection::DownOnly);
	if (UCanvasPanelSlot* PanelScaleCanvasSlot = RootCanvasPanel->AddChildToCanvas(PanelScaleBox))
	{
		PanelScaleCanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		PanelScaleCanvasSlot->SetOffsets(FMargin(FSRUILayoutPolicy::DefaultSafeMargin));
	}

	PanelDesignSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		TEXT("AugmentChoicePanelDesignSizeBox"));
	PanelDesignSizeBox->SetWidthOverride(FMath::Max(240.0f, PanelWidth));
	PanelDesignSizeBox->SetHeightOverride(FMath::Max(240.0f, PanelHeight));
	if (UScaleBoxSlot* DesignSlot = Cast<UScaleBoxSlot>(PanelScaleBox->AddChild(PanelDesignSizeBox)))
	{
		DesignSlot->SetHorizontalAlignment(HAlign_Center);
		DesignSlot->SetVerticalAlignment(VAlign_Center);
	}

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("AugmentChoicePanelBorder"));
	PanelBorder->SetBrushColor(PanelColor);
	PanelBorder->SetPadding(FMargin(20.0f, 18.0f));
	PanelDesignSizeBox->AddChild(PanelBorder);

	PanelVerticalBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("AugmentChoicePanelVerticalBox"));
	PanelBorder->SetContent(PanelVerticalBox);

	TitleTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("AugmentChoiceTitleTextBlock"));
	TitleTextBlock->SetText(TitleText);
	USRUIThemeLibrary::ApplyTextStyle(TitleTextBlock, ESRUITextStyle::Title);
	TitleTextBlock->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* TitleSlot = PanelVerticalBox->AddChildToVerticalBox(TitleTextBlock))
	{
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	}

	SubtitleTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("AugmentChoiceSubtitleTextBlock"));
	SubtitleTextBlock->SetText(ResolveSubtitleText());
	USRUIThemeLibrary::ApplyTextStyle(SubtitleTextBlock, ESRUITextStyle::Body);
	SubtitleTextBlock->SetJustification(ETextJustify::Center);
	SubtitleTextBlock->SetAutoWrapText(true);
	if (UVerticalBoxSlot* SubtitleSlot = PanelVerticalBox->AddChildToVerticalBox(SubtitleTextBlock))
	{
		SubtitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}

	HeaderStatusBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		TEXT("AugmentChoiceHeaderStatusBox"));
	if (UVerticalBoxSlot* HeaderStatusSlot = PanelVerticalBox->AddChildToVerticalBox(HeaderStatusBox))
	{
		HeaderStatusSlot->SetHorizontalAlignment(HAlign_Center);
		HeaderStatusSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 12.0f));
	}

	CycleStatusBadge = WidgetTree->ConstructWidget<USRStatusBadgeWidget>(
		USRStatusBadgeWidget::StaticClass(),
		TEXT("AugmentChoiceCycleStatusBadge"));
	GuaranteeStatusBadge = WidgetTree->ConstructWidget<USRStatusBadgeWidget>(
		USRStatusBadgeWidget::StaticClass(),
		TEXT("AugmentChoiceGuaranteeStatusBadge"));
	DecisionStatusBadge = WidgetTree->ConstructWidget<USRStatusBadgeWidget>(
		USRStatusBadgeWidget::StaticClass(),
		TEXT("AugmentChoiceDecisionStatusBadge"));
	for (USRStatusBadgeWidget* Badge : { CycleStatusBadge, GuaranteeStatusBadge, DecisionStatusBadge })
	{
		if (UHorizontalBoxSlot* BadgeSlot = HeaderStatusBox->AddChildToHorizontalBox(Badge))
		{
			BadgeSlot->SetPadding(FMargin(4.0f, 0.0f));
		}
	}

	ChoicesScrollBox = WidgetTree->ConstructWidget<UScrollBox>(
		UScrollBox::StaticClass(),
		TEXT("AugmentChoiceChoicesScrollBox"));
	ChoicesScrollBox->SetOrientation(Orient_Horizontal);
	ChoicesScrollBox->SetScrollBarVisibility(ESlateVisibility::Visible);
	ChoicesScrollBox->SetAnimateWheelScrolling(true);
	if (UVerticalBoxSlot* ScrollSlot = PanelVerticalBox->AddChildToVerticalBox(ChoicesScrollBox))
	{
		ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	ChoicesHorizontalBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		TEXT("AugmentChoiceChoicesHorizontalBox"));
	ChoicesScrollBox->AddChild(ChoicesHorizontalBox);

	RefreshHeaderBadges();
	RebuildChoiceButtons();
}

void USRAugmentChoiceWidget::CacheAugmentChoiceWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}

	PanelScaleBox = Cast<UScaleBox>(WidgetTree->FindWidget(TEXT("AugmentChoicePanelScaleBox")));
	PanelDesignSizeBox = Cast<USizeBox>(WidgetTree->FindWidget(TEXT("AugmentChoicePanelDesignSizeBox")));
	PanelBorder = Cast<UBorder>(WidgetTree->FindWidget(TEXT("AugmentChoicePanelBorder")));
	PanelVerticalBox = Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("AugmentChoicePanelVerticalBox")));
	TitleTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("AugmentChoiceTitleTextBlock")));
	SubtitleTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("AugmentChoiceSubtitleTextBlock")));
	CycleTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("AugmentChoiceCycleTextBlock")));
	HeaderStatusBox = Cast<UHorizontalBox>(WidgetTree->FindWidget(TEXT("AugmentChoiceHeaderStatusBox")));
	CycleStatusBadge = Cast<USRStatusBadgeWidget>(WidgetTree->FindWidget(TEXT("AugmentChoiceCycleStatusBadge")));
	GuaranteeStatusBadge = Cast<USRStatusBadgeWidget>(WidgetTree->FindWidget(TEXT("AugmentChoiceGuaranteeStatusBadge")));
	DecisionStatusBadge = Cast<USRStatusBadgeWidget>(WidgetTree->FindWidget(TEXT("AugmentChoiceDecisionStatusBadge")));
	ChoicesScrollBox = Cast<UScrollBox>(WidgetTree->FindWidget(TEXT("AugmentChoiceChoicesScrollBox")));
	ChoicesHorizontalBox = Cast<UHorizontalBox>(WidgetTree->FindWidget(TEXT("AugmentChoiceChoicesHorizontalBox")));

	if (TitleTextBlock)
	{
		TitleTextBlock->SetText(TitleText);
		USRUIThemeLibrary::ApplyTextStyle(TitleTextBlock, ESRUITextStyle::Title);
	}
	if (SubtitleTextBlock)
	{
		SubtitleTextBlock->SetText(ResolveSubtitleText());
		USRUIThemeLibrary::ApplyTextStyle(SubtitleTextBlock, ESRUITextStyle::Body);
	}
	if (CycleTextBlock)
	{
		CycleTextBlock->SetText(FormatCycleText());
		USRUIThemeLibrary::ApplyTextStyle(CycleTextBlock, ESRUITextStyle::Caption);
	}
	RefreshHeaderBadges();
}

void USRAugmentChoiceWidget::RebuildChoiceButtons()
{
	if (!ChoicesHorizontalBox || !WidgetTree)
	{
		return;
	}

	ChoicesHorizontalBox->ClearChildren();
	ChoiceActions.Reset();
	ChoiceButtons.Reset();
	FocusedChoiceIndex = INDEX_NONE;
	FSRAugmentBuildContextV2 PresentationContext;
	if (USRAugmentSubsystem* AugmentSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USRAugmentSubsystem>() : nullptr)
	{
		PresentationContext = AugmentSubsystem->BuildResourceV2OfferContext();
	}

	for (int32 ChoiceIndex = 0; ChoiceIndex < Choices.Num(); ++ChoiceIndex)
	{
		const FSRAugmentChoice& Choice = Choices[ChoiceIndex];
		const FSRAugmentChoicePresentation Presentation =
			FSRAugmentChoicePresentationBuilder::Build(Choice, PresentationContext);

		USizeBox* ChoiceSizeBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			FName(*FString::Printf(TEXT("AugmentChoiceSizeBox%d"), ChoiceIndex + 1)));
		ChoiceSizeBox->SetWidthOverride(FMath::Max(240.0f, ChoiceButtonWidth));
		ChoiceSizeBox->SetHeightOverride(FMath::Max(240.0f, ChoiceButtonHeight));

		UButton* ChoiceButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(),
			FName(*FString::Printf(TEXT("AugmentChoiceButton%d"), ChoiceIndex + 1)));
		ChoiceButton->SetBackgroundColor(ChoiceButtonColor);
		ChoiceButton->SetToolTipText(Presentation.FullDetailText);
		ChoiceButton->SetIsEnabled(Presentation.bEligibleInContext);
		ChoiceButtons.Add(ChoiceButton);

		USRAugmentChoiceButtonAction* ChoiceAction = NewObject<USRAugmentChoiceButtonAction>(this);
		ChoiceAction->Initialize(this, ChoiceIndex);
		ChoiceActions.Add(ChoiceAction);
		ChoiceButton->OnClicked.AddDynamic(ChoiceAction, &USRAugmentChoiceButtonAction::HandleClicked);

		USRThemedCardWidget* ChoiceCard = WidgetTree->ConstructWidget<USRThemedCardWidget>(
			USRThemedCardWidget::StaticClass(),
			FName(*FString::Printf(TEXT("AugmentChoiceCard%d"), ChoiceIndex + 1)));
		ChoiceCard->SetVisualState(Presentation.CardState);
		ChoiceCard->SetCardPadding(FMargin(14.0f, 12.0f));

		UVerticalBox* ChoiceContentBox = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(),
			FName(*FString::Printf(TEXT("AugmentChoiceContentBox%d"), ChoiceIndex + 1)));

		UHorizontalBox* BadgeRow = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(),
			FName(*FString::Printf(TEXT("AugmentChoiceBadgeRow%d"), ChoiceIndex + 1)));
		if (UVerticalBoxSlot* BadgeRowSlot = ChoiceContentBox->AddChildToVerticalBox(BadgeRow))
		{
			BadgeRowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));
		}
		USRStatusBadgeWidget* OfferBadge = WidgetTree->ConstructWidget<USRStatusBadgeWidget>(
			USRStatusBadgeWidget::StaticClass(),
			FName(*FString::Printf(TEXT("AugmentChoiceOfferBadge%d"), ChoiceIndex + 1)));
		OfferBadge->SetBadge(Presentation.OfferBadgeText, Presentation.OfferState);
		if (UHorizontalBoxSlot* OfferSlot = BadgeRow->AddChildToHorizontalBox(OfferBadge))
		{
			OfferSlot->SetPadding(FMargin(0.0f, 0.0f, 5.0f, 0.0f));
		}
		USRStatusBadgeWidget* RarityBadge = WidgetTree->ConstructWidget<USRStatusBadgeWidget>(
			USRStatusBadgeWidget::StaticClass(),
			FName(*FString::Printf(TEXT("AugmentChoiceRarityBadge%d"), ChoiceIndex + 1)));
		RarityBadge->SetBadge(Presentation.RarityBadgeText, ESRUIVisualState::Neutral);
		BadgeRow->AddChildToHorizontalBox(RarityBadge);

		UTextBlock* NameTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			FName(*FString::Printf(TEXT("AugmentChoiceNameTextBlock%d"), ChoiceIndex + 1)));
		NameTextBlock->SetText(Presentation.TitleText);
		USRUIThemeLibrary::ApplyTextStyle(NameTextBlock, ESRUITextStyle::Heading);
		NameTextBlock->SetAutoWrapText(true);
		if (UVerticalBoxSlot* NameSlot = ChoiceContentBox->AddChildToVerticalBox(NameTextBlock))
		{
			NameSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		}

		UHorizontalBox* RunContextBadgeRow = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(),
			FName(*FString::Printf(TEXT("AugmentChoiceRunContextBadgeRow%d"), ChoiceIndex + 1)));
		if (UVerticalBoxSlot* ContextRowSlot = ChoiceContentBox->AddChildToVerticalBox(RunContextBadgeRow))
		{
			ContextRowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));
		}
		USRStatusBadgeWidget* RunFitBadge = WidgetTree->ConstructWidget<USRStatusBadgeWidget>(
			USRStatusBadgeWidget::StaticClass(),
			FName(*FString::Printf(TEXT("AugmentChoiceFitBadge%d"), ChoiceIndex + 1)));
		RunFitBadge->SetBadge(Presentation.RunFitBadgeText, Presentation.RunFitState);
		if (UHorizontalBoxSlot* FitBadgeSlot = RunContextBadgeRow->AddChildToHorizontalBox(RunFitBadge))
		{
			FitBadgeSlot->SetPadding(FMargin(0.0f, 0.0f, 5.0f, 0.0f));
		}
		USRStatusBadgeWidget* StrategyBadge = WidgetTree->ConstructWidget<USRStatusBadgeWidget>(
			USRStatusBadgeWidget::StaticClass(),
			FName(*FString::Printf(TEXT("AugmentChoiceStrategyBadge%d"), ChoiceIndex + 1)));
		StrategyBadge->SetBadge(Presentation.StrategyBadgeText, Presentation.OfferState);
		RunContextBadgeRow->AddChildToHorizontalBox(StrategyBadge);

		auto AddSection = [this, ChoiceContentBox, ChoiceIndex](
			const TCHAR* HeaderName,
			const TCHAR* DetailName,
			const FText& HeaderText,
			const FText& DetailText,
			ESRUIVisualState DetailState)
		{
			UTextBlock* HeaderTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(),
				FName(*FString::Printf(TEXT("%s%d"), HeaderName, ChoiceIndex + 1)));
			HeaderTextBlock->SetText(HeaderText);
			USRUIThemeLibrary::ApplyTextStyle(HeaderTextBlock, ESRUITextStyle::Caption, DetailState, true);
			if (UVerticalBoxSlot* HeaderSlot = ChoiceContentBox->AddChildToVerticalBox(HeaderTextBlock))
			{
				HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
			}
			UTextBlock* DetailTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(),
				FName(*FString::Printf(TEXT("%s%d"), DetailName, ChoiceIndex + 1)));
			DetailTextBlock->SetText(DetailText);
			USRUIThemeLibrary::ApplyTextStyle(DetailTextBlock, ESRUITextStyle::Caption, DetailState);
			DetailTextBlock->SetAutoWrapText(true);
			if (UVerticalBoxSlot* DetailSlot = ChoiceContentBox->AddChildToVerticalBox(DetailTextBlock))
			{
				DetailSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
			}
		};

		AddSection(
			TEXT("AugmentChoiceFitHeaderTextBlock"),
			TEXT("AugmentChoiceFitTextBlock"),
			Presentation.FitSectionText,
			Presentation.FitDetailText,
			Presentation.RunFitState);
		AddSection(
			TEXT("AugmentChoiceUnlockHeaderTextBlock"),
			TEXT("AugmentChoiceGrantTextBlock"),
			Presentation.UnlockSectionText,
			Presentation.UnlockDetailText,
			Presentation.bEligibleInContext ? ESRUIVisualState::Positive : ESRUIVisualState::Disabled);

		UTextBlock* FlowHeaderTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			FName(*FString::Printf(TEXT("AugmentChoiceFlowHeaderTextBlock%d"), ChoiceIndex + 1)));
		FlowHeaderTextBlock->SetText(NSLOCTEXT("StarRoversAugmentChoice", "ConditionEffectSection", "WHEN  >  RESULT"));
		USRUIThemeLibrary::ApplyTextStyle(FlowHeaderTextBlock, ESRUITextStyle::Caption, ESRUIVisualState::Info, true);
		if (UVerticalBoxSlot* FlowHeaderSlot = ChoiceContentBox->AddChildToVerticalBox(FlowHeaderTextBlock))
		{
			FlowHeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f));
		}

		UVerticalBox* ConditionEffectBox = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(),
			FName(*FString::Printf(TEXT("AugmentChoiceConditionEffectBox%d"), ChoiceIndex + 1)));
		if (UVerticalBoxSlot* FlowBoxSlot = ChoiceContentBox->AddChildToVerticalBox(ConditionEffectBox))
		{
			FlowBoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));
		}

		for (int32 FlowIndex = 0; FlowIndex < Presentation.ConditionEffectRows.Num(); ++FlowIndex)
		{
			const FSRAugmentConditionEffectPresentation& Flow = Presentation.ConditionEffectRows[FlowIndex];
			UHorizontalBox* FlowRow = WidgetTree->ConstructWidget<UHorizontalBox>(
				UHorizontalBox::StaticClass(),
				FName(*FString::Printf(TEXT("AugmentChoiceConditionEffectRow%d_%d"), ChoiceIndex + 1, FlowIndex + 1)));
			FlowRow->SetToolTipText(Flow.DetailText);
			if (UVerticalBoxSlot* FlowRowSlot = ConditionEffectBox->AddChildToVerticalBox(FlowRow))
			{
				FlowRowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, FlowIndex + 1 < Presentation.ConditionEffectRows.Num() ? 4.0f : 0.0f));
			}

			USRThemedCardWidget* ConditionCard = WidgetTree->ConstructWidget<USRThemedCardWidget>(
				USRThemedCardWidget::StaticClass(),
				FName(*FString::Printf(TEXT("AugmentChoiceConditionCard%d_%d"), ChoiceIndex + 1, FlowIndex + 1)));
			ConditionCard->SetVisualState(Flow.ConditionState);
			ConditionCard->SetCardPadding(FMargin(5.0f, 5.0f));
			UTextBlock* ConditionTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(),
				FName(*FString::Printf(TEXT("AugmentChoiceConditionTextBlock%d_%d"), ChoiceIndex + 1, FlowIndex + 1)));
			ConditionTextBlock->SetText(Flow.ConditionText);
			ConditionTextBlock->SetJustification(ETextJustify::Center);
			ConditionTextBlock->SetAutoWrapText(true);
			USRUIThemeLibrary::ApplyTextStyle(ConditionTextBlock, ESRUITextStyle::Caption, Flow.ConditionState, true);
			ConditionCard->SetContent(ConditionTextBlock);
			if (UHorizontalBoxSlot* ConditionSlot = FlowRow->AddChildToHorizontalBox(ConditionCard))
			{
				ConditionSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				ConditionSlot->SetVerticalAlignment(VAlign_Fill);
			}

			UTextBlock* ArrowTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(),
				FName(*FString::Printf(TEXT("AugmentChoiceFlowArrow%d_%d"), ChoiceIndex + 1, FlowIndex + 1)));
			ArrowTextBlock->SetText(FText::FromString(TEXT(">")));
			ArrowTextBlock->SetJustification(ETextJustify::Center);
			USRUIThemeLibrary::ApplyTextStyle(ArrowTextBlock, ESRUITextStyle::Body, ESRUIVisualState::Info, true);
			if (UHorizontalBoxSlot* ArrowSlot = FlowRow->AddChildToHorizontalBox(ArrowTextBlock))
			{
				ArrowSlot->SetPadding(FMargin(5.0f, 0.0f));
				ArrowSlot->SetVerticalAlignment(VAlign_Center);
			}

			USRThemedCardWidget* EffectCard = WidgetTree->ConstructWidget<USRThemedCardWidget>(
				USRThemedCardWidget::StaticClass(),
				FName(*FString::Printf(TEXT("AugmentChoiceEffectCard%d_%d"), ChoiceIndex + 1, FlowIndex + 1)));
			EffectCard->SetVisualState(Flow.EffectState);
			EffectCard->SetCardPadding(FMargin(5.0f, 5.0f));
			UTextBlock* EffectTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(),
				FName(*FString::Printf(TEXT("AugmentChoiceEffectTextBlock%d_%d"), ChoiceIndex + 1, FlowIndex + 1)));
			EffectTextBlock->SetText(Flow.EffectText);
			EffectTextBlock->SetJustification(ETextJustify::Center);
			EffectTextBlock->SetAutoWrapText(true);
			USRUIThemeLibrary::ApplyTextStyle(EffectTextBlock, ESRUITextStyle::Caption, Flow.EffectState, true);
			EffectCard->SetContent(EffectTextBlock);
			if (UHorizontalBoxSlot* EffectSlot = FlowRow->AddChildToHorizontalBox(EffectCard))
			{
				EffectSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				EffectSlot->SetVerticalAlignment(VAlign_Fill);
			}
		}

		AddSection(
			TEXT("AugmentChoiceImpactHeaderTextBlock"),
			TEXT("AugmentChoiceImpactTextBlock"),
			NSLOCTEXT("StarRoversAugmentChoice", "LineImpactSection", "LINE SHAPE"),
			Presentation.ImpactDetailText,
			ESRUIVisualState::Info);
		AddSection(
			TEXT("AugmentChoiceRiskHeaderTextBlock"),
			TEXT("AugmentChoiceRiskTextBlock"),
			NSLOCTEXT("StarRoversAugmentChoice", "WatchSection", "WATCH"),
			Presentation.WatchSummaryText,
			Presentation.RiskState);

		USpacer* FlexibleSpacer = WidgetTree->ConstructWidget<USpacer>(
			USpacer::StaticClass(),
			FName(*FString::Printf(TEXT("AugmentChoiceFlexibleSpacer%d"), ChoiceIndex + 1)));
		if (UVerticalBoxSlot* SpacerSlot = ChoiceContentBox->AddChildToVerticalBox(FlexibleSpacer))
		{
			SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		UTextBlock* SelectTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			FName(*FString::Printf(TEXT("AugmentChoiceSelectTextBlock%d"), ChoiceIndex + 1)));
		SelectTextBlock->SetText(Presentation.SelectActionText);
		SelectTextBlock->SetJustification(ETextJustify::Center);
		USRUIThemeLibrary::ApplyTextStyle(SelectTextBlock, ESRUITextStyle::Body, ESRUIVisualState::Selected, true);
		if (UVerticalBoxSlot* SelectSlot = ChoiceContentBox->AddChildToVerticalBox(SelectTextBlock))
		{
			SelectSlot->SetPadding(FMargin(0.0f, 7.0f, 0.0f, 2.0f));
		}

		ChoiceCard->SetContent(ChoiceContentBox);
		ChoiceButton->AddChild(ChoiceCard);
		ChoiceSizeBox->AddChild(ChoiceButton);
		if (UHorizontalBoxSlot* ChoiceSlot = ChoicesHorizontalBox->AddChildToHorizontalBox(ChoiceSizeBox))
		{
			ChoiceSlot->SetPadding(FMargin(ChoiceIndex == 0 ? 0.0f : 10.0f, 0.0f, 0.0f, 0.0f));
			ChoiceSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}
}

bool USRAugmentChoiceWidget::FocusChoiceByIndex(int32 ChoiceIndex)
{
	if (!ChoiceButtons.IsValidIndex(ChoiceIndex)
		|| !IsValid(ChoiceButtons[ChoiceIndex])
		|| !ChoiceButtons[ChoiceIndex]->GetIsEnabled())
	{
		return false;
	}

	FocusedChoiceIndex = ChoiceIndex;
	UButton* ChoiceButton = ChoiceButtons[ChoiceIndex];
	if (APlayerController* OwningPlayer = GetOwningPlayer())
	{
		ChoiceButton->SetUserFocus(OwningPlayer);
	}
	ChoiceButton->SetKeyboardFocus();
	if (ChoicesScrollBox)
	{
		ChoicesScrollBox->ScrollWidgetIntoView(
			ChoiceButton,
			true,
			EDescendantScrollDestination::IntoView,
			16.0f);
	}
	return true;
}

void USRAugmentChoiceWidget::RefreshHeaderBadges()
{
	if (CycleStatusBadge)
	{
		CycleStatusBadge->SetBadge(
			CycleIndex > 0
				? FormatCycleText()
				: NSLOCTEXT("StarRoversAugmentChoice", "OfferStatus", "AUGMENT OFFER"),
			ESRUIVisualState::Info);
	}
	if (GuaranteeStatusBadge)
	{
		GuaranteeStatusBadge->SetBadge(
			NSLOCTEXT("StarRoversAugmentChoice", "CoreTechGuaranteed", "CORE FACILITIES OWNED"),
			ESRUIVisualState::Positive);
	}
	if (DecisionStatusBadge)
	{
		bool bContainsPackage = false;
		for (const FSRAugmentChoice& Choice : Choices)
		{
			bContainsPackage |= Choice.ChoiceKind == ESRAugmentChoiceKind::ResourceV2Package;
		}
		DecisionStatusBadge->SetBadge(
			bContainsPackage
				? NSLOCTEXT("StarRoversAugmentChoice", "ChooseOnePackage", "CHOOSE 1 LINE PACKAGE")
				: NSLOCTEXT("StarRoversAugmentChoice", "ChooseOneFacility", "CHOOSE 1 FACILITY"),
			ESRUIVisualState::Warning);
	}
}

FText USRAugmentChoiceWidget::FormatCycleText() const
{
	return CycleIndex > 0
		? FText::Format(FTextFormat(NSLOCTEXT("StarRoversAugmentChoice", "CycleTextFormat", "Cycle {0}")), FText::AsNumber(CycleIndex))
		: FText::GetEmpty();
}

FText USRAugmentChoiceWidget::ResolveSubtitleText() const
{
	for (const FSRAugmentChoice& Choice : Choices)
	{
		if (Choice.ChoiceKind == ESRAugmentChoiceKind::ResourceV2Package)
		{
			return PackageSubtitleText;
		}
	}
	return SubtitleText;
}
