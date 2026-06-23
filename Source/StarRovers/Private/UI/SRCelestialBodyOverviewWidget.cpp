#include "UI/SRCelestialBodyOverviewWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Framework/Application/SlateApplication.h"

void USRCelestialBodyOverviewEntryAction::Initialize(
	USRCelestialBodyOverviewWidget* InOwnerWidget,
	AActor* InCelestialBodyActor)
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

void USRCelestialBodyOverviewWidget::NativeTick(const FGeometry& MyGeometry,
												float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshNameplateButtonLayout();
}

FReply USRCelestialBodyOverviewWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (IsScreenPositionOverOverviewUi(InMouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply USRCelestialBodyOverviewWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (IsScreenPositionOverOverviewUi(InMouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply USRCelestialBodyOverviewWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (IsScreenPositionOverOverviewUi(InMouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Handled();
	}

	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

void USRCelestialBodyOverviewWidget::SetCelestialBodies(
	const TArray<AActor*>& NewCelestialBodies)
{
	CelestialBodies.Reset();
	for (AActor* CelestialBodyActor : NewCelestialBodies)
	{
		if (IsValid(CelestialBodyActor) &&
			USRCelestialBodyRuntimeLibrary::IsCelestialBodyActor(
				CelestialBodyActor))
		{
			CelestialBodies.AddUnique(CelestialBodyActor);
		}
	}

	SortStarSystemBodies(CelestialBodies);
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

bool USRCelestialBodyOverviewWidget::IsPointerOverOverviewUi() const
{
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}

	return IsScreenPositionOverOverviewUi(FSlateApplication::Get().GetCursorPos());
}

void USRCelestialBodyOverviewWidget::DispatchEntryClicked(
	AActor* CelestialBodyActor)
{
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

bool USRCelestialBodyOverviewWidget::IsScreenPositionOverOverviewUi(const FVector2D& ScreenPosition) const
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
