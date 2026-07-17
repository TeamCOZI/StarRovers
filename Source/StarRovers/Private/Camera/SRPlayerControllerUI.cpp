#include "Camera/SRPlayerController.h"

#include "Utility/SRLog.h"
#include "Assembly/SRAssemblyComponent.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "SRPlayerControllerStructureBuildSelectionState.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "Structure/SRStructureDataAsset.h"
#include "UI/SRAugmentChoiceWidget.h"
#include "UI/SRCelestialBodyFocusInfoWidget.h"
#include "UI/SRCelestialBodyOverviewWidget.h"
#include "UI/SRFacilityControlWidget.h"
#include "UI/SRFocusedHubShortcutWidget.h"
#include "UI/SRGameOverWidget.h"
#include "UI/SRStructureSelectionWidget.h"
#include "UI/SRTimeControlWidget.h"

namespace
{
	void AppendStructureDataAssets(
		const TArray<TObjectPtr<USRStructureDataAsset>>& SourceAssets,
		TArray<USRStructureDataAsset*>& OutStructureDataAssets)
	{
		for (USRStructureDataAsset* StructureDataAsset : SourceAssets)
		{
			if (IsValid(StructureDataAsset))
			{
				OutStructureDataAssets.Add(StructureDataAsset);
			}
		}
	}

	void AppendStructureDataAssets(
		const FSRAvailableStructureDataAssetOperationCategory& SourceCategory,
		TArray<USRStructureDataAsset*>& OutStructureDataAssets)
	{
		AppendStructureDataAssets(SourceCategory.Processor, OutStructureDataAssets);
		AppendStructureDataAssets(SourceCategory.Synthesizer, OutStructureDataAssets);
		AppendStructureDataAssets(SourceCategory.Miner, OutStructureDataAssets);
		AppendStructureDataAssets(SourceCategory.Conveyor, OutStructureDataAssets);
		AppendStructureDataAssets(SourceCategory.Hub, OutStructureDataAssets);
	}

	void AppendStructureDataAssets(
		const FSRAvailableStructureDataAssetCategories& SourceCategories,
		TArray<USRStructureDataAsset*>& OutStructureDataAssets)
	{
		AppendStructureDataAssets(SourceCategories.Starting, OutStructureDataAssets);
		AppendStructureDataAssets(SourceCategories.Basic, OutStructureDataAssets);
		AppendStructureDataAssets(SourceCategories.Advance, OutStructureDataAssets);
		AppendStructureDataAssets(SourceCategories.Expert, OutStructureDataAssets);
		AppendStructureDataAssets(SourceCategories.Innovation, OutStructureDataAssets);
	}
}

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

USRAugmentChoiceWidget* ASRPlayerController::GetAugmentChoiceWidget() const
{
	return AugmentChoiceWidget;
}

USRFacilityControlWidget* ASRPlayerController::GetFacilityControlWidget() const
{
	return FacilityControlWidget;
}

USRFocusedHubShortcutWidget* ASRPlayerController::GetFocusedHubShortcutWidget() const
{
	return FocusedHubShortcutWidget;
}

USRGameOverWidget* ASRPlayerController::GetGameOverWidget() const
{
	return GameOverWidget;
}

bool ASRPlayerController::IsPointerOverFacilityControlWidget() const
{
	return IsValid(FacilityControlWidget) && FacilityControlWidget->IsPointerOverControlPanel();
}

bool ASRPlayerController::IsPointerOverBlockingUI() const
{
	return (IsValid(FacilityControlWidget) && FacilityControlWidget->IsPointerOverControlPanel())
		|| (IsValid(FocusedHubShortcutWidget) && FocusedHubShortcutWidget->IsPointerOverHubShortcutUI())
		|| (IsValid(FocusInfoWidget) && FocusInfoWidget->IsPointerOverFocusInfoUI())
		|| (IsValid(OverviewWidget) && OverviewWidget->IsPointerOverOverviewUI())
		|| (IsValid(TimeControlWidget) && TimeControlWidget->IsPointerOverTimeControlPanel())
		|| (IsValid(AugmentChoiceWidget) && AugmentChoiceWidget->IsVisible())
		|| (IsValid(StructureSelectionWidget) && StructureSelectionWidget->IsPointerOverStructureSelectionPanel())
		|| (IsValid(GameOverWidget) && GameOverWidget->IsVisible());
}

int32 ASRPlayerController::ResolveWidgetLayerZOrder(ESRPlayerUILayer WidgetLayer) const
{
	return StarRovers::PlayerControllerUI::ResolveWidgetLayerZOrder(WidgetLayerOrder, WidgetLayer);
}

