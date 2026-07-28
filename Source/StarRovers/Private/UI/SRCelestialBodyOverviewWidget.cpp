#include "UI/SRCelestialBodyOverviewWidget.h"

#include "Utility/SRLog.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/PlayerCameraManager.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Simulation/SRRunMilestoneSubsystem.h"
#include "Styling/SlateColor.h"
#include "UI/SRCelestialBodyOperationsSummary.h"
#include "UI/SRResourceGlyph.h"
#include "UI/SRUIComponents.h"
#include "UI/SRUITheme.h"

namespace
{
	constexpr float NameplateLayoutRefreshIntervalSeconds = 0.20f;
	constexpr float NameplateCameraMoveRefreshDistance = 25.0f;
	constexpr float NameplateCameraRotationRefreshDegrees = 0.10f;
	constexpr float NameplateViewportSizeRefreshTolerance = 0.5f;
	constexpr float OperationsBadgeRefreshIntervalSeconds = 0.50f;

	FLinearColor GetOperationsBadgeColor(
		const ESRCelestialBodyOperationsPressure Pressure,
		const ESRResourceReservePressure ReservePressure)
	{
		if (Pressure == ESRCelestialBodyOperationsPressure::OverCapacity
			|| ReservePressure == ESRResourceReservePressure::Depleted)
		{
			return FLinearColor(1.0f, 0.30f, 0.24f, 1.0f);
		}
		if (Pressure == ESRCelestialBodyOperationsPressure::AtCapacity
			|| Pressure == ESRCelestialBodyOperationsPressure::NearCapacity
			|| ReservePressure == ESRResourceReservePressure::Critical)
		{
			return FLinearColor(1.0f, 0.72f, 0.22f, 1.0f);
		}
		if (ReservePressure == ESRResourceReservePressure::Low)
		{
			return FLinearColor(0.95f, 0.82f, 0.30f, 1.0f);
		}
		switch (Pressure)
		{
		case ESRCelestialBodyOperationsPressure::Idle:
			return FLinearColor(0.55f, 0.68f, 0.74f, 1.0f);
		case ESRCelestialBodyOperationsPressure::Nominal:
		default:
			return FLinearColor(0.30f, 0.90f, 0.70f, 1.0f);
		}
	}

}

void USRCelestialBodyOverviewEntryAction::Initialize(
	USRCelestialBodyOverviewWidget* InOwnerWidget,
	AActor* InCelestialBodyActor)
{
	OwnerWidget = InOwnerWidget;
	CelestialBodyActor = InCelestialBodyActor;
}

void USRCelestialBodyOverviewEntryAction::HandleClicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: Overview Entry OnClicked Actor=%s"),
		*GetNameSafe(CelestialBodyActor.Get()));

	if (IsValid(OwnerWidget))
	{
		OwnerWidget->DispatchEntryClicked(CelestialBodyActor);
	}
}

TSharedRef<SWidget> USRCelestialBodyOverviewWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}

	BuildOverviewWidgetTree();
	return Super::RebuildWidget();
}

void USRCelestialBodyOverviewWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildOverviewWidgetTree();
	RefreshInitialSystemScanRecommendation();
	RebuildStarSystemScrollBox();
	RebuildNameplateButtons();
}

void USRCelestialBodyOverviewWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	BuildOverviewWidgetTree();
	RefreshInitialSystemScanRecommendation();
	RebuildStarSystemScrollBox();
	RebuildNameplateButtons();
}

