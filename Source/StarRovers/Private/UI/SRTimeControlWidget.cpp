#include "UI/SRTimeControlWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Fonts/SlateFontInfo.h"
#include "Simulation/SRTimeControlSubsystem.h"
#include "Styling/SlateColor.h"

namespace
{
	FText MakeTimeControlText(bool bIsPaused, float TimeScale)
	{
		if (bIsPaused)
		{
			return FText::Format(FTextFormat(NSLOCTEXT("StarRoversTimeControl", "PausedStatus", "Time: Paused ({0}x)")), FText::AsNumber(TimeScale));
		}

		return FText::Format(FTextFormat(NSLOCTEXT("StarRoversTimeControl", "RunningStatus", "Time: {0}x")), FText::AsNumber(TimeScale));
	}

	void AddTimeControlButtonToHorizontalBox(UHorizontalBox* TimeControlHorizontalBox, UButton* TimeControlButton)
	{
		if (!TimeControlHorizontalBox || !TimeControlButton)
		{
			return;
		}

		if (UHorizontalBoxSlot* TimeControlButtonSlot = TimeControlHorizontalBox->AddChildToHorizontalBox(TimeControlButton))
		{
			TimeControlButtonSlot->SetPadding(FMargin(4.0f, 0.0f));
		}
	}
}

TSharedRef<SWidget> USRTimeControlWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}

	BuildTimeControlWidgetTree();
	return Super::RebuildWidget();
}

void USRTimeControlWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildTimeControlWidgetTree();
	BindTimeControlButtonHandlers();
	RefreshTimeControlState();
}

void USRTimeControlWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	BuildTimeControlWidgetTree();
	RefreshTimeControlState();
}

