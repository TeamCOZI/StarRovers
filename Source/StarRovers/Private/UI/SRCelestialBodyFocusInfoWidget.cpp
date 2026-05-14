#include "UI/SRCelestialBodyFocusInfoWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateColor.h"

TSharedRef<SWidget> USRCelestialBodyFocusInfoWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}

	BuildFocusInfoWidgetTree();
	return Super::RebuildWidget();
}

void USRCelestialBodyFocusInfoWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildFocusInfoWidgetTree();
	BindAssemblyModeButtonHandler();
	RefreshFocusInfoText();
}

void USRCelestialBodyFocusInfoWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	BuildFocusInfoWidgetTree();
	BindAssemblyModeButtonHandler();
	RefreshFocusInfoText();
}

void USRCelestialBodyFocusInfoWidget::SetFocusInfo(const FSRCelestialBodyFocusInfo& NewFocusInfo)
{
	FocusInfo = NewFocusInfo;
	bHasFocusInfo = NewFocusInfo.bIsValid;
	RefreshFocusInfoText();
	OnFocusInfoChanged(FocusInfo);
}

void USRCelestialBodyFocusInfoWidget::ClearFocusInfo()
{
	FocusInfo = FSRCelestialBodyFocusInfo();
	bHasFocusInfo = false;
	RefreshFocusInfoText();
	OnFocusCleared();
}

bool USRCelestialBodyFocusInfoWidget::HasFocusInfo() const
{
	return bHasFocusInfo;
}

FSRCelestialBodyFocusInfo USRCelestialBodyFocusInfoWidget::GetFocusInfo() const
{
	return FocusInfo;
}

void USRCelestialBodyFocusInfoWidget::SetAssemblyModeActive(bool bNewAssemblyModeActive)
{
	bAssemblyModeActive = bNewAssemblyModeActive;
	RefreshAssemblyModeButton();
}

bool USRCelestialBodyFocusInfoWidget::IsAssemblyModeActive() const
{
	return bAssemblyModeActive;
}

FSRStarRoversAssemblyModeRequestedSignature& USRCelestialBodyFocusInfoWidget::OnAssemblyModeRequested()
{
	return AssemblyModeRequestedEvent;
}

void USRCelestialBodyFocusInfoWidget::BuildFocusInfoWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}

	if (WidgetTree->RootWidget)
	{
		EnsureAssemblyModeButton(WidgetTree->RootWidget);
		BindAssemblyModeButtonHandler();
		RefreshAssemblyModeButton();
		return;
	}

	UCanvasPanel* FocusInfoCanvasPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FocusInfoCanvasPanel"));
	WidgetTree->RootWidget = FocusInfoCanvasPanel;

	FocusInfoBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FocusInfoBorder"));
	FocusInfoBorder->SetPadding(FMargin(16.0f));
	FocusInfoBorder->SetBrushColor(FLinearColor(0.02f, 0.04f, 0.08f, 0.92f));

	if (UCanvasPanelSlot* FocusInfoBorderSlot = FocusInfoCanvasPanel->AddChildToCanvas(FocusInfoBorder))
	{
		FocusInfoBorderSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
		FocusInfoBorderSlot->SetAlignment(FVector2D(1.0f, 0.0f));
		FocusInfoBorderSlot->SetPosition(FVector2D(-32.0f, 32.0f));
		FocusInfoBorderSlot->SetAutoSize(true);
	}

	UVerticalBox* FocusInfoVerticalBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FocusInfoVerticalBox"));
	FocusInfoBorder->SetContent(FocusInfoVerticalBox);

	DisplayNameTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DisplayNameTextBlock"));

	if (DisplayNameTextBlock)
	{
		FSlateFontInfo DisplayNameFont = DisplayNameTextBlock->GetFont();
		DisplayNameFont.Size = 20;
		DisplayNameTextBlock->SetFont(DisplayNameFont);
		DisplayNameTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		DisplayNameTextBlock->SetAutoWrapText(true);
		if (UVerticalBoxSlot* DisplayNameTextBlockSlot = FocusInfoVerticalBox->AddChildToVerticalBox(DisplayNameTextBlock))
		{
			DisplayNameTextBlockSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		}
	}

	EnsureAssemblyModeButton(FocusInfoVerticalBox);
	BindAssemblyModeButtonHandler();
	RefreshAssemblyModeButton();
}

