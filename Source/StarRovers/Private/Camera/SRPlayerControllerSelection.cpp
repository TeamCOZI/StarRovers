#include "Camera/SRPlayerController.h"

#include "Assembly/SRAssemblyComponent.h"
#include "Camera/SRCameraPawn.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "Surface/SRPlanetSurfaceGrid.h"
#include "UI/SRCelestialBodyFocusInfoWidget.h"

namespace
{
	constexpr float SpaceTraceDistanceMultiplier = 100.0f;
}
AActor* ASRPlayerController::GetSelectedActor() const
{
	return SelectedActor;
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
	RefreshFacilityControlWidget();
}

void ASRPlayerController::SetSelectedSurfaceStructureInfo(bool bHasSelectedSurfaceStructure, const FSRFocusedSurfaceStructureInfo& SelectedSurfaceStructureInfo)
{
	if (!SelectedActorFocusInfo.bIsValid)
	{
		SelectedActorFocusInfo = USRCelestialBodyRuntimeLibrary::BuildCelestialBodyFocusInfo(SelectedActor);
	}

	if (!SelectedActorFocusInfo.bIsValid)
	{
		return;
	}

	SelectedActorFocusInfo.bHasSelectedSurfaceStructure = bHasSelectedSurfaceStructure;
	SelectedActorFocusInfo.SelectedSurfaceStructureInfo = bHasSelectedSurfaceStructure
		? SelectedSurfaceStructureInfo
		: FSRFocusedSurfaceStructureInfo();

	if (FocusInfoWidget)
	{
		FocusInfoWidget->SetFocusInfo(SelectedActorFocusInfo);
		FocusInfoWidget->SetAssemblyModeActive(IsAssemblyModeActive());
	}
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
		if (USRPlanetSurfaceGrid* PreviousSurfaceGrid = USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(SelectedActor))
		{
			PreviousSurfaceGrid->ClearSelectedCell();
			PreviousSurfaceGrid->ClearOccupiedPreviewCells();
			PreviousSurfaceGrid->ClearFacilityPortPreviewCells();
		}
		SelectedActor = NewSelectedActor;
		RefreshOverviewWidget();
		OnSelectionChanged(SelectedActor);
	}

	SelectedActorFocusInfo = USRCelestialBodyRuntimeLibrary::BuildCelestialBodyFocusInfo(SelectedActor);
	SelectedActorFocusInfo.bHasSelectedSurfaceStructure = SelectedSurfaceStructureInfo.bIsValid;
	SelectedActorFocusInfo.SelectedSurfaceStructureInfo = SelectedSurfaceStructureInfo.bIsValid
		? SelectedSurfaceStructureInfo
		: FSRFocusedSurfaceStructureInfo();

	if (FocusInfoWidget)
	{
		FocusInfoWidget->SetFocusInfo(SelectedActorFocusInfo);
		FocusInfoWidget->SetAssemblyModeActive(IsAssemblyModeActive());
		FocusInfoWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	RefreshFacilityControlWidget();
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
	const bool bSameSelectedActor = SelectedActor == NewSelectedActor;
	const bool bPreserveSelectedSurfaceStructure = bSameSelectedActor
		&& SelectedActorFocusInfo.bHasSelectedSurfaceStructure
		&& SelectedActorFocusInfo.SelectedSurfaceStructureInfo.bIsValid;
	const FSRFocusedSurfaceStructureInfo PreservedSelectedSurfaceStructureInfo = bPreserveSelectedSurfaceStructure
		? SelectedActorFocusInfo.SelectedSurfaceStructureInfo
		: FSRFocusedSurfaceStructureInfo();

	if (SelectedActor != NewSelectedActor)
	{
		if (USRPlanetSurfaceGrid* PreviousSurfaceGrid = USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(SelectedActor))
		{
			PreviousSurfaceGrid->ClearSelectedCell();
			PreviousSurfaceGrid->ClearOccupiedPreviewCells();
			PreviousSurfaceGrid->ClearFacilityPortPreviewCells();
		}
	}

	SelectedActor = NewSelectedActor;
	RefreshFocusInfoWidget();
	if (bPreserveSelectedSurfaceStructure)
	{
		SetSelectedSurfaceStructureInfo(true, PreservedSelectedSurfaceStructureInfo);
	}
	else
	{
		RefreshFacilityControlWidget();
	}
	RefreshOverviewWidget();
	OnSelectionChanged(SelectedActor);
}
