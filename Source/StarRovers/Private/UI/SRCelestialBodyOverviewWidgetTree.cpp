#include "UI/SRCelestialBodyOverviewWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Celestial/SRCelestialBody.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateColor.h"
#include "UI/SRUIComponents.h"
#include "UI/SRUILayoutPolicy.h"
#include "UI/SRUITheme.h"

namespace
{
	int32 GetCategorySortRank(const AActor* CelestialBodyActor)
	{
		const ASRCelestialBody* ProceduralBody =
			Cast<ASRCelestialBody>(CelestialBodyActor);
		if (!ProceduralBody)
		{
			return 100;
		}

		switch (ProceduralBody->GetBodyCategory())
		{
		case ESRCelestialBodyCategory::Star:
			return 0;
		case ESRCelestialBodyCategory::Planet:
			return 1;
		case ESRCelestialBodyCategory::Moon:
			return 2;
		default:
			return 100;
		}
	}

	float GetParentSortDistance(const AActor* CelestialBodyActor)
	{
		float OrbitRadius = 0.0f;
		if (USRCelestialBodyRuntimeLibrary::GetCelestialOrbitRadius(
				CelestialBodyActor, OrbitRadius))
		{
			return FMath::Max(0.0f, OrbitRadius);
		}

		AActor* ParentBody = nullptr;
		if (USRCelestialBodyRuntimeLibrary::GetCelestialParentBody(CelestialBodyActor,
																   ParentBody) &&
			IsValid(ParentBody) && IsValid(CelestialBodyActor))
		{
			return FVector::Dist(ParentBody->GetActorLocation(),
								 CelestialBodyActor->GetActorLocation());
		}

		return TNumericLimits<float>::Max();
	}
} // namespace

