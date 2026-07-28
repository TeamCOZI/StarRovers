#include "UI/SRPlayerGuidanceWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Camera/SRPlayerController.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Framework/Application/SlateApplication.h"
#include "Simulation/SRRunMilestoneSubsystem.h"
#include "Simulation/SRTimeControlSubsystem.h"
#include "Simulation/SRAugmentSubsystem.h"
#include "UI/SRAugmentChoiceWidget.h"
#include "UI/SRCelestialBodyOperationsSummary.h"
#include "UI/SRFacilityControlWidget.h"
#include "UI/SRResourceGlyph.h"
#include "UI/SRStructureSelectionWidget.h"
#include "UI/SRUIComponents.h"
#include "UI/SRUILayoutPolicy.h"
#include "UI/SRUITheme.h"

TSharedRef<SWidget> USRPlayerGuidanceWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}
	BuildGuidanceWidgetTree();
	return Super::RebuildWidget();
}

void USRPlayerGuidanceWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildGuidanceWidgetTree();
	ContextRefreshAccumulator = ContextRefreshIntervalSeconds;
	if (bAutomaticContextEvaluation)
	{
		EvaluateCurrentContext();
	}
	RefreshPresentation();
	FVector2D ViewportSize = GetCachedGeometry().GetLocalSize();
	if (ViewportSize.X <= UE_SMALL_NUMBER || ViewportSize.Y <= UE_SMALL_NUMBER)
	{
		int32 ViewportWidth = 0;
		int32 ViewportHeight = 0;
		if (APlayerController* PlayerController = GetOwningPlayer())
		{
			PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
		}
		ViewportSize = FVector2D(
			static_cast<float>(ViewportWidth),
			static_cast<float>(ViewportHeight));
	}
	if (ViewportSize.X <= UE_SMALL_NUMBER || ViewportSize.Y <= UE_SMALL_NUMBER)
	{
		// Headless PIE has no render viewport. Start from the lowest supported
		// readability contract until a real viewport geometry becomes available.
		ViewportSize = FVector2D(
			FSRUILayoutPolicy::DefaultValidationViewportWidth,
			FSRUILayoutPolicy::DefaultValidationViewportHeight);
	}
	RefreshResponsiveLayout(ViewportSize, true);
}

void USRPlayerGuidanceWidget::NativeTick(
	const FGeometry& MyGeometry,
	float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshResponsiveLayout(MyGeometry.GetLocalSize());

	bool bNeedsRefresh = false;
	if (TransientMessage.IsVisible() && FPlatformTime::Seconds() >= TransientExpirySeconds)
	{
		TransientMessage = FSRPlayerGuidanceMessage();
		TransientExpirySeconds = 0.0;
		bNeedsRefresh = true;
	}

	if (bAutomaticContextEvaluation)
	{
		ContextRefreshAccumulator += FMath::Max(0.0f, InDeltaTime);
		if (ContextRefreshAccumulator >= FMath::Max(0.05f, ContextRefreshIntervalSeconds))
		{
			ContextRefreshAccumulator = 0.0f;
			ContextMessage = FSRPlayerGuidancePresentationBuilder::Evaluate(BuildCurrentSnapshot());
			bNeedsRefresh = true;
		}
	}

	if (bNeedsRefresh)
	{
		RefreshPresentation();
	}
}

void USRPlayerGuidanceWidget::SetGuidanceMessage(const FSRPlayerGuidanceMessage& NewMessage)
{
	ContextMessage = NewMessage;
	RefreshPresentation();
}

void USRPlayerGuidanceWidget::ClearGuidanceMessage()
{
	ContextMessage = FSRPlayerGuidanceMessage();
	TransientMessage = FSRPlayerGuidanceMessage();
	TransientExpirySeconds = 0.0;
	RefreshPresentation();
}

