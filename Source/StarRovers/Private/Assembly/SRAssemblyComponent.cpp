#include "Assembly/SRAssemblyComponent.h"

#include "Camera/SRCameraPawn.h"
#include "Camera/SRPlayerController.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Conveyor/SRConveyorNetworkComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Structure/SRBuildableStructureInterface.h"
#include "Structure/SRStructure.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Structure/SRStructurePlacementLibrary.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	constexpr uint64 StructureGhostPlacementDebugMessageKey = 0x535247686F73744FULL;

	void AppendGridLineCellIds(
		const FSRPlanetSurfaceGridCellId& StartCellId,
		const FSRPlanetSurfaceGridCellId& EndCellId,
		TArray<FSRPlanetSurfaceGridCellId>& OutCellIds)
	{
		if (StartCellId.Face != EndCellId.Face)
		{
			OutCellIds.Add(EndCellId);
			return;
		}

		const int32 DeltaX = FMath::Abs(EndCellId.CellX - StartCellId.CellX);
		const int32 DeltaY = FMath::Abs(EndCellId.CellY - StartCellId.CellY);
		const int32 StepX = StartCellId.CellX < EndCellId.CellX ? 1 : -1;
		const int32 StepY = StartCellId.CellY < EndCellId.CellY ? 1 : -1;

		int32 CurrentX = StartCellId.CellX;
		int32 CurrentY = StartCellId.CellY;
		int32 Error = DeltaX - DeltaY;

		while (true)
		{
			FSRPlanetSurfaceGridCellId CellId;
			CellId.Face = StartCellId.Face;
			CellId.CellX = CurrentX;
			CellId.CellY = CurrentY;
			OutCellIds.Add(CellId);

			if (CurrentX == EndCellId.CellX && CurrentY == EndCellId.CellY)
			{
				break;
			}

			const int32 Error2 = Error * 2;
			if (Error2 > -DeltaY)
			{
				Error -= DeltaY;
				CurrentX += StepX;
			}
			if (Error2 < DeltaX)
			{
				Error += DeltaX;
				CurrentY += StepY;
			}
		}
	}
}

USRAssemblyComponent::USRAssemblyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	bAssemblyModeActive = false;
	MaxStructurePlacementsPerFrame = 4;
	MaxQueuedStructurePlacements = 256;
	ActiveAssemblySurfaceGrid = nullptr;
	LastHoveredSampleSurfaceGrid = nullptr;
	LastPublishedHoveredSurfaceGrid = nullptr;
	StructureGhostActor = nullptr;
	StructureGhostDataAsset = nullptr;
	LastLoggedInvalidGhostDataAsset = nullptr;
	LastHoveredSampleMousePosition = FVector2D::ZeroVector;
	bHasLastHoveredSampleMousePosition = false;
	bHasLastPublishedHoveredCellInfo = false;
	LastPublishedHoveredCellId = FSRPlanetSurfaceGridCellId();
	StructureGhostCellId = FSRPlanetSurfaceGridCellId();
	bHasStructureGhostCellId = false;
	bIsStructurePlacementDragActive = false;
	LastStructurePlacementDragSurfaceGrid = nullptr;
	LastStructurePlacementDragCellId = FSRPlanetSurfaceGridCellId();
	bHasLastStructurePlacementDragCellId = false;
	PendingConveyorStartSurfaceGrid = nullptr;
	PendingConveyorStartCellId = FSRPlanetSurfaceGridCellId();
	bHasPendingConveyorStartCell = false;
}

void USRAssemblyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateSurfaceHover();
	ProcessQueuedStructurePlacements();
	UpdateStructureGhostPreview();
}

void USRAssemblyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyStructureGhostPreview();

	Super::EndPlay(EndPlayReason);
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
	if (!bAssemblyModeActive)
	{
		EndStructurePlacementDrag();
		ClearPendingConveyorPathStart();
		PendingStructurePlacementQueue.Reset();
		DestroyStructureGhostPreview();
	}
}

void USRAssemblyComponent::ToggleAssemblyMode()
{
	SetAssemblyModeActive(!bAssemblyModeActive);
}

