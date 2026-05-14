#include "Assembly/SRAssemblyComponent.h"

#include "Camera/SRCameraPawn.h"
#include "Camera/SRPlayerController.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "EngineUtils.h"
#include "Surface/SRPlanetSurfaceGrid.h"

USRAssemblyComponent::USRAssemblyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bAssemblyModeActive = false;
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
	FHitResult CursorHitResult;
	const bool bHasCursorHit = PlayerController->GetHitResultUnderCursor(ECC_Visibility, false, CursorHitResult);

	if (TryGetFocusedSurfaceGrid(FocusedActor, FocusedSurfaceGrid)
		&& bHasCursorHit
		&& CursorHitResult.GetActor() == FocusedActor
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

	if (!IsValid(SurfaceActor) || CurrentSurfaceGrid == HoveredSurfaceGrid)
	{
		HoveredSurfaceGrid = nullptr;
	}
}

void USRAssemblyComponent::ClearSurfaceHover()
{
	if (IsValid(HoveredSurfaceGrid))
	{
		HoveredSurfaceGrid->ClearHoveredCell();
	}

	HoveredSurfaceGrid = nullptr;
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

	FHitResult HitResult;
	const bool bHasCursorHit = PlayerController->GetHitResultUnderCursor(ECC_Visibility, false, HitResult);
	if (!bHasCursorHit || HitResult.GetActor() != FocusedActor)
	{
		ClearSurfaceHover();
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
}

void USRAssemblyComponent::ApplyAssemblyModeToFocusedSurfaceGrid()
{
	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* FocusedSurfaceGrid = nullptr;
	const bool bHasFocusedSurfaceGrid = TryGetFocusedSurfaceGrid(FocusedActor, FocusedSurfaceGrid);

	HideAllSurfaceGridsExcept(bAssemblyModeActive && bHasFocusedSurfaceGrid ? FocusedSurfaceGrid : nullptr);

	if (bHasFocusedSurfaceGrid)
	{
		FocusedSurfaceGrid->SetGridVisible(bAssemblyModeActive);
	}

	if (!bAssemblyModeActive)
	{
		ClearSurfaceHover();
	}
}

void USRAssemblyComponent::HideAllSurfaceGridsExcept(USRPlanetSurfaceGrid* SurfaceGridToKeep)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor))
		{
			continue;
		}

		if (USRPlanetSurfaceGrid* SurfaceGrid = USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(Actor))
		{
			if (SurfaceGrid != SurfaceGridToKeep)
			{
				SurfaceGrid->SetGridVisible(false);
			}
		}
	}
}
