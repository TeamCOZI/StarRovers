#include "Camera/SRPlayerController.h"

#include "Assembly/SRAssemblyComponent.h"
#include "Camera/SRCameraPawn.h"
#include "Celestial/SRCelestialBody.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "Structure/SRStructureDataAsset.h"
#include "Surface/SRPlanetSurfaceGrid.h"
#include "TimerManager.h"
#include "UI/SRCelestialBodyFocusInfoWidget.h"
#include "UI/SRCelestialBodyOverviewWidget.h"
#include "UI/SRStructureSelectionWidget.h"
#include "UI/SRTimeControlWidget.h"
#include "UObject/ConstructorHelpers.h"

namespace StarRoversControllerInputPaths
{
	static constexpr TCHAR LeftClickAction[] = TEXT("/Game/BlueprintClasses/Core/IA_LeftClick.IA_LeftClick");
	static constexpr TCHAR FocusParentAction[] = TEXT("/Game/BlueprintClasses/Core/IA_FocusParent.IA_FocusParent");
}

namespace
{
	constexpr float SpaceTraceDistanceMultiplier = 100.0f;
}

ASRPlayerController::ASRPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	DefaultMouseCursor = EMouseCursor::Default;

	static ConstructorHelpers::FObjectFinder<UInputAction> LeftClickActionFinder(StarRoversControllerInputPaths::LeftClickAction);
	if (LeftClickActionFinder.Succeeded())
	{
		LeftClickAction = LeftClickActionFinder.Object;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ASRPlayerController requires LeftClickAction at '%s'."), StarRoversControllerInputPaths::LeftClickAction);
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> FocusParentActionFinder(StarRoversControllerInputPaths::FocusParentAction);
	if (FocusParentActionFinder.Succeeded())
	{
		FocusParentAction = FocusParentActionFinder.Object;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ASRPlayerController requires FocusParentAction at '%s'."), StarRoversControllerInputPaths::FocusParentAction);
	}

	FocusInfoWidgetZOrder = 0;
	OverviewWidgetZOrder = 1;
	TimeControlWidgetZOrder = 2;
	StructureSelectionWidgetZOrder = 3;
	MaxStructurePlacementsPerFrame = 4;
	MaxQueuedStructurePlacements = 256;
	SelectedStructureBuildId = NAME_None;
	bHasSelectedStructureBuildId = false;
	SelectedStructureDataAsset = nullptr;
	bPendingInitialPrimaryStarFocus = true;

	AssemblyComponent = CreateDefaultSubobject<USRAssemblyComponent>(TEXT("AssemblyComponent"));
	AssemblyComponent->ConfigurePlacementPerformance(MaxStructurePlacementsPerFrame, MaxQueuedStructurePlacements);
}

void ASRPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (AssemblyComponent)
	{
		AssemblyComponent->ConfigurePlacementPerformance(MaxStructurePlacementsPerFrame, MaxQueuedStructurePlacements);
	}

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);

	UpdateHitResultTraceDistance();
	TryBindCameraPawnFocusEvents();
	TryBindCelestialBodyRegistryEvents();
	CreateFocusInfoWidget();
	RefreshFocusInfoWidget();
	CreateOverviewWidget();
	RefreshOverviewWidget();
	CreateTimeControlWidget();
	CreateStructureSelectionWidget();
	RefreshStructureSelectionWidget();
	TryAutoFocusPrimaryStar();
}

void ASRPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateHitResultTraceDistance();
	TryBindCameraPawnFocusEvents();
	TryBindCelestialBodyRegistryEvents();
}

void ASRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (LeftClickAction)
		{
			EnhancedInputComponent->BindAction(LeftClickAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleLeftClick);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ASRPlayerController requires LeftClickAction before input binding."));
		}

		if (FocusParentAction)
		{
			EnhancedInputComponent->BindAction(FocusParentAction, ETriggerEvent::Started, this, &ASRPlayerController::HandleFocusParent);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ASRPlayerController requires FocusParentAction before input binding."));
		}
	}
}

AActor* ASRPlayerController::GetSelectedActor() const
{
	return SelectedActor;
}

void ASRPlayerController::ClearSelection()
{
	UpdateSelection(nullptr);
}

USRCelestialBodyFocusInfoWidget* ASRPlayerController::GetFocusInfoWidget() const
{
	return FocusInfoWidget;
}

USRCelestialBodyOverviewWidget* ASRPlayerController::GetOverviewWidget() const
{
	return OverviewWidget;
}

bool ASRPlayerController::HasSelectedActorFocusInfo() const
{
	return SelectedActorFocusInfo.bIsValid;
}

FSRCelestialBodyFocusInfo ASRPlayerController::GetSelectedActorFocusInfo() const
{
	return SelectedActorFocusInfo;
}