void USRAssemblyComponent::ConfigurePlacementPerformance(int32 NewMaxStructurePlacementsPerFrame, int32 NewMaxQueuedStructurePlacements)
{
	MaxStructurePlacementsPerFrame = FMath::Max(1, NewMaxStructurePlacementsPerFrame);
	MaxQueuedStructurePlacements = FMath::Max(1, NewMaxQueuedStructurePlacements);
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
		OutSelectedActor = FocusedActor;
		if (USRStructureDataAsset* SelectedStructureDataAsset = PlayerController->GetSelectedStructureDataAsset())
		{
			const FSRStructureData StructureData = SelectedStructureDataAsset->BuildData();
			if (StructureData.BuildKind == ESRStructureBuildKind::Conveyor)
			{
				if (!bHasPendingConveyorStartCell || PendingConveyorStartSurfaceGrid != FocusedSurfaceGrid)
				{
					PendingConveyorStartSurfaceGrid = FocusedSurfaceGrid;
					PendingConveyorStartCellId = HoveredCell.CellId;
					bHasPendingConveyorStartCell = true;
					FocusedSurfaceGrid->SetSelectedCell(HoveredCell.CellId);
					PublishHoveredCellInfo(FocusedSurfaceGrid, HoveredCell);
					return true;
				}

				const FSRPlanetSurfaceGridCellId StartCellId = PendingConveyorStartCellId;
				ClearPendingConveyorPathStart();
				FocusedSurfaceGrid->ClearSelectedCell();
				TryPlaceSelectedConveyorPath(FocusedSurfaceGrid, StartCellId, HoveredCell, SelectedStructureDataAsset);
				return true;
			}

			ClearPendingConveyorPathStart();
			FocusedSurfaceGrid->ClearSelectedCell();
			TryPlaceSelectedStructure(FocusedSurfaceGrid, HoveredCell);
			return true;
		}

		ClearPendingConveyorPathStart();
		FocusedSurfaceGrid->SetSelectedCell(HoveredCell.CellId);
		return true;
	}

	return false;
}

bool USRAssemblyComponent::TryHandleAssemblyDelete(AActor*& OutSelectedActor)
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
	if (!TryGetFocusedSurfaceGrid(FocusedActor, FocusedSurfaceGrid)
		|| !TryProjectCursorToSurfaceCell(FocusedSurfaceGrid, HoveredCell, HoverHitLocation))
	{
		return false;
	}

	OutSelectedActor = FocusedActor;
	if (!TryDeleteStructureAtCell(FocusedActor, FocusedSurfaceGrid, HoveredCell.CellId))
	{
		return true;
	}

	ClearPendingConveyorPathStart();
	PendingStructurePlacementQueue.Reset();
	DestroyStructureGhostPreview();
	FocusedSurfaceGrid->SetHoveredCell(HoveredCell.CellId);
	FocusedSurfaceGrid->ClearSelectedCell();
	bHasLastPublishedHoveredCellInfo = false;
	LastPublishedHoveredSurfaceGrid = nullptr;
	LastPublishedHoveredCellId = FSRPlanetSurfaceGridCellId();
	PublishHoveredCellInfo(FocusedSurfaceGrid, HoveredCell);
	return true;
}

bool USRAssemblyComponent::ShouldHandleStructurePlacementDrag() const
{
	return false;
}

bool USRAssemblyComponent::BeginStructurePlacementDrag(AActor*& OutSelectedActor)
{
	OutSelectedActor = nullptr;
	EndStructurePlacementDrag();

	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* SurfaceGrid = nullptr;
	FSRPlanetSurfaceGridCell TargetCell;
	if (!TryResolveStructurePlacementDragTarget(FocusedActor, SurfaceGrid, TargetCell))
	{
		return false;
	}

	ASRPlayerController* PlayerController = GetOwnerController();
	USRStructureDataAsset* SelectedStructureDataAsset = PlayerController ? PlayerController->GetSelectedStructureDataAsset() : nullptr;
	if (IsValid(SelectedStructureDataAsset) && SelectedStructureDataAsset->BuildData().BuildKind == ESRStructureBuildKind::Conveyor)
	{
		return false;
	}

	bIsStructurePlacementDragActive = true;
	OutSelectedActor = FocusedActor;
	return TryPlaceStructureDragPath(SurfaceGrid, TargetCell);
}

bool USRAssemblyComponent::ContinueStructurePlacementDrag(AActor*& OutSelectedActor)
{
	OutSelectedActor = nullptr;
	if (!bIsStructurePlacementDragActive)
	{
		return false;
	}

	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* SurfaceGrid = nullptr;
	FSRPlanetSurfaceGridCell TargetCell;
	if (!TryResolveStructurePlacementDragTarget(FocusedActor, SurfaceGrid, TargetCell))
	{
		return false;
	}

	OutSelectedActor = FocusedActor;
	return TryPlaceStructureDragPath(SurfaceGrid, TargetCell);
}

void USRAssemblyComponent::EndStructurePlacementDrag()
{
	bIsStructurePlacementDragActive = false;
	LastStructurePlacementDragSurfaceGrid = nullptr;
	LastStructurePlacementDragCellId = FSRPlanetSurfaceGridCellId();
	bHasLastStructurePlacementDragCellId = false;
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
	ClearPublishedHoveredCellInfo();
	ResetHoverSampleCache();
	EndStructurePlacementDrag();
	ClearPendingConveyorPathStart();
	PendingStructurePlacementQueue.Reset();
	DestroyStructureGhostPreview();
}

void USRAssemblyComponent::ClearSurfaceHover()
{
	if (IsValid(HoveredSurfaceGrid))
	{
		HoveredSurfaceGrid->ClearHoveredCell();
	}

	HoveredSurfaceGrid = nullptr;
	ClearPublishedHoveredCellInfo();
	ResetHoverSampleCache();
	EndStructurePlacementDrag();
	ClearPendingConveyorPathStart();
	PendingStructurePlacementQueue.Reset();
	DestroyStructureGhostPreview();
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
	PublishHoveredCellInfo(SurfaceGrid, HoveredCell);
	if (bHasMousePosition)
	{
		LastHoveredSampleSurfaceGrid = SurfaceGrid;
		LastHoveredSampleMousePosition = CurrentMousePosition;
		bHasLastHoveredSampleMousePosition = true;
	}
}

