#include "UI/SRCelestialBodyOverviewWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"

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