void USRCelestialBodyFocusInfoWidget::EnsureAssemblyModeButton(UWidget* AssemblyModeButtonParent)
{
	if (!WidgetTree)
	{
		return;
	}

	if (!AssemblyModeButton)
	{
		AssemblyModeButton = Cast<UButton>(WidgetTree->FindWidget(FName(TEXT("AssemblyModeButton"))));
	}

	if (!AssemblyModeButtonTextBlock)
	{
		AssemblyModeButtonTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("AssemblyModeButtonTextBlock"))));
	}

	if (!AssemblyModeButton)
	{
		AssemblyModeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("AssemblyModeButton"));
	}

	if (!AssemblyModeButtonTextBlock)
	{
		AssemblyModeButtonTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AssemblyModeButtonTextBlock"));
	}

	if (AssemblyModeButton)
	{
		AssemblyModeButton->SetBackgroundColor(FLinearColor(0.16f, 0.22f, 0.28f, 0.95f));
		if (AssemblyModeButtonTextBlock && AssemblyModeButtonTextBlock->GetParent() != AssemblyModeButton)
		{
			AssemblyModeButtonTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			AssemblyModeButtonTextBlock->SetJustification(ETextJustify::Center);
			AssemblyModeButton->AddChild(AssemblyModeButtonTextBlock);
		}

		if (AssemblyModeButton->GetParent())
		{
			return;
		}

		if (UVerticalBox* ParentVerticalBox = Cast<UVerticalBox>(AssemblyModeButtonParent))
		{
			if (UVerticalBoxSlot* AssemblyModeButtonSlot = ParentVerticalBox->AddChildToVerticalBox(AssemblyModeButton))
			{
				AssemblyModeButtonSlot->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 0.0f));
			}
			return;
		}

		if (UCanvasPanel* ParentCanvasPanel = Cast<UCanvasPanel>(AssemblyModeButtonParent))
		{
			if (UCanvasPanelSlot* AssemblyModeButtonSlot = ParentCanvasPanel->AddChildToCanvas(AssemblyModeButton))
			{
				AssemblyModeButtonSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
				AssemblyModeButtonSlot->SetAlignment(FVector2D(1.0f, 0.0f));
				AssemblyModeButtonSlot->SetPosition(FVector2D(-32.0f, 220.0f));
				AssemblyModeButtonSlot->SetAutoSize(true);
			}
			return;
		}

		if (UPanelWidget* ParentPanelWidget = Cast<UPanelWidget>(AssemblyModeButtonParent))
		{
			ParentPanelWidget->AddChild(AssemblyModeButton);
		}
	}
}

void USRCelestialBodyFocusInfoWidget::BindAssemblyModeButtonHandler()
{
	if (!AssemblyModeButton && WidgetTree)
	{
		AssemblyModeButton = Cast<UButton>(WidgetTree->FindWidget(FName(TEXT("AssemblyModeButton"))));
	}

	if (!AssemblyModeButtonTextBlock && WidgetTree)
	{
		AssemblyModeButtonTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("AssemblyModeButtonTextBlock"))));
	}

	if (AssemblyModeButton)
	{
		AssemblyModeButton->OnClicked.RemoveAll(this);
		AssemblyModeButton->OnClicked.AddDynamic(this, &USRCelestialBodyFocusInfoWidget::HandleAssemblyModeButtonClicked);
	}
}

void USRCelestialBodyFocusInfoWidget::RefreshFocusInfoText()
{
	if (DisplayNameTextBlock)
	{
		DisplayNameTextBlock->SetText(
			bHasFocusInfo
				? FocusInfo.DisplayName
				: NSLOCTEXT("StarRoversFocusInfo", "NoSelectionTitle", "No body selected")
		);
	}

	RefreshAssemblyModeButton();
}

void USRCelestialBodyFocusInfoWidget::RefreshAssemblyModeButton()
{
	if (!AssemblyModeButton)
	{
		return;
	}

	const bool bCanUseAssemblyMode = bHasFocusInfo && (FocusInfo.bCanConstruct || FocusInfo.bHasSurfaceGrid);
	AssemblyModeButton->SetVisibility(bCanUseAssemblyMode ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	AssemblyModeButton->SetIsEnabled(bCanUseAssemblyMode);
	AssemblyModeButton->SetBackgroundColor(
		bAssemblyModeActive
			? FLinearColor(0.25f, 0.48f, 0.34f, 0.98f)
			: FLinearColor(0.16f, 0.22f, 0.28f, 0.95f));

	if (AssemblyModeButtonTextBlock)
	{
		AssemblyModeButtonTextBlock->SetText(
			bAssemblyModeActive
				? NSLOCTEXT("StarRoversFocusInfo", "ExitAssemblyModeButtonLabel", "Exit Assembly")
				: NSLOCTEXT("StarRoversFocusInfo", "EnterAssemblyModeButtonLabel", "Assembly Mode"));
	}
}

void USRCelestialBodyFocusInfoWidget::HandleAssemblyModeButtonClicked()
{
	AssemblyModeRequestedEvent.Broadcast();
}