void USRAssemblyComponent::ProcessQueuedStructurePlacements()
{
	if (PendingStructurePlacementQueue.IsEmpty())
	{
		return;
	}

	const int32 PlacementBudget = FMath::Max(1, MaxStructurePlacementsPerFrame);
	TSet<USRPlanetSurfaceGrid*> BatchedSurfaceGrids;
	BatchedSurfaceGrids.Reserve(PlacementBudget);
	bool bPlacedAnyStructure = false;

	const int32 PlacementCount = FMath::Min(PlacementBudget, PendingStructurePlacementQueue.Num());
	for (int32 PlacementIndex = 0; PlacementIndex < PlacementCount; ++PlacementIndex)
	{
		FSRQueuedStructurePlacement QueuedPlacement = PendingStructurePlacementQueue[0];
		PendingStructurePlacementQueue.RemoveAt(0, 1, EAllowShrinking::No);

		USRPlanetSurfaceGrid* SurfaceGrid = QueuedPlacement.SurfaceGrid.Get();
		if (!IsValid(SurfaceGrid))
		{
			continue;
		}

		if (!BatchedSurfaceGrids.Contains(SurfaceGrid))
		{
			SurfaceGrid->BeginInteractionHighlightBatch();
			BatchedSurfaceGrids.Add(SurfaceGrid);
		}

		FSRPlanetSurfaceGridCell TargetCell;
		if (SurfaceGrid->GetCellById(QueuedPlacement.CellId, TargetCell))
		{
			bPlacedAnyStructure |= TryPlaceSelectedStructure(SurfaceGrid, TargetCell, false);
		}
	}

	for (USRPlanetSurfaceGrid* SurfaceGrid : BatchedSurfaceGrids)
	{
		if (IsValid(SurfaceGrid))
		{
			SurfaceGrid->EndInteractionHighlightBatch();
		}
	}

	if (bPlacedAnyStructure)
	{
		DestroyStructureGhostPreview();
		if (IsValid(HoveredSurfaceGrid))
		{
			FSRPlanetSurfaceGridCell HoveredCell;
			if (HoveredSurfaceGrid->GetHoveredCell(HoveredCell))
			{
				bHasLastPublishedHoveredCellInfo = false;
				LastPublishedHoveredSurfaceGrid = nullptr;
				LastPublishedHoveredCellId = FSRPlanetSurfaceGridCellId();
				PublishHoveredCellInfo(HoveredSurfaceGrid, HoveredCell);
			}
		}
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
		DestroyStructureGhostPreview();
	}
}

void USRAssemblyComponent::ResetHoverSampleCache()
{
	LastHoveredSampleSurfaceGrid = nullptr;
	LastHoveredSampleMousePosition = FVector2D::ZeroVector;
	bHasLastHoveredSampleMousePosition = false;
}

void USRAssemblyComponent::PublishHoveredCellInfo(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& HoveredCell)
{
	ASRPlayerController* PlayerController = GetOwnerController();
	if (!PlayerController || !IsValid(SurfaceGrid))
	{
		return;
	}

	if (bHasLastPublishedHoveredCellInfo
		&& LastPublishedHoveredSurfaceGrid == SurfaceGrid
		&& LastPublishedHoveredCellId == HoveredCell.CellId)
	{
		return;
	}

	FSRPlanetSurfaceGridCellInfo HoveredCellInfo;
	if (!SurfaceGrid->GetCellInfoById(HoveredCell.CellId, HoveredCellInfo))
	{
		ClearPublishedHoveredCellInfo();
		return;
	}

	bHasLastPublishedHoveredCellInfo = true;
	LastPublishedHoveredSurfaceGrid = SurfaceGrid;
	LastPublishedHoveredCellId = HoveredCell.CellId;
	PlayerController->SetHoveredSurfaceCellInfo(true, HoveredCellInfo);
}

void USRAssemblyComponent::ClearPublishedHoveredCellInfo()
{
	if (!bHasLastPublishedHoveredCellInfo)
	{
		return;
	}

	bHasLastPublishedHoveredCellInfo = false;
	LastPublishedHoveredSurfaceGrid = nullptr;
	LastPublishedHoveredCellId = FSRPlanetSurfaceGridCellId();
	if (ASRPlayerController* PlayerController = GetOwnerController())
	{
		PlayerController->SetHoveredSurfaceCellInfo(false, FSRPlanetSurfaceGridCellInfo());
	}
}