void USRCelestialBodyOverviewWidget::NativeTick(const FGeometry& MyGeometry,
												float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!IsVisible())
	{
		return;
	}

	OperationsBadgeRefreshAccumulator += FMath::Max(0.0f, InDeltaTime);
	if (OperationsBadgeRefreshAccumulator >= OperationsBadgeRefreshIntervalSeconds)
	{
		OperationsBadgeRefreshAccumulator = 0.0f;
		if (RefreshInitialSystemScanRecommendation())
		{
			RebuildStarSystemScrollBox();
			RebuildNameplateButtons();
		}
		else
		{
			RefreshOperationsBadges();
		}
	}

	if ((!bShowNameplateButtons || NameplateButtons.IsEmpty())
		&& (!bShowStrategyOverlay || StrategicPresentation.Routes.IsEmpty()))
	{
		return;
	}

	APlayerController* PlayerController = GetOwningPlayer();
	if (!IsValid(PlayerController) || !IsValid(PlayerController->PlayerCameraManager))
	{
		return;
	}

	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
	const FVector2D CurrentViewportSize(static_cast<float>(ViewportWidth), static_cast<float>(ViewportHeight));
	const FVector CurrentCameraLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
	const FRotator CurrentCameraRotation = PlayerController->PlayerCameraManager->GetCameraRotation();

	NameplateLayoutRefreshAccumulator += FMath::Max(0.0f, InDeltaTime);
	const bool bCameraMoved = FVector::DistSquared(CurrentCameraLocation, LastNameplateCameraLocation)
		>= FMath::Square(NameplateCameraMoveRefreshDistance);
	const bool bCameraRotated = !CurrentCameraRotation.Equals(LastNameplateCameraRotation, NameplateCameraRotationRefreshDegrees);
	const bool bViewportSizeChanged = !CurrentViewportSize.Equals(LastNameplateViewportSize, NameplateViewportSizeRefreshTolerance);
	const bool bShouldRefresh = !bHasNameplateLayoutState
		|| bCameraMoved
		|| bCameraRotated
		|| bViewportSizeChanged
		|| NameplateLayoutRefreshAccumulator >= NameplateLayoutRefreshIntervalSeconds;
	if (!bShouldRefresh)
	{
		return;
	}

	NameplateLayoutRefreshAccumulator = 0.0f;
	LastNameplateCameraLocation = CurrentCameraLocation;
	LastNameplateCameraRotation = CurrentCameraRotation;
	LastNameplateViewportSize = CurrentViewportSize;
	bHasNameplateLayoutState = true;
	RefreshNameplateButtonLayout();
}

void USRCelestialBodyOverviewWidget::RefreshOperationsBadges()
{
	RefreshStrategicOverlay();

	if (OperationsBadgeActors.Num() != OperationsBadgeTextBlocks.Num()
		|| StarSystemRowActors.Num() != StarSystemRowButtons.Num())
	{
		return;
	}
	for (int32 Index = 0; Index < StarSystemRowActors.Num(); ++Index)
	{
		if (UButton* RowButton = StarSystemRowButtons[Index])
		{
			const AActor* RowActor = StarSystemRowActors[Index];
			const FSRStrategicBodyPresentation* StrategyBody =
				StrategicPresentation.FindBody(RowActor);
			RowButton->SetBackgroundColor(RowActor == RecommendedSystemScanBody
				? RecommendedSystemScanColor
				: RowActor == SelectedActor
					? SelectedStarSystemScrollBoxButtonColor
					: StrategyBody && StrategyBody->bHasBottleneck
						? USRUIThemeLibrary::ResolveStatePalette(
							StrategyBody->VisualState).SurfaceColor
					: StarSystemScrollBoxButtonColor);
		}
	}

	for (int32 Index = 0; Index < OperationsBadgeActors.Num(); ++Index)
	{
		AActor* BodyActor = OperationsBadgeActors[Index];
		UTextBlock* BadgeTextBlock = OperationsBadgeTextBlocks[Index];
		if (!IsValid(BadgeTextBlock))
		{
			continue;
		}

		const bool bRecommended = BodyActor == RecommendedSystemScanBody;
		const FSRStrategicBodyPresentation* StrategyBody =
			StrategicPresentation.FindBody(BodyActor);
		const FSRCelestialBodyOperationsSummary* Summary = StrategyBody
			? &StrategyBody->Operations
			: nullptr;

		TArray<FString> BadgeParts;
		TArray<FString> ToolTipParts;
		if (bRecommended)
		{
			BadgeParts.Add(RecommendedSystemScanBadgeText.ToString());
			ToolTipParts.Add(RecommendedSystemScanToolTipText.ToString());
		}
		if (StrategyBody && StrategyBody->bHasBottleneck)
		{
			BadgeParts.Add(StrategyBody->ShortBadgeText.ToString());
			ToolTipParts.Add(StrategyBody->ToolTipText.ToString());
		}
		else if (Summary && Summary->bIsValid)
		{
			ToolTipParts.Add(
				FSRCelestialBodyOperationsSummaryBuilder::BuildOperationalToolTipText(*Summary));
		}
		if (Summary && Summary->bIsValid)
		{
			BadgeParts.Add(
				FSRCelestialBodyOperationsSummaryBuilder::BuildOperationalBadgeText(*Summary));
		}

		BadgeTextBlock->SetText(FText::FromString(FString::Join(BadgeParts, TEXT(" | "))));
		BadgeTextBlock->SetToolTipText(FText::FromString(
			FString::Join(ToolTipParts, TEXT("\n\n"))));
		BadgeTextBlock->SetVisibility(BadgeParts.IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::Visible);
		const FLinearColor BadgeColor = bRecommended
			? RecommendedSystemScanColor
			: StrategyBody && StrategyBody->bHasBottleneck
				? USRUIThemeLibrary::ResolveStatePalette(
					StrategyBody->VisualState).AccentColor
				: Summary
					? GetOperationsBadgeColor(
						FSRCelestialBodyOperationsSummaryBuilder::ResolveOperationalPressure(*Summary),
						Summary->ResourceReserve.Pressure)
					: StarSystemNameplateTextColor;
		BadgeTextBlock->SetColorAndOpacity(FSlateColor(BadgeColor));
	}
}