void ASRPlayerController::SetHoveredSurfaceCellInfo(bool bHasHoveredSurfaceCell, const FSRPlanetSurfaceGridCellInfo& HoveredSurfaceCellInfo)
{
	if (!SelectedActorFocusInfo.bIsValid)
	{
		SelectedActorFocusInfo = USRCelestialBodyRuntimeLibrary::BuildCelestialBodyFocusInfo(SelectedActor);
	}

	if (!SelectedActorFocusInfo.bIsValid)
	{
		return;
	}

	SelectedActorFocusInfo.bHasHoveredSurfaceCell = bHasHoveredSurfaceCell;
	SelectedActorFocusInfo.HoveredSurfaceCellInfo = bHasHoveredSurfaceCell
		? HoveredSurfaceCellInfo
		: FSRPlanetSurfaceGridCellInfo();
	SelectedActorFocusInfo.HoveredSurfaceGridPatchCellIds.Reset();
	if (bHasHoveredSurfaceCell)
	{
		if (USRPlanetSurfaceGrid* SurfaceGrid = USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(SelectedActor))
		{
			SurfaceGrid->GetInteractionGridPatchCellIds(
				HoveredSurfaceCellInfo.CellId,
				SelectedActorFocusInfo.HoveredSurfaceGridPatchCellIds);
		}
	}

	if (FocusInfoWidget)
	{
		FocusInfoWidget->SetFocusInfo(SelectedActorFocusInfo);
		FocusInfoWidget->SetAssemblyModeActive(IsAssemblyModeActive());
	}
}

USRTimeControlWidget* ASRPlayerController::GetTimeControlWidget() const
{
	return TimeControlWidget;
}

USRStructureSelectionWidget* ASRPlayerController::GetStructureSelectionWidget() const
{
	return StructureSelectionWidget;
}

bool ASRPlayerController::IsAssemblyModeActive() const
{
	return AssemblyComponent && AssemblyComponent->IsAssemblyModeActive();
}

void ASRPlayerController::SetAssemblyModeActive(bool bNewAssemblyModeActive)
{
	if (AssemblyComponent)
	{
		AssemblyComponent->SetAssemblyModeActive(bNewAssemblyModeActive);
	}
	RefreshFocusInfoWidget();
	RefreshStructureSelectionWidget();
}

void ASRPlayerController::ToggleAssemblyMode()
{
	SetAssemblyModeActive(!IsAssemblyModeActive());
}

bool ASRPlayerController::HasSelectedStructureBuildId() const
{
	return bHasSelectedStructureBuildId;
}

FName ASRPlayerController::GetSelectedStructureBuildId() const
{
	return SelectedStructureBuildId;
}

USRStructureDataAsset* ASRPlayerController::GetSelectedStructureDataAsset() const
{
	return SelectedStructureDataAsset;
}

bool ASRPlayerController::ShouldHandleAssemblyPlacementDrag() const
{
	return AssemblyComponent && AssemblyComponent->ShouldHandleStructurePlacementDrag();
}

bool ASRPlayerController::BeginAssemblyPlacementDrag()
{
	AActor* AssemblySelectedActor = nullptr;
	if (!AssemblyComponent || !AssemblyComponent->BeginStructurePlacementDrag(AssemblySelectedActor))
	{
		return false;
	}

	UpdateSelection(AssemblySelectedActor);
	return true;
}

bool ASRPlayerController::ContinueAssemblyPlacementDrag()
{
	AActor* AssemblySelectedActor = nullptr;
	if (!AssemblyComponent || !AssemblyComponent->ContinueStructurePlacementDrag(AssemblySelectedActor))
	{
		return false;
	}

	if (IsValid(AssemblySelectedActor) && SelectedActor != AssemblySelectedActor)
	{
		UpdateSelection(AssemblySelectedActor);
	}
	return true;
}

void ASRPlayerController::EndAssemblyPlacementDrag()
{
	if (AssemblyComponent)
	{
		AssemblyComponent->EndStructurePlacementDrag();
	}
}

USRCelestialBodyRegistrySubsystem* ASRPlayerController::GetCelestialBodyRegistry() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<USRCelestialBodyRegistrySubsystem>() : nullptr;
}