void USRAssemblyComponent::UpdateStructureGhostPreview()
{
	ASRPlayerController* PlayerController = GetOwnerController();
	USRStructureDataAsset* SelectedStructureDataAsset = PlayerController ? PlayerController->GetSelectedStructureDataAsset() : nullptr;
	if (!bAssemblyModeActive || !IsValid(PlayerController) || !IsValid(SelectedStructureDataAsset) || !IsValid(HoveredSurfaceGrid))
	{
		DestroyStructureGhostPreview();
		return;
	}

	FSRPlanetSurfaceGridCell HoveredCell;
	if (!HoveredSurfaceGrid->GetHoveredCell(HoveredCell))
	{
		DestroyStructureGhostPreview();
		return;
	}

	FSRPlanetSurfaceGridCellInfo HoveredCellInfo;
	if (!HoveredSurfaceGrid->GetCellInfoById(HoveredCell.CellId, HoveredCellInfo)
		|| !HoveredCellInfo.bCanConstruct
		|| HoveredCellInfo.bOccupied)
	{
		DestroyStructureGhostPreview();
		return;
	}

	const FSRStructureData StructureData = SelectedStructureDataAsset->BuildData();
	if (StructureData.BuildKind == ESRStructureBuildKind::Conveyor)
	{
		DestroyStructureGhostPreview();
		return;
	}

	TArray<FSRPlanetSurfaceGridCellId> FootprintCellIds;
	if (!HoveredSurfaceGrid->GetFootprintCellIds(HoveredCell.CellId, StructureData.FootprintCellsX, StructureData.FootprintCellsY, FootprintCellIds)
		|| !HoveredSurfaceGrid->CanOccupyCells(FootprintCellIds))
	{
		DestroyStructureGhostPreview();
		return;
	}

	UClass* StructureActorClass = StructureData.StructureActorClass.Get();
	if (!IsValid(StructureActorClass))
	{
		StructureActorClass = ASRStructure::StaticClass();
	}
	if (!IsValid(StructureActorClass))
	{
		LogInvalidGhostDataAssetOnce(SelectedStructureDataAsset, TEXT("StructureActorClass is not set"));
		DestroyStructureGhostPreview();
		return;
	}

	if (!StructureActorClass->ImplementsInterface(USRBuildableStructureInterface::StaticClass()))
	{
		LogInvalidGhostDataAssetOnce(SelectedStructureDataAsset, TEXT("StructureActorClass does not implement ISRBuildableStructureInterface"));
		DestroyStructureGhostPreview();
		return;
	}

	FTransform GhostTransform;
	if (!BuildStructureGhostTransform(HoveredSurfaceGrid, HoveredCell.CellId, SelectedStructureDataAsset, GhostTransform))
	{
		DestroyStructureGhostPreview();
		return;
	}

	const bool bNeedsNewGhostActor = !IsValid(StructureGhostActor)
		|| StructureGhostDataAsset != SelectedStructureDataAsset
		|| StructureGhostActor->GetClass() != StructureActorClass;
	if (bNeedsNewGhostActor)
	{
		DestroyStructureGhostPreview();

		UWorld* World = GetWorld();
		if (!World)
		{
			return;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = PlayerController;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		StructureGhostActor = World->SpawnActor<AActor>(StructureActorClass, GhostTransform, SpawnParameters);
		if (!IsValid(StructureGhostActor))
		{
			return;
		}

		StructureGhostDataAsset = SelectedStructureDataAsset;
		ISRBuildableStructureInterface::Execute_ApplyStructureDataAsset(StructureGhostActor, SelectedStructureDataAsset);
		ISRBuildableStructureInterface::Execute_SetStructureGhostMode(StructureGhostActor, true);
		if (!ISRBuildableStructureInterface::Execute_CanPlaceOnSurfaceCell(StructureGhostActor, HoveredCellInfo))
		{
			DestroyStructureGhostPreview();
			return;
		}
		StructureGhostActor->SetActorHiddenInGame(false);
		LastLoggedInvalidGhostDataAsset = nullptr;
	}
	else if (!ISRBuildableStructureInterface::Execute_CanPlaceOnSurfaceCell(StructureGhostActor, HoveredCellInfo))
	{
		DestroyStructureGhostPreview();
		return;
	}

	StructureGhostActor->SetActorTransform(GhostTransform);
	StructureGhostActor->SetActorHiddenInGame(false);
	PublishStructureGhostPlacementDebug(
		HoveredSurfaceGrid,
		HoveredCell,
		GhostTransform,
		StructureData.ConstructionHeightOffset,
		!bHasStructureGhostCellId || !(StructureGhostCellId == HoveredCell.CellId));
	StructureGhostCellId = HoveredCell.CellId;
	bHasStructureGhostCellId = true;
}

void USRAssemblyComponent::DestroyStructureGhostPreview()
{
	if (IsValid(StructureGhostActor))
	{
		StructureGhostActor->Destroy();
	}

	StructureGhostActor = nullptr;
	StructureGhostDataAsset = nullptr;
	StructureGhostCellId = FSRPlanetSurfaceGridCellId();
	bHasStructureGhostCellId = false;
}

bool USRAssemblyComponent::BuildStructureGhostTransform(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& CellId,
	USRStructureDataAsset* StructureDataAsset,
	FTransform& OutTransform) const
{
	OutTransform = FTransform::Identity;
	if (!IsValid(SurfaceGrid) || !IsValid(StructureDataAsset))
	{
		return false;
	}

	return USRStructurePlacementLibrary::BuildStructurePlacementTransform(SurfaceGrid, CellId, StructureDataAsset, OutTransform);
}

void USRAssemblyComponent::PublishStructureGhostPlacementDebug(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCell& HoveredCell,
	const FTransform& GhostTransform,
	float StructureHeightOffset,
	bool bLogDebug) const
{
	if (!IsValid(SurfaceGrid))
	{
		return;
	}

	const AActor* SurfaceOwner = SurfaceGrid->GetOwner();
	if (!IsValid(SurfaceOwner))
	{
		return;
	}

	const FTransform& OwnerTransform = SurfaceOwner->GetActorTransform();
	const FTransform& GridTransform = SurfaceGrid->GetComponentTransform();
	const FVector CellWorldPosition = GridTransform.TransformPosition(HoveredCell.LocalCenter);
	const FVector GhostWorldPosition = GhostTransform.GetLocation();
	const FVector CellPlanetLocalPosition = OwnerTransform.InverseTransformPosition(CellWorldPosition);
	const FVector GhostPlanetLocalPosition = OwnerTransform.InverseTransformPosition(GhostWorldPosition);
	const FVector PlanetLocalDelta = GhostPlanetLocalPosition - CellPlanetLocalPosition;
	const FVector CellPlanetLocalNormal = OwnerTransform.InverseTransformVectorNoScale(
		GridTransform.TransformVectorNoScale(HoveredCell.LocalNormal)).GetSafeNormal();
	const float NormalDelta = CellPlanetLocalNormal.IsNearlyZero()
		? 0.0f
		: FVector::DotProduct(PlanetLocalDelta, CellPlanetLocalNormal);

	const FString DebugText = FString::Printf(
		TEXT("Structure Ghost Debug | Cell=(%d,%d,%d)\nCell Local=%s\nGhost Local=%s\nDelta=%s\nStructureOffset=%.3f | NormalDelta=%.3f"),
		static_cast<int32>(HoveredCell.CellId.Face),
		HoveredCell.CellId.CellX,
		HoveredCell.CellId.CellY,
		*CellPlanetLocalPosition.ToCompactString(),
		*GhostPlanetLocalPosition.ToCompactString(),
		*PlanetLocalDelta.ToCompactString(),
		StructureHeightOffset,
		NormalDelta);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			StructureGhostPlacementDebugMessageKey,
			0.1f,
			FColor::Cyan,
			DebugText);
	}

	if (bLogDebug)
	{
		UE_LOG(LogTemp, Display, TEXT("%s"), *DebugText);
	}
}