void ASRPlayerController::ClearFacilityFocus()
{
	if (AssemblyComponent)
	{
		AssemblyComponent->ClearSelectedStructureFocus();
		return;
	}

	SetSelectedSurfaceStructureInfo(false, FSRFocusedSurfaceStructureInfo());
}

void ASRPlayerController::CreateFocusInfoWidget()
{
	if (!IsLocalController() || FocusInfoWidget)
	{
		return;
	}
	if (!FocusInfoWidgetClass)
	{
		SR_LOG(Camera, LogTemp, Error, TEXT("ASRPlayerController requires FocusInfoWidgetClass to create the focus widget."));
		return;
	}

	FocusInfoWidget = CreateWidget<USRCelestialBodyFocusInfoWidget>(this, FocusInfoWidgetClass);
	if (!FocusInfoWidget)
	{
		SR_LOG(Camera, LogTemp, Error, TEXT("ASRPlayerController failed to create FocusInfoWidget from '%s'."), *GetNameSafe(FocusInfoWidgetClass));
		return;
	}

	FocusInfoWidget->AddToViewport(ResolveWidgetLayerZOrder(ESRPlayerUILayer::FocusInfo));
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
		RefreshFacilityControlWidget();
		return;
	}

	FocusInfoWidget->ClearFocusInfo();
	FocusInfoWidget->SetVisibility(ESlateVisibility::Collapsed);
	RefreshFacilityControlWidget();
}

void ASRPlayerController::CreateOverviewWidget()
{
	if (!IsLocalController() || OverviewWidget)
	{
		return;
	}
	if (!OverviewWidgetClass)
	{
		SR_LOG(Camera, LogTemp, Error, TEXT("ASRPlayerController requires OverviewWidgetClass to create the overview widget."));
		return;
	}

	OverviewWidget = CreateWidget<USRCelestialBodyOverviewWidget>(this, OverviewWidgetClass);
	if (!OverviewWidget)
	{
		SR_LOG(Camera, LogTemp, Error, TEXT("ASRPlayerController failed to create OverviewWidget from '%s'."), *GetNameSafe(OverviewWidgetClass));
		return;
	}

	OverviewWidget->AddToViewport(ResolveWidgetLayerZOrder(ESRPlayerUILayer::Overview));
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
	RuntimeState.bPendingInitialPrimaryStarFocus = false;
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
		SR_LOG(Camera, LogTemp, Error, TEXT("ASRPlayerController requires TimeControlWidgetClass to create the time control widget."));
		return;
	}

	TimeControlWidget = CreateWidget<USRTimeControlWidget>(this, TimeControlWidgetClass);
	if (!TimeControlWidget)
	{
		SR_LOG(Camera, LogTemp, Error, TEXT("ASRPlayerController failed to create TimeControlWidget from '%s'."), *GetNameSafe(TimeControlWidgetClass));
		return;
	}

	TimeControlWidget->AddToViewport(ResolveWidgetLayerZOrder(ESRPlayerUILayer::TimeControl));
	TimeControlWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void ASRPlayerController::CreateAugmentChoiceWidget()
{
	if (!IsLocalController() || AugmentChoiceWidget)
	{
		return;
	}

	TSubclassOf<USRAugmentChoiceWidget> WidgetClass = AugmentChoiceWidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = USRAugmentChoiceWidget::StaticClass();
	}

	AugmentChoiceWidget = CreateWidget<USRAugmentChoiceWidget>(this, WidgetClass);
	if (!AugmentChoiceWidget)
	{
		SR_LOG(Camera, LogTemp, Error, TEXT("ASRPlayerController failed to create AugmentChoiceWidget from '%s'."), *GetNameSafe(WidgetClass));
		return;
	}

	AugmentChoiceWidget->AddToViewport(ResolveWidgetLayerZOrder(ESRPlayerUILayer::AugmentChoice));
	AugmentChoiceWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void ASRPlayerController::BindAugmentSubsystem()
{
	USRAugmentSubsystem* AugmentSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USRAugmentSubsystem>() : nullptr;
	if (!AugmentSubsystem)
	{
		return;
	}

	AugmentSubsystem->OnAugmentChoicesReady.RemoveDynamic(this, &ASRPlayerController::HandleAugmentChoicesReady);
	AugmentSubsystem->OnAugmentChoicesReady.AddDynamic(this, &ASRPlayerController::HandleAugmentChoicesReady);
	AugmentSubsystem->OnAugmentChoiceSelected.RemoveDynamic(this, &ASRPlayerController::HandleAugmentChoiceSelected);
	AugmentSubsystem->OnAugmentChoiceSelected.AddDynamic(this, &ASRPlayerController::HandleAugmentChoiceSelected);
	AugmentSubsystem->OnUnlockedStructuresChanged.RemoveDynamic(this, &ASRPlayerController::HandleUnlockedStructuresChanged);
	AugmentSubsystem->OnUnlockedStructuresChanged.AddDynamic(this, &ASRPlayerController::HandleUnlockedStructuresChanged);
}

