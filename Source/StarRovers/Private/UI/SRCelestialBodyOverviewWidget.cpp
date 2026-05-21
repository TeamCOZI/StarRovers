#include "UI/SRCelestialBodyOverviewWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Celestial/SRCelestialBody.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PrimitiveComponent.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Fonts/SlateFontInfo.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"
#include "Styling/SlateColor.h"

namespace
{
	static const FName OverviewGravityLineTag(TEXT("StarRovers.GravityLine"));
	static const FName OverviewGravityLineRootTag(TEXT("StarRovers.GravityLineRoot"));
	static const FName OverviewGravityLineSegmentTag(TEXT("StarRovers.GravityLineSegment"));
	static const FName OverviewOrbitLineTag(TEXT("StarRovers.OrbitLine"));
	static const FName OverviewOrbitLineRootTag(TEXT("StarRovers.OrbitLineRoot"));

	int32 GetCategorySortRank(const AActor* CelestialBodyActor)
	{
		const ASRCelestialBody* ProceduralBody = Cast<ASRCelestialBody>(CelestialBodyActor);
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

	bool IsNameplateVisualComponent(const UPrimitiveComponent* PrimitiveComponent)
	{
		return IsValid(PrimitiveComponent)
			&& PrimitiveComponent->IsVisible()
			&& !PrimitiveComponent->ComponentHasTag(OverviewGravityLineTag)
			&& !PrimitiveComponent->ComponentHasTag(OverviewGravityLineRootTag)
			&& !PrimitiveComponent->ComponentHasTag(OverviewGravityLineSegmentTag)
			&& !PrimitiveComponent->ComponentHasTag(OverviewOrbitLineTag)
			&& !PrimitiveComponent->ComponentHasTag(OverviewOrbitLineRootTag);
	}

	bool ResolveNameplateVisualBounds(const AActor* CelestialBodyActor, FVector& OutCenter, float& OutRadius)
	{
		if (!IsValid(CelestialBodyActor))
		{
			return false;
		}

		if (Cast<ASRCelestialBody>(CelestialBodyActor))
		{
			OutCenter = CelestialBodyActor->GetActorLocation();
			OutRadius = USRCelestialBodyRuntimeLibrary::GetCelestialApproximateRadius(CelestialBodyActor);
			return OutRadius > KINDA_SMALL_NUMBER;
		}

		const UPrimitiveComponent* BestComponent = nullptr;
		float BestRadius = 0.0f;

		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(CelestialBodyActor);
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
}

void USRCelestialBodyOverviewEntryAction::Initialize(USRCelestialBodyOverviewWidget* InOwnerWidget, AActor* InCelestialBodyActor)
{
	OwnerWidget = InOwnerWidget;
	CelestialBodyActor = InCelestialBodyActor;
}

void USRCelestialBodyOverviewEntryAction::HandleClicked()
{
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
	RebuildStarSystemScrollBox();
	RebuildNameplateButtons();
}

void USRCelestialBodyOverviewWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	BuildOverviewWidgetTree();
	RebuildStarSystemScrollBox();
	RebuildNameplateButtons();
}

void USRCelestialBodyOverviewWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshNameplateButtonLayout();
}

int32 USRCelestialBodyOverviewWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	if (!bShowNameplateButtons)
	{
		return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}

	const int32 NameplateButtonLayerId = LayerId;
	for (const FSRNameplateButtonLayout& Layout : NameplateButtonLayouts)
	{
		if (!Layout.bIsVisible)
		{
			continue;
		}

		const FLinearColor NameplateOutlineColor = Layout.CelestialBodyActor.Get() == SelectedActor
			? FLinearColor(0.9f, 1.0f, 1.0f, 0.95f)
			: FLinearColor(0.72f, 0.86f, 0.9f, 0.9f);

		TArray<FVector2D> NameplateLeaderPoints;
		NameplateLeaderPoints.Add(Layout.LeaderStart);
		NameplateLeaderPoints.Add(Layout.LeaderEnd);
		NameplateLeaderPoints.Add(Layout.LabelPosition);
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			NameplateButtonLayerId,
			AllottedGeometry.ToPaintGeometry(),
			NameplateLeaderPoints,
			ESlateDrawEffect::None,
			NameplateOutlineColor,
			true,
			NameplateOutlineLineThickness);
	}

	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, NameplateButtonLayerId + 1, InWidgetStyle, bParentEnabled);
}