bool USRAssemblyComponent::TryResolveStructurePlacementDragTarget(
	AActor*& OutFocusedActor,
	USRPlanetSurfaceGrid*& OutSurfaceGrid,
	FSRPlanetSurfaceGridCell& OutTargetCell) const
{
	OutFocusedActor = nullptr;
	OutSurfaceGrid = nullptr;
	OutTargetCell = FSRPlanetSurfaceGridCell();

	const ASRPlayerController* PlayerController = GetOwnerController();
	if (!PlayerController || !bAssemblyModeActive || !IsValid(PlayerController->GetSelectedStructureDataAsset()))
	{
		return false;
	}

	FVector HoverHitLocation = FVector::ZeroVector;
	return TryGetFocusedSurfaceGrid(OutFocusedActor, OutSurfaceGrid)
		&& TryProjectCursorToSurfaceCell(OutSurfaceGrid, OutTargetCell, HoverHitLocation);
}

bool USRAssemblyComponent::TryGetFocusedConveyorNetwork(AActor*& OutFocusedActor, USRConveyorNetworkComponent*& OutConveyorNetwork) const
{
	OutFocusedActor = nullptr;
	OutConveyorNetwork = nullptr;

	USRPlanetSurfaceGrid* UnusedSurfaceGrid = nullptr;
	if (!TryGetFocusedSurfaceGrid(OutFocusedActor, UnusedSurfaceGrid) || !IsValid(OutFocusedActor))
	{
		return false;
	}

	OutConveyorNetwork = OutFocusedActor->FindComponentByClass<USRConveyorNetworkComponent>();
	return IsValid(OutConveyorNetwork);
}

void USRAssemblyComponent::EnqueueStructurePlacement(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& CellId)
{
	if (!IsValid(SurfaceGrid))
	{
		return;
	}

	for (const FSRQueuedStructurePlacement& PendingPlacement : PendingStructurePlacementQueue)
	{
		if (PendingPlacement.SurfaceGrid.Get() == SurfaceGrid && PendingPlacement.CellId == CellId)
		{
			return;
		}
	}

	const int32 MaxQueueSize = FMath::Max(1, MaxQueuedStructurePlacements);
	if (PendingStructurePlacementQueue.Num() >= MaxQueueSize)
	{
		PendingStructurePlacementQueue.RemoveAt(0, PendingStructurePlacementQueue.Num() - MaxQueueSize + 1, EAllowShrinking::No);
	}

	FSRQueuedStructurePlacement QueuedPlacement;
	QueuedPlacement.SurfaceGrid = SurfaceGrid;
	QueuedPlacement.CellId = CellId;
	PendingStructurePlacementQueue.Add(QueuedPlacement);
}