void USRPlayerGuidanceWidget::PushTransientNotification(
	FName MessageId,
	FText Title,
	FText Detail,
	FText Action,
	ESRUIVisualState VisualState,
	float DurationSeconds)
{
	TransientMessage = FSRPlayerGuidanceMessage();
	TransientMessage.MessageId = MessageId.IsNone() ? FName(TEXT("TransientUpdate")) : MessageId;
	TransientMessage.CategoryText = NSLOCTEXT("StarRoversGuidance", "SystemUpdateCategory", "SYSTEM UPDATE");
	TransientMessage.TitleText = MoveTemp(Title);
	TransientMessage.DetailText = MoveTemp(Detail);
	TransientMessage.ActionText = MoveTemp(Action);
	TransientMessage.VisualState = VisualState;
	TransientMessage.Priority = 60;
	TransientMessage.bTransient = true;
	TransientExpirySeconds = FPlatformTime::Seconds() + FMath::Max(0.25f, DurationSeconds);
	RefreshPresentation();
}

void USRPlayerGuidanceWidget::SetAutomaticContextEvaluationEnabled(bool bEnabled)
{
	bAutomaticContextEvaluation = bEnabled;
	ContextRefreshAccumulator = ContextRefreshIntervalSeconds;
	if (bAutomaticContextEvaluation)
	{
		EvaluateCurrentContext();
	}
}

FSRPlayerGuidanceMessage USRPlayerGuidanceWidget::GetDisplayedGuidanceMessage() const
{
	return DisplayedMessage;
}

FSRPlayerGuidanceSnapshot USRPlayerGuidanceWidget::BuildCurrentSnapshot() const
{
	FSRPlayerGuidanceSnapshot Snapshot;
	const ASRPlayerController* PlayerController = Cast<ASRPlayerController>(GetOwningPlayer());
	if (!IsValid(PlayerController))
	{
		return Snapshot;
	}

	const AActor* FocusedActor = PlayerController->GetSelectedActor();
	Snapshot.bHasFocusedActor = IsValid(FocusedActor);
	if (PlayerController->HasSelectedActorFocusInfo())
	{
		Snapshot.bCanConstructOnFocusedActor =
			PlayerController->GetSelectedActorFocusInfo().bCanConstruct;
	}
	if (const USRFacilityControlWidget* FacilityControl = PlayerController->GetFacilityControlWidget())
	{
		Snapshot.bHasSelectedFacility = FacilityControl->HasFocusedFacility();
	}
	if (const USRAugmentSubsystem* AugmentSubsystem = GetWorld()
		? GetWorld()->GetSubsystem<USRAugmentSubsystem>()
		: nullptr)
	{
		Snapshot.bBlockingChoiceVisible = AugmentSubsystem->IsAugmentChoicePending();
	}
	if (const USRAugmentChoiceWidget* ChoiceWidget = PlayerController->GetAugmentChoiceWidget())
	{
		Snapshot.bBlockingChoiceVisible |= ChoiceWidget->IsVisible();
	}
	if (const UWorld* World = GetWorld())
	{
		if (const USRTimeControlSubsystem* TimeControl =
			World->GetSubsystem<USRTimeControlSubsystem>())
		{
			Snapshot.bSimulationPaused = TimeControl->IsSimulationPaused();
		}
		if (USRRunMilestoneSubsystem* MilestoneSubsystem =
			World->GetSubsystem<USRRunMilestoneSubsystem>())
		{
			MilestoneSubsystem->RefreshFromWorld();
			Snapshot.FirstFuelMilestone =
				MilestoneSubsystem->GetFirstFuelMilestoneSnapshot();
		}
	}

	FSRCelestialBodyOperationsSummary OperationsSummary;
	Snapshot.bOperationsAvailable =
		FSRCelestialBodyOperationsSummaryBuilder::BuildSummary(FocusedActor, OperationsSummary);
	if (Snapshot.bOperationsAvailable)
	{
		Snapshot.FacilityCount = OperationsSummary.FacilityCount;
		Snapshot.ProcessingFacilityCount = OperationsSummary.ProcessingFacilityCount;
		Snapshot.ThrottledFacilityCount = OperationsSummary.ThrottledFacilityCount;
		Snapshot.OperationalPressure =
			FSRCelestialBodyOperationsSummaryBuilder::ResolveOperationalPressure(OperationsSummary);
		Snapshot.OperationalLoad = OperationsSummary.OperationalCapacity.TotalDemand;
		Snapshot.OperationalCapacity = OperationsSummary.OperationalCapacity.TotalCapacity;
		Snapshot.HubCount = OperationsSummary.HubCount;
		Snapshot.ConnectedRouteCount = OperationsSummary.ConnectedRouteCount;
		Snapshot.BlockedRouteCount = OperationsSummary.BlockedRouteCount;
		Snapshot.FleetAvailableCapacity = OperationsSummary.FleetAvailableCapacity;
		Snapshot.FleetQueuedDepartureCount = OperationsSummary.FleetQueuedDepartureCount;
	}
	return Snapshot;
}

