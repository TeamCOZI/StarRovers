#include "UI/SRFocusedHubShortcutWidget.h"

#include "Utility/SRLog.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateColor.h"

namespace
{
	constexpr float HubShortcutPanelWidth = 190.0f;
	constexpr float HubShortcutButtonHeight = 50.0f;

	int32 GetCubeSphereFaceNumber(ESRCubeSphereFace Face)
	{
		return static_cast<int32>(Face) + 1;
	}

	FString BuildHubShortcutButtonLabel(const FSRFocusedHubShortcutInfo& HubInfo)
	{
		const FString DisplayName = HubInfo.DisplayName.IsEmpty()
			? HubInfo.OccupantId.ToString()
			: HubInfo.DisplayName.ToString();
		return FString::Printf(
			TEXT("%s\nF%d (%d,%d)"),
			*DisplayName,
			GetCubeSphereFaceNumber(HubInfo.OriginCellId.Face),
			HubInfo.OriginCellId.CellX,
			HubInfo.OriginCellId.CellY);
	}
}

void USRFocusedHubShortcutButtonAction::Initialize(
	USRFocusedHubShortcutWidget* InOwnerWidget,
	const FSRFocusedHubShortcutInfo& InHubInfo,
	UButton* InButton)
{
	OwnerWidget = InOwnerWidget;
	HubInfo = InHubInfo;
	Button = InButton;

	if (Button)
	{
		Button->OnClicked.RemoveAll(this);
		Button->OnClicked.AddDynamic(this, &USRFocusedHubShortcutButtonAction::HandleClicked);
	}
}

void USRFocusedHubShortcutButtonAction::HandleClicked()
{
	if (OwnerWidget)
	{
		OwnerWidget->RequestHubShortcut(HubInfo);
	}
}

bool USRFocusedHubShortcutButtonAction::TryHandleManualClick(const FVector2D& ScreenPosition)
{
	if (!Button || !Button->IsVisible() || !Button->GetCachedGeometry().IsUnderLocation(ScreenPosition))
	{
		return false;
	}

	HandleClicked();
	return true;
}

TSharedRef<SWidget> USRFocusedHubShortcutWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}

	BuildHubShortcutWidgetTree();
	return Super::RebuildWidget();
}

void USRFocusedHubShortcutWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildHubShortcutWidgetTree();
	RebuildHubButtons();
}

void USRFocusedHubShortcutWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	BuildHubShortcutWidgetTree();
	RebuildHubButtons();
}

FReply USRFocusedHubShortcutWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D ScreenPosition = InMouseEvent.GetScreenSpacePosition();
	if (IsScreenPositionOverHubShortcutUI(ScreenPosition))
	{
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: HubShortcut NativeOnMouseButtonDown handled Mouse=(%.1f, %.1f)"),
			ScreenPosition.X,
			ScreenPosition.Y);
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply USRFocusedHubShortcutWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D ScreenPosition = InMouseEvent.GetScreenSpacePosition();
	if (IsScreenPositionOverHubShortcutUI(ScreenPosition))
	{
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: HubShortcut NativeOnMouseButtonUp handled Mouse=(%.1f, %.1f)"),
			ScreenPosition.X,
			ScreenPosition.Y);
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply USRFocusedHubShortcutWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (IsScreenPositionOverHubShortcutUI(InMouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Handled();
	}

	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

void USRFocusedHubShortcutWidget::SetHubShortcuts(const TArray<FSRFocusedHubShortcutInfo>& NewHubShortcuts)
{
	HubShortcuts = NewHubShortcuts;

	const FString NewSignature = BuildHubShortcutSignature();
	if (HubShortcutSignature == NewSignature)
	{
		SetVisibility(HubShortcuts.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		return;
	}

	HubShortcutSignature = NewSignature;
	RebuildHubButtons();
	SetVisibility(HubShortcuts.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
}

void USRFocusedHubShortcutWidget::ClearHubShortcuts()
{
	HubShortcuts.Reset();
	HubShortcutSignature.Reset();
	RebuildHubButtons();
	SetVisibility(ESlateVisibility::Collapsed);
}

bool USRFocusedHubShortcutWidget::HasHubShortcuts() const
{
	return !HubShortcuts.IsEmpty();
}

bool USRFocusedHubShortcutWidget::IsPointerOverHubShortcutUI() const
{
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}

	return IsScreenPositionOverHubShortcutUI(FSlateApplication::Get().GetCursorPos());
}

bool USRFocusedHubShortcutWidget::TryHandleHubShortcutPointerClick()
{
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}

	const FVector2D ScreenPosition = FSlateApplication::Get().GetCursorPos();
	for (USRFocusedHubShortcutButtonAction* ButtonAction : ButtonActions)
	{
		if (ButtonAction && ButtonAction->TryHandleManualClick(ScreenPosition))
		{
			return true;
		}
	}

	return IsScreenPositionOverHubShortcutUI(ScreenPosition);
}

void USRFocusedHubShortcutWidget::RequestHubShortcut(const FSRFocusedHubShortcutInfo& HubInfo)
{
	HubShortcutRequestedEvent.Broadcast(HubInfo);
}

FSRFocusedHubShortcutRequestedSignature& USRFocusedHubShortcutWidget::OnHubShortcutRequested()
{
	return HubShortcutRequestedEvent;
}

void USRFocusedHubShortcutWidget::BuildHubShortcutWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}

	if (WidgetTree->RootWidget)
	{
		PanelBorder = Cast<UBorder>(WidgetTree->FindWidget(FName(TEXT("FocusedHubShortcutPanelBorder"))));
		TitleTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FocusedHubShortcutTitleTextBlock"))));
		ButtonBox = Cast<UVerticalBox>(WidgetTree->FindWidget(FName(TEXT("FocusedHubShortcutButtonBox"))));
		return;
	}

	UCanvasPanel* CanvasPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FocusedHubShortcutCanvasPanel"));
	WidgetTree->RootWidget = CanvasPanel;

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FocusedHubShortcutPanelBorder"));
	PanelBorder->SetPadding(FMargin(10.0f));
	PanelBorder->SetBrushColor(FLinearColor(0.015f, 0.025f, 0.045f, 0.88f));

	if (UCanvasPanelSlot* PanelSlot = CanvasPanel->AddChildToCanvas(PanelBorder))
	{
		PanelSlot->SetAnchors(FAnchors(1.0f, 0.5f, 1.0f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(1.0f, 0.5f));
		PanelSlot->SetPosition(FVector2D(-28.0f, 0.0f));
		PanelSlot->SetAutoSize(true);
	}

	USizeBox* PanelSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("FocusedHubShortcutPanelSizeBox"));
	PanelSizeBox->SetWidthOverride(HubShortcutPanelWidth);
	PanelBorder->SetContent(PanelSizeBox);

	UVerticalBox* PanelVerticalBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FocusedHubShortcutPanelVerticalBox"));
	PanelSizeBox->SetContent(PanelVerticalBox);

	TitleTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("FocusedHubShortcutTitleTextBlock"));
	if (TitleTextBlock)
	{
		FSlateFontInfo TitleFont = TitleTextBlock->GetFont();
		TitleFont.Size = 14;
		TitleTextBlock->SetFont(TitleFont);
		TitleTextBlock->SetText(NSLOCTEXT("StarRoversHubShortcut", "FocusedHubShortcutTitle", "Hubs"));
		TitleTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(0.90f, 0.96f, 1.0f, 1.0f)));
		TitleTextBlock->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* TitleSlot = PanelVerticalBox->AddChildToVerticalBox(TitleTextBlock))
		{
			TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		}
	}

	ButtonBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FocusedHubShortcutButtonBox"));
	if (ButtonBox)
	{
		PanelVerticalBox->AddChildToVerticalBox(ButtonBox);
	}
}

