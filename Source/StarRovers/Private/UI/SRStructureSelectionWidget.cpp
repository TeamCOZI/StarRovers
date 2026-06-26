#include "UI/SRStructureSelectionWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "Structure/SRStructureDataAsset.h"
#include "Styling/SlateColor.h"

namespace
{
	FText GetBuildOptionDisplayName(const FSRStructureBuildOption& BuildOption)
	{
		return BuildOption.DisplayName.IsEmpty()
			? FText::FromName(BuildOption.StructureId)
			: BuildOption.DisplayName;
	}

	FText MakeSelectedStructureText(const FSRStructureBuildOption* SelectedBuildOption)
	{
		return SelectedBuildOption
			? FText::Format(
				FTextFormat(NSLOCTEXT("StarRoversStructureSelection", "SelectedStructureFormat", "Selected: {0}")),
				GetBuildOptionDisplayName(*SelectedBuildOption))
			: NSLOCTEXT("StarRoversStructureSelection", "NoSelectedStructure", "Select a structure");
	}
}

void USRStructureSelectionEntryAction::Initialize(USRStructureSelectionWidget* InOwnerWidget, FName InStructureId)
{
	OwnerWidget = InOwnerWidget;
	StructureId = InStructureId;
}

void USRStructureSelectionEntryAction::HandleClicked()
{
	if (IsValid(OwnerWidget))
	{
		OwnerWidget->DispatchBuildOptionSelected(StructureId);
	}
}

TSharedRef<SWidget> USRStructureSelectionWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}

	BuildStructureSelectionWidgetTree();
	return Super::RebuildWidget();
}

void USRStructureSelectionWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildStructureSelectionWidgetTree();
	RebuildBuildOptions();
	RefreshSelectedStructureText();
}

void USRStructureSelectionWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	BuildStructureSelectionWidgetTree();
	RebuildBuildOptions();
	RefreshSelectedStructureText();
}