bool USRPlayerGuidanceWidget::ExecuteDisplayedAction()
{
	ASRPlayerController* PlayerController = Cast<ASRPlayerController>(GetOwningPlayer());
	if (!IsValid(PlayerController))
	{
		return false;
	}

	const FSRPlayerGuidanceSnapshot CurrentSnapshot = BuildCurrentSnapshot();
	const FSRFirstFuelMilestoneSnapshot& Milestone = CurrentSnapshot.FirstFuelMilestone;
	bool bExecuted = false;
	switch (DisplayedMessage.ActionKind)
	{
	case ESRPlayerGuidanceActionKind::ResumeSimulation:
		if (USRTimeControlSubsystem* TimeControl = GetWorld()
			? GetWorld()->GetSubsystem<USRTimeControlSubsystem>()
			: nullptr)
		{
			TimeControl->ResumeSimulation();
			bExecuted = true;
		}
		break;
	case ESRPlayerGuidanceActionKind::ActivateEmergencyProspecting:
		if (USRRunMilestoneSubsystem* MilestoneSubsystem = GetWorld()
			? GetWorld()->GetSubsystem<USRRunMilestoneSubsystem>()
			: nullptr)
		{
			bExecuted =
				MilestoneSubsystem->TryActivateEmergencyProspectingRecovery();
		}
		break;
	case ESRPlayerGuidanceActionKind::BuildExtractor:
		bExecuted = ExecuteBuildAction(
			ESRStructureBuildRole::Extraction,
			ESRResourceFamily::None,
			Milestone);
		break;
	case ESRPlayerGuidanceActionKind::BuildFamilyProcessor:
		bExecuted = ExecuteBuildAction(
			ESRStructureBuildRole::FamilyProcessing,
			Milestone.FirstResourceFamily,
			Milestone);
		break;
	case ESRPlayerGuidanceActionKind::BuildStellarFuelFabricator:
		bExecuted = ExecuteBuildAction(
			ESRStructureBuildRole::StellarFuelFabrication,
			ESRResourceFamily::None,
			Milestone);
		break;
	case ESRPlayerGuidanceActionKind::BuildHub:
		bExecuted = ExecuteBuildAction(
			ESRStructureBuildRole::Hub,
			ESRResourceFamily::None,
			Milestone);
		break;
	case ESRPlayerGuidanceActionKind::InspectExtractor:
	case ESRPlayerGuidanceActionKind::InspectFamilyProcessor:
	case ESRPlayerGuidanceActionKind::InspectStellarFuelFabricator:
	case ESRPlayerGuidanceActionKind::InspectHub:
		bExecuted = ExecuteFacilityFocusAction(Milestone);
		break;
	case ESRPlayerGuidanceActionKind::FocusPrimaryStar:
		if (IsValid(Milestone.PrimaryStarActor.Get()))
		{
			PlayerController->SetAssemblyModeActive(false);
			PlayerController->RequestActorFocus(Milestone.PrimaryStarActor.Get(), true);
			bExecuted = true;
		}
		break;
	case ESRPlayerGuidanceActionKind::None:
	default:
		break;
	}

	if (bExecuted)
	{
		EvaluateCurrentContext();
	}
	return bExecuted;
}