void USRCelestialBodyOverviewWidget::BuildOverviewWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}

	if (WidgetTree->RootWidget)
	{
		OverviewCanvasPanel = Cast<UCanvasPanel>(
			WidgetTree->FindWidget(FName(TEXT("OverviewCanvasPanel"))));
		OverviewBorder =
			Cast<UBorder>(WidgetTree->FindWidget(FName(TEXT("OverviewBorder"))));
		OverviewVerticalBox = Cast<UVerticalBox>(
			WidgetTree->FindWidget(FName(TEXT("OverviewVerticalBox"))));
		StarSystemTextBlock = Cast<UTextBlock>(
			WidgetTree->FindWidget(FName(TEXT("StarSystemTextBlock"))));
		StarSystemScrollBox = Cast<UScrollBox>(
			WidgetTree->FindWidget(FName(TEXT("StarSystemScrollBox"))));
		NameplateToggleButton = Cast<UButton>(
			WidgetTree->FindWidget(FName(TEXT("NameplateToggleButton"))));
		NameplateToggleButtonTextBlock = Cast<UTextBlock>(
			WidgetTree->FindWidget(FName(TEXT("NameplateToggleButtonTextBlock"))));
		StrategicStatusBadge = Cast<USRStatusBadgeWidget>(
			WidgetTree->FindWidget(FName(TEXT("StrategicStatusBadge"))));
		StrategicDetailTextBlock = Cast<UTextBlock>(
			WidgetTree->FindWidget(FName(TEXT("StrategicDetailTextBlock"))));
		StrategicFocusButton = Cast<UButton>(
			WidgetTree->FindWidget(FName(TEXT("StrategicFocusButton"))));
		StrategicFocusButtonTextBlock = Cast<UTextBlock>(
			WidgetTree->FindWidget(FName(TEXT("StrategicFocusButtonTextBlock"))));
		StrategyOverlayToggleButton = Cast<UButton>(
			WidgetTree->FindWidget(FName(TEXT("StrategyOverlayToggleButton"))));
		StrategyOverlayToggleButtonTextBlock = Cast<UTextBlock>(
			WidgetTree->FindWidget(FName(TEXT("StrategyOverlayToggleButtonTextBlock"))));
		if (OverviewCanvasPanel && OverviewBorder && OverviewVerticalBox
			&& StarSystemTextBlock && StarSystemScrollBox
			&& NameplateToggleButton && NameplateToggleButtonTextBlock
			&& StrategicStatusBadge && StrategicDetailTextBlock
			&& StrategicFocusButton && StrategicFocusButtonTextBlock
			&& StrategyOverlayToggleButton && StrategyOverlayToggleButtonTextBlock)
		{
			NameplateToggleButton->OnClicked.RemoveDynamic(
				this, &USRCelestialBodyOverviewWidget::HandleNameplateToggleClicked);
			NameplateToggleButton->OnClicked.AddDynamic(
				this, &USRCelestialBodyOverviewWidget::HandleNameplateToggleClicked);
			StrategyOverlayToggleButton->OnClicked.RemoveDynamic(
				this, &USRCelestialBodyOverviewWidget::HandleStrategyOverlayToggleClicked);
			StrategyOverlayToggleButton->OnClicked.AddDynamic(
				this, &USRCelestialBodyOverviewWidget::HandleStrategyOverlayToggleClicked);
			StrategicFocusButton->OnClicked.RemoveDynamic(
				this, &USRCelestialBodyOverviewWidget::HandleStrategicFocusClicked);
			StrategicFocusButton->OnClicked.AddDynamic(
				this, &USRCelestialBodyOverviewWidget::HandleStrategicFocusClicked);
			NameplateToggleButtonTextBlock->SetText(bShowNameplateButtons
				? NSLOCTEXT("StarRoversOverview", "NameplateButtonsOnCompact", "NAMES ON")
				: NSLOCTEXT("StarRoversOverview", "NameplateButtonsOffCompact", "NAMES OFF"));
			StrategyOverlayToggleButtonTextBlock->SetText(bShowStrategyOverlay
				? NSLOCTEXT("StarRoversOverview", "StrategyOverlayOn", "ROUTES ON")
				: NSLOCTEXT("StarRoversOverview", "StrategyOverlayOff", "ROUTES OFF"));
			RefreshStrategicHeader();
			return;
		}

		// Upgrade an existing Blueprint tree authored before the strategic
		// command-layer contract without requiring an asset migration pass.
		WidgetTree->RootWidget = nullptr;
		NameplateActors.Reset();
		NameplateButtons.Reset();
		NameplateTextBlocks.Reset();
		NameplateButtonLayouts.Reset();
		StrategicRouteLineLayouts.Reset();
	}

	OverviewCanvasPanel = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("OverviewCanvasPanel"));
	WidgetTree->RootWidget = OverviewCanvasPanel;

	OverviewBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
														  TEXT("OverviewBorder"));
	OverviewBorder->SetPadding(FMargin(12.0f));
	OverviewBorder->SetBrushColor(OverviewBorderColor);

	if (UCanvasPanelSlot* CanvasSlot =
			OverviewCanvasPanel->AddChildToCanvas(OverviewBorder))
	{
		CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
		CanvasSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		CanvasSlot->SetPosition(FVector2D(FSRUILayoutPolicy::DefaultSafeMargin, 76.0f));
		CanvasSlot->SetSize(FVector2D(332.0f, 510.0f));
	}

	OverviewVerticalBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("OverviewVerticalBox"));
	OverviewBorder->SetContent(OverviewVerticalBox);

	StarSystemTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("StarSystemTextBlock"));
	StarSystemTextBlock->SetText(StarSystemText);
	StarSystemTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	FSlateFontInfo StarSystemFont = StarSystemTextBlock->GetFont();
	StarSystemFont.Size = 18;
	StarSystemTextBlock->SetFont(StarSystemFont);

	if (UVerticalBoxSlot* StarSystemTextBlockSlot =
			OverviewVerticalBox->AddChildToVerticalBox(StarSystemTextBlock))
	{
		StarSystemTextBlockSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	}

	StrategicStatusBadge = WidgetTree->ConstructWidget<USRStatusBadgeWidget>(
		USRStatusBadgeWidget::StaticClass(), TEXT("StrategicStatusBadge"));
	StrategicStatusBadge->SetBadge(
		NSLOCTEXT("StarRoversOverview", "NetworkNominalInitial", "NETWORK NOMINAL"),
		ESRUIVisualState::Positive);
	if (UVerticalBoxSlot* StrategyBadgeSlot =
			OverviewVerticalBox->AddChildToVerticalBox(StrategicStatusBadge))
	{
		StrategyBadgeSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	}

	StrategicDetailTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("StrategicDetailTextBlock"));
	StrategicDetailTextBlock->SetAutoWrapText(true);
	USRUIThemeLibrary::ApplyTextStyle(
		StrategicDetailTextBlock, ESRUITextStyle::Caption, ESRUIVisualState::Neutral);
	if (UVerticalBoxSlot* StrategyDetailSlot =
			OverviewVerticalBox->AddChildToVerticalBox(StrategicDetailTextBlock))
	{
		StrategyDetailSlot->SetPadding(FMargin(2.0f, 0.0f, 2.0f, 6.0f));
	}

	StrategicFocusButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("StrategicFocusButton"));
	StrategicFocusButton->SetBackgroundColor(
		USRUIThemeLibrary::ResolveStatePalette(ESRUIVisualState::Selected).SurfaceColor);
	StrategicFocusButtonTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("StrategicFocusButtonTextBlock"));
	StrategicFocusButtonTextBlock->SetJustification(ETextJustify::Center);
	USRUIThemeLibrary::ApplyTextStyle(
		StrategicFocusButtonTextBlock, ESRUITextStyle::Caption, ESRUIVisualState::Selected, true);
	StrategicFocusButton->AddChild(StrategicFocusButtonTextBlock);
	StrategicFocusButton->OnClicked.AddDynamic(
		this, &USRCelestialBodyOverviewWidget::HandleStrategicFocusClicked);
	StrategicFocusButton->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* StrategyFocusSlot =
			OverviewVerticalBox->AddChildToVerticalBox(StrategicFocusButton))
	{
		StrategyFocusSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}

	UHorizontalBox* OverlayToggleRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("OverviewOverlayToggleRow"));
	if (UVerticalBoxSlot* ToggleRowSlot =
			OverviewVerticalBox->AddChildToVerticalBox(OverlayToggleRow))
	{
		ToggleRowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	}

	NameplateToggleButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("NameplateToggleButton"));
	NameplateToggleButton->SetBackgroundColor(
		FLinearColor(0.12f, 0.16f, 0.20f, 0.94f));
	NameplateToggleButtonTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("NameplateToggleButtonTextBlock"));
	NameplateToggleButtonTextBlock->SetText(bShowNameplateButtons
		? NSLOCTEXT("StarRoversOverview", "NameplateButtonsOnCompact", "NAMES ON")
		: NSLOCTEXT("StarRoversOverview", "NameplateButtonsOffCompact", "NAMES OFF"));
	NameplateToggleButtonTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	NameplateToggleButtonTextBlock->SetJustification(ETextJustify::Center);
	NameplateToggleButton->AddChild(NameplateToggleButtonTextBlock);
	NameplateToggleButton->OnClicked.AddDynamic(
		this, &USRCelestialBodyOverviewWidget::HandleNameplateToggleClicked);
	if (UHorizontalBoxSlot* NameplateToggleSlot =
			OverlayToggleRow->AddChildToHorizontalBox(NameplateToggleButton))
	{
		NameplateToggleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		NameplateToggleSlot->SetPadding(FMargin(0.0f, 0.0f, 3.0f, 0.0f));
	}

	StrategyOverlayToggleButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("StrategyOverlayToggleButton"));
	StrategyOverlayToggleButton->SetBackgroundColor(
		FLinearColor(0.12f, 0.16f, 0.20f, 0.94f));
	StrategyOverlayToggleButtonTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("StrategyOverlayToggleButtonTextBlock"));
	StrategyOverlayToggleButtonTextBlock->SetText(bShowStrategyOverlay
		? NSLOCTEXT("StarRoversOverview", "StrategyOverlayOn", "ROUTES ON")
		: NSLOCTEXT("StarRoversOverview", "StrategyOverlayOff", "ROUTES OFF"));
	StrategyOverlayToggleButtonTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	StrategyOverlayToggleButtonTextBlock->SetJustification(ETextJustify::Center);
	StrategyOverlayToggleButton->AddChild(StrategyOverlayToggleButtonTextBlock);
	StrategyOverlayToggleButton->OnClicked.AddDynamic(
		this, &USRCelestialBodyOverviewWidget::HandleStrategyOverlayToggleClicked);
	if (UHorizontalBoxSlot* StrategyToggleSlot =
			OverlayToggleRow->AddChildToHorizontalBox(StrategyOverlayToggleButton))
	{
		StrategyToggleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		StrategyToggleSlot->SetPadding(FMargin(3.0f, 0.0f, 0.0f, 0.0f));
	}

	StarSystemScrollBox = WidgetTree->ConstructWidget<UScrollBox>(
		UScrollBox::StaticClass(), TEXT("StarSystemScrollBox"));
	if (UVerticalBoxSlot* StarSystemScrollBoxSlot =
			OverviewVerticalBox->AddChildToVerticalBox(StarSystemScrollBox))
	{
		StarSystemScrollBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	RefreshStrategicHeader();
}