void USRCelestialBodyOverviewWidget::RefreshStrategicOverlay()
{
	TArray<AActor*> BodyActors;
	BodyActors.Reserve(CelestialBodies.Num());
	for (AActor* BodyActor : CelestialBodies)
	{
		if (IsValid(BodyActor))
		{
			BodyActors.Add(BodyActor);
		}
	}
	StrategicPresentation = FSRStrategicOverlayPresentationBuilder::BuildFromWorld(
		GetWorld(), BodyActors);
	RefreshStrategicHeader();
	RefreshNameplateStrategicVisuals();
	RefreshStrategicRouteLineLayouts();
}

void USRCelestialBodyOverviewWidget::RefreshStrategicHeader()
{
	if (StrategicStatusBadge)
	{
		const FText Label = StrategicPresentation.SummaryLabel.IsEmpty()
			? NSLOCTEXT("StarRoversOverview", "NetworkNominalFallback", "NETWORK NOMINAL")
			: StrategicPresentation.SummaryLabel;
		StrategicStatusBadge->SetBadge(Label, StrategicPresentation.SummaryState);
		StrategicStatusBadge->SetToolTipText(StrategicPresentation.SummaryDetailText);
	}
	if (StrategicDetailTextBlock)
	{
		StrategicDetailTextBlock->SetText(StrategicPresentation.SummaryDetailText);
		USRUIThemeLibrary::ApplyTextStyle(
			StrategicDetailTextBlock,
			ESRUITextStyle::Caption,
			StrategicPresentation.SummaryState);
	}
	if (StrategicFocusButton)
	{
		StrategicFocusButton->SetVisibility(StrategicPresentation.bHasRecommendation
			&& IsValid(StrategicPresentation.RecommendedBodyActor.Get())
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
		StrategicFocusButton->SetBackgroundColor(
			USRUIThemeLibrary::ResolveStatePalette(
				StrategicPresentation.SummaryState).SurfaceColor);
	}
	if (StrategicFocusButtonTextBlock)
	{
		StrategicFocusButtonTextBlock->SetText(StrategicPresentation.FocusActionText);
		USRUIThemeLibrary::ApplyTextStyle(
			StrategicFocusButtonTextBlock,
			ESRUITextStyle::Caption,
			StrategicPresentation.SummaryState,
			true);
	}
	if (StrategyOverlayToggleButton)
	{
		StrategyOverlayToggleButton->SetBackgroundColor(
			USRUIThemeLibrary::ResolveStatePalette(bShowStrategyOverlay
				? ESRUIVisualState::Selected
				: ESRUIVisualState::Disabled).SurfaceColor);
	}
	if (StrategyOverlayToggleButtonTextBlock)
	{
		StrategyOverlayToggleButtonTextBlock->SetText(bShowStrategyOverlay
			? NSLOCTEXT("StarRoversOverview", "StrategyOverlayOn", "ROUTES ON")
			: NSLOCTEXT("StarRoversOverview", "StrategyOverlayOff", "ROUTES OFF"));
	}
}

void USRCelestialBodyOverviewWidget::RefreshNameplateStrategicVisuals()
{
	if (NameplateActors.Num() != NameplateButtons.Num()
		|| NameplateActors.Num() != NameplateTextBlocks.Num())
	{
		return;
	}
	for (int32 Index = 0; Index < NameplateActors.Num(); ++Index)
	{
		AActor* BodyActor = NameplateActors[Index];
		UButton* NameplateButton = NameplateButtons[Index];
		UTextBlock* NameplateTextBlock = NameplateTextBlocks[Index];
		if (!IsValid(BodyActor) || !IsValid(NameplateButton) || !IsValid(NameplateTextBlock))
		{
			continue;
		}
		const FSRStrategicBodyPresentation* StrategyBody =
			StrategicPresentation.FindBody(BodyActor);
		NameplateButton->SetBackgroundColor(BodyActor == RecommendedSystemScanBody
			? RecommendedSystemScanColor
			: BodyActor == SelectedActor
				? SelectedNameplateButtonColor
				: bShowStrategyOverlay && StrategyBody && StrategyBody->bHasBottleneck
					? USRUIThemeLibrary::ResolveStatePalette(
						StrategyBody->VisualState).SurfaceColor
					: NameplateButtonColor);
		NameplateTextBlock->SetText(GetWorldNameplateText(BodyActor));
		NameplateButton->SetToolTipText(StrategyBody && StrategyBody->bHasBottleneck
			? StrategyBody->ToolTipText
			: FText::GetEmpty());
	}
}

bool USRCelestialBodyOverviewWidget::RefreshInitialSystemScanRecommendation()
{
	AActor* PreviousBody = RecommendedSystemScanBody.Get();
	const FText PreviousBadge = RecommendedSystemScanBadgeText;
	RecommendedSystemScanBody = nullptr;
	RecommendedSystemScanBadgeText = FText::GetEmpty();
	RecommendedSystemScanToolTipText = FText::GetEmpty();

	USRRunMilestoneSubsystem* MilestoneSubsystem = GetWorld()
		? GetWorld()->GetSubsystem<USRRunMilestoneSubsystem>()
		: nullptr;
	if (IsValid(MilestoneSubsystem))
	{
		MilestoneSubsystem->RefreshFromWorld();
		if (MilestoneSubsystem->IsInitialSystemScanRecommendationActive())
		{
			const FSRSystemScanSnapshot Scan =
				MilestoneSubsystem->GetInitialSystemScanSnapshot();
			if (const FSRSystemScanCandidate* Candidate = Scan.GetRecommendedCandidate())
			{
				const FSRResourceGlyphPresentation ResourceGlyph =
					FSRResourceGlyphPresentationBuilder::BuildIdentity(
						Candidate->ResourceDisplayName,
						Candidate->ResourceId,
						ESRResourceClass::Card,
						Candidate->Family,
						Candidate->SeedEnergy,
						Candidate->Spectrum,
						Candidate->Grade);
				RecommendedSystemScanBody = Candidate->BodyActor;
				RecommendedSystemScanBadgeText = FText::Format(
					NSLOCTEXT("StarRoversOverview", "InitialSystemScanBadge", "★ {0}"),
					ResourceGlyph.SpectrumGradeToken);
				RecommendedSystemScanToolTipText = FText::Format(
					NSLOCTEXT(
						"StarRoversOverview",
						"InitialSystemScanTooltip",
						"시작 추천 {0}/100\n{1} · {2} · Energy {3}\nCapacity 여유 {4} · 인접 건설 가능"),
					FText::AsNumber(Candidate->Score.TotalScore),
					Candidate->BodyDisplayName,
					Candidate->ResourceDisplayName,
					FText::AsNumber(Candidate->SeedEnergy),
					FText::AsNumber(Candidate->OperationalHeadroom));
			}
		}
	}

	return PreviousBody != RecommendedSystemScanBody
		|| !PreviousBadge.EqualTo(RecommendedSystemScanBadgeText);
}

FReply USRCelestialBodyOverviewWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D ScreenPosition = InMouseEvent.GetScreenSpacePosition();
	if (IsScreenPositionOverOverviewUI(ScreenPosition))
	{
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: Overview NativeOnMouseButtonDown handled Mouse=(%.1f, %.1f)"),
			ScreenPosition.X,
			ScreenPosition.Y);
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply USRCelestialBodyOverviewWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D ScreenPosition = InMouseEvent.GetScreenSpacePosition();
	if (IsScreenPositionOverOverviewUI(ScreenPosition))
	{
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: Overview NativeOnMouseButtonUp handled Mouse=(%.1f, %.1f)"),
			ScreenPosition.X,
			ScreenPosition.Y);
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply USRCelestialBodyOverviewWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (IsScreenPositionOverOverviewUI(InMouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Handled();
	}

	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

void USRCelestialBodyOverviewWidget::SetCelestialBodies(
	const TArray<AActor*>& NewCelestialBodies)
{
	CelestialBodies.Reset();
	CelestialBodies.Reserve(NewCelestialBodies.Num());
	TSet<AActor*> UniqueCelestialBodies;
	UniqueCelestialBodies.Reserve(NewCelestialBodies.Num());
	for (AActor* CelestialBodyActor : NewCelestialBodies)
	{
		if (IsValid(CelestialBodyActor) &&
			USRCelestialBodyRuntimeLibrary::IsCelestialBodyActor(
				CelestialBodyActor))
		{
			const int32 PreviousBodyCount = UniqueCelestialBodies.Num();
			UniqueCelestialBodies.Add(CelestialBodyActor);
			if (UniqueCelestialBodies.Num() != PreviousBodyCount)
			{
				CelestialBodies.Add(CelestialBodyActor);
			}
		}
	}

	SortStarSystemBodies(CelestialBodies);
	RefreshInitialSystemScanRecommendation();
	RebuildStarSystemScrollBox();
	RebuildNameplateButtons();
}

void USRCelestialBodyOverviewWidget::SetSelectedActor(
	AActor* NewSelectedActor)
{
	if (SelectedActor == NewSelectedActor)
	{
		return;
	}

	SelectedActor = NewSelectedActor;
	RebuildStarSystemScrollBox();
	RebuildNameplateButtons();
}

bool USRCelestialBodyOverviewWidget::IsPointerOverOverviewUI() const
{
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}

	return IsScreenPositionOverOverviewUI(FSlateApplication::Get().GetCursorPos());
}

bool USRCelestialBodyOverviewWidget::IsBodyInitialSystemScanRecommendation(
	const AActor* CelestialBodyActor) const
{
	if (!IsValid(CelestialBodyActor))
	{
		return false;
	}

	// The visual cache refreshes at a deliberately coarse interval. Queries
	// used by input, Blueprint, and automation should still observe the scan's
	// authoritative result immediately after asynchronous generation finishes.
	const USRRunMilestoneSubsystem* MilestoneSubsystem = GetWorld()
		? GetWorld()->GetSubsystem<USRRunMilestoneSubsystem>()
		: nullptr;
	if (IsValid(MilestoneSubsystem)
		&& MilestoneSubsystem->IsInitialSystemScanRecommendationActive())
	{
		const FSRSystemScanSnapshot Scan =
			MilestoneSubsystem->GetInitialSystemScanSnapshot();
		if (const FSRSystemScanCandidate* Candidate = Scan.GetRecommendedCandidate())
		{
			return Candidate->BodyActor == CelestialBodyActor;
		}
	}

	return CelestialBodyActor == RecommendedSystemScanBody;
}

bool USRCelestialBodyOverviewWidget::IsBodyStrategicBottleneck(
	const AActor* CelestialBodyActor) const
{
	const FSRStrategicBodyPresentation* Body =
		StrategicPresentation.FindBody(CelestialBodyActor);
	return Body && Body->bHasBottleneck;
}

AActor* USRCelestialBodyOverviewWidget::GetRecommendedStrategicBody() const
{
	return StrategicPresentation.RecommendedBodyActor.Get();
}

FText USRCelestialBodyOverviewWidget::GetStrategicSummaryLabel() const
{
	return StrategicPresentation.SummaryLabel;
}

bool USRCelestialBodyOverviewWidget::IsStrategyOverlayVisible() const
{
	return bShowStrategyOverlay;
}

bool USRCelestialBodyOverviewWidget::FocusRecommendedStrategicBody()
{
	AActor* RecommendedBody = StrategicPresentation.RecommendedBodyActor.Get();
	if (!IsValid(RecommendedBody))
	{
		return false;
	}
	DispatchEntryClicked(RecommendedBody);
	return true;
}

void USRCelestialBodyOverviewWidget::DispatchEntryClicked(
	AActor* CelestialBodyActor)
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: Overview DispatchEntryClicked Actor=%s"),
		*GetNameSafe(CelestialBodyActor));

	if (IsValid(CelestialBodyActor))
	{
		CelestialBodyRequestedEvent.Broadcast(CelestialBodyActor);
	}
}

FSRStarRoversCelestialBodyRequestedSignature&
USRCelestialBodyOverviewWidget::OnCelestialBodyRequested()
{
	return CelestialBodyRequestedEvent;
}

bool USRCelestialBodyOverviewWidget::IsScreenPositionOverOverviewUI(const FVector2D& ScreenPosition) const
{
	if (!IsVisible())
	{
		return false;
	}

	if (OverviewBorder && OverviewBorder->GetCachedGeometry().IsUnderLocation(ScreenPosition))
	{
		return true;
	}

	for (const UButton* NameplateButton : NameplateButtons)
	{
		if (IsValid(NameplateButton)
			&& NameplateButton->IsVisible()
			&& NameplateButton->GetCachedGeometry().IsUnderLocation(ScreenPosition))
		{
			return true;
		}
	}

	return false;
}
