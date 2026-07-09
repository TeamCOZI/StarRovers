#include "Camera/SRPlayerController.h"

#include "Camera/SRCameraPawn.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
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
	FSRPlayerControllerFocusInfoState::ApplyToFocusInfoWidget(
		SelectedActorFocusInfo,
		IsAssemblyModeActive(),
		FocusInfoWidget,
		true);
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

	FSRPlayerControllerSurfaceSelectionState::ClearFocusedActorHover(NewFocusedActor, AssemblyComponent);

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
		RefreshFacilityControlWidget();
	}
	RefreshOverviewWidget();
	OnSelectionChanged(SelectedActor);
}
