#include "Camera/SRPlayerController.h"

#include "Assembly/SRAssemblyComponent.h"
#include "Assembly/SRAssemblyStructurePlacementPreview.h"
#include "Automation/SRFacilityDataAsset.h"
#include "Camera/SRCameraPawn.h"
#include "Celestial/SRStar.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"
#include "SRPlayerControllerFocusInfoState.h"
#include "SRPlayerControllerSurfaceSelectionState.h"

namespace
{
	constexpr float SpaceTraceDistanceMultiplier = 100.0f;
}
AActor* ASRPlayerController::GetSelectedActor() const
{
	return SelectedActor;
}

void ASRPlayerController::RequestActorFocus(AActor* NewFocusedActor, bool bSnapImmediately)
{
	RuntimeState.bPendingInitialPrimaryStarFocus = false;
	RequestFocusActor(NewFocusedActor, bSnapImmediately);
}

void ASRPlayerController::ClearSelection()
{
	UpdateSelection(nullptr);
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
	if (!FSRPlayerControllerFocusInfoState::SetHoveredSurfaceCellInfo(
		SelectedActor,
		SelectedActorFocusInfo,
		bHasHoveredSurfaceCell,
		HoveredSurfaceCellInfo))
	{
		return;
	}

	RefreshMiningResourceDepositHighlights();
	FSRPlayerControllerFocusInfoState::ApplyToFocusInfoWidget(
		SelectedActorFocusInfo,
		IsAssemblyModeActive(),
		FocusInfoWidget,
		false);
	RefreshFacilityControlWidget();
}

void ASRPlayerController::SetSelectedSurfaceStructureInfo(bool bHasSelectedSurfaceStructure, const FSRFocusedSurfaceStructureInfo& SelectedSurfaceStructureInfo)
{
	if (!FSRPlayerControllerFocusInfoState::SetSelectedSurfaceStructureInfo(
		SelectedActor,
		SelectedActorFocusInfo,
		bHasSelectedSurfaceStructure,
		SelectedSurfaceStructureInfo))
	{
		return;
	}

	RefreshMiningResourceDepositHighlights();
	FSRPlayerControllerFocusInfoState::ApplyToFocusInfoWidget(
		SelectedActorFocusInfo,
		IsAssemblyModeActive(),
		FocusInfoWidget,
		false);
	RefreshFacilityControlWidget();
}

void ASRPlayerController::SetSelectedActorSurfaceStructureInfo(AActor* NewSelectedActor, const FSRFocusedSurfaceStructureInfo& SelectedSurfaceStructureInfo)
{
	if (!IsValid(NewSelectedActor))
	{
		return;
	}

	const bool bSelectionChanged = SelectedActor != NewSelectedActor;
	if (bSelectionChanged)
	{
		FSRPlayerControllerSurfaceSelectionState::ClearSelectedActorSurfacePreview(SelectedActor);
		SelectedActor = NewSelectedActor;
		RefreshOverviewWidget();
		OnSelectionChanged(SelectedActor);
	}

	FSRPlayerControllerFocusInfoState::RebuildSelectedActorSurfaceStructureInfo(
		SelectedActor,
		SelectedActorFocusInfo,
		SelectedSurfaceStructureInfo);
	RefreshMiningResourceDepositHighlights();
	FSRPlayerControllerFocusInfoState::ApplyToFocusInfoWidget(
		SelectedActorFocusInfo,
		IsAssemblyModeActive(),
		FocusInfoWidget,
		true);
	RefreshFacilityControlWidget();
}

void ASRPlayerController::RefreshMiningResourceDepositHighlights()
{
	AActor* SurfaceActor = SelectedActor;
	if (!IsValid(SurfaceActor))
	{
		return;
	}

	USRStructureInstanceManagerComponent* StructureManager =
		SurfaceActor->FindComponentByClass<USRStructureInstanceManagerComponent>();
	if (!IsValid(StructureManager))
	{
		return;
	}

	USRPlanetSurfaceGrid* SurfaceGrid =
		USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(SurfaceActor);
	if (!IsValid(SurfaceGrid))
	{
		StructureManager->ClearMiningResourceDepositHighlights();
		return;
	}

	const auto IsMiningStructure = [](const USRStructureDataAsset* StructureDataAsset)
	{
		if (!IsValid(StructureDataAsset))
		{
			return false;
		}

		const FSRStructureData StructureData = StructureDataAsset->BuildData();
		const USRFacilityDataAsset* FacilityDataAsset = StructureData.FacilityDataAsset.Get();
		return IsValid(FacilityDataAsset)
			&& FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Mine;
	};

	TArray<FSRPlanetSurfaceGridCellId> MinerFootprintCellIds;
	bool bMiningSelectionActive = false;
	if (SelectedActorFocusInfo.bHasSelectedSurfaceStructure
		&& SelectedActorFocusInfo.SelectedSurfaceStructureInfo.bIsValid
		&& IsMiningStructure(
			SelectedActorFocusInfo.SelectedSurfaceStructureInfo.StructureDataAsset.Get()))
	{
		bMiningSelectionActive = true;
		MinerFootprintCellIds =
			SelectedActorFocusInfo.SelectedSurfaceStructureInfo.FootprintCellIds;
	}
	else if (IsAssemblyModeActive()
		&& bHasSelectedStructureBuildId
		&& IsMiningStructure(SelectedStructureDataAsset))
	{
		bMiningSelectionActive = true;
		const int32 PlacementRotationSteps = AssemblyComponent
			? AssemblyComponent->GetStructurePlacementRotationSteps()
			: 0;
		MinerFootprintCellIds = FSRAssemblyStructurePlacementPreviewEvaluator::Evaluate(
			SurfaceGrid,
			SelectedStructureDataAsset,
			PlacementRotationSteps).FootprintCellIds;
	}

	if (bMiningSelectionActive)
	{
		StructureManager->SetMiningResourceDepositHighlights(
			SurfaceGrid,
			MinerFootprintCellIds);
	}
	else
	{
		StructureManager->ClearMiningResourceDepositHighlights(SurfaceGrid);
	}
}