void USRFocusedHubShortcutWidget::RebuildHubButtons()
{
	BuildHubShortcutWidgetTree();
	if (!WidgetTree || !ButtonBox)
	{
		return;
	}

	ButtonBox->ClearChildren();
	ButtonActions.Reset();
	ButtonActions.Reserve(HubShortcuts.Num());

	for (const FSRFocusedHubShortcutInfo& HubInfo : HubShortcuts)
	{
		UButton* HubButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock* HubButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		if (!HubButton || !HubButtonText)
		{
			continue;
		}

		HubButton->SetBackgroundColor(FLinearColor(0.12f, 0.18f, 0.24f, 0.96f));

		FSlateFontInfo ButtonFont = HubButtonText->GetFont();
		ButtonFont.Size = 12;
		HubButtonText->SetFont(ButtonFont);
		HubButtonText->SetText(FText::FromString(BuildHubShortcutButtonLabel(HubInfo)));
		HubButtonText->SetJustification(ETextJustify::Center);
		HubButtonText->SetAutoWrapText(true);
		HubButtonText->SetWrapTextAt(HubShortcutPanelWidth - 30.0f);
		HubButtonText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		HubButton->AddChild(HubButtonText);

		USizeBox* ButtonSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		if (!ButtonSizeBox)
		{
			continue;
		}
		ButtonSizeBox->SetWidthOverride(HubShortcutPanelWidth - 20.0f);
		ButtonSizeBox->SetHeightOverride(HubShortcutButtonHeight);
		ButtonSizeBox->SetContent(HubButton);

		if (UVerticalBoxSlot* ButtonSlot = ButtonBox->AddChildToVerticalBox(ButtonSizeBox))
		{
			ButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		}

		USRFocusedHubShortcutButtonAction* ButtonAction = NewObject<USRFocusedHubShortcutButtonAction>(this);
		if (ButtonAction)
		{
			ButtonAction->Initialize(this, HubInfo, HubButton);
			ButtonActions.Add(ButtonAction);
		}
	}
}

bool USRFocusedHubShortcutWidget::IsScreenPositionOverHubShortcutUI(const FVector2D& ScreenPosition) const
{
	if (!IsVisible())
	{
		return false;
	}

	return PanelBorder
		&& PanelBorder->IsVisible()
		&& PanelBorder->GetCachedGeometry().IsUnderLocation(ScreenPosition);
}

FString USRFocusedHubShortcutWidget::BuildHubShortcutSignature() const
{
	FString Signature;
	Signature.Reserve(HubShortcuts.Num() * 64);
	for (const FSRFocusedHubShortcutInfo& HubInfo : HubShortcuts)
	{
		Signature += FString::Printf(
			TEXT("|%s:%s:%d:%d:%d:%s"),
			*GetNameSafe(HubInfo.BodyActor.Get()),
			*HubInfo.OccupantId.ToString(),
			static_cast<int32>(HubInfo.OriginCellId.Face),
			HubInfo.OriginCellId.CellX,
			HubInfo.OriginCellId.CellY,
			*HubInfo.DisplayName.ToString());
	}
	return Signature;
}
