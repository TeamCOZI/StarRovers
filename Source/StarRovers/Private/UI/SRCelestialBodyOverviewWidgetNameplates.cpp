#include "UI/SRCelestialBodyOverviewWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/PlayerCameraManager.h"
#include "Celestial/SRCelestialBody.h"
#include "Celestial/SRDynamicMeshBaseDataAsset.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PrimitiveComponent.h"
#include "Components/TextBlock.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"
#include "Styling/SlateColor.h"
#include "UI/SRUITheme.h"

namespace
{
	static const FName OverviewGravityLineTag(TEXT("StarRovers.GravityLine"));
	static const FName
		OverviewGravityLineRootTag(TEXT("StarRovers.GravityLineRoot"));
	static const FName
		OverviewGravityLineSegmentTag(TEXT("StarRovers.GravityLineSegment"));
	static const FName OverviewOrbitLineTag(TEXT("StarRovers.OrbitLine"));
	static const FName OverviewOrbitLineRootTag(TEXT("StarRovers.OrbitLineRoot"));
	static const FName
		OverviewRotationAxisLineTag(TEXT("StarRovers.RotationAxisLine"));
	static const FName
		OverviewRotationAxisLineRootTag(TEXT("StarRovers.RotationAxisLineRoot"));

	bool IsNameplateVisualComponent(const UPrimitiveComponent* PrimitiveComponent)
	{
		return IsValid(PrimitiveComponent) && PrimitiveComponent->IsVisible() &&
			   !PrimitiveComponent->ComponentHasTag(OverviewGravityLineTag) &&
			   !PrimitiveComponent->ComponentHasTag(OverviewGravityLineRootTag) &&
			   !PrimitiveComponent->ComponentHasTag(OverviewGravityLineSegmentTag) &&
			   !PrimitiveComponent->ComponentHasTag(OverviewOrbitLineTag) &&
			   !PrimitiveComponent->ComponentHasTag(OverviewOrbitLineRootTag) &&
			   !PrimitiveComponent->ComponentHasTag(OverviewRotationAxisLineTag) &&
			   !PrimitiveComponent->ComponentHasTag(OverviewRotationAxisLineRootTag);
	}