bool USRAssemblyComponent::TryPlaceStructureDragPath(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& TargetCell)
{
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	ASRPlayerController* PlayerController = GetOwnerController();
	USRStructureDataAsset* SelectedStructureDataAsset = PlayerController ? PlayerController->GetSelectedStructureDataAsset() : nullptr;
	if (IsValid(SelectedStructureDataAsset) && SelectedStructureDataAsset->BuildData().BuildKind == ESRStructureBuildKind::Conveyor)
	{
		return TryPlaceConveyorDragPath(SurfaceGrid, TargetCell, SelectedStructureDataAsset);
	}

	if (LastStructurePlacementDragSurfaceGrid == SurfaceGrid
		&& bHasLastStructurePlacementDragCellId
		&& LastStructurePlacementDragCellId == TargetCell.CellId)
	{
		return true;
	}

	TArray<FSRPlanetSurfaceGridCellId> PathCellIds;
	if (LastStructurePlacementDragSurfaceGrid == SurfaceGrid && bHasLastStructurePlacementDragCellId)
	{
		AppendGridLineCellIds(LastStructurePlacementDragCellId, TargetCell.CellId, PathCellIds);
	}
	else
	{
		PathCellIds.Add(TargetCell.CellId);
	}

	for (const FSRPlanetSurfaceGridCellId& PathCellId : PathCellIds)
	{
		if (LastStructurePlacementDragSurfaceGrid == SurfaceGrid
			&& bHasLastStructurePlacementDragCellId
			&& LastStructurePlacementDragCellId == PathCellId)
		{
			continue;
		}

		EnqueueStructurePlacement(SurfaceGrid, PathCellId);
	}

	SurfaceGrid->SetHoveredCell(TargetCell.CellId);
	PublishHoveredCellInfo(SurfaceGrid, TargetCell);
	LastStructurePlacementDragSurfaceGrid = SurfaceGrid;
	LastStructurePlacementDragCellId = TargetCell.CellId;
	bHasLastStructurePlacementDragCellId = true;
	return !PathCellIds.IsEmpty();
}

bool USRAssemblyComponent::TryPlaceConveyorDragPath(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& TargetCell, USRStructureDataAsset* ConveyorDataAsset)
{
	if (!IsValid(SurfaceGrid) || !IsValid(ConveyorDataAsset))
	{
		return false;
	}

	AActor* FocusedActor = nullptr;
	USRConveyorNetworkComponent* ConveyorNetwork = nullptr;
	if (!TryGetFocusedConveyorNetwork(FocusedActor, ConveyorNetwork))
	{
		return false;
	}

	if (LastStructurePlacementDragSurfaceGrid == SurfaceGrid
		&& bHasLastStructurePlacementDragCellId
		&& LastStructurePlacementDragCellId == TargetCell.CellId)
	{
		return true;
	}

	const FSRStructureData ConveyorData = ConveyorDataAsset->BuildData();
	TArray<FSRPlanetSurfaceGridCellId> PathCellIds;
	if (LastStructurePlacementDragSurfaceGrid == SurfaceGrid && bHasLastStructurePlacementDragCellId)
	{
		if (!ConveyorNetwork->FindConveyorPath(SurfaceGrid, LastStructurePlacementDragCellId, TargetCell.CellId, ConveyorData.ConveyorLayer, PathCellIds))
		{
			return false;
		}
	}
	else
	{
		PathCellIds.Add(TargetCell.CellId);
	}

	const FName NetworkId = FName(*FString::Printf(TEXT("Conveyor_%s_%d"), *GetNameSafe(FocusedActor), static_cast<int32>(ConveyorData.ConveyorLayer)));
	if (!ConveyorNetwork->TryPlaceConveyorPath(
		SurfaceGrid,
		PathCellIds,
		ConveyorData.ConveyorLayer,
		ConveyorData.ConveyorLayerHeight,
		ConveyorDataAsset,
		NetworkId))
	{
		return false;
	}

	SurfaceGrid->SetHoveredCell(TargetCell.CellId);
	PublishHoveredCellInfo(SurfaceGrid, TargetCell);
	LastStructurePlacementDragSurfaceGrid = SurfaceGrid;
	LastStructurePlacementDragCellId = TargetCell.CellId;
	bHasLastStructurePlacementDragCellId = true;
	return true;
}