void USRCelestialBodyOverviewWidget::RebuildStarSystemScrollBox()
{
	if (!WidgetTree || !StarSystemScrollBox)
	{
		return;
	}

	StarSystemScrollBox->ClearChildren();
	EntryActions.Reset();
	OperationsBadgeActors.Reset();
	OperationsBadgeTextBlocks.Reset();
	StarSystemRowActors.Reset();
	StarSystemRowButtons.Reset();

	TSet<AActor*> CelestialBodySet;
	CelestialBodySet.Reserve(CelestialBodies.Num());
	for (AActor* CelestialBodyActor : CelestialBodies)
	{
		if (IsValid(CelestialBodyActor))
		{
			CelestialBodySet.Add(CelestialBodyActor);
		}
	}

	TArray<TObjectPtr<AActor>> RootStarSystemBodies;
	TMap<AActor*, TArray<AActor*>> ChildrenByParent;
	RootStarSystemBodies.Reserve(CelestialBodies.Num());
	ChildrenByParent.Reserve(CelestialBodies.Num());
	for (AActor* CelestialBodyActor : CelestialBodies)
	{
		if (!IsValid(CelestialBodyActor))
		{
			continue;
		}

		AActor* ParentBody = nullptr;
		if (USRCelestialBodyRuntimeLibrary::GetCelestialParentBody(
				CelestialBodyActor, ParentBody) &&
			CelestialBodySet.Contains(ParentBody))
		{
			ChildrenByParent.FindOrAdd(ParentBody).Add(CelestialBodyActor);
		}
		else
		{
			RootStarSystemBodies.Add(CelestialBodyActor);
		}
	}

	SortStarSystemBodies(RootStarSystemBodies);
	for (TPair<AActor*, TArray<AActor*>>& Pair : ChildrenByParent)
	{
		Pair.Value.Sort([this](const AActor& Left, const AActor& Right)
		{
			return CompareStarSystemBodies(Left, Right);
		});
	}

	for (AActor* RootStarSystemBody : RootStarSystemBodies)
	{
		AddStarSystemScrollBoxButton(RootStarSystemBody, 0, ChildrenByParent);
	}

	OperationsBadgeRefreshAccumulator = 0.0f;
	RefreshOperationsBadges();
}