void USRCelestialBodyOverviewWidget::SetCelestialBodies(const TArray<AActor*>& NewCelestialBodies)
{
	CelestialBodies.Reset();
	for (AActor* CelestialBodyActor : NewCelestialBodies)
	{
		if (IsValid(CelestialBodyActor) && USRCelestialBodyRuntimeLibrary::IsCelestialBodyActor(CelestialBodyActor))
		{
			CelestialBodies.AddUnique(CelestialBodyActor);
		}
	}

	SortStarSystemBodies(CelestialBodies);
	RebuildStarSystemScrollBox();
	RebuildNameplateButtons();
}

void USRCelestialBodyOverviewWidget::SetSelectedActor(AActor* NewSelectedActor)
{
	if (SelectedActor == NewSelectedActor)
	{
		return;
	}

	SelectedActor = NewSelectedActor;
	RebuildStarSystemScrollBox();
	RebuildNameplateButtons();
}

void USRCelestialBodyOverviewWidget::DispatchEntryClicked(AActor* CelestialBodyActor)
{
	if (IsValid(CelestialBodyActor))
	{
		CelestialBodyRequestedEvent.Broadcast(CelestialBodyActor);
	}
}

FSRStarRoversCelestialBodyRequestedSignature& USRCelestialBodyOverviewWidget::OnCelestialBodyRequested()
{
	return CelestialBodyRequestedEvent;
}

void USRCelestialBodyOverviewWidget::HandleNameplateToggleClicked()
{
	bShowNameplateButtons = !bShowNameplateButtons;
	if (NameplateToggleButtonTextBlock)
	{
		NameplateToggleButtonTextBlock->SetText(bShowNameplateButtons
			? NSLOCTEXT("StarRoversOverview", "NameplateButtonsOn", "Nameplates: On")
			: NSLOCTEXT("StarRoversOverview", "NameplateButtonsOff", "Nameplates: Off"));
	}

	RebuildNameplateButtons();
}

void USRCelestialBodyOverviewWidget::BuildOverviewWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}

	if (WidgetTree->RootWidget)
	{
		OverviewCanvasPanel = Cast<UCanvasPanel>(WidgetTree->FindWidget(FName(TEXT("OverviewCanvasPanel"))));
		OverviewBorder = Cast<UBorder>(WidgetTree->FindWidget(FName(TEXT("OverviewBorder"))));
		OverviewVerticalBox = Cast<UVerticalBox>(WidgetTree->FindWidget(FName(TEXT("OverviewVerticalBox"))));
		StarSystemTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("StarSystemTextBlock"))));
		StarSystemScrollBox = Cast<UScrollBox>(WidgetTree->FindWidget(FName(TEXT("StarSystemScrollBox"))));
		NameplateToggleButton = Cast<UButton>(WidgetTree->FindWidget(FName(TEXT("NameplateToggleButton"))));
		NameplateToggleButtonTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("NameplateToggleButtonTextBlock"))));
		return;
	}

	OverviewCanvasPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("OverviewCanvasPanel"));
	WidgetTree->RootWidget = OverviewCanvasPanel;

	OverviewBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("OverviewBorder"));
	OverviewBorder->SetPadding(FMargin(12.0f));
	OverviewBorder->SetBrushColor(OverviewBorderColor);

	if (UCanvasPanelSlot* CanvasSlot = OverviewCanvasPanel->AddChildToCanvas(OverviewBorder))
	{
		CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
		CanvasSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		CanvasSlot->SetPosition(FVector2D(16.0f, 76.0f));
		CanvasSlot->SetSize(FVector2D(280.0f, 420.0f));
	}

	OverviewVerticalBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("OverviewVerticalBox"));
	OverviewBorder->SetContent(OverviewVerticalBox);

	StarSystemTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StarSystemTextBlock"));
	StarSystemTextBlock->SetText(StarSystemText);
	StarSystemTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	FSlateFontInfo StarSystemFont = StarSystemTextBlock->GetFont();
	StarSystemFont.Size = 18;
	StarSystemTextBlock->SetFont(StarSystemFont);

	if (UVerticalBoxSlot* StarSystemTextBlockSlot = OverviewVerticalBox->AddChildToVerticalBox(StarSystemTextBlock))
	{
		StarSystemTextBlockSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	}

	NameplateToggleButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("NameplateToggleButton"));
	NameplateToggleButton->SetBackgroundColor(FLinearColor(0.12f, 0.16f, 0.20f, 0.94f));
	NameplateToggleButtonTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NameplateToggleButtonTextBlock"));
	NameplateToggleButtonTextBlock->SetText(NSLOCTEXT("StarRoversOverview", "NameplateButtonsOn", "Nameplates: On"));
	NameplateToggleButtonTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	NameplateToggleButtonTextBlock->SetJustification(ETextJustify::Center);
	NameplateToggleButton->AddChild(NameplateToggleButtonTextBlock);
	NameplateToggleButton->OnClicked.AddDynamic(this, &USRCelestialBodyOverviewWidget::HandleNameplateToggleClicked);

	if (UVerticalBoxSlot* NameplateToggleButtonSlot = OverviewVerticalBox->AddChildToVerticalBox(NameplateToggleButton))
	{
		NameplateToggleButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	}

	StarSystemScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("StarSystemScrollBox"));
	if (UVerticalBoxSlot* StarSystemScrollBoxSlot = OverviewVerticalBox->AddChildToVerticalBox(StarSystemScrollBox))
	{
		StarSystemScrollBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
}