void ASRPlayerController::RegisterAvailableStructuresForAugments()
{
	USRAugmentSubsystem* AugmentSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USRAugmentSubsystem>() : nullptr;
	if (!AugmentSubsystem)
	{
		return;
	}

	TArray<USRStructureDataAsset*> StructureDataAssets;
	AppendStructureDataAssets(AvailableStructureDataAssets, StructureDataAssets);

	AugmentSubsystem->RegisterStructureDataAssets(StructureDataAssets);
}

void ASRPlayerController::HandleAugmentChoicesReady(const TArray<FSRAugmentChoice>& Choices, int32 CycleIndex)
{
	TArray<FSRAugmentChoice> ResolvedChoices = Choices;
	int32 ResolvedCycleIndex = CycleIndex;
	if (ResolvedChoices.IsEmpty() || ResolvedCycleIndex <= 0)
	{
		if (const USRAugmentSubsystem* AugmentSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USRAugmentSubsystem>() : nullptr)
		{
			ResolvedChoices = AugmentSubsystem->GetCurrentAugmentChoices();
			ResolvedCycleIndex = AugmentSubsystem->GetCurrentAugmentChoiceCycleIndex();
		}
	}

	if (ResolvedChoices.IsEmpty())
	{
		SR_LOG(Camera, LogTemp, Warning, TEXT("ASRPlayerController received an augment choice event without choices for cycle %d."), CycleIndex);
		return;
	}

	CreateAugmentChoiceWidget();
	if (!AugmentChoiceWidget)
	{
		return;
	}

	SR_LOG(Camera, LogTemp, Log, TEXT("ASRPlayerController showing %d augment choices for cycle %d."), ResolvedChoices.Num(), ResolvedCycleIndex);
	AugmentChoiceWidget->SetAugmentChoices(ResolvedChoices, ResolvedCycleIndex);
	AugmentChoiceWidget->SetVisibility(ESlateVisibility::Visible);
}