bool USRPlayerGuidanceWidget::IsPointerOverGuidanceUI() const
{
	if (!IsVisible()
		|| !ActionButton
		|| ActionButton->GetVisibility() != ESlateVisibility::Visible
		|| !FSlateApplication::IsInitialized())
	{
		return false;
	}
	return ActionButton->GetCachedGeometry().IsUnderLocation(
		FSlateApplication::Get().GetCursorPos());
}

FSRUITopCenterLaneLayout USRPlayerGuidanceWidget::GetResolvedCommandLaneLayout() const
{
	return ResolvedCommandLaneLayout;
}

bool USRPlayerGuidanceWidget::IsCompactCommandLane() const
{
	return ResolvedCommandLaneLayout.bCompact;
}

bool USRPlayerGuidanceWidget::ExecuteBuildAction(
	ESRStructureBuildRole Role,
	ESRResourceFamily PreferredFamily,
	const FSRFirstFuelMilestoneSnapshot& MilestoneSnapshot)
{
	ASRPlayerController* PlayerController = Cast<ASRPlayerController>(GetOwningPlayer());
	if (!IsValid(PlayerController))
	{
		return false;
	}

	const FSRSystemScanCandidate* ScanCandidate =
		MilestoneSnapshot.InitialSystemScan.GetRecommendedCandidate();
	AActor* TargetBody = Role == ESRStructureBuildRole::Extraction
		&& ScanCandidate
		&& IsValid(ScanCandidate->BodyActor.Get())
		? ScanCandidate->BodyActor.Get()
		: PlayerController->GetSelectedActor();
	if (!IsValid(TargetBody)
		|| !USRCelestialBodyRuntimeLibrary::GetCelestialCanConstruct(TargetBody))
	{
		TargetBody = MilestoneSnapshot.RecommendedBodyActor.Get();
	}
	if (!IsValid(TargetBody)
		|| !USRCelestialBodyRuntimeLibrary::GetCelestialCanConstruct(TargetBody))
	{
		return false;
	}

	const bool bFocusedRecommendedDeposit = Role == ESRStructureBuildRole::Extraction
		&& ScanCandidate
		&& ScanCandidate->BodyActor == TargetBody
		&& !ScanCandidate->DepositOccupantId.IsNone()
		&& PlayerController->RequestSurfaceStructureFocus(
			TargetBody,
			ScanCandidate->DepositOccupantId,
			true);
	if (!bFocusedRecommendedDeposit)
	{
		PlayerController->RequestActorFocus(TargetBody, true);
	}
	PlayerController->SetAssemblyModeActive(true);
	USRStructureSelectionWidget* BuildDock = PlayerController->GetStructureSelectionWidget();
	const bool bSelectedBuildOption = IsValid(BuildDock)
		&& BuildDock->SelectRecommendedBuildOption(Role, PreferredFamily, true);
	if (!bSelectedBuildOption)
	{
		return false;
	}

	// SetAssemblyModeActive refreshes the body focus model, so restore the
	// concrete deposit after the construction option has been selected. The
	// controller recognizes that the body is already selected and preserves
	// both Assembly mode and the Build Dock selection on this second pass.
	if (bFocusedRecommendedDeposit)
	{
		PlayerController->RequestSurfaceStructureFocus(
			TargetBody,
			ScanCandidate->DepositOccupantId,
			true);
	}
	return true;
}

bool USRPlayerGuidanceWidget::ExecuteFacilityFocusAction(
	const FSRFirstFuelMilestoneSnapshot& MilestoneSnapshot)
{
	ASRPlayerController* PlayerController = Cast<ASRPlayerController>(GetOwningPlayer());
	return IsValid(PlayerController)
		&& IsValid(MilestoneSnapshot.TargetFacilityBodyActor.Get())
		&& !MilestoneSnapshot.TargetFacilityOccupantId.IsNone()
		&& PlayerController->RequestFacilityFocus(
			MilestoneSnapshot.TargetFacilityBodyActor.Get(),
			MilestoneSnapshot.TargetFacilityOccupantId,
			true);
}

void USRPlayerGuidanceWidget::HandleActionButtonClicked()
{
	ExecuteDisplayedAction();
}