void USRCelestialBodyOverviewWidget::RebuildStarSystemScrollBox()
{
	if (!WidgetTree || !StarSystemScrollBox)
	{
		return;
	}

	StarSystemScrollBox->ClearChildren();
	EntryActions.Reset();

	TSet<AActor*> CelestialBodySet;
	for (AActor* CelestialBodyActor : CelestialBodies)
	{
		if (IsValid(CelestialBodyActor))
		{
			CelestialBodySet.Add(CelestialBodyActor);
		}
	}

	TArray<TObjectPtr<AActor>> RootStarSystemBodies;
	TMap<AActor*, TArray<AActor*>> ChildrenByParent;
	for (AActor* CelestialBodyActor : CelestialBodies)
	{
		if (!IsValid(CelestialBodyActor))
		{
			continue;
		}

		AActor* ParentBody = nullptr;
		if (USRCelestialBodyRuntimeLibrary::GetCelestialParentBody(CelestialBodyActor, ParentBody) && CelestialBodySet.Contains(ParentBody))
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
		TArray<TObjectPtr<AActor>> SortedChildren;
		for (AActor* ChildBody : Pair.Value)
		{
			SortedChildren.Add(ChildBody);
		}
		SortStarSystemBodies(SortedChildren);
		Pair.Value.Reset();
		for (AActor* ChildBody : SortedChildren)
		{
			Pair.Value.Add(ChildBody);
		}
	}

	for (AActor* RootStarSystemBody : RootStarSystemBodies)
	{
		AddStarSystemScrollBoxButton(RootStarSystemBody, 0, ChildrenByParent);
	}
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

		UButton* NameplateButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		NameplateButton->SetBackgroundColor(CelestialBodyActor == SelectedActor ? SelectedNameplateButtonColor : NameplateButtonColor);

		UTextBlock* NameplateTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		NameplateTextBlock->SetText(FText::Format(
			FTextFormat(NSLOCTEXT("StarRoversOverview", "NameplateButtonTextFormat", "{0} {1}")),
			GetStarSystemNameplatePrefixText(CelestialBodyActor),
			GetStarSystemNameplateText(CelestialBodyActor)));
		NameplateTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		NameplateTextBlock->SetJustification(ETextJustify::Center);
		NameplateButton->AddChild(NameplateTextBlock);

		USRCelestialBodyOverviewEntryAction* EntryAction = NewObject<USRCelestialBodyOverviewEntryAction>(this);
		EntryAction->Initialize(this, CelestialBodyActor);
		EntryActions.Add(EntryAction);
		NameplateButton->OnClicked.AddDynamic(EntryAction, &USRCelestialBodyOverviewEntryAction::HandleClicked);

		if (UCanvasPanelSlot* NameplateSlot = OverviewCanvasPanel->AddChildToCanvas(NameplateButton))
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

	if (!bShowNameplateButtons || NameplateActors.Num() != NameplateButtons.Num())
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
		const bool bHasLayout = BuildNameplateButtonLayoutForActor(CelestialBodyActor, Index, Layout);
		NameplateButtonLayouts.Add(Layout);
		NameplateButton->SetVisibility((bHasLayout && Layout.bIsVisible) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

		if (bHasLayout && Layout.bIsVisible)
		{
			if (UCanvasPanelSlot* NameplateSlot = Cast<UCanvasPanelSlot>(NameplateButton->Slot))
			{
				NameplateSlot->SetAlignment(Layout.LabelAlignment);
				NameplateSlot->SetPosition(Layout.LabelPosition);
			}
		}
	}
}

