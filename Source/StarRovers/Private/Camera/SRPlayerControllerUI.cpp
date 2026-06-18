#include "Camera/SRPlayerController.h"

#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "Structure/SRStructureDataAsset.h"
#include "UI/SRCelestialBodyFocusInfoWidget.h"
#include "UI/SRCelestialBodyOverviewWidget.h"
#include "UI/SRStructureSelectionWidget.h"
#include "UI/SRTimeControlWidget.h"
USRCelestialBodyFocusInfoWidget* ASRPlayerController::GetFocusInfoWidget() const
{
	return FocusInfoWidget;
}

USRCelestialBodyOverviewWidget* ASRPlayerController::GetOverviewWidget() const
{
	return OverviewWidget;
}

USRTimeControlWidget* ASRPlayerController::GetTimeControlWidget() const
{
	return TimeControlWidget;
}

USRStructureSelectionWidget* ASRPlayerController::GetStructureSelectionWidget() const
{
	return StructureSelectionWidget;
}

void ASRPlayerController::CreateFocusInfoWidget()
{
	if (!IsLocalController() || FocusInfoWidget)
	{
		return;
	}
	if (!FocusInfoWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("ASRPlayerController requires FocusInfoWidgetClass to create the focus widget."));
		return;
	}

	FocusInfoWidget = CreateWidget<USRCelestialBodyFocusInfoWidget>(this, FocusInfoWidgetClass);
	if (!FocusInfoWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("ASRPlayerController failed to create FocusInfoWidget from '%s'."), *GetNameSafe(FocusInfoWidgetClass));
		return;
	}

	FocusInfoWidget->AddToViewport(FocusInfoWidgetZOrder);
	FocusInfoWidget->OnAssemblyModeRequested().AddUObject(this, &ASRPlayerController::ToggleAssemblyMode);
	FocusInfoWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void ASRPlayerController::RefreshFocusInfoWidget()
{
	SelectedActorFocusInfo = USRCelestialBodyRuntimeLibrary::BuildCelestialBodyFocusInfo(SelectedActor);

	if (!FocusInfoWidget)
	{
		return;
	}

	if (SelectedActorFocusInfo.bIsValid)
	{
		FocusInfoWidget->SetFocusInfo(SelectedActorFocusInfo);
		FocusInfoWidget->SetAssemblyModeActive(IsAssemblyModeActive());
		FocusInfoWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		return;
	}

	FocusInfoWidget->ClearFocusInfo();
	FocusInfoWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void ASRPlayerController::CreateOverviewWidget()
{
	if (!IsLocalController() || OverviewWidget)
	{
		return;
	}
	if (!OverviewWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("ASRPlayerController requires OverviewWidgetClass to create the overview widget."));
		return;
	}

	OverviewWidget = CreateWidget<USRCelestialBodyOverviewWidget>(this, OverviewWidgetClass);
	if (!OverviewWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("ASRPlayerController failed to create OverviewWidget from '%s'."), *GetNameSafe(OverviewWidgetClass));
		return;
	}

	OverviewWidget->AddToViewport(OverviewWidgetZOrder);
	OverviewWidget->OnCelestialBodyRequested().AddUObject(this, &ASRPlayerController::HandleOverviewCelestialBodyRequested);
	OverviewWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void ASRPlayerController::RefreshOverviewWidget()
{
	if (!OverviewWidget)
	{
		return;
	}

	TArray<AActor*> CelestialBodies;
	if (USRCelestialBodyRegistrySubsystem* CelestialBodyRegistry = GetCelestialBodyRegistry())
	{
		CelestialBodyRegistry->GetCelestialBodies(CelestialBodies);
		if (CelestialBodies.Num() == 0)
		{
			CelestialBodyRegistry->RefreshCelestialBodies();
			CelestialBodyRegistry->GetCelestialBodies(CelestialBodies);
		}
	}

	OverviewWidget->SetCelestialBodies(CelestialBodies);
	OverviewWidget->SetSelectedActor(SelectedActor);
}