void ASRPlayerController::HandleAugmentChoiceSelected(const FSRAugmentChoice& Choice)
{
	if (AugmentChoiceWidget)
	{
		AugmentChoiceWidget->ClearAugmentChoices();
		AugmentChoiceWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	RefreshStructureSelectionWidget();
}

void ASRPlayerController::HandleUnlockedStructuresChanged()
{
	RefreshStructureSelectionWidget();
}

void ASRPlayerController::CreateStructureSelectionWidget()
{
	if (!IsLocalController() || StructureSelectionWidget)
	{
		return;
	}

	if (!StructureSelectionWidgetClass)
	{
		SR_LOG(Camera, LogTemp, Error, TEXT("ASRPlayerController requires StructureSelectionWidgetClass to create the structure selection widget."));
		return;
	}

	StructureSelectionWidget = CreateWidget<USRStructureSelectionWidget>(this, StructureSelectionWidgetClass);
	if (!StructureSelectionWidget)
	{
		SR_LOG(Camera, LogTemp, Error, TEXT("ASRPlayerController failed to create StructureSelectionWidget from '%s'."), *GetNameSafe(StructureSelectionWidgetClass));
		return;
	}

	StructureSelectionWidget->AddToViewport(ResolveWidgetLayerZOrder(ESRPlayerUILayer::StructureSelection));
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
		FSRPlayerControllerStructureBuildSelectionState::ResetSelection(
			SelectedStructureBuildId,
			bHasSelectedStructureBuildId,
			SelectedStructureDataAsset,
			SelectedActor);
	}
	StructureSelectionWidget->SetVisibility(bShowStructureSelection ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (!bShowStructureSelection)
	{
		FSRPlayerControllerStructureBuildSelectionState::ResetSelection(
			SelectedStructureBuildId,
			bHasSelectedStructureBuildId,
			SelectedStructureDataAsset,
			SelectedActor);
		StructureSelectionWidget->ClearSelectedStructureId();
	}
	else if (bHasSelectedStructureBuildId)
	{
		FSRPlayerControllerStructureBuildSelectionState::SyncSelectionFromWidget(
			SelectedStructureBuildId,
			bHasSelectedStructureBuildId,
			SelectedStructureDataAsset,
			SelectedActor,
			StructureSelectionWidget);
	}
}

void ASRPlayerController::CreateFacilityControlWidget()
{
	if (!IsLocalController() || FacilityControlWidget)
	{
		return;
	}

	if (!FacilityControlWidgetClass)
	{
		SR_LOG(Camera, LogTemp, Error, TEXT("ASRPlayerController requires FacilityControlWidgetClass to create the facility control widget."));
		return;
	}

	FacilityControlWidget = CreateWidget<USRFacilityControlWidget>(this, FacilityControlWidgetClass);
	if (!FacilityControlWidget)
	{
		SR_LOG(Camera, LogTemp, Error, TEXT("ASRPlayerController failed to create FacilityControlWidget from '%s'."), *GetNameSafe(FacilityControlWidgetClass));
		return;
	}

	FacilityControlWidget->AddToViewport(ResolveWidgetLayerZOrder(ESRPlayerUILayer::FacilityControl));
	FacilityControlWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void ASRPlayerController::CreateFocusedHubShortcutWidget()
{
	if (!IsLocalController() || FocusedHubShortcutWidget)
	{
		return;
	}

	TSubclassOf<USRFocusedHubShortcutWidget> WidgetClass = FocusedHubShortcutWidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = USRFocusedHubShortcutWidget::StaticClass();
	}

	FocusedHubShortcutWidget = CreateWidget<USRFocusedHubShortcutWidget>(this, WidgetClass);
	if (!FocusedHubShortcutWidget)
	{
		SR_LOG(Camera, LogTemp, Error, TEXT("ASRPlayerController failed to create FocusedHubShortcutWidget from '%s'."), *GetNameSafe(WidgetClass));
		return;
	}

	FocusedHubShortcutWidget->AddToViewport(ResolveWidgetLayerZOrder(ESRPlayerUILayer::HubShortcut));
	FocusedHubShortcutWidget->OnHubShortcutRequested().AddUObject(this, &ASRPlayerController::HandleFocusedHubShortcutRequested);
	FocusedHubShortcutWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void ASRPlayerController::RefreshFacilityControlWidget()
{
	if (!FacilityControlWidget)
	{
		return;
	}

	const FSRFocusedSurfaceStructureInfo& StructureInfo = SelectedActorFocusInfo.SelectedSurfaceStructureInfo;
	if (SelectedActorFocusInfo.bIsValid
		&& SelectedActorFocusInfo.bHasSelectedSurfaceStructure
		&& StructureInfo.bIsValid
		&& StructureInfo.bHasFacilityRuntimeInfo
		&& StructureInfo.FacilityRuntimeInfo.bIsValid
		&& !StructureInfo.OccupantId.IsNone())
	{
		FacilityControlWidget->SetFocusedFacility(SelectedActor, StructureInfo.OccupantId);
		FacilityControlWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		return;
	}

	FacilityControlWidget->ClearFocusedFacility();
	FacilityControlWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void ASRPlayerController::CreateGameOverWidget()
{
	if (!IsLocalController() || GameOverWidget)
	{
		return;
	}

	TSubclassOf<USRGameOverWidget> WidgetClass = GameOverWidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = USRGameOverWidget::StaticClass();
	}

	GameOverWidget = CreateWidget<USRGameOverWidget>(this, WidgetClass);
	if (!GameOverWidget)
	{
		SR_LOG(Camera, LogTemp, Error, TEXT("ASRPlayerController failed to create GameOverWidget from '%s'."), *GetNameSafe(WidgetClass));
		return;
	}

	GameOverWidget->AddToViewport(ResolveWidgetLayerZOrder(ESRPlayerUILayer::GameOver));
	GameOverWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void ASRPlayerController::ShowGameOverScreen(ASRStar* Star)
{
	CreateGameOverWidget();
	if (!GameOverWidget)
	{
		return;
	}

	GameOverWidget->SetGameOverStar(Star);
	GameOverWidget->SetVisibility(ESlateVisibility::Visible);
	bShowMouseCursor = true;
}

void ASRPlayerController::GetAvailableStructureDataAssets(TArray<USRStructureDataAsset*>& OutStructureDataAssets) const
{
	OutStructureDataAssets.Reset();
	TArray<USRStructureDataAsset*> ConfiguredStructureDataAssets;
	AppendStructureDataAssets(AvailableStructureDataAssets, ConfiguredStructureDataAssets);

	OutStructureDataAssets.Reserve(ConfiguredStructureDataAssets.Num());
	const USRAugmentSubsystem* AugmentSubsystem = GetWorld() ? GetWorld()->GetSubsystem<USRAugmentSubsystem>() : nullptr;
	for (USRStructureDataAsset* StructureDataAsset : ConfiguredStructureDataAssets)
	{
		if (IsValid(StructureDataAsset)
			&& (!AugmentSubsystem || AugmentSubsystem->IsStructureUnlocked(StructureDataAsset)))
		{
			OutStructureDataAssets.Add(StructureDataAsset);
		}
	}
}