void USRPlayerGuidanceWidget::EvaluateCurrentContext()
{
	ContextMessage = FSRPlayerGuidancePresentationBuilder::Evaluate(BuildCurrentSnapshot());
	ContextRefreshAccumulator = 0.0f;
	RefreshPresentation();
}

void USRPlayerGuidanceWidget::BuildGuidanceWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}
	if (WidgetTree->RootWidget)
	{
		CacheGuidanceWidgetTree();
		if (BannerScaleBox && BannerDesignSizeBox && BannerCard
			&& ResourceGlyphWidget && ActionButton)
		{
			RefreshPresentation();
			return;
		}

		// Upgrade an optional configured Blueprint authored before the shared
		// viewport fitting policy without requiring an asset migration pass.
		WidgetTree->RootWidget = nullptr;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("PlayerGuidanceCanvasPanel"));
	WidgetTree->RootWidget = RootCanvas;
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	BannerScaleBox = WidgetTree->ConstructWidget<UScaleBox>(
		UScaleBox::StaticClass(),
		TEXT("PlayerGuidanceBannerScaleBox"));
	BannerScaleBox->SetStretch(EStretch::ScaleToFit);
	BannerScaleBox->SetStretchDirection(EStretchDirection::DownOnly);
	if (UCanvasPanelSlot* BannerScaleSlot = RootCanvas->AddChildToCanvas(BannerScaleBox))
	{
		BannerScaleSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		BannerScaleSlot->SetOffsets(FMargin(FSRUILayoutPolicy::DefaultSafeMargin));
	}

	BannerDesignSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		TEXT("PlayerGuidanceBannerDesignSizeBox"));
	BannerDesignSizeBox->SetWidthOverride(FMath::Max(320.0f, BannerWidth));
	BannerDesignSizeBox->SetHeightOverride(FMath::Max(72.0f, BannerHeight));
	if (UScaleBoxSlot* BannerDesignSlot = Cast<UScaleBoxSlot>(BannerScaleBox->AddChild(BannerDesignSizeBox)))
	{
		BannerDesignSlot->SetHorizontalAlignment(HAlign_Center);
		BannerDesignSlot->SetVerticalAlignment(VAlign_Top);
	}

	BannerCard = WidgetTree->ConstructWidget<USRThemedCardWidget>(
		USRThemedCardWidget::StaticClass(),
		TEXT("PlayerGuidanceBannerCard"));
	BannerCard->SetCardPadding(FMargin(16.0f, 12.0f));
	BannerCard->SetVisibility(ESlateVisibility::Collapsed);
	BannerDesignSizeBox->AddChild(BannerCard);

	UHorizontalBox* ContentRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		TEXT("PlayerGuidanceContentRow"));
	BannerCard->SetContent(ContentRow);

	CategoryBadge = WidgetTree->ConstructWidget<USRStatusBadgeWidget>(
		USRStatusBadgeWidget::StaticClass(),
		TEXT("PlayerGuidanceCategoryBadge"));
	if (UHorizontalBoxSlot* BadgeSlot = ContentRow->AddChildToHorizontalBox(CategoryBadge))
	{
		BadgeSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		BadgeSlot->SetVerticalAlignment(VAlign_Top);
		BadgeSlot->SetPadding(FMargin(0.0f, 1.0f, 14.0f, 0.0f));
	}

	UVerticalBox* TextColumn = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("PlayerGuidanceTextColumn"));
	if (UHorizontalBoxSlot* TextColumnSlot = ContentRow->AddChildToHorizontalBox(TextColumn))
	{
		TextColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TextColumnSlot->SetVerticalAlignment(VAlign_Center);
	}

	TitleTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("PlayerGuidanceTitleTextBlock"));
	USRUIThemeLibrary::ApplyTextStyle(TitleTextBlock, ESRUITextStyle::Heading);
	TitleTextBlock->SetAutoWrapText(true);
	if (UVerticalBoxSlot* TitleSlot = TextColumn->AddChildToVerticalBox(TitleTextBlock))
	{
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f));
	}

	ResourceGlyphWidget = WidgetTree->ConstructWidget<USRResourceGlyphWidget>(
		USRResourceGlyphWidget::StaticClass(),
		TEXT("PlayerGuidanceResourceGlyph"));
	ResourceGlyphWidget->SetGlyphMode(ESRResourceGlyphMode::Identity);
	ResourceGlyphWidget->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* GlyphSlot = TextColumn->AddChildToVerticalBox(ResourceGlyphWidget))
	{
		GlyphSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		GlyphSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}

	DetailTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("PlayerGuidanceDetailTextBlock"));
	USRUIThemeLibrary::ApplyTextStyle(DetailTextBlock, ESRUITextStyle::Body);
	DetailTextBlock->SetAutoWrapText(true);
	if (UVerticalBoxSlot* DetailSlot = TextColumn->AddChildToVerticalBox(DetailTextBlock))
	{
		DetailSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}

	ActionTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("PlayerGuidanceActionTextBlock"));
	USRUIThemeLibrary::ApplyTextStyle(
		ActionTextBlock,
		ESRUITextStyle::Caption,
		ESRUIVisualState::Info,
		true);
	ActionTextBlock->SetAutoWrapText(true);
	TextColumn->AddChildToVerticalBox(ActionTextBlock);

	ActionButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(),
		TEXT("PlayerGuidanceActionButton"));
	ActionButton->OnClicked.AddDynamic(this, &USRPlayerGuidanceWidget::HandleActionButtonClicked);
	if (UHorizontalBoxSlot* ActionSlot = ContentRow->AddChildToHorizontalBox(ActionButton))
	{
		ActionSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		ActionSlot->SetVerticalAlignment(VAlign_Center);
		ActionSlot->SetPadding(FMargin(18.0f, 0.0f, 0.0f, 0.0f));
	}
	ActionButtonTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("PlayerGuidanceActionButtonTextBlock"));
	ActionButtonTextBlock->SetJustification(ETextJustify::Center);
	ActionButton->AddChild(ActionButtonTextBlock);

	RefreshPresentation();
}