bool USRCelestialBodyOverviewWidget::BuildNameplateButtonLayoutForActor(
	AActor* CelestialBodyActor,
	int32 NameplateButtonIndex,
	FSRNameplateButtonLayout& OutLayout) const
{
	OutLayout = FSRNameplateButtonLayout();
	OutLayout.CelestialBodyActor = CelestialBodyActor;

	APlayerController* PlayerController = GetOwningPlayer();
	if (!IsValid(PlayerController) || !IsValid(PlayerController->PlayerCameraManager) || !IsValid(CelestialBodyActor))
	{
		return false;
	}

	FVector CelestialBodyLocation = CelestialBodyActor->GetActorLocation();
	float CelestialBodyRadius = USRCelestialBodyRuntimeLibrary::GetCelestialApproximateRadius(CelestialBodyActor);
	ResolveNameplateVisualBounds(CelestialBodyActor, CelestialBodyLocation, CelestialBodyRadius);

	FVector2D CenterScreenPosition;
	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PlayerController, CelestialBodyLocation, CenterScreenPosition, true))
	{
		return false;
	}

	const FVector CameraLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
	const FVector CameraForward = PlayerController->PlayerCameraManager->GetCameraRotation().Vector().GetSafeNormal();
	if (FVector::DotProduct(CelestialBodyLocation - CameraLocation, CameraForward) <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FRotator CameraRotation = PlayerController->PlayerCameraManager->GetCameraRotation();
	const FVector CameraRight = CameraRotation.RotateVector(FVector::RightVector).GetSafeNormal();
	const FVector CameraUp = CameraRotation.RotateVector(FVector::UpVector).GetSafeNormal();

	FVector2D RadiusRightScreenPosition = CenterScreenPosition;
	FVector2D RadiusUpScreenPosition = CenterScreenPosition;
	UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PlayerController, CelestialBodyLocation + CameraRight * CelestialBodyRadius, RadiusRightScreenPosition, true);
	UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PlayerController, CelestialBodyLocation + CameraUp * CelestialBodyRadius, RadiusUpScreenPosition, true);

	const float ProjectedRadius = FMath::Max(
		FVector2D::Distance(CenterScreenPosition, RadiusRightScreenPosition),
		FVector2D::Distance(CenterScreenPosition, RadiusUpScreenPosition));
	const float DesiredOutlineRadius = ProjectedRadius + FMath::Max(0.0f, NameplateOutlinePaddingPixels);
	if (DesiredOutlineRadius > NameplateOutlineMaxRadiusPixels)
	{
		return false;
	}

	const float OutlineRadius = FMath::Max(DesiredOutlineRadius, NameplateOutlineMinRadiusPixels);

	static const FVector2D LabelDirections[] =
	{
		FVector2D(1.0f, -0.62f),
		FVector2D(-1.0f, -0.62f),
		FVector2D(1.0f, 0.72f),
		FVector2D(-1.0f, 0.72f)
	};
	const FVector2D LabelDirection = LabelDirections[NameplateButtonIndex % UE_ARRAY_COUNT(LabelDirections)].GetSafeNormal();
	const FVector2D LeaderStart = CenterScreenPosition + LabelDirection * OutlineRadius;
	const FVector2D LeaderEnd = CenterScreenPosition + LabelDirection * (OutlineRadius + NameplateLeaderLengthPixels * 0.55f);
	const FVector2D LabelPosition = CenterScreenPosition + LabelDirection * (OutlineRadius + NameplateLeaderLengthPixels);
	const FVector2D LabelAlignment = LabelDirection.Y > 0.0f
		? FVector2D(0.5f, 0.0f)
		: FVector2D(0.5f, 1.0f);

	const bool bOverlapsOverviewPanel = LabelPosition.X < 350.0f && LabelPosition.Y > 72.0f;
	OutLayout.Center = CenterScreenPosition;
	OutLayout.OutlineRadius = OutlineRadius;
	OutLayout.LeaderStart = LeaderStart;
	OutLayout.LeaderEnd = LeaderEnd;
	OutLayout.LabelPosition = LabelPosition;
	OutLayout.LabelAlignment = LabelAlignment;
	OutLayout.bIsVisible = !bOverlapsOverviewPanel;
	return true;
}