	bool ResolveNameplateVisualBounds(const AActor* CelestialBodyActor,
									  FVector& OutCenter, float& OutRadius)
	{
		if (!IsValid(CelestialBodyActor))
		{
			return false;
		}

		if (const ASRCelestialBody* ProceduralBody =
				Cast<ASRCelestialBody>(CelestialBodyActor))
		{
			const FSRCelestialBodyData BodyData = ProceduralBody->GetData();
			OutCenter = CelestialBodyActor->GetActorLocation();
			if (IsValid(BodyData.StaticMesh.Get()))
			{
				OutRadius = BodyData.StaticMesh->GetBounds().SphereRadius *
							FMath::Max(0.0f, BodyData.Scale);
			}
			else
			{
				OutRadius = IsValid(BodyData.DynamicMeshBaseDataAsset.Get())
								? BodyData.DynamicMeshBaseDataAsset->GetSafeBaseRadius() *
									  FMath::Max(0.0f, BodyData.Scale)
								: 0.0f;
			}
			return OutRadius > KINDA_SMALL_NUMBER;
		}

		const UPrimitiveComponent* BestComponent = nullptr;
		float BestRadius = 0.0f;

		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(
			CelestialBodyActor);
		CelestialBodyActor->GetComponents(PrimitiveComponents);
		for (const UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (!IsNameplateVisualComponent(PrimitiveComponent))
			{
				continue;
			}

			if (PrimitiveComponent->Bounds.SphereRadius > BestRadius)
			{
				BestComponent = PrimitiveComponent;
				BestRadius = PrimitiveComponent->Bounds.SphereRadius;
			}
		}

		if (!BestComponent || BestRadius <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		OutCenter = BestComponent->Bounds.Origin;
		OutRadius = BestRadius;
		return true;
	}

	int32 GetStrategicRoutePaintRank(ESRUIVisualState VisualState)
	{
		switch (VisualState)
		{
		case ESRUIVisualState::Danger:
			return 4;
		case ESRUIVisualState::Warning:
			return 3;
		case ESRUIVisualState::Positive:
		case ESRUIVisualState::Info:
		case ESRUIVisualState::Selected:
			return 2;
		case ESRUIVisualState::Neutral:
			return 1;
		case ESRUIVisualState::Disabled:
		case ESRUIVisualState::Locked:
		default:
			return 0;
		}
	}
} // namespace

int32 USRCelestialBodyOverviewWidget::NativePaint(
	const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	int32 PaintLayerId = LayerId;
	if (bShowStrategyOverlay)
	{
		for (const FSRStrategicRouteLineLayout& Layout : StrategicRouteLineLayouts)
		{
			if (!Layout.bIsVisible)
			{
				continue;
			}
			FLinearColor RouteColor = USRUIThemeLibrary::ResolveStatePalette(
				Layout.VisualState).AccentColor;
			if (!Layout.bEnabled)
			{
				RouteColor.A *= 0.45f;
			}
			const float Thickness = Layout.VisualState == ESRUIVisualState::Danger
				? 4.0f
				: Layout.VisualState == ESRUIVisualState::Warning
					? 3.0f
					: 1.5f;
			TArray<FVector2D> RoutePoints = {Layout.Start, Layout.End};
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				PaintLayerId,
				AllottedGeometry.ToPaintGeometry(),
				RoutePoints,
				ESlateDrawEffect::None,
				RouteColor,
				true,
				Thickness);

			const FVector2D Direction = (Layout.End - Layout.Start).GetSafeNormal();
			const FVector2D Perpendicular(-Direction.Y, Direction.X);
			const FVector2D ArrowTip = Layout.Start
				+ (Layout.End - Layout.Start) * 0.62f;
			const FVector2D ArrowBase = ArrowTip - Direction * 10.0f;
			TArray<FVector2D> ArrowPoints = {
				ArrowBase + Perpendicular * 5.0f,
				ArrowTip,
				ArrowBase - Perpendicular * 5.0f};
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				PaintLayerId,
				AllottedGeometry.ToPaintGeometry(),
				ArrowPoints,
				ESlateDrawEffect::None,
				RouteColor,
				true,
				Thickness);
		}
		++PaintLayerId;
	}

	if (bShowNameplateButtons)
	{
		for (const FSRNameplateButtonLayout& Layout : NameplateButtonLayouts)
		{
			if (!Layout.bIsVisible)
			{
				continue;
			}

			const FSRStrategicBodyPresentation* StrategyBody =
				StrategicPresentation.FindBody(Layout.CelestialBodyActor.Get());
			const FLinearColor NameplateOutlineColor =
				Layout.CelestialBodyActor.Get() == RecommendedSystemScanBody
				? RecommendedSystemScanColor
				: Layout.CelestialBodyActor.Get() == SelectedActor
					? FLinearColor(0.9f, 1.0f, 1.0f, 0.95f)
					: bShowStrategyOverlay && StrategyBody && StrategyBody->bHasBottleneck
						? USRUIThemeLibrary::ResolveStatePalette(
							StrategyBody->VisualState).AccentColor
						: FLinearColor(0.72f, 0.86f, 0.9f, 0.9f);

			TArray<FVector2D> NameplateLeaderPoints;
			NameplateLeaderPoints.Add(Layout.LeaderStart);
			NameplateLeaderPoints.Add(Layout.LeaderEnd);
			NameplateLeaderPoints.Add(Layout.LabelPosition);
			FSlateDrawElement::MakeLines(OutDrawElements, PaintLayerId,
										 AllottedGeometry.ToPaintGeometry(),
										 NameplateLeaderPoints, ESlateDrawEffect::None,
										 NameplateOutlineColor, true,
										 NameplateOutlineLineThickness);
		}
		++PaintLayerId;
	}

	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
							  OutDrawElements, PaintLayerId,
							  InWidgetStyle, bParentEnabled);
}