FReply USRStructureSelectionWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (IsScreenPositionOverStructureSelectionPanel(InMouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply USRStructureSelectionWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (IsScreenPositionOverStructureSelectionPanel(InMouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply USRStructureSelectionWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (IsScreenPositionOverStructureSelectionPanel(InMouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Handled();
	}

	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

void USRStructureSelectionWidget::SetBuildOptions(const TArray<FSRStructureBuildOption>& NewBuildOptions)
{
	BuildOptions = NewBuildOptions;
	if (bHasSelectedStructureId && !FindBuildOption(SelectedStructureId))
	{
		ClearSelectedStructureId();
	}

	RebuildBuildOptions();
	RefreshSelectedStructureText();
}

void USRStructureSelectionWidget::SetBuildOptionsFromDataAssets(const TArray<USRStructureDataAsset*>& StructureDataAssets)
{
	TArray<FSRStructureBuildOption> NewBuildOptions;
	NewBuildOptions.Reserve(StructureDataAssets.Num());

	for (USRStructureDataAsset* StructureDataAsset : StructureDataAssets)
	{
		if (!IsValid(StructureDataAsset))
		{
			continue;
		}

		const FSRStructureData StructureData = StructureDataAsset->BuildData();
		if (StructureData.StructureId.IsNone())
		{
			continue;
		}

		FSRStructureBuildOption BuildOption;
		BuildOption.StructureId = StructureData.StructureId;
		BuildOption.DisplayName = StructureData.DisplayName;
		BuildOption.Description = StructureData.Description;
		BuildOption.StructureDataAsset = StructureDataAsset;
		BuildOption.bEnabled = StructureData.bAvailableForConstruction;
		NewBuildOptions.Add(BuildOption);
	}

	SetBuildOptions(NewBuildOptions);
}

TArray<FSRStructureBuildOption> USRStructureSelectionWidget::GetBuildOptions() const
{
	return BuildOptions;
}

void USRStructureSelectionWidget::SetSelectedStructureId(FName NewSelectedStructureId)
{
	const FSRStructureBuildOption* BuildOption = FindBuildOption(NewSelectedStructureId);
	if (!BuildOption || !BuildOption->bEnabled)
	{
		return;
	}

	SelectedStructureId = NewSelectedStructureId;
	bHasSelectedStructureId = !SelectedStructureId.IsNone();
	RebuildBuildOptions();
	RefreshSelectedStructureText();
	OnSelectedStructureChanged(SelectedStructureId, bHasSelectedStructureId);
}

void USRStructureSelectionWidget::ClearSelectedStructureId()
{
	if (!bHasSelectedStructureId && SelectedStructureId.IsNone())
	{
		return;
	}

	SelectedStructureId = NAME_None;
	bHasSelectedStructureId = false;
	RebuildBuildOptions();
	RefreshSelectedStructureText();
	OnSelectedStructureChanged(SelectedStructureId, bHasSelectedStructureId);
}

bool USRStructureSelectionWidget::HasSelectedStructureId() const
{
	return bHasSelectedStructureId;
}

FName USRStructureSelectionWidget::GetSelectedStructureId() const
{
	return SelectedStructureId;
}

USRStructureDataAsset* USRStructureSelectionWidget::GetSelectedStructureDataAsset() const
{
	const FSRStructureBuildOption* BuildOption = bHasSelectedStructureId ? FindBuildOption(SelectedStructureId) : nullptr;
	return BuildOption ? BuildOption->StructureDataAsset.Get() : nullptr;
}

bool USRStructureSelectionWidget::IsPointerOverStructureSelectionPanel() const
{
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}

	return IsScreenPositionOverStructureSelectionPanel(FSlateApplication::Get().GetCursorPos());
}

void USRStructureSelectionWidget::DispatchBuildOptionSelected(FName StructureId)
{
	SetSelectedStructureId(StructureId);
	if (bHasSelectedStructureId)
	{
		BuildOptionSelectedEvent.Broadcast(SelectedStructureId, GetSelectedStructureDataAsset());
	}
}

FSRStarRoversStructureBuildOptionSelectedSignature& USRStructureSelectionWidget::OnBuildOptionSelected()
{
	return BuildOptionSelectedEvent;
}

void USRStructureSelectionWidget::BuildStructureSelectionWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}

	if (WidgetTree->RootWidget)
	{
		StructureSelectionBorder = Cast<UBorder>(WidgetTree->FindWidget(FName(TEXT("StructureSelectionBorder"))));
		StructureSelectionVerticalBox = Cast<UVerticalBox>(WidgetTree->FindWidget(FName(TEXT("StructureSelectionVerticalBox"))));
		TitleTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("StructureSelectionTitleTextBlock"))));
		SelectedStructureTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("SelectedStructureTextBlock"))));
		BuildOptionsScrollBox = Cast<UScrollBox>(WidgetTree->FindWidget(FName(TEXT("BuildOptionsScrollBox"))));
		return;
	}

	UCanvasPanel* StructureSelectionCanvasPanel = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("StructureSelectionCanvasPanel"));
	WidgetTree->RootWidget = StructureSelectionCanvasPanel;
	StructureSelectionCanvasPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	StructureSelectionBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("StructureSelectionBorder"));
	StructureSelectionBorder->SetPadding(FMargin(12.0f));
	StructureSelectionBorder->SetBrushColor(PanelColor);

	if (UCanvasPanelSlot* CanvasSlot = StructureSelectionCanvasPanel->AddChildToCanvas(StructureSelectionBorder))
	{
		CanvasSlot->SetAnchors(FAnchors(0.0f, 1.0f, 0.0f, 1.0f));
		CanvasSlot->SetAlignment(FVector2D(0.0f, 1.0f));
		CanvasSlot->SetPosition(FVector2D(16.0f, -32.0f));
		CanvasSlot->SetSize(FVector2D(320.0f, 330.0f));
	}

	StructureSelectionVerticalBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("StructureSelectionVerticalBox"));
	StructureSelectionBorder->SetContent(StructureSelectionVerticalBox);

	TitleTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("StructureSelectionTitleTextBlock"));
	TitleTextBlock->SetText(TitleText);
	TitleTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	FSlateFontInfo TitleFont = TitleTextBlock->GetFont();
	TitleFont.Size = 18;
	TitleTextBlock->SetFont(TitleFont);
	if (UVerticalBoxSlot* TitleSlot = StructureSelectionVerticalBox->AddChildToVerticalBox(TitleTextBlock))
	{
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}

	SelectedStructureTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("SelectedStructureTextBlock"));
	SelectedStructureTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.9f, 1.0f, 1.0f)));
	FSlateFontInfo SelectedFont = SelectedStructureTextBlock->GetFont();
	SelectedFont.Size = 13;
	SelectedStructureTextBlock->SetFont(SelectedFont);
	if (UVerticalBoxSlot* SelectedSlot = StructureSelectionVerticalBox->AddChildToVerticalBox(SelectedStructureTextBlock))
	{
		SelectedSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	}

	BuildOptionsScrollBox = WidgetTree->ConstructWidget<UScrollBox>(
		UScrollBox::StaticClass(),
		TEXT("BuildOptionsScrollBox"));
	if (UVerticalBoxSlot* ScrollBoxSlot = StructureSelectionVerticalBox->AddChildToVerticalBox(BuildOptionsScrollBox))
	{
		ScrollBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
}