void USRCelestialBodyOverviewWidget::AddStarSystemScrollBoxButton(
	AActor* CelestialBodyActor,
	int32 Depth,
	const TMap<AActor*, TArray<AActor*>>& ChildrenByParent)
{
	if (!WidgetTree || !StarSystemScrollBox || !IsValid(CelestialBodyActor))
	{
		return;
	}

	UButton* StarSystemScrollBoxButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	StarSystemScrollBoxButton->SetBackgroundColor(CelestialBodyActor == SelectedActor ? SelectedStarSystemScrollBoxButtonColor : StarSystemScrollBoxButtonColor);

	UHorizontalBox* StarSystemHorizontalBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	StarSystemScrollBoxButton->AddChild(StarSystemHorizontalBox);

	UTextBlock* StarSystemNameplatePrefixTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	StarSystemNameplatePrefixTextBlock->SetText(GetStarSystemNameplatePrefixText(CelestialBodyActor));
	StarSystemNameplatePrefixTextBlock->SetColorAndOpacity(FSlateColor(StarSystemNameplateTextColor));
	StarSystemNameplatePrefixTextBlock->SetJustification(ETextJustify::Center);
	if (UHorizontalBoxSlot* PrefixSlot = StarSystemHorizontalBox->AddChildToHorizontalBox(StarSystemNameplatePrefixTextBlock))
	{
		PrefixSlot->SetPadding(FMargin(Depth * StarSystemNameplateIndentPixels, 0.0f, 8.0f, 0.0f));
		PrefixSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}

	UTextBlock* StarSystemNameplateTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	StarSystemNameplateTextBlock->SetText(GetStarSystemNameplateText(CelestialBodyActor));
	StarSystemNameplateTextBlock->SetColorAndOpacity(FSlateColor(StarSystemNameplateTextColor));
	StarSystemNameplateTextBlock->SetAutoWrapText(false);
	if (UHorizontalBoxSlot* NameSlot = StarSystemHorizontalBox->AddChildToHorizontalBox(StarSystemNameplateTextBlock))
	{
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	USRCelestialBodyOverviewEntryAction* EntryAction = NewObject<USRCelestialBodyOverviewEntryAction>(this);
	EntryAction->Initialize(this, CelestialBodyActor);
	EntryActions.Add(EntryAction);
	StarSystemScrollBoxButton->OnClicked.AddDynamic(EntryAction, &USRCelestialBodyOverviewEntryAction::HandleClicked);

	if (UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(StarSystemScrollBox->AddChild(StarSystemScrollBoxButton)))
	{
		RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));
	}

	if (const TArray<AActor*>* Children = ChildrenByParent.Find(CelestialBodyActor))
	{
		for (AActor* ChildBody : *Children)
		{
			AddStarSystemScrollBoxButton(ChildBody, Depth + 1, ChildrenByParent);
		}
	}
}

FText USRCelestialBodyOverviewWidget::GetStarSystemNameplateText(AActor* CelestialBodyActor) const
{
	if (const ASRCelestialBody* ProceduralBody = Cast<ASRCelestialBody>(CelestialBodyActor))
	{
		const FSRCelestialBodySpec BodySpec = ProceduralBody->GetSpec();
		if (!BodySpec.DisplayName.IsEmpty())
		{
			return BodySpec.DisplayName;
		}
	}

	return IsValid(CelestialBodyActor) ? FText::FromString(CelestialBodyActor->GetName()) : FText::GetEmpty();
}

FText USRCelestialBodyOverviewWidget::GetStarSystemNameplatePrefixText(AActor* CelestialBodyActor) const
{
	const ASRCelestialBody* ProceduralBody = Cast<ASRCelestialBody>(CelestialBodyActor);
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

void USRCelestialBodyOverviewWidget::SortStarSystemBodies(TArray<TObjectPtr<AActor>>& StarSystemBodiesToSort) const
{
	StarSystemBodiesToSort.Sort([this](const TObjectPtr<AActor>& Left, const TObjectPtr<AActor>& Right)
	{
		const int32 LeftRank = GetCategorySortRank(Left.Get());
		const int32 RightRank = GetCategorySortRank(Right.Get());
		if (LeftRank != RightRank)
		{
			return LeftRank < RightRank;
		}

		return GetStarSystemNameplateText(Left.Get()).ToString() < GetStarSystemNameplateText(Right.Get()).ToString();
	});
}