void USRCelestialBodyOverviewWidget::HandleNameplateToggleClicked()
{
	bShowNameplateButtons = !bShowNameplateButtons;
	if (NameplateToggleButtonTextBlock)
	{
		NameplateToggleButtonTextBlock->SetText(
			bShowNameplateButtons
				? NSLOCTEXT("StarRoversOverview", "NameplateButtonsOnCompact", "NAMES ON")
				: NSLOCTEXT("StarRoversOverview", "NameplateButtonsOffCompact", "NAMES OFF"));
	}

	RebuildNameplateButtons();
}

void USRCelestialBodyOverviewWidget::HandleStrategyOverlayToggleClicked()
{
	bShowStrategyOverlay = !bShowStrategyOverlay;
	RefreshStrategicHeader();
	RefreshNameplateStrategicVisuals();
	RefreshNameplateButtonLayout();
	InvalidateLayoutAndVolatility();
}

void USRCelestialBodyOverviewWidget::HandleStrategicFocusClicked()
{
	FocusRecommendedStrategicBody();
}

void USRCelestialBodyOverviewWidget::RebuildNameplateButtons()
{
	if (!WidgetTree || !OverviewCanvasPanel)
	{
		return;
	}

	for (UButton* NameplateButton : NameplateButtons)
	{
		if (IsValid(NameplateButton))
		{
			NameplateButton->RemoveFromParent();
		}
	}
	NameplateButtons.Reset();
	NameplateActors.Reset();
	NameplateTextBlocks.Reset();
	NameplateButtonLayouts.Reset();

	if (!bShowNameplateButtons)
	{
		RefreshStrategicRouteLineLayouts();
		return;
	}

	for (int32 BodyIndex = 0; BodyIndex < CelestialBodies.Num(); ++BodyIndex)
	{
		AActor* CelestialBodyActor = CelestialBodies[BodyIndex];
		if (!IsValid(CelestialBodyActor))
		{
			continue;
		}

		UButton* NameplateButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(),
			FName(*FString::Printf(TEXT("CelestialNameplateButton%d"), BodyIndex + 1)));

		UTextBlock* NameplateTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			FName(*FString::Printf(TEXT("CelestialNameplateTextBlock%d"), BodyIndex + 1)));
		NameplateTextBlock->SetText(GetWorldNameplateText(CelestialBodyActor));
		NameplateTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		NameplateTextBlock->SetJustification(ETextJustify::Center);
		NameplateButton->AddChild(NameplateTextBlock);

		USRCelestialBodyOverviewEntryAction* EntryAction =
			NewObject<USRCelestialBodyOverviewEntryAction>(this);
		EntryAction->Initialize(this, CelestialBodyActor);
		EntryActions.Add(EntryAction);
		NameplateButton->OnClicked.AddDynamic(
			EntryAction, &USRCelestialBodyOverviewEntryAction::HandleClicked);

		if (UCanvasPanelSlot* NameplateSlot =
				OverviewCanvasPanel->AddChildToCanvas(NameplateButton))
		{
			NameplateSlot->SetAutoSize(true);
			NameplateSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		}

		NameplateActors.Add(CelestialBodyActor);
		NameplateButtons.Add(NameplateButton);
		NameplateTextBlocks.Add(NameplateTextBlock);
	}

	RefreshNameplateStrategicVisuals();
	RefreshNameplateButtonLayout();
}