bool USRAssemblyComponent::TryPlaceSelectedStructure(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& TargetCell, bool bRefreshPreviewAndUI)
{
	ASRPlayerController* PlayerController = GetOwnerController();
	USRStructureDataAsset* SelectedStructureDataAsset = PlayerController ? PlayerController->GetSelectedStructureDataAsset() : nullptr;
	if (!IsValid(PlayerController) || !IsValid(SurfaceGrid) || !IsValid(SelectedStructureDataAsset))
	{
		return false;
	}

	const FSRStructureData StructureData = SelectedStructureDataAsset->BuildData();
	if (StructureData.BuildKind == ESRStructureBuildKind::Conveyor)
	{
		return TryPlaceSelectedConveyor(SurfaceGrid, TargetCell, SelectedStructureDataAsset, bRefreshPreviewAndUI);
	}

	TArray<FSRPlanetSurfaceGridCellId> FootprintCellIds;
	if (!SurfaceGrid->GetFootprintCellIds(TargetCell.CellId, StructureData.FootprintCellsX, StructureData.FootprintCellsY, FootprintCellIds)
		|| !SurfaceGrid->CanOccupyCells(FootprintCellIds))
	{
		if (bRefreshPreviewAndUI)
		{
			DestroyStructureGhostPreview();
		}
		return false;
	}

	if (AActor* SurfaceOwner = SurfaceGrid->GetOwner())
	{
		if (USRStructureInstanceManagerComponent* StructureInstanceManager = SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>())
		{
			FName OccupantId = NAME_None;
			if (StructureInstanceManager->TryPlaceStructureOnSurfaceGrid(SurfaceGrid, TargetCell.CellId, SelectedStructureDataAsset, OccupantId, false))
			{
				if (bRefreshPreviewAndUI)
				{
					DestroyStructureGhostPreview();

					bHasLastPublishedHoveredCellInfo = false;
					LastPublishedHoveredSurfaceGrid = nullptr;
					LastPublishedHoveredCellId = FSRPlanetSurfaceGridCellId();
					PublishHoveredCellInfo(SurfaceGrid, TargetCell);
				}
				return true;
			}
		}
	}

	AActor* PlacedStructureActor = nullptr;
	if (!USRStructurePlacementLibrary::TryPlaceStructureOnSurfaceGrid(SurfaceGrid, TargetCell.CellId, SelectedStructureDataAsset, PlacedStructureActor))
	{
		return false;
	}

	if (bRefreshPreviewAndUI)
	{
		DestroyStructureGhostPreview();

		bHasLastPublishedHoveredCellInfo = false;
		LastPublishedHoveredSurfaceGrid = nullptr;
		LastPublishedHoveredCellId = FSRPlanetSurfaceGridCellId();
		PublishHoveredCellInfo(SurfaceGrid, TargetCell);
	}
	return true;
}

bool USRAssemblyComponent::TryPlaceSelectedConveyor(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& TargetCell, USRStructureDataAsset* ConveyorDataAsset, bool bRefreshPreviewAndUI)
{
	if (!IsValid(SurfaceGrid) || !IsValid(ConveyorDataAsset))
	{
		return false;
	}

	AActor* FocusedActor = nullptr;
	USRConveyorNetworkComponent* ConveyorNetwork = nullptr;
	if (!TryGetFocusedConveyorNetwork(FocusedActor, ConveyorNetwork))
	{
		return false;
	}

	const FSRStructureData ConveyorData = ConveyorDataAsset->BuildData();
	const TArray<FSRPlanetSurfaceGridCellId> PathCellIds = { TargetCell.CellId };
	const FName NetworkId = FName(*FString::Printf(TEXT("Conveyor_%s_%d"), *GetNameSafe(FocusedActor), static_cast<int32>(ConveyorData.ConveyorLayer)));
	if (!ConveyorNetwork->TryPlaceConveyorPath(
		SurfaceGrid,
		PathCellIds,
		ConveyorData.ConveyorLayer,
		ConveyorData.ConveyorLayerHeight,
		ConveyorDataAsset,
		NetworkId))
	{
		return false;
	}

	if (bRefreshPreviewAndUI)
	{
		DestroyStructureGhostPreview();
		bHasLastPublishedHoveredCellInfo = false;
		LastPublishedHoveredSurfaceGrid = nullptr;
		LastPublishedHoveredCellId = FSRPlanetSurfaceGridCellId();
		PublishHoveredCellInfo(SurfaceGrid, TargetCell);
	}
	return true;
}

bool USRAssemblyComponent::TryPlaceSelectedConveyorPath(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& StartCellId, const FSRPlanetSurfaceGridCell& TargetCell, USRStructureDataAsset* ConveyorDataAsset, bool bRefreshPreviewAndUI)
{
	if (!IsValid(SurfaceGrid) || !IsValid(ConveyorDataAsset))
	{
		return false;
	}

	AActor* FocusedActor = nullptr;
	USRConveyorNetworkComponent* ConveyorNetwork = nullptr;
	if (!TryGetFocusedConveyorNetwork(FocusedActor, ConveyorNetwork))
	{
		return false;
	}

	const FSRStructureData ConveyorData = ConveyorDataAsset->BuildData();
	TArray<FSRPlanetSurfaceGridCellId> PathCellIds;
	if (!ConveyorNetwork->FindConveyorPath(SurfaceGrid, StartCellId, TargetCell.CellId, ConveyorData.ConveyorLayer, PathCellIds))
	{
		return false;
	}

	const FName NetworkId = FName(*FString::Printf(TEXT("Conveyor_%s_%d"), *GetNameSafe(FocusedActor), static_cast<int32>(ConveyorData.ConveyorLayer)));
	if (!ConveyorNetwork->TryPlaceConveyorPath(
		SurfaceGrid,
		PathCellIds,
		ConveyorData.ConveyorLayer,
		ConveyorData.ConveyorLayerHeight,
		ConveyorDataAsset,
		NetworkId))
	{
		return false;
	}

	if (bRefreshPreviewAndUI)
	{
		DestroyStructureGhostPreview();
		bHasLastPublishedHoveredCellInfo = false;
		LastPublishedHoveredSurfaceGrid = nullptr;
		LastPublishedHoveredCellId = FSRPlanetSurfaceGridCellId();
		PublishHoveredCellInfo(SurfaceGrid, TargetCell);
	}
	return true;
}

