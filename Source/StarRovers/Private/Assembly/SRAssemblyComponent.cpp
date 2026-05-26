#include "Assembly/SRAssemblyComponent.h"

#include "Camera/SRCameraPawn.h"
#include "Camera/SRPlayerController.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Surface/SRPlanetSurfaceGrid.h"

USRAssemblyComponent::USRAssemblyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	bAssemblyModeActive = false;
	ActiveAssemblySurfaceGrid = nullptr;
	LastHoveredSampleSurfaceGrid = nullptr;
	LastHoveredSampleMousePosition = FVector2D::ZeroVector;
	bHasLastHoveredSampleMousePosition = false;
}

void USRAssemblyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateSurfaceHover();
}

bool USRAssemblyComponent::IsAssemblyModeActive() const
{
	return bAssemblyModeActive;
}

void USRAssemblyComponent::SetAssemblyModeActive(bool bNewAssemblyModeActive)
{
	if (bNewAssemblyModeActive)
	{
		AActor* FocusedActor = nullptr;
		USRPlanetSurfaceGrid* FocusedSurfaceGrid = nullptr;
		if (!TryGetFocusedSurfaceGrid(FocusedActor, FocusedSurfaceGrid))
		{
			bNewAssemblyModeActive = false;
		}
	}

	if (bAssemblyModeActive == bNewAssemblyModeActive)
	{
		ApplyAssemblyModeToFocusedSurfaceGrid();
		return;
	}

	bAssemblyModeActive = bNewAssemblyModeActive;
	ResetHoverSampleCache();
	ApplyAssemblyModeToFocusedSurfaceGrid();
}

void USRAssemblyComponent::ToggleAssemblyMode()
{
	SetAssemblyModeActive(!bAssemblyModeActive);
}

bool USRAssemblyComponent::TryHandleAssemblyClick(AActor*& OutSelectedActor)
{
	OutSelectedActor = nullptr;

	ASRPlayerController* PlayerController = GetOwnerController();
	if (!PlayerController || !bAssemblyModeActive)
	{
		return false;
	}

	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* FocusedSurfaceGrid = nullptr;
	FSRPlanetSurfaceGridCell HoveredCell;
	FVector HoverHitLocation = FVector::ZeroVector;

	if (TryGetFocusedSurfaceGrid(FocusedActor, FocusedSurfaceGrid)
		&& TryProjectCursorToSurfaceCell(FocusedSurfaceGrid, HoveredCell, HoverHitLocation))
	{
		FocusedSurfaceGrid->SetSelectedCell(HoveredCell.CellId);
		OutSelectedActor = FocusedActor;
		return true;
	}

	return false;
}

void USRAssemblyComponent::ClearSurfaceGridInteraction(AActor* SurfaceActor)
{
	USRPlanetSurfaceGrid* CurrentSurfaceGrid = USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(SurfaceActor);
	if (CurrentSurfaceGrid)
	{
		CurrentSurfaceGrid->ClearHoveredCell();
		CurrentSurfaceGrid->ClearSelectedCell();
		CurrentSurfaceGrid->SetGridVisible(false);
	}
	if (CurrentSurfaceGrid == ActiveAssemblySurfaceGrid)
	{
		ActiveAssemblySurfaceGrid = nullptr;
	}

	if (!IsValid(SurfaceActor) || CurrentSurfaceGrid == HoveredSurfaceGrid)
	{
		HoveredSurfaceGrid = nullptr;
	}
	ResetHoverSampleCache();
}

void USRAssemblyComponent::ClearSurfaceHover()
{
	if (IsValid(HoveredSurfaceGrid))
	{
		HoveredSurfaceGrid->ClearHoveredCell();
	}

	HoveredSurfaceGrid = nullptr;
	ResetHoverSampleCache();
}

ASRPlayerController* USRAssemblyComponent::GetOwnerController() const
{
	return Cast<ASRPlayerController>(GetOwner());
}

bool USRAssemblyComponent::GetCursorRay(FVector& OutRayOrigin, FVector& OutRayDirection) const
{
	OutRayOrigin = FVector::ZeroVector;
	OutRayDirection = FVector::ZeroVector;

	const ASRPlayerController* PlayerController = GetOwnerController();
	return PlayerController
		&& PlayerController->DeprojectMousePositionToWorld(OutRayOrigin, OutRayDirection)
		&& !OutRayDirection.IsNearlyZero();
}