void USRCelestialBodyOverviewWidget::RefreshNameplateButtonLayout()
{
	RefreshStrategicRouteLineLayouts();
	NameplateButtonLayouts.Reset();
	NameplateButtonLayouts.Reserve(NameplateActors.Num());

	if (!bShowNameplateButtons ||
		NameplateActors.Num() != NameplateButtons.Num())
	{
		for (UButton* NameplateButton : NameplateButtons)
		{
			if (IsValid(NameplateButton))
			{
				NameplateButton->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		return;
	}

	for (int32 Index = 0; Index < NameplateActors.Num(); ++Index)
	{
		AActor* CelestialBodyActor = NameplateActors[Index];
		UButton* NameplateButton = NameplateButtons[Index];
		if (!IsValid(CelestialBodyActor) || !IsValid(NameplateButton))
		{
			continue;
		}

		FSRNameplateButtonLayout Layout;
		const bool bHasLayout =
			BuildNameplateButtonLayoutForActor(CelestialBodyActor, Index, Layout);
		NameplateButtonLayouts.Add(Layout);
		NameplateButton->SetVisibility((bHasLayout && Layout.bIsVisible)
										   ? ESlateVisibility::Visible
										   : ESlateVisibility::Collapsed);

		if (bHasLayout && Layout.bIsVisible)
		{
			if (UCanvasPanelSlot* NameplateSlot =
					Cast<UCanvasPanelSlot>(NameplateButton->Slot))
			{
				NameplateSlot->SetAlignment(Layout.LabelAlignment);
				NameplateSlot->SetPosition(Layout.LabelPosition);
			}
		}
	}
}

void USRCelestialBodyOverviewWidget::RefreshStrategicRouteLineLayouts()
{
	StrategicRouteLineLayouts.Reset();
	if (!bShowStrategyOverlay)
	{
		return;
	}

	APlayerController* PlayerController = GetOwningPlayer();
	if (!IsValid(PlayerController) || !IsValid(PlayerController->PlayerCameraManager))
	{
		return;
	}
	const FVector CameraLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
	const FVector CameraForward = PlayerController->PlayerCameraManager->GetCameraRotation()
		.Vector().GetSafeNormal();

	StrategicRouteLineLayouts.Reserve(StrategicPresentation.Routes.Num());
	for (const FSRStrategicRoutePresentation& Route : StrategicPresentation.Routes)
	{
		AActor* SourceBody = Route.SourceBodyActor.Get();
		AActor* DestinationBody = Route.DestinationBodyActor.Get();
		if (!IsValid(SourceBody) || !IsValid(DestinationBody)
			|| SourceBody == DestinationBody)
		{
			continue;
		}

		const FVector SourceLocation = SourceBody->GetActorLocation();
		const FVector DestinationLocation = DestinationBody->GetActorLocation();
		if (FVector::DotProduct(SourceLocation - CameraLocation, CameraForward)
				<= KINDA_SMALL_NUMBER
			|| FVector::DotProduct(DestinationLocation - CameraLocation, CameraForward)
				<= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		FVector2D Start;
		FVector2D End;
		if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
				PlayerController, SourceLocation, Start, true)
			|| !UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
				PlayerController, DestinationLocation, End, true))
		{
			continue;
		}
		const FVector2D Direction = (End - Start).GetSafeNormal();
		const float ScreenLength = FVector2D::Distance(Start, End);
		if (ScreenLength <= 8.0f)
		{
			continue;
		}
		const float EndpointInset = FMath::Min(18.0f, ScreenLength * 0.18f);

		FSRStrategicRouteLineLayout& Layout =
			StrategicRouteLineLayouts.AddDefaulted_GetRef();
		Layout.RouteId = Route.RouteId;
		Layout.SourceBodyActor = SourceBody;
		Layout.DestinationBodyActor = DestinationBody;
		Layout.Start = Start + Direction * EndpointInset;
		Layout.End = End - Direction * EndpointInset;
		Layout.VisualState = Route.VisualState;
		Layout.bEnabled = Route.bEnabled;
		Layout.bIsVisible = true;
	}

	StrategicRouteLineLayouts.StableSort([](
		const FSRStrategicRouteLineLayout& Left,
		const FSRStrategicRouteLineLayout& Right)
	{
		const int32 LeftRank = GetStrategicRoutePaintRank(Left.VisualState);
		const int32 RightRank = GetStrategicRoutePaintRank(Right.VisualState);
		return LeftRank != RightRank
			? LeftRank < RightRank
			: Left.RouteId.LexicalLess(Right.RouteId);
	});
}