void ASRPlayerController::HandleLeftClick()
{
	bPendingInitialPrimaryStarFocus = false;

	if (BeginAssemblyPlacementDrag())
	{
		return;
	}

	AActor* AssemblySelectedActor = nullptr;
	if (AssemblyComponent && AssemblyComponent->TryHandleAssemblyClick(AssemblySelectedActor))
	{
		UpdateSelection(AssemblySelectedActor);
		return;
	}

	FHitResult CursorHitResult;
	const bool bHasCursorHit = GetHitResultUnderCursor(ECC_Visibility, false, CursorHitResult);

	AActor* HitActor = bHasCursorHit ? CursorHitResult.GetActor() : nullptr;
	AActor* SelectedBody = USRCelestialBodyRuntimeLibrary::IsCelestialBodyActor(HitActor) ? HitActor : nullptr;
	if (!SelectedBody)
	{
		if (const ASRCameraPawn* CameraPawn = Cast<ASRCameraPawn>(GetPawn()))
		{
			AActor* CurrentFocusActor = CameraPawn->GetFocusedActor();
			if (IsValid(CurrentFocusActor)
				&& USRCelestialBodyRuntimeLibrary::IsCelestialBodyActor(CurrentFocusActor)
				&& !USRCelestialBodyRuntimeLibrary::IsCelestialStarActor(CurrentFocusActor))
			{
				return;
			}
		}
	}

	RequestFocusActor(SelectedBody);
}