bool USRAssemblyComponent::TryGetFocusedSurfaceGrid(AActor*& OutFocusedActor, USRPlanetSurfaceGrid*& OutSurfaceGrid) const
{
	OutFocusedActor = nullptr;
	OutSurfaceGrid = nullptr;

	const ASRPlayerController* PlayerController = GetOwnerController();
	const ASRCameraPawn* CameraPawn = PlayerController ? Cast<ASRCameraPawn>(PlayerController->GetPawn()) : nullptr;
	if (!CameraPawn)
	{
		return false;
	}

	AActor* FocusedActor = CameraPawn->GetFocusedActor();
	if (!IsValid(FocusedActor))
	{
		return false;
	}

	USRPlanetSurfaceGrid* SurfaceGrid = USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(FocusedActor);
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	OutFocusedActor = FocusedActor;
	OutSurfaceGrid = SurfaceGrid;
	return true;
}

bool USRAssemblyComponent::TryProjectCursorToSurfaceCell(USRPlanetSurfaceGrid* SurfaceGrid, FSRPlanetSurfaceGridCell& OutCell, FVector& OutHitLocation) const
{
	OutCell = FSRPlanetSurfaceGridCell();
	OutHitLocation = FVector::ZeroVector;

	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	if (AActor* OwnerActor = SurfaceGrid->GetOwner())
	{
		OwnerActor->UpdateComponentTransforms();
	}
	SurfaceGrid->UpdateComponentToWorld();

	FVector RayOrigin = FVector::ZeroVector;
	FVector RayDirection = FVector::ZeroVector;
	if (!GetCursorRay(RayOrigin, RayDirection))
	{
		return false;
	}

	return SurfaceGrid->RaycastCell(RayOrigin, RayDirection, OutCell, OutHitLocation);
}

void USRAssemblyComponent::UpdateSurfaceHover()
{
	if (!bAssemblyModeActive)
	{
		ClearSurfaceHover();
		return;
	}

	ASRPlayerController* PlayerController = GetOwnerController();
	if (!PlayerController)
	{
		ClearSurfaceHover();
		return;
	}

	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* SurfaceGrid = nullptr;
	if (!TryGetFocusedSurfaceGrid(FocusedActor, SurfaceGrid))
	{
		ClearSurfaceHover();
		return;
	}

	FVector2D CurrentMousePosition = FVector2D::ZeroVector;
	const bool bHasMousePosition = PlayerController->GetMousePosition(CurrentMousePosition.X, CurrentMousePosition.Y);
	if (bHasMousePosition
		&& bHasLastHoveredSampleMousePosition
		&& LastHoveredSampleSurfaceGrid == SurfaceGrid
		&& HoveredSurfaceGrid == SurfaceGrid
		&& SurfaceGrid->HasHoveredCell()
		&& FVector2D::Distance(CurrentMousePosition, LastHoveredSampleMousePosition) <= 0.5f)
	{
		return;
	}

	FSRPlanetSurfaceGridCell HoveredCell;
	FVector HoverHitLocation = FVector::ZeroVector;
	if (!TryProjectCursorToSurfaceCell(SurfaceGrid, HoveredCell, HoverHitLocation))
	{
		ClearSurfaceHover();
		return;
	}

	if (HoveredSurfaceGrid && HoveredSurfaceGrid != SurfaceGrid)
	{
		HoveredSurfaceGrid->ClearHoveredCell();
	}

	HoveredSurfaceGrid = SurfaceGrid;
	HoveredSurfaceGrid->SetHoveredCell(HoveredCell.CellId);
	if (bHasMousePosition)
	{
		LastHoveredSampleSurfaceGrid = SurfaceGrid;
		LastHoveredSampleMousePosition = CurrentMousePosition;
		bHasLastHoveredSampleMousePosition = true;
	}
}

void USRAssemblyComponent::ApplyAssemblyModeToFocusedSurfaceGrid()
{
	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* FocusedSurfaceGrid = nullptr;
	const bool bHasFocusedSurfaceGrid = TryGetFocusedSurfaceGrid(FocusedActor, FocusedSurfaceGrid);
	USRPlanetSurfaceGrid* DesiredSurfaceGrid = bAssemblyModeActive && bHasFocusedSurfaceGrid ? FocusedSurfaceGrid : nullptr;

	if (ActiveAssemblySurfaceGrid && ActiveAssemblySurfaceGrid != DesiredSurfaceGrid)
	{
		ActiveAssemblySurfaceGrid->SetGridVisible(false);
	}

	ActiveAssemblySurfaceGrid = DesiredSurfaceGrid;
	if (ActiveAssemblySurfaceGrid)
	{
		ActiveAssemblySurfaceGrid->SetGridVisible(true);
	}

	if (!bAssemblyModeActive)
	{
		ClearSurfaceHover();
	}
}

void USRAssemblyComponent::ResetHoverSampleCache()
{
	LastHoveredSampleSurfaceGrid = nullptr;
	LastHoveredSampleMousePosition = FVector2D::ZeroVector;
	bHasLastHoveredSampleMousePosition = false;
}