bool USRCelestialBodyOverviewWidget::BuildNameplateButtonLayoutForActor(
	AActor* CelestialBodyActor, int32 NameplateButtonIndex,
	FSRNameplateButtonLayout& OutLayout) const
{
	OutLayout = FSRNameplateButtonLayout();
	OutLayout.CelestialBodyActor = CelestialBodyActor;

	APlayerController* PlayerController = GetOwningPlayer();
	if (!IsValid(PlayerController) ||
		!IsValid(PlayerController->PlayerCameraManager) ||
		!IsValid(CelestialBodyActor))
	{
		return false;
	}

	FVector CelestialBodyLocation = CelestialBodyActor->GetActorLocation();
	float CelestialBodyRadius = 0.0f;
	ResolveNameplateVisualBounds(CelestialBodyActor, CelestialBodyLocation,
								 CelestialBodyRadius);

	FVector2D CenterScreenPosition;
	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
			PlayerController, CelestialBodyLocation, CenterScreenPosition,
			true))
	{
		return false;
	}

	const FVector CameraLocation =
		PlayerController->PlayerCameraManager->GetCameraLocation();
	const FVector CameraForward =
		PlayerController->PlayerCameraManager->GetCameraRotation()
			.Vector()
			.GetSafeNormal();
	if (FVector::DotProduct(CelestialBodyLocation - CameraLocation,
							CameraForward) <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FRotator CameraRotation =
		PlayerController->PlayerCameraManager->GetCameraRotation();
	const FVector CameraRight =
		CameraRotation.RotateVector(FVector::RightVector).GetSafeNormal();
	const FVector CameraUp =
		CameraRotation.RotateVector(FVector::UpVector).GetSafeNormal();

	FVector2D RadiusRightScreenPosition = CenterScreenPosition;
	FVector2D RadiusUpScreenPosition = CenterScreenPosition;
	UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
		PlayerController,
		CelestialBodyLocation + CameraRight * CelestialBodyRadius,
		RadiusRightScreenPosition, true);
	UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
		PlayerController, CelestialBodyLocation + CameraUp * CelestialBodyRadius,
		RadiusUpScreenPosition, true);

	const float ProjectedRadius = FMath::Max(
		FVector2D::Distance(CenterScreenPosition, RadiusRightScreenPosition),
		FVector2D::Distance(CenterScreenPosition, RadiusUpScreenPosition));
	const float DesiredOutlineRadius =
		ProjectedRadius + FMath::Max(0.0f, NameplateOutlinePaddingPixels);
	if (DesiredOutlineRadius > NameplateOutlineMaxRadiusPixels)
	{
		return false;
	}

	const float OutlineRadius =
		FMath::Max(DesiredOutlineRadius, NameplateOutlineMinRadiusPixels);

	static const FVector2D LabelDirections[] = {
		FVector2D(1.0f, -0.62f), FVector2D(-1.0f, -0.62f), FVector2D(1.0f, 0.72f),
		FVector2D(-1.0f, 0.72f)};
	const FVector2D LabelDirection =
		LabelDirections[NameplateButtonIndex % UE_ARRAY_COUNT(LabelDirections)]
			.GetSafeNormal();
	const FVector2D LeaderStart =
		CenterScreenPosition + LabelDirection * OutlineRadius;
	const FVector2D LeaderEnd =
		CenterScreenPosition +
		LabelDirection * (OutlineRadius + NameplateLeaderLengthPixels * 0.55f);
	const FVector2D LabelPosition =
		CenterScreenPosition +
		LabelDirection * (OutlineRadius + NameplateLeaderLengthPixels);
	const FVector2D LabelAlignment =
		LabelDirection.Y > 0.0f ? FVector2D(0.5f, 0.0f) : FVector2D(0.5f, 1.0f);

	const bool bOverlapsOverviewPanel =
		LabelPosition.X < 380.0f && LabelPosition.Y > 72.0f;
	OutLayout.Center = CenterScreenPosition;
	OutLayout.OutlineRadius = OutlineRadius;
	OutLayout.LeaderStart = LeaderStart;
	OutLayout.LeaderEnd = LeaderEnd;
	OutLayout.LabelPosition = LabelPosition;
	OutLayout.LabelAlignment = LabelAlignment;
	OutLayout.bIsVisible = !bOverlapsOverviewPanel;
	return true;
}