void USRCelestialBodyOverviewWidget::AddStarSystemScrollBoxButton(
	AActor* CelestialBodyActor, int32 Depth,
	const TMap<AActor*, TArray<AActor*>>& ChildrenByParent)
{
	if (!WidgetTree || !StarSystemScrollBox || !IsValid(CelestialBodyActor))
	{
		return;
	}

	const int32 RowNumber = StarSystemRowButtons.Num() + 1;
	UButton* StarSystemScrollBoxButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(),
		FName(*FString::Printf(TEXT("StarSystemRowButton%d"), RowNumber)));
	StarSystemScrollBoxButton->SetBackgroundColor(
		CelestialBodyActor == RecommendedSystemScanBody
			? RecommendedSystemScanColor
			: CelestialBodyActor == SelectedActor
			? SelectedStarSystemScrollBoxButtonColor
			: StarSystemScrollBoxButtonColor);
	StarSystemRowActors.Add(CelestialBodyActor);
	StarSystemRowButtons.Add(StarSystemScrollBoxButton);

	UHorizontalBox* StarSystemHorizontalBox =
		WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(),
			FName(*FString::Printf(TEXT("StarSystemRowContent%d"), RowNumber)));
	StarSystemScrollBoxButton->AddChild(StarSystemHorizontalBox);

	UTextBlock* StarSystemNameplatePrefixTextBlock =
		WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			FName(*FString::Printf(TEXT("StarSystemRowPrefix%d"), RowNumber)));
	StarSystemNameplatePrefixTextBlock->SetText(
		GetStarSystemTreePrefixText(CelestialBodyActor));
	StarSystemNameplatePrefixTextBlock->SetColorAndOpacity(
		FSlateColor(StarSystemNameplateTextColor));
	StarSystemNameplatePrefixTextBlock->SetJustification(ETextJustify::Center);
	if (UHorizontalBoxSlot* PrefixSlot =
			StarSystemHorizontalBox->AddChildToHorizontalBox(
				StarSystemNameplatePrefixTextBlock))
	{
		PrefixSlot->SetPadding(
			FMargin(Depth * StarSystemNameplateIndentPixels, 0.0f, 8.0f, 0.0f));
		PrefixSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}

	UTextBlock* StarSystemNameplateTextBlock =
		WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			FName(*FString::Printf(TEXT("StarSystemRowName%d"), RowNumber)));
	StarSystemNameplateTextBlock->SetText(
		GetStarSystemNameplateText(CelestialBodyActor));
	StarSystemNameplateTextBlock->SetColorAndOpacity(
		FSlateColor(StarSystemNameplateTextColor));
	StarSystemNameplateTextBlock->SetAutoWrapText(false);
	if (UHorizontalBoxSlot* NameSlot =
			StarSystemHorizontalBox->AddChildToHorizontalBox(
				StarSystemNameplateTextBlock))
	{
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UTextBlock* OperationsBadgeTextBlock =
		WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			FName(*FString::Printf(TEXT("StarSystemRowStatus%d"), RowNumber)));
	OperationsBadgeTextBlock->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.55f, 0.68f, 0.74f, 1.0f)));
	OperationsBadgeTextBlock->SetJustification(ETextJustify::Right);
	OperationsBadgeTextBlock->SetAutoWrapText(false);
	FSlateFontInfo OperationsBadgeFont = OperationsBadgeTextBlock->GetFont();
	OperationsBadgeFont.Size = 11;
	OperationsBadgeTextBlock->SetFont(OperationsBadgeFont);
	if (UHorizontalBoxSlot* OperationsBadgeSlot =
			StarSystemHorizontalBox->AddChildToHorizontalBox(
				OperationsBadgeTextBlock))
	{
		OperationsBadgeSlot->SetPadding(FMargin(8.0f, 1.0f, 4.0f, 0.0f));
		OperationsBadgeSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
	OperationsBadgeActors.Add(CelestialBodyActor);
	OperationsBadgeTextBlocks.Add(OperationsBadgeTextBlock);

	USRCelestialBodyOverviewEntryAction* EntryAction =
		NewObject<USRCelestialBodyOverviewEntryAction>(this);
	EntryAction->Initialize(this, CelestialBodyActor);
	EntryActions.Add(EntryAction);
	StarSystemScrollBoxButton->OnClicked.AddDynamic(
		EntryAction, &USRCelestialBodyOverviewEntryAction::HandleClicked);

	if (UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(
			StarSystemScrollBox->AddChild(StarSystemScrollBoxButton)))
	{
		RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));
	}

	if (const TArray<AActor*>* Children =
			ChildrenByParent.Find(CelestialBodyActor))
	{
		for (AActor* ChildBody : *Children)
		{
			AddStarSystemScrollBoxButton(ChildBody, Depth + 1, ChildrenByParent);
		}
	}
}