void ASRPlayerController::HandleOverviewCelestialBodyRequested(AActor* RequestedActor)
{
	bPendingInitialPrimaryStarFocus = false;
	RequestFocusActor(RequestedActor);
}

void ASRPlayerController::CreateTimeControlWidget()
{
	if (!IsLocalController() || TimeControlWidget)
	{
		return;
	}
	if (!TimeControlWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("ASRPlayerController requires TimeControlWidgetClass to create the time control widget."));
		return;
	}

	TimeControlWidget = CreateWidget<USRTimeControlWidget>(this, TimeControlWidgetClass);
	if (!TimeControlWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("ASRPlayerController failed to create TimeControlWidget from '%s'."), *GetNameSafe(TimeControlWidgetClass));
		return;
	}

	TimeControlWidget->AddToViewport(TimeControlWidgetZOrder);
}

void ASRPlayerController::CreateStructureSelectionWidget()
{
	if (!IsLocalController() || StructureSelectionWidget)
	{
		return;
	}

	if (!StructureSelectionWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("ASRPlayerController requires StructureSelectionWidgetClass to create the structure selection widget."));
		return;
	}

	StructureSelectionWidget = CreateWidget<USRStructureSelectionWidget>(this, StructureSelectionWidgetClass);
	if (!StructureSelectionWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("ASRPlayerController failed to create StructureSelectionWidget from '%s'."), *GetNameSafe(StructureSelectionWidgetClass));
		return;
	}

	StructureSelectionWidget->AddToViewport(StructureSelectionWidgetZOrder);
	StructureSelectionWidget->OnBuildOptionSelected().AddUObject(this, &ASRPlayerController::HandleStructureBuildOptionSelected);
	TArray<USRStructureDataAsset*> StructureDataAssets;
	GetAvailableStructureDataAssets(StructureDataAssets);
	StructureSelectionWidget->SetBuildOptionsFromDataAssets(StructureDataAssets);
	StructureSelectionWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void ASRPlayerController::RefreshStructureSelectionWidget()
{
	if (!StructureSelectionWidget)
	{
		return;
	}

	const bool bShowStructureSelection = IsAssemblyModeActive();
	TArray<USRStructureDataAsset*> StructureDataAssets;
	GetAvailableStructureDataAssets(StructureDataAssets);
	StructureSelectionWidget->SetBuildOptionsFromDataAssets(StructureDataAssets);
	if (bHasSelectedStructureBuildId && !StructureSelectionWidget->HasSelectedStructureId())
	{
		SelectedStructureBuildId = NAME_None;
		bHasSelectedStructureBuildId = false;
		SelectedStructureDataAsset = nullptr;
	}
	StructureSelectionWidget->SetVisibility(bShowStructureSelection ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (!bShowStructureSelection)
	{
		SelectedStructureBuildId = NAME_None;
		bHasSelectedStructureBuildId = false;
		SelectedStructureDataAsset = nullptr;
		StructureSelectionWidget->ClearSelectedStructureId();
	}
	else if (bHasSelectedStructureBuildId)
	{
		StructureSelectionWidget->SetSelectedStructureId(SelectedStructureBuildId);
		SelectedStructureDataAsset = StructureSelectionWidget->GetSelectedStructureDataAsset();
	}
}

void ASRPlayerController::GetAvailableStructureDataAssets(TArray<USRStructureDataAsset*>& OutStructureDataAssets) const
{
	OutStructureDataAssets.Reset();
	OutStructureDataAssets.Reserve(AvailableStructureDataAssets.Num());
	for (USRStructureDataAsset* StructureDataAsset : AvailableStructureDataAssets)
	{
		if (IsValid(StructureDataAsset))
		{
			OutStructureDataAssets.Add(StructureDataAsset);
		}
	}
}