void USRPlayerGuidanceWidget::CacheGuidanceWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}
	BannerScaleBox = Cast<UScaleBox>(WidgetTree->FindWidget(TEXT("PlayerGuidanceBannerScaleBox")));
	BannerDesignSizeBox = Cast<USizeBox>(WidgetTree->FindWidget(TEXT("PlayerGuidanceBannerDesignSizeBox")));
	BannerCard = Cast<USRThemedCardWidget>(WidgetTree->FindWidget(TEXT("PlayerGuidanceBannerCard")));
	CategoryBadge = Cast<USRStatusBadgeWidget>(WidgetTree->FindWidget(TEXT("PlayerGuidanceCategoryBadge")));
	TitleTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("PlayerGuidanceTitleTextBlock")));
	ResourceGlyphWidget = Cast<USRResourceGlyphWidget>(WidgetTree->FindWidget(TEXT("PlayerGuidanceResourceGlyph")));
	DetailTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("PlayerGuidanceDetailTextBlock")));
	ActionTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("PlayerGuidanceActionTextBlock")));
	ActionButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("PlayerGuidanceActionButton")));
	ActionButtonTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("PlayerGuidanceActionButtonTextBlock")));
}

void USRPlayerGuidanceWidget::RefreshResponsiveLayout(
	const FVector2D& ViewportSize,
	bool bForceRefresh)
{
	if (!BannerScaleBox || !BannerDesignSizeBox
		|| ViewportSize.X <= UE_SMALL_NUMBER || ViewportSize.Y <= UE_SMALL_NUMBER)
	{
		return;
	}
	if (!bForceRefresh && ViewportSize.Equals(LastResponsiveViewportSize, 0.5f))
	{
		return;
	}

	LastResponsiveViewportSize = ViewportSize;
	ResolvedCommandLaneLayout = FSRUILayoutPolicy::ResolveTopCenterLane(
		FVector2D(
			FMath::Max(320.0f, BannerWidth),
			FMath::Max(72.0f, BannerHeight)),
		ViewportSize);
	if (UCanvasPanelSlot* BannerScaleSlot = Cast<UCanvasPanelSlot>(BannerScaleBox->Slot))
	{
		BannerScaleSlot->SetOffsets(ResolvedCommandLaneLayout.Insets);
	}
	BannerDesignSizeBox->SetWidthOverride(FMath::Max(
		320.0f,
		ResolvedCommandLaneLayout.DesignSize.X));
	BannerDesignSizeBox->SetHeightOverride(FMath::Max(72.0f, BannerHeight));
	if (DetailTextBlock)
	{
		DetailTextBlock->SetVisibility(
			ResolvedCommandLaneLayout.DesignSize.X
				>= FMath::Max(320.0f, CompactDetailVisibilityWidth)
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
	}
}

void USRPlayerGuidanceWidget::RefreshPresentation()
{
	if (!BannerCard || !CategoryBadge || !TitleTextBlock || !ResourceGlyphWidget || !DetailTextBlock
		|| !ActionTextBlock || !ActionButton || !ActionButtonTextBlock)
	{
		return;
	}

	DisplayedMessage = ResolveMessageToDisplay();
	if (!DisplayedMessage.IsVisible())
	{
		BannerCard->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	BannerCard->SetVisualState(DisplayedMessage.VisualState);
	BannerCard->SetToolTipText(DisplayedMessage.ToolTipText);
	BannerCard->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	CategoryBadge->SetBadge(DisplayedMessage.CategoryText, DisplayedMessage.VisualState);
	TitleTextBlock->SetText(DisplayedMessage.TitleText);
	if (DisplayedMessage.bShowResourceGlyph && DisplayedMessage.ResourceGlyph.bHasResource)
	{
		ResourceGlyphWidget->SetGlyphMode(ESRResourceGlyphMode::Identity);
		ResourceGlyphWidget->SetPresentation(DisplayedMessage.ResourceGlyph);
		ResourceGlyphWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		ResourceGlyphWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	DetailTextBlock->SetText(DisplayedMessage.DetailText);
	DetailTextBlock->SetVisibility(
		ResolvedCommandLaneLayout.DesignSize.X
			>= FMath::Max(320.0f, CompactDetailVisibilityWidth)
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	const bool bHasDirectAction = DisplayedMessage.ActionKind != ESRPlayerGuidanceActionKind::None
		&& !DisplayedMessage.ActionText.IsEmpty();
	ActionButtonTextBlock->SetText(DisplayedMessage.ActionText);
	ActionButton->SetToolTipText(DisplayedMessage.ToolTipText);
	DetailTextBlock->SetToolTipText(DisplayedMessage.ToolTipText);
	ActionButton->SetVisibility(bHasDirectAction
		? ESlateVisibility::Visible
		: ESlateVisibility::Collapsed);
	ActionButton->SetIsEnabled(bHasDirectAction);
	ActionTextBlock->SetText(DisplayedMessage.ActionText);
	ActionTextBlock->SetVisibility(!bHasDirectAction && !DisplayedMessage.ActionText.IsEmpty()
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed);
	const FSRUIStatePalette Palette = USRUIThemeLibrary::ResolveStatePalette(DisplayedMessage.VisualState);
	ActionButton->SetBackgroundColor(Palette.AccentColor);
	USRUIThemeLibrary::ApplyTextStyle(
		ActionButtonTextBlock,
		ESRUITextStyle::Caption,
		DisplayedMessage.VisualState,
		false);
	USRUIThemeLibrary::ApplyTextStyle(
		ActionTextBlock,
		ESRUITextStyle::Caption,
		DisplayedMessage.VisualState,
		true);
}

FSRPlayerGuidanceMessage USRPlayerGuidanceWidget::ResolveMessageToDisplay() const
{
	if (ContextMessage.MessageId == FName(TEXT("BlockingChoice")))
	{
		return FSRPlayerGuidanceMessage();
	}
	if (TransientMessage.IsVisible()
		&& (!ContextMessage.IsVisible() || TransientMessage.Priority >= ContextMessage.Priority))
	{
		return TransientMessage;
	}
	return ContextMessage;
}