FText USRCelestialBodyOverviewWidget::GetStarSystemNameplateText(
	const AActor* CelestialBodyActor) const
{
	if (const ASRCelestialBody* ProceduralBody =
			Cast<ASRCelestialBody>(CelestialBodyActor))
	{
		const FSRCelestialBodyData BodyData = ProceduralBody->GetData();
		if (!BodyData.VariableName.IsEmpty())
		{
			return BodyData.VariableName;
		}
	}

	return IsValid(CelestialBodyActor)
			   ? FText::FromString(CelestialBodyActor->GetName())
			   : FText::GetEmpty();
}

FText USRCelestialBodyOverviewWidget::GetWorldNameplateText(
	const AActor* CelestialBodyActor) const
{
	const FText Prefix = GetStarSystemNameplatePrefixText(
		const_cast<AActor*>(CelestialBodyActor));
	const FText BodyName = GetStarSystemNameplateText(CelestialBodyActor);
	const FSRStrategicBodyPresentation* StrategyBody =
		StrategicPresentation.FindBody(CelestialBodyActor);
	if (bShowStrategyOverlay && StrategyBody && StrategyBody->bHasBottleneck)
	{
		return FText::Format(
			NSLOCTEXT("StarRoversOverview", "StrategicNameplateFormat", "{0} {1}  [{2}]"),
			Prefix,
			BodyName,
			StrategyBody->ShortBadgeText);
	}
	if (CelestialBodyActor == RecommendedSystemScanBody)
	{
		return FText::Format(
			NSLOCTEXT("StarRoversOverview", "RecommendedNameplateFormat", "SCAN {0} {1}"),
			Prefix,
			BodyName);
	}
	return FText::Format(
		NSLOCTEXT("StarRoversOverview", "NameplateFormat", "{0} {1}"),
		Prefix,
		BodyName);
}

