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

namespace
{
	int32 GetCubeSphereFaceNumber(const ESRCubeSphereFace Face)
	{
		return static_cast<int32>(Face) + 1;
	}

	FIntPoint GetSurfaceGridDisplayCellCoord(const FSRPlanetSurfaceGridCellId& CellId, int32 FaceResolution)
	{
		FIntPoint DisplayCoord(CellId.CellX, CellId.CellY);
		if (CellId.Face == ESRCubeSphereFace::PositiveZ || CellId.Face == ESRCubeSphereFace::NegativeZ)
		{
			const int32 SafeFaceResolution = FMath::Max(1, FaceResolution);
			DisplayCoord.X = SafeFaceResolution - 1 - CellId.CellX;
			DisplayCoord.Y = SafeFaceResolution - 1 - CellId.CellY;
		}
		return DisplayCoord;
	}
}

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
	RefreshFocusInfoText();
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
		EnsureHoveredCellTextBlock(WidgetTree->RootWidget);
		EnsureAssemblyModeButton(WidgetTree->RootWidget);
		BindAssemblyModeButtonHandler();
		RefreshFocusInfoText();
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

	VariableNameTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("VariableNameTextBlock"));

	if (VariableNameTextBlock)
	{
		FSlateFontInfo VariableNameFont = VariableNameTextBlock->GetFont();
		VariableNameFont.Size = 20;
		VariableNameTextBlock->SetFont(VariableNameFont);
		VariableNameTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		VariableNameTextBlock->SetAutoWrapText(true);
		if (UVerticalBoxSlot* VariableNameTextBlockSlot = FocusInfoVerticalBox->AddChildToVerticalBox(VariableNameTextBlock))
		{
			VariableNameTextBlockSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		}
	}

	EnsureHoveredCellTextBlock(FocusInfoVerticalBox);

	EnsureAssemblyModeButton(FocusInfoVerticalBox);
	BindAssemblyModeButtonHandler();
	RefreshFocusInfoText();
	RefreshAssemblyModeButton();
}

void USRCelestialBodyFocusInfoWidget::EnsureHoveredCellTextBlock(UWidget* HoveredCellTextBlockParent)
{
	if (!WidgetTree)
	{
		return;
	}

	if (!HoveredCellTextBlock)
	{
		HoveredCellTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("HoveredCellTextBlock"))));
	}

	if (!HoveredCellTextBlock)
	{
		HoveredCellTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HoveredCellTextBlock"));
	}

	if (!HoveredCellTextBlock)
	{
		return;
	}

	FSlateFontInfo HoveredCellFont = HoveredCellTextBlock->GetFont();
	HoveredCellFont.Size = 13;
	HoveredCellTextBlock->SetFont(HoveredCellFont);
	HoveredCellTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.9f, 1.0f, 1.0f)));
	HoveredCellTextBlock->SetAutoWrapText(false);

	if (HoveredCellTextBlock->GetParent())
	{
		return;
	}

	if (UVerticalBox* ParentVerticalBox = Cast<UVerticalBox>(HoveredCellTextBlockParent))
	{
		if (UVerticalBoxSlot* HoveredCellTextBlockSlot = ParentVerticalBox->AddChildToVerticalBox(HoveredCellTextBlock))
		{
			HoveredCellTextBlockSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 8.0f));
		}
		return;
	}

	if (UCanvasPanel* ParentCanvasPanel = Cast<UCanvasPanel>(HoveredCellTextBlockParent))
	{
		if (UCanvasPanelSlot* HoveredCellTextBlockSlot = ParentCanvasPanel->AddChildToCanvas(HoveredCellTextBlock))
		{
			HoveredCellTextBlockSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
			HoveredCellTextBlockSlot->SetAlignment(FVector2D(1.0f, 0.0f));
			HoveredCellTextBlockSlot->SetPosition(FVector2D(-32.0f, 128.0f));
			HoveredCellTextBlockSlot->SetAutoSize(true);
		}
		return;
	}

	if (UPanelWidget* ParentPanelWidget = Cast<UPanelWidget>(HoveredCellTextBlockParent))
	{
		ParentPanelWidget->AddChild(HoveredCellTextBlock);
	}
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
	if (VariableNameTextBlock)
	{
		VariableNameTextBlock->SetText(
			bHasFocusInfo
				? FocusInfo.VariableName
				: NSLOCTEXT("StarRoversFocusInfo", "NoSelectionTitle", "No body selected")
		);
	}

	if (HoveredCellTextBlock)
	{
		if (bHasFocusInfo && bAssemblyModeActive && FocusInfo.bHasHoveredSurfaceCell)
		{
			const FSRPlanetSurfaceGridCellInfo& CellInfo = FocusInfo.HoveredSurfaceCellInfo;
			FString CellText = FString::Printf(
				TEXT("Face: %d\nCell: %d,%d\nDisplay: %d,%d\nLatitude: %.1f deg"),
				GetCubeSphereFaceNumber(CellInfo.CellId.Face),
				CellInfo.CellId.CellX,
				CellInfo.CellId.CellY,
				CellInfo.DisplayCellX,
				CellInfo.DisplayCellY,
				CellInfo.LatitudeDegrees);
			CellText += FString::Printf(TEXT("\nPatchDisplayCells: %d"), FocusInfo.HoveredSurfaceGridPatchCellIds.Num());
			for (int32 CellIndex = 0; CellIndex < FocusInfo.HoveredSurfaceGridPatchCellIds.Num(); ++CellIndex)
			{
				const FSRPlanetSurfaceGridCellId& PatchCellId = FocusInfo.HoveredSurfaceGridPatchCellIds[CellIndex];
				const FIntPoint PatchDisplayCoord = GetSurfaceGridDisplayCellCoord(PatchCellId, CellInfo.FaceResolution);
				CellText += (CellIndex % 5 == 0) ? TEXT("\n") : TEXT(" ");
				CellText += FString::Printf(
					TEXT("F%d(%d,%d)"),
					GetCubeSphereFaceNumber(PatchCellId.Face),
					PatchDisplayCoord.X,
					PatchDisplayCoord.Y);
			}
			HoveredCellTextBlock->SetText(FText::FromString(CellText));
			HoveredCellTextBlock->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			HoveredCellTextBlock->SetText(FText::GetEmpty());
			HoveredCellTextBlock->SetVisibility(ESlateVisibility::Collapsed);
		}
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