void ASRPlayerController::HandleFocusParent()
{
	bPendingInitialPrimaryStarFocus = false;

	ASRCameraPawn* CameraPawn = Cast<ASRCameraPawn>(GetPawn());
	if (!CameraPawn)
	{
		return;
	}

	AActor* CurrentFocusActor = CameraPawn->GetFocusedActor();
	if (!IsValid(CurrentFocusActor))
	{
		return;
	}

	AActor* ParentBody = nullptr;
	if (!USRCelestialBodyRuntimeLibrary::GetCelestialParentBody(CurrentFocusActor, ParentBody) || !IsValid(ParentBody))
	{
		return;
	}

	if (AssemblyComponent)
	{
		AssemblyComponent->ClearSurfaceGridInteraction(CurrentFocusActor);
	}
	SetAssemblyModeActive(false);
	RequestFocusActor(ParentBody);
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

void ASRPlayerController::HandleStructureBuildOptionSelected(FName StructureId, USRStructureDataAsset* StructureDataAsset)
{
	if (StructureId.IsNone() || !IsValid(StructureDataAsset))
	{
		if (!StructureId.IsNone())
		{
			UE_LOG(LogTemp, Error, TEXT("ASRPlayerController received structure build option '%s' without a valid StructureDataAsset."), *StructureId.ToString());
		}
		SelectedStructureBuildId = NAME_None;
		bHasSelectedStructureBuildId = false;
		SelectedStructureDataAsset = nullptr;
		return;
	}

	SelectedStructureBuildId = StructureId;
	bHasSelectedStructureBuildId = true;
	SelectedStructureDataAsset = StructureDataAsset;
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

void ASRPlayerController::UpdateHitResultTraceDistance()
{
	if (const ASRCameraPawn* CameraPawn = Cast<ASRCameraPawn>(GetPawn()))
	{
		const float HitResultTraceDistanceForCamera = CameraPawn->GetMaxZoomDistance() * SpaceTraceDistanceMultiplier;
		if (FMath::IsFinite(HitResultTraceDistanceForCamera) && HitResultTraceDistanceForCamera > 0.0f)
		{
			HitResultTraceDistance = HitResultTraceDistanceForCamera;
		}
	}
}

void ASRPlayerController::RequestFocusActor(AActor* NewFocusedActor, bool bSnapImmediately)
{
	ASRCameraPawn* CameraPawn = Cast<ASRCameraPawn>(GetPawn());
	if (!CameraPawn)
	{
		UpdateSelection(NewFocusedActor);
		return;
	}

	AActor* CurrentFocusedActor = CameraPawn->GetFocusedActor();
	if (CurrentFocusedActor == NewFocusedActor)
	{
		UpdateSelection(NewFocusedActor);
		return;
	}

	if (IsValid(NewFocusedActor))
	{
		CameraPawn->FocusActorWithTransition(NewFocusedActor, !bSnapImmediately);
		if (bSnapImmediately)
		{
			CameraPawn->SnapToFocusTarget();
		}
		return;
	}

	CameraPawn->ClearFocusActor();
}

void ASRPlayerController::TryAutoFocusPrimaryStar()
{
	if (!bPendingInitialPrimaryStarFocus)
	{
		return;
	}

	ASRCameraPawn* CameraPawn = Cast<ASRCameraPawn>(GetPawn());
	if (!CameraPawn)
	{
		return;
	}

	if (IsValid(CameraPawn->GetFocusedActor()) || IsValid(SelectedActor))
	{
		bPendingInitialPrimaryStarFocus = false;
		return;
	}

	USRCelestialBodyRegistrySubsystem* CelestialBodyRegistry = GetCelestialBodyRegistry();
	if (!IsValid(CelestialBodyRegistry))
	{
		return;
	}

	AActor* PrimaryStarActor = CelestialBodyRegistry->GetPrimaryStarActor();
	if (!IsValid(PrimaryStarActor))
	{
		CelestialBodyRegistry->RefreshCelestialBodies();
		PrimaryStarActor = CelestialBodyRegistry->GetPrimaryStarActor();
	}

	if (!IsValid(PrimaryStarActor) || !USRCelestialBodyRuntimeLibrary::IsCelestialBodyActor(PrimaryStarActor))
	{
		return;
	}

	RequestFocusActor(PrimaryStarActor, true);
	bPendingInitialPrimaryStarFocus = false;
}

void ASRPlayerController::TryBindCameraPawnFocusEvents()
{
	ASRCameraPawn* CameraPawn = Cast<ASRCameraPawn>(GetPawn());
	if (BoundCameraPawn == CameraPawn)
	{
		return;
	}

	if (IsValid(BoundCameraPawn))
	{
		BoundCameraPawn->OnFocusedActorChanged().RemoveAll(this);
	}

	BoundCameraPawn = CameraPawn;
	if (IsValid(BoundCameraPawn))
	{
		BoundCameraPawn->OnFocusedActorChanged().AddUObject(this, &ASRPlayerController::HandleFocusedActorChanged);
	}
}

void ASRPlayerController::TryBindCelestialBodyRegistryEvents()
{
	USRCelestialBodyRegistrySubsystem* CelestialBodyRegistry = GetCelestialBodyRegistry();
	if (BoundCelestialBodyRegistry == CelestialBodyRegistry)
	{
		return;
	}

	if (IsValid(BoundCelestialBodyRegistry))
	{
		BoundCelestialBodyRegistry->OnCelestialBodiesChanged().RemoveAll(this);
		BoundCelestialBodyRegistry->OnPrimaryStarActorChanged().RemoveAll(this);
	}

	BoundCelestialBodyRegistry = CelestialBodyRegistry;
	if (IsValid(BoundCelestialBodyRegistry))
	{
		BoundCelestialBodyRegistry->OnCelestialBodiesChanged().AddUObject(this, &ASRPlayerController::HandleCelestialBodiesChanged);
		BoundCelestialBodyRegistry->OnPrimaryStarActorChanged().AddUObject(this, &ASRPlayerController::HandlePrimaryStarActorChanged);
	}
}

void ASRPlayerController::HandleFocusedActorChanged(AActor* NewFocusedActor)
{
	SetAssemblyModeActive(false);

	if (!IsValid(NewFocusedActor))
	{
		if (AssemblyComponent)
		{
			AssemblyComponent->ClearSurfaceHover();
		}
	}
	else if (USRPlanetSurfaceGrid* FocusedSurfaceGrid = USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(NewFocusedActor))
	{
		FocusedSurfaceGrid->ClearHoveredCell();
	}

	if (ASRCelestialBody* FocusedBody = Cast<ASRCelestialBody>(NewFocusedActor))
	{
		if (!FocusedBody->HasCelestialBodyDynamicMeshBuild())
		{
			TWeakObjectPtr<ASRCelestialBody> WeakFocusedBody = FocusedBody;
			TWeakObjectPtr<ASRCameraPawn> WeakCameraPawn = BoundCameraPawn;
			FTimerHandle PrepareDynamicMeshTimerHandle;
			GetWorldTimerManager().SetTimer(
				PrepareDynamicMeshTimerHandle,
				[WeakFocusedBody, WeakCameraPawn]()
				{
					ASRCelestialBody* Body = WeakFocusedBody.Get();
					ASRCameraPawn* CameraPawn = WeakCameraPawn.Get();
					if (Body && CameraPawn && CameraPawn->GetFocusedActor() == Body)
					{
						Body->PrepareCelestialBodyDynamicMesh();
					}
				},
				0.75f,
				false);
		}
	}

	if (!IsValid(NewFocusedActor) || SelectedActor != NewFocusedActor)
	{
		UpdateSelection(NewFocusedActor);
	}
}

void ASRPlayerController::HandleCelestialBodiesChanged()
{
	RefreshOverviewWidget();
}

void ASRPlayerController::HandlePrimaryStarActorChanged(AActor* NewPrimaryStarActor)
{
	if (!bPendingInitialPrimaryStarFocus)
	{
		return;
	}

	if (!IsValid(NewPrimaryStarActor))
	{
		return;
	}

	RequestFocusActor(NewPrimaryStarActor, true);
	bPendingInitialPrimaryStarFocus = false;
}

void ASRPlayerController::UpdateSelection(AActor* NewSelectedActor)
{
	if (SelectedActor != NewSelectedActor)
	{
		if (USRPlanetSurfaceGrid* PreviousSurfaceGrid = USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(SelectedActor))
		{
			PreviousSurfaceGrid->ClearSelectedCell();
		}
	}

	SelectedActor = NewSelectedActor;
	RefreshFocusInfoWidget();
	RefreshOverviewWidget();
	OnSelectionChanged(SelectedActor);
}