FText USRCelestialBodyOverviewWidget::GetStarSystemNameplatePrefixText(
	AActor* CelestialBodyActor) const
{
	const ASRCelestialBody* ProceduralBody =
		Cast<ASRCelestialBody>(CelestialBodyActor);
	if (!ProceduralBody)
	{
		return FText::FromString(TEXT("."));
	}

	switch (ProceduralBody->GetBodyCategory())
	{
	case ESRCelestialBodyCategory::Star:
		return FText::FromString(TEXT("*"));
	case ESRCelestialBodyCategory::Planet:
		return FText::FromString(TEXT("o"));
	case ESRCelestialBodyCategory::Moon:
		return FText::FromString(TEXT("-"));
	default:
		return FText::FromString(TEXT("."));
	}
}

FText USRCelestialBodyOverviewWidget::GetStarSystemTreePrefixText(
	AActor* CelestialBodyActor) const
{
	const ASRCelestialBody* ProceduralBody =
		Cast<ASRCelestialBody>(CelestialBodyActor);
	if (!ProceduralBody)
	{
		return FText::FromString(TEXT("."));
	}

	if (ProceduralBody->GetBodyCategory() == ESRCelestialBodyCategory::Star)
	{
		return FText::FromString(TEXT("*"));
	}

	const int32 SiblingSortIndex =
		GetStarSystemSiblingSortIndex(CelestialBodyActor);
	return SiblingSortIndex > 0
			   ? FText::FromString(FString::Printf(TEXT("%d."), SiblingSortIndex))
			   : FText::FromString(TEXT("."));
}

int32 USRCelestialBodyOverviewWidget::GetStarSystemSiblingSortIndex(
	AActor* CelestialBodyActor) const
{
	if (!IsValid(CelestialBodyActor))
	{
		return INDEX_NONE;
	}

	AActor* ParentBody = nullptr;
	if (!USRCelestialBodyRuntimeLibrary::GetCelestialParentBody(
			CelestialBodyActor, ParentBody) ||
		!IsValid(ParentBody))
	{
		return INDEX_NONE;
	}

	TArray<TObjectPtr<AActor>> SiblingBodies;
	for (AActor* CandidateBody : CelestialBodies)
	{
		if (!IsValid(CandidateBody))
		{
			continue;
		}

		AActor* CandidateParentBody = nullptr;
		if (USRCelestialBodyRuntimeLibrary::GetCelestialParentBody(
				CandidateBody, CandidateParentBody) &&
			CandidateParentBody == ParentBody)
		{
			SiblingBodies.Add(CandidateBody);
		}
	}

	SortStarSystemBodies(SiblingBodies);
	for (int32 SiblingIndex = 0; SiblingIndex < SiblingBodies.Num();
		 ++SiblingIndex)
	{
		if (SiblingBodies[SiblingIndex] == CelestialBodyActor)
		{
			return SiblingIndex + 1;
		}
	}

	return INDEX_NONE;
}

void USRCelestialBodyOverviewWidget::SortStarSystemBodies(
	TArray<TObjectPtr<AActor>>& StarSystemBodiesToSort) const
{
	StarSystemBodiesToSort.Sort([this](const AActor& Left, const AActor& Right)
	{
		return CompareStarSystemBodies(Left, Right);
	});
}

bool USRCelestialBodyOverviewWidget::CompareStarSystemBodies(
	const AActor& Left,
	const AActor& Right) const
{
    AActor *LeftParent = nullptr;
    AActor *RightParent = nullptr;
    const bool bLeftHasParent =
        USRCelestialBodyRuntimeLibrary::GetCelestialParentBody(&Left,
                                                               LeftParent) &&
        IsValid(LeftParent);
    const bool bRightHasParent =
        USRCelestialBodyRuntimeLibrary::GetCelestialParentBody(&Right,
                                                               RightParent) &&
        IsValid(RightParent);
    if (bLeftHasParent && bRightHasParent && LeftParent == RightParent) {
      const float LeftDistance = GetParentSortDistance(&Left);
      const float RightDistance = GetParentSortDistance(&Right);
      if (!FMath::IsNearlyEqual(LeftDistance, RightDistance)) {
        return LeftDistance < RightDistance;
      }
    }

    const int32 LeftRank = GetCategorySortRank(&Left);
    const int32 RightRank = GetCategorySortRank(&Right);
    if (LeftRank != RightRank) {
      return LeftRank < RightRank;
    }

    return GetStarSystemNameplateText(&Left).ToString() <
           GetStarSystemNameplateText(&Right).ToString();
}