void USRTimeControlWidget::BuildTimeControlWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* TimeControlCanvasPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TimeControlCanvasPanel"));
	WidgetTree->RootWidget = TimeControlCanvasPanel;

	TimeControlBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TimeControlBorder"));
	TimeControlBorder->SetPadding(FMargin(14.0f));
	TimeControlBorder->SetBrushColor(FLinearColor(0.03f, 0.05f, 0.08f, 0.94f));

	if (UCanvasPanelSlot* TimeControlBorderSlot = TimeControlCanvasPanel->AddChildToCanvas(TimeControlBorder))
	{
		TimeControlBorderSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
		TimeControlBorderSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		TimeControlBorderSlot->SetPosition(FVector2D(32.0f, 32.0f));
		TimeControlBorderSlot->SetAutoSize(true);
	}

	UVerticalBox* TimeControlVerticalbox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TimeControlVerticalbox"));
	TimeControlBorder->SetContent(TimeControlVerticalbox);

	TimeControlTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TimeControlTextBlock"));
	if (TimeControlTextBlock)
	{
		FSlateFontInfo TimeControlFont = TimeControlTextBlock->GetFont();
		TimeControlFont.Size = 18;
		TimeControlTextBlock->SetFont(TimeControlFont);
		TimeControlTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		TimeControlTextBlock->SetAutoWrapText(true);

		if (UVerticalBoxSlot* TimeControlTextBlockSlot = TimeControlVerticalbox->AddChildToVerticalBox(TimeControlTextBlock))
		{
			TimeControlTextBlockSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
		}
	}

	UHorizontalBox* TimeControlHorizontalBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TimeControlHorizontalBox"));
	if (UVerticalBoxSlot* TimeControlHorizontalBoxSlot = TimeControlVerticalbox->AddChildToVerticalBox(TimeControlHorizontalBox))
	{
		TimeControlHorizontalBoxSlot->SetPadding(FMargin(0.0f));
	}

	PauseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PauseButton"));
	PauseButton->SetBackgroundColor(FLinearColor(0.16f, 0.20f, 0.26f, 0.95f));
	UTextBlock* PauseButtonTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PauseButtonTextBlock"));
	PauseButtonTextBlock->SetText(NSLOCTEXT("StarRoversTimeControl", "PauseButtonLabel", "Pause"));
	PauseButtonTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	PauseButton->AddChild(PauseButtonTextBlock);
	AddTimeControlButtonToHorizontalBox(TimeControlHorizontalBox, PauseButton);

	ResumeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ResumeButton"));
	ResumeButton->SetBackgroundColor(FLinearColor(0.16f, 0.20f, 0.26f, 0.95f));
	UTextBlock* ResumeButtonTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ResumeButtonTextBlock"));
	ResumeButtonTextBlock->SetText(NSLOCTEXT("StarRoversTimeControl", "ResumeButtonLabel", "Resume"));
	ResumeButtonTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	ResumeButton->AddChild(ResumeButtonTextBlock);
	AddTimeControlButtonToHorizontalBox(TimeControlHorizontalBox, ResumeButton);

	TwoXButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("TwoXButton"));
	TwoXButton->SetBackgroundColor(FLinearColor(0.16f, 0.20f, 0.26f, 0.95f));
	UTextBlock* TwoXButtonTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TwoXButtonTextBlock"));
	TwoXButtonTextBlock->SetText(NSLOCTEXT("StarRoversTimeControl", "TwoXButtonLabel", "2x"));
	TwoXButtonTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	TwoXButton->AddChild(TwoXButtonTextBlock);
	AddTimeControlButtonToHorizontalBox(TimeControlHorizontalBox, TwoXButton);

	FourXButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("FourXButton"));
	FourXButton->SetBackgroundColor(FLinearColor(0.16f, 0.20f, 0.26f, 0.95f));
	UTextBlock* FourXButtonTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("FourXButtonTextBlock"));
	FourXButtonTextBlock->SetText(NSLOCTEXT("StarRoversTimeControl", "FourXButtonLabel", "4x"));
	FourXButtonTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	FourXButton->AddChild(FourXButtonTextBlock);
	AddTimeControlButtonToHorizontalBox(TimeControlHorizontalBox, FourXButton);

	EightXButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("EightXButton"));
	EightXButton->SetBackgroundColor(FLinearColor(0.16f, 0.20f, 0.26f, 0.95f));
	UTextBlock* EightXButtonTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EightXButtonTextBlock"));
	EightXButtonTextBlock->SetText(NSLOCTEXT("StarRoversTimeControl", "EightXButtonLabel", "8x"));
	EightXButtonTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	EightXButton->AddChild(EightXButtonTextBlock);
	AddTimeControlButtonToHorizontalBox(TimeControlHorizontalBox, EightXButton);
}

void USRTimeControlWidget::BindTimeControlButtonHandlers()
{
	if (PauseButton)
	{
		PauseButton->OnClicked.RemoveAll(this);
		PauseButton->OnClicked.AddDynamic(this, &USRTimeControlWidget::HandlePauseClicked);
	}

	if (ResumeButton)
	{
		ResumeButton->OnClicked.RemoveAll(this);
		ResumeButton->OnClicked.AddDynamic(this, &USRTimeControlWidget::HandleResumeClicked);
	}

	if (TwoXButton)
	{
		TwoXButton->OnClicked.RemoveAll(this);
		TwoXButton->OnClicked.AddDynamic(this, &USRTimeControlWidget::HandleTwoXClicked);
	}

	if (FourXButton)
	{
		FourXButton->OnClicked.RemoveAll(this);
		FourXButton->OnClicked.AddDynamic(this, &USRTimeControlWidget::HandleFourXClicked);
	}

	if (EightXButton)
	{
		EightXButton->OnClicked.RemoveAll(this);
		EightXButton->OnClicked.AddDynamic(this, &USRTimeControlWidget::HandleEightXClicked);
	}
}