bool USRAssemblyComponent::TryDeleteStructureAtCell(AActor* FocusedActor, USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& TargetCellId)
{
	if (!IsValid(FocusedActor) || !IsValid(SurfaceGrid))
	{
		return false;
	}

	USRConveyorNetworkComponent* ConveyorNetwork = FocusedActor->FindComponentByClass<USRConveyorNetworkComponent>();
	if (IsValid(ConveyorNetwork))
	{
		TArray<int32> CandidateConveyorLayers;
		if (const ASRPlayerController* PlayerController = GetOwnerController())
		{
			if (USRStructureDataAsset* SelectedStructureDataAsset = PlayerController->GetSelectedStructureDataAsset())
			{
				const FSRStructureData SelectedStructureData = SelectedStructureDataAsset->BuildData();
				if (SelectedStructureData.BuildKind == ESRStructureBuildKind::Conveyor)
				{
					CandidateConveyorLayers.Add(FMath::Max(0, SelectedStructureData.ConveyorLayer));
				}
			}
		}
		CandidateConveyorLayers.AddUnique(0);

		for (const int32 CandidateLayer : CandidateConveyorLayers)
		{
			if (ConveyorNetwork->TryRemoveConveyorAtCell(SurfaceGrid, TargetCellId, CandidateLayer))
			{
				return true;
			}
		}
	}

	FSRPlanetSurfaceGridCellInfo TargetCellInfo;
	if (!SurfaceGrid->GetCellInfoById(TargetCellId, TargetCellInfo) || !TargetCellInfo.bOccupied || TargetCellInfo.OccupantId.IsNone())
	{
		return false;
	}

	if (USRStructureInstanceManagerComponent* StructureInstanceManager = FocusedActor->FindComponentByClass<USRStructureInstanceManagerComponent>())
	{
		if (StructureInstanceManager->TryRemoveStructureAtCell(SurfaceGrid, TargetCellId))
		{
			return true;
		}
	}

	TArray<FSRPlanetSurfaceGridCellId> OccupantCellIds;
	for (const FSRPlanetSurfaceGridCell& Cell : SurfaceGrid->GetCells())
	{
		if (Cell.bOccupied && Cell.OccupantId == TargetCellInfo.OccupantId)
		{
			OccupantCellIds.Add(Cell.CellId);
		}
	}

	if (OccupantCellIds.IsEmpty())
	{
		OccupantCellIds.Add(TargetCellId);
	}

	TryDestroyAttachedOccupantActor(FocusedActor, TargetCellInfo.OccupantId);
	return SurfaceGrid->SetCellsOccupied(OccupantCellIds, false, NAME_None);
}

bool USRAssemblyComponent::TryDestroyAttachedOccupantActor(AActor* SurfaceOwner, FName OccupantId) const
{
	if (!IsValid(SurfaceOwner) || OccupantId.IsNone())
	{
		return false;
	}

	TArray<AActor*> AttachedActors;
	SurfaceOwner->GetAttachedActors(AttachedActors);
	for (AActor* AttachedActor : AttachedActors)
	{
		if (IsValid(AttachedActor) && AttachedActor->GetFName() == OccupantId)
		{
			AttachedActor->Destroy();
			return true;
		}
	}

	return false;
}

void USRAssemblyComponent::ClearPendingConveyorPathStart()
{
	if (IsValid(PendingConveyorStartSurfaceGrid))
	{
		PendingConveyorStartSurfaceGrid->ClearSelectedCell();
	}

	PendingConveyorStartSurfaceGrid = nullptr;
	PendingConveyorStartCellId = FSRPlanetSurfaceGridCellId();
	bHasPendingConveyorStartCell = false;
}

void USRAssemblyComponent::LogInvalidGhostDataAssetOnce(USRStructureDataAsset* StructureDataAsset, const TCHAR* Reason)
{
	if (!IsValid(StructureDataAsset) || LastLoggedInvalidGhostDataAsset == StructureDataAsset)
	{
		return;
	}

	LastLoggedInvalidGhostDataAsset = StructureDataAsset;
	UE_LOG(
		LogTemp,
		Error,
		TEXT("Structure ghost preview cannot use StructureDataAsset '%s': %s."),
		*GetNameSafe(StructureDataAsset),
		Reason ? Reason : TEXT("Invalid structure data"));
}
