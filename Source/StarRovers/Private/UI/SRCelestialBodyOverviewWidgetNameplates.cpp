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
} // namespace

int32 USRCelestialBodyOverviewWidget::NativePaint(
	const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	if (!bShowNameplateButtons)
	{
		return Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
								  OutDrawElements, LayerId, InWidgetStyle,
								  bParentEnabled);
	}

	const int32 NameplateButtonLayerId = LayerId;
	for (const FSRNameplateButtonLayout& Layout : NameplateButtonLayouts)
	{
		if (!Layout.bIsVisible)
		{
			continue;
		}

		const FLinearColor NameplateOutlineColor =
			Layout.CelestialBodyActor.Get() == SelectedActor
				? FLinearColor(0.9f, 1.0f, 1.0f, 0.95f)
				: FLinearColor(0.72f, 0.86f, 0.9f, 0.9f);

		TArray<FVector2D> NameplateLeaderPoints;
		NameplateLeaderPoints.Add(Layout.LeaderStart);
		NameplateLeaderPoints.Add(Layout.LeaderEnd);
		NameplateLeaderPoints.Add(Layout.LabelPosition);
		FSlateDrawElement::MakeLines(OutDrawElements, NameplateButtonLayerId,
									 AllottedGeometry.ToPaintGeometry(),
									 NameplateLeaderPoints, ESlateDrawEffect::None,
									 NameplateOutlineColor, true,
									 NameplateOutlineLineThickness);
	}

	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
							  OutDrawElements, NameplateButtonLayerId + 1,
							  InWidgetStyle, bParentEnabled);
}

void USRCelestialBodyOverviewWidget::HandleNameplateToggleClicked()
{
	bShowNameplateButtons = !bShowNameplateButtons;
	if (NameplateToggleButtonTextBlock)
	{
		NameplateToggleButtonTextBlock->SetText(
			bShowNameplateButtons
				? NSLOCTEXT("StarRoversOverview", "NameplateButtonsOn",
							"Nameplates: On")
				: NSLOCTEXT("StarRoversOverview", "NameplateButtonsOff",
							"Nameplates: Off"));
	}

	RebuildNameplateButtons();
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
	NameplateButtonLayouts.Reset();

	if (!bShowNameplateButtons)
	{
		return;
	}

	for (AActor* CelestialBodyActor : CelestialBodies)
	{
		if (!IsValid(CelestialBodyActor))
		{
			continue;
		}

		UButton* NameplateButton =
			WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		NameplateButton->SetBackgroundColor(CelestialBodyActor == SelectedActor
												? SelectedNameplateButtonColor
												: NameplateButtonColor);

		UTextBlock* NameplateTextBlock =
			WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		NameplateTextBlock->SetText(FText::Format(
			FTextFormat(NSLOCTEXT("StarRoversOverview", "NameplateButtonTextFormat",
								  "{0} {1}")),
			GetStarSystemNameplatePrefixText(CelestialBodyActor),
			GetStarSystemNameplateText(CelestialBodyActor)));
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
	}

	RefreshNameplateButtonLayout();
}

void USRCelestialBodyOverviewWidget::RefreshNameplateButtonLayout()
{
	NameplateButtonLayouts.Reset();

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
		LabelPosition.X < 350.0f && LabelPosition.Y > 72.0f;
	OutLayout.Center = CenterScreenPosition;
	OutLayout.OutlineRadius = OutlineRadius;
	OutLayout.LeaderStart = LeaderStart;
	OutLayout.LeaderEnd = LeaderEnd;
	OutLayout.LabelPosition = LabelPosition;
	OutLayout.LabelAlignment = LabelAlignment;
	OutLayout.bIsVisible = !bOverlapsOverviewPanel;
	return true;
}