void USRTimeControlWidget::RefreshTimeControlState()
{
	const USRTimeControlSubsystem* TimeControlSubsystem = GetTimeControlSubsystem();
	const bool bHasTimeControlSubsystem = IsValid(TimeControlSubsystem);
	const bool bIsPaused = bHasTimeControlSubsystem ? TimeControlSubsystem->IsSimulationPaused() : false;
	const float CurrentTimeScale = bHasTimeControlSubsystem ? TimeControlSubsystem->GetTimeScale() : 1.0f;

	if (TimeControlTextBlock)
	{
		TimeControlTextBlock->SetText(
			bHasTimeControlSubsystem
				? MakeTimeControlText(bIsPaused, CurrentTimeScale)
				: NSLOCTEXT("StarRoversTimeControl", "NoManagerStatus", "Time: Subsystem missing")
		);
	}

	const bool bIs1xActive = !bIsPaused && FMath::IsNearlyEqual(CurrentTimeScale, 1.0f);
	const bool bIs2xActive = !bIsPaused && FMath::IsNearlyEqual(CurrentTimeScale, 2.0f);
	const bool bIs4xActive = !bIsPaused && FMath::IsNearlyEqual(CurrentTimeScale, 4.0f);
	const bool bIs8xActive = !bIsPaused && FMath::IsNearlyEqual(CurrentTimeScale, 8.0f);

	UpdateButtonStyle(PauseButton, bHasTimeControlSubsystem && bIsPaused);
	UpdateButtonStyle(ResumeButton, bHasTimeControlSubsystem && bIs1xActive);
	UpdateButtonStyle(TwoXButton, bHasTimeControlSubsystem && bIs2xActive);
	UpdateButtonStyle(FourXButton, bHasTimeControlSubsystem && bIs4xActive);
	UpdateButtonStyle(EightXButton, bHasTimeControlSubsystem && bIs8xActive);

	if (PauseButton)
	{
		PauseButton->SetIsEnabled(bHasTimeControlSubsystem && !bIsPaused);
	}

	if (ResumeButton)
	{
		ResumeButton->SetIsEnabled(bHasTimeControlSubsystem);
	}

	if (TwoXButton)
	{
		TwoXButton->SetIsEnabled(bHasTimeControlSubsystem);
	}

	if (FourXButton)
	{
		FourXButton->SetIsEnabled(bHasTimeControlSubsystem);
	}

	if (EightXButton)
	{
		EightXButton->SetIsEnabled(bHasTimeControlSubsystem);
	}

	OnTimeControlChanged(bIsPaused, CurrentTimeScale);
}

USRTimeControlSubsystem* USRTimeControlWidget::GetTimeControlSubsystem() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<USRTimeControlSubsystem>() : nullptr;
}

void USRTimeControlWidget::UpdateButtonStyle(UButton* Button, bool bIsActive) const
{
	if (!Button)
	{
		return;
	}

	Button->SetBackgroundColor(
		bIsActive
			? FLinearColor(0.15f, 0.42f, 0.30f, 1.0f)
			: FLinearColor(0.16f, 0.20f, 0.26f, 0.95f)
	);
}

void USRTimeControlWidget::HandlePauseClicked()
{
	if (USRTimeControlSubsystem* TimeControlSubsystem = GetTimeControlSubsystem())
	{
		TimeControlSubsystem->PauseSimulation();
	}

	RefreshTimeControlState();
}

void USRTimeControlWidget::HandleResumeClicked()
{
	if (USRTimeControlSubsystem* TimeControlSubsystem = GetTimeControlSubsystem())
	{
		TimeControlSubsystem->ResumeSimulation();
	}

	RefreshTimeControlState();
}

void USRTimeControlWidget::HandleTwoXClicked()
{
	if (USRTimeControlSubsystem* TimeControlSubsystem = GetTimeControlSubsystem())
	{
		TimeControlSubsystem->SetSimulationSpeedPreset(2.0f);
	}

	RefreshTimeControlState();
}

void USRTimeControlWidget::HandleFourXClicked()
{
	if (USRTimeControlSubsystem* TimeControlSubsystem = GetTimeControlSubsystem())
	{
		TimeControlSubsystem->SetSimulationSpeedPreset(4.0f);
	}

	RefreshTimeControlState();
}

void USRTimeControlWidget::HandleEightXClicked()
{
	if (USRTimeControlSubsystem* TimeControlSubsystem = GetTimeControlSubsystem())
	{
		TimeControlSubsystem->SetSimulationSpeedPreset(8.0f);
	}

	RefreshTimeControlState();
}