void USRStructureSelectionWidget::RebuildBuildOptions()
{
	if (!WidgetTree || !BuildOptionsScrollBox)
	{
		return;
	}

	BuildOptionsScrollBox->ClearChildren();
	EntryActions.Reset();

	for (const FSRStructureBuildOption& BuildOption : BuildOptions)
	{
		if (BuildOption.StructureId.IsNone())
		{
			continue;
		}

		UButton* BuildOptionButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		BuildOptionButton->SetIsEnabled(BuildOption.bEnabled);
		BuildOptionButton->SetBackgroundColor(
			BuildOption.bEnabled
				? (bHasSelectedStructureId && SelectedStructureId == BuildOption.StructureId ? SelectedButtonColor : ButtonColor)
				: DisabledButtonColor);

		UVerticalBox* BuildOptionVerticalBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		BuildOptionButton->AddChild(BuildOptionVerticalBox);

		UTextBlock* NameTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		NameTextBlock->SetText(GetBuildOptionDisplayName(BuildOption));
		NameTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		FSlateFontInfo NameFont = NameTextBlock->GetFont();
		NameFont.Size = 14;
		NameTextBlock->SetFont(NameFont);
		NameTextBlock->SetAutoWrapText(false);
		if (UVerticalBoxSlot* NameSlot = BuildOptionVerticalBox->AddChildToVerticalBox(NameTextBlock))
		{
			NameSlot->SetPadding(FMargin(8.0f, 6.0f, 8.0f, BuildOption.Description.IsEmpty() ? 6.0f : 2.0f));
		}

		if (!BuildOption.Description.IsEmpty())
		{
			UTextBlock* DescriptionTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			DescriptionTextBlock->SetText(BuildOption.Description);
			DescriptionTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.78f, 0.82f, 1.0f)));
			FSlateFontInfo DescriptionFont = DescriptionTextBlock->GetFont();
			DescriptionFont.Size = 11;
			DescriptionTextBlock->SetFont(DescriptionFont);
			DescriptionTextBlock->SetAutoWrapText(true);
			if (UVerticalBoxSlot* DescriptionSlot = BuildOptionVerticalBox->AddChildToVerticalBox(DescriptionTextBlock))
			{
				DescriptionSlot->SetPadding(FMargin(8.0f, 0.0f, 8.0f, 7.0f));
			}
		}

		USRStructureSelectionEntryAction* EntryAction = NewObject<USRStructureSelectionEntryAction>(this);
		EntryAction->Initialize(this, BuildOption.StructureId);
		EntryActions.Add(EntryAction);
		BuildOptionButton->OnClicked.AddDynamic(EntryAction, &USRStructureSelectionEntryAction::HandleClicked);

		if (UScrollBoxSlot* ButtonSlot = Cast<UScrollBoxSlot>(BuildOptionsScrollBox->AddChild(BuildOptionButton)))
		{
			ButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		}
	}
}

void USRStructureSelectionWidget::RefreshSelectedStructureText()
{
	if (!SelectedStructureTextBlock)
	{
		return;
	}

	SelectedStructureTextBlock->SetText(MakeSelectedStructureText(
		bHasSelectedStructureId ? FindBuildOption(SelectedStructureId) : nullptr));
}

const FSRStructureBuildOption* USRStructureSelectionWidget::FindBuildOption(FName StructureId) const
{
	if (StructureId.IsNone())
	{
		return nullptr;
	}

	return BuildOptions.FindByPredicate([StructureId](const FSRStructureBuildOption& BuildOption)
	{
		return BuildOption.StructureId == StructureId;
	});
}

bool USRStructureSelectionWidget::IsScreenPositionOverStructureSelectionPanel(const FVector2D& ScreenPosition) const
{
	return IsVisible()
		&& StructureSelectionBorder
		&& StructureSelectionBorder->GetCachedGeometry().IsUnderLocation(ScreenPosition);
}