USRCelestialBodyRegistrySubsystem* ASRPlayerController::GetCelestialBodyRegistry() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<USRCelestialBodyRegistrySubsystem>() : nullptr;
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
	if (!RuntimeState.bPendingInitialPrimaryStarFocus)
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
		RuntimeState.bPendingInitialPrimaryStarFocus = false;
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
	RuntimeState.bPendingInitialPrimaryStarFocus = false;
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
	BindPrimaryStarGameOver(nullptr);

	BoundCelestialBodyRegistry = CelestialBodyRegistry;
	if (IsValid(BoundCelestialBodyRegistry))
	{
		BoundCelestialBodyRegistry->OnCelestialBodiesChanged().AddUObject(this, &ASRPlayerController::HandleCelestialBodiesChanged);
		BoundCelestialBodyRegistry->OnPrimaryStarActorChanged().AddUObject(this, &ASRPlayerController::HandlePrimaryStarActorChanged);
		BindPrimaryStarGameOver(BoundCelestialBodyRegistry->GetPrimaryStarActor());
	}
}

void ASRPlayerController::HandleFocusedActorChanged(AActor* NewFocusedActor)
{
	SetAssemblyModeActive(false);

	FSRPlayerControllerSurfaceSelectionState::ClearFocusedActorHover(NewFocusedActor, AssemblyComponent);

	if (!IsValid(NewFocusedActor) || SelectedActor != NewFocusedActor)
	{
		UpdateSelection(NewFocusedActor);
	}
	RefreshFocusedHubShortcutWidget(true);
}

void ASRPlayerController::HandleCelestialBodiesChanged()
{
	RefreshOverviewWidget();
	RefreshFocusedHubShortcutWidget(true);
}

void ASRPlayerController::HandlePrimaryStarActorChanged(AActor* NewPrimaryStarActor)
{
	BindPrimaryStarGameOver(NewPrimaryStarActor);

	if (!RuntimeState.bPendingInitialPrimaryStarFocus)
	{
		return;
	}

	if (!IsValid(NewPrimaryStarActor))
	{
		return;
	}

	RequestFocusActor(NewPrimaryStarActor, true);
	RuntimeState.bPendingInitialPrimaryStarFocus = false;
}

void ASRPlayerController::BindPrimaryStarGameOver(AActor* PrimaryStarActor)
{
	ASRStar* PrimaryStar = Cast<ASRStar>(PrimaryStarActor);
	if (BoundGameOverStar == PrimaryStar)
	{
		if (IsValid(PrimaryStar) && PrimaryStar->HasStellarRunEnded())
		{
			ShowGameOverScreen(PrimaryStar);
		}
		return;
	}

	if (IsValid(BoundGameOverStar))
	{
		BoundGameOverStar->OnStellarRunCompleted.RemoveDynamic(this, &ASRPlayerController::HandlePrimaryStarGameOver);
	}

	BoundGameOverStar = PrimaryStar;
	if (!IsValid(BoundGameOverStar))
	{
		return;
	}

	BoundGameOverStar->OnStellarRunCompleted.RemoveDynamic(this, &ASRPlayerController::HandlePrimaryStarGameOver);
	BoundGameOverStar->OnStellarRunCompleted.AddDynamic(this, &ASRPlayerController::HandlePrimaryStarGameOver);
	if (BoundGameOverStar->HasStellarRunEnded())
	{
		ShowGameOverScreen(BoundGameOverStar);
	}
}

void ASRPlayerController::HandlePrimaryStarGameOver(ASRStar* Star)
{
	if (!IsValid(Star))
	{
		return;
	}

	ShowGameOverScreen(Star);
}

void ASRPlayerController::UpdateSelection(AActor* NewSelectedActor)
{
	const bool bSameSelectedActor = SelectedActor == NewSelectedActor;
	const bool bPreserveSelectedSurfaceStructure = bSameSelectedActor
		&& SelectedActorFocusInfo.bHasSelectedSurfaceStructure
		&& SelectedActorFocusInfo.SelectedSurfaceStructureInfo.bIsValid;
	const FSRFocusedSurfaceStructureInfo PreservedSelectedSurfaceStructureInfo = bPreserveSelectedSurfaceStructure
		? SelectedActorFocusInfo.SelectedSurfaceStructureInfo
		: FSRFocusedSurfaceStructureInfo();

	if (SelectedActor != NewSelectedActor)
	{
		FSRPlayerControllerSurfaceSelectionState::ClearSelectedActorSurfacePreview(SelectedActor);
	}

	SelectedActor = NewSelectedActor;
	RefreshFocusInfoWidget();
	if (bPreserveSelectedSurfaceStructure)
	{
		SetSelectedSurfaceStructureInfo(true, PreservedSelectedSurfaceStructureInfo);
	}
	else
	{
		RefreshMiningResourceDepositHighlights();
		RefreshFacilityControlWidget();
	}
	RefreshOverviewWidget();
	OnSelectionChanged(SelectedActor);
	RefreshFocusedHubShortcutWidget(true);
}
