#include "Assembly/SRAssemblyComponent.h"

#include "Camera/SRPlayerController.h"
#include "Conveyor/SRConveyorNetworkComponent.h"
#include "GameFramework/Actor.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	struct FSRAssemblyAreaFaceBridge
	{
		FSRPlanetSurfaceGridCellId StartFaceCellId;
		FSRPlanetSurfaceGridCellId EndFaceCellId;
	};

	void AppendAssemblyAreaRectCellIds(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& CornerA,
		const FSRPlanetSurfaceGridCellId& CornerB,
		TArray<FSRPlanetSurfaceGridCellId>& OutCellIds)
	{
		if (!IsValid(SurfaceGrid) || CornerA.Face != CornerB.Face)
		{
			return;
		}

		const int32 MinX = FMath::Min(CornerA.CellX, CornerB.CellX);
		const int32 MaxX = FMath::Max(CornerA.CellX, CornerB.CellX);
		const int32 MinY = FMath::Min(CornerA.CellY, CornerB.CellY);
		const int32 MaxY = FMath::Max(CornerA.CellY, CornerB.CellY);

		for (int32 CellY = MinY; CellY <= MaxY; ++CellY)
		{
			for (int32 CellX = MinX; CellX <= MaxX; ++CellX)
			{
				FSRPlanetSurfaceGridCellId CellId;
				CellId.Face = CornerA.Face;
				CellId.CellX = CellX;
				CellId.CellY = CellY;

				FSRPlanetSurfaceGridCell Cell;
				if (SurfaceGrid->GetCellById(CellId, Cell))
				{
					OutCellIds.AddUnique(CellId);
				}
			}
		}
	}

	void CollectAssemblyAreaFaceBridges(
		USRPlanetSurfaceGrid* SurfaceGrid,
		ESRCubeSphereFace StartFace,
		ESRCubeSphereFace EndFace,
		TArray<FSRAssemblyAreaFaceBridge>& OutBridges)
	{
		OutBridges.Reset();
		if (!IsValid(SurfaceGrid) || StartFace == EndFace)
		{
			return;
		}

		const int32 FaceResolution = SurfaceGrid->GetFaceResolution();
		if (FaceResolution <= 0)
		{
			return;
		}

		for (int32 CellY = 0; CellY < FaceResolution; ++CellY)
		{
			for (int32 CellX = 0; CellX < FaceResolution; ++CellX)
			{
				FSRPlanetSurfaceGridCellId StartCellId;
				StartCellId.Face = StartFace;
				StartCellId.CellX = CellX;
				StartCellId.CellY = CellY;

				FSRPlanetSurfaceGridCellNeighbors Neighbors;
				if (!SurfaceGrid->GetCellNeighbors(StartCellId, Neighbors))
				{
					continue;
				}

				const FSRPlanetSurfaceGridCellId NeighborCellIds[] =
				{
					Neighbors.NegativeU,
					Neighbors.PositiveU,
					Neighbors.NegativeV,
					Neighbors.PositiveV,
				};

				for (const FSRPlanetSurfaceGridCellId& NeighborCellId : NeighborCellIds)
				{
					if (NeighborCellId.Face != EndFace)
					{
						continue;
					}

					FSRAssemblyAreaFaceBridge Bridge;
					Bridge.StartFaceCellId = StartCellId;
					Bridge.EndFaceCellId = NeighborCellId;
					OutBridges.Add(Bridge);
				}
			}
		}
	}

	bool AreAssemblyAreaBridgeCoordsConstant(
		const TArray<FSRAssemblyAreaFaceBridge>& Bridges,
		bool bUseStartFace,
		bool bUseX)
	{
		if (Bridges.IsEmpty())
		{
			return false;
		}

		const FSRPlanetSurfaceGridCellId& FirstCellId = bUseStartFace ? Bridges[0].StartFaceCellId : Bridges[0].EndFaceCellId;
		const int32 FirstCoord = bUseX ? FirstCellId.CellX : FirstCellId.CellY;
		for (const FSRAssemblyAreaFaceBridge& Bridge : Bridges)
		{
			const FSRPlanetSurfaceGridCellId& CellId = bUseStartFace ? Bridge.StartFaceCellId : Bridge.EndFaceCellId;
			const int32 Coord = bUseX ? CellId.CellX : CellId.CellY;
			if (Coord != FirstCoord)
			{
				return false;
			}
		}

		return true;
	}

	int32 GetAssemblyAreaBridgeAxisCoord(const FSRPlanetSurfaceGridCellId& CellId, bool bUseY)
	{
		return bUseY ? CellId.CellY : CellId.CellX;
	}

	const FSRAssemblyAreaFaceBridge* FindClosestAssemblyAreaBridgeByAxisCoord(
		const TArray<FSRAssemblyAreaFaceBridge>& Bridges,
		bool bUseStartFace,
		bool bUseY,
		int32 TargetCoord)
	{
		const FSRAssemblyAreaFaceBridge* BestBridge = nullptr;
		int32 BestDistance = MAX_int32;
		for (const FSRAssemblyAreaFaceBridge& Bridge : Bridges)
		{
			const FSRPlanetSurfaceGridCellId& CellId = bUseStartFace ? Bridge.StartFaceCellId : Bridge.EndFaceCellId;
			const int32 Distance = FMath::Abs(GetAssemblyAreaBridgeAxisCoord(CellId, bUseY) - TargetCoord);
			if (Distance < BestDistance)
			{
				BestBridge = &Bridge;
				BestDistance = Distance;
			}
		}

		return BestBridge;
	}
}

bool USRAssemblyComponent::ShouldHandleAreaSelectionDrag() const
{
	const ASRPlayerController* PlayerController = GetOwnerController();
	if (!bAssemblyModeActive
		|| bIsAreaCopyPlacementActive
		|| !IsValid(PlayerController)
		|| PlayerController->IsPointerOverBlockingUi())
	{
		return false;
	}

	return PlayerController->IsAssemblyShiftModifierActive();
}

bool USRAssemblyComponent::BeginAreaSelectionDrag(AActor*& OutSelectedActor)
{
	OutSelectedActor = nullptr;
	if (!ShouldHandleAreaSelectionDrag())
	{
		return false;
	}

	EndStructurePlacementDrag(false);
	ClearAreaSelection();
	ClearAreaDeletion();

	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* SurfaceGrid = nullptr;
	FSRPlanetSurfaceGridCell TargetCell;
	FVector HitLocation = FVector::ZeroVector;
	if (!TryGetFocusedSurfaceGrid(FocusedActor, SurfaceGrid)
		|| !TryProjectCursorToSurfaceCell(SurfaceGrid, TargetCell, HitLocation))
	{
		return false;
	}

	bIsAreaSelectionDragActive = true;
	AreaSelectionSurfaceGrid = SurfaceGrid;
	AreaSelectionStartCellId = TargetCell.CellId;
	LastAreaSelectionTargetCellId = FSRPlanetSurfaceGridCellId();
	bHasAreaSelectionStartCell = true;
	bHasLastAreaSelectionTargetCell = false;
	OutSelectedActor = FocusedActor;
	return UpdateAreaSelection(SurfaceGrid, TargetCell);
}

bool USRAssemblyComponent::ContinueAreaSelectionDrag(AActor*& OutSelectedActor)
{
	OutSelectedActor = nullptr;
	if (!bIsAreaSelectionDragActive || !bHasAreaSelectionStartCell)
	{
		return false;
	}

	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* SurfaceGrid = nullptr;
	FSRPlanetSurfaceGridCell TargetCell;
	FVector HitLocation = FVector::ZeroVector;
	if (!TryGetFocusedSurfaceGrid(FocusedActor, SurfaceGrid)
		|| SurfaceGrid != AreaSelectionSurfaceGrid
		|| !TryProjectCursorToSurfaceCell(SurfaceGrid, TargetCell, HitLocation))
	{
		return false;
	}

	OutSelectedActor = FocusedActor;
	return UpdateAreaSelection(SurfaceGrid, TargetCell);
}

void USRAssemblyComponent::EndAreaSelectionDrag()
{
	bIsAreaSelectionDragActive = false;
}

void USRAssemblyComponent::ClearAreaSelection()
{
	bIsAreaSelectionDragActive = false;

	if (IsValid(AreaSelectionSurfaceGrid))
	{
		AreaSelectionSurfaceGrid->ClearAreaSelectionCells();

		if (AActor* SurfaceOwner = AreaSelectionSurfaceGrid->GetOwner())
		{
			if (USRStructureInstanceManagerComponent* StructureInstanceManager = SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>())
			{
				StructureInstanceManager->ClearGhostedStructures();
			}
		}
	}

	AreaSelectionSurfaceGrid = nullptr;
	AreaSelectionStartCellId = FSRPlanetSurfaceGridCellId();
	LastAreaSelectionTargetCellId = FSRPlanetSurfaceGridCellId();
	bHasAreaSelectionStartCell = false;
	bHasLastAreaSelectionTargetCell = false;
	AreaSelectionCellIds.Reset();
}

bool USRAssemblyComponent::TryDeleteAreaSelection()
{
	if (!bAssemblyModeActive || !IsValid(AreaSelectionSurfaceGrid) || AreaSelectionCellIds.IsEmpty())
	{
		return false;
	}

	if (!DeleteAreaCells(AreaSelectionSurfaceGrid, AreaSelectionCellIds))
	{
		return false;
	}

	ClearAreaSelection();
	return true;
}

bool USRAssemblyComponent::BuildAreaSelectionCellIds(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& StartCellId,
	const FSRPlanetSurfaceGridCellId& EndCellId,
	TArray<FSRPlanetSurfaceGridCellId>& OutCellIds) const
{
	OutCellIds.Reset();
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	if (StartCellId.Face == EndCellId.Face)
	{
		AppendAssemblyAreaRectCellIds(SurfaceGrid, StartCellId, EndCellId, OutCellIds);
		return !OutCellIds.IsEmpty();
	}

	TArray<FSRAssemblyAreaFaceBridge> Bridges;
	CollectAssemblyAreaFaceBridges(SurfaceGrid, StartCellId.Face, EndCellId.Face, Bridges);
	if (Bridges.IsEmpty())
	{
		return false;
	}

	const bool bStartEdgeXConstant = AreAssemblyAreaBridgeCoordsConstant(Bridges, true, true);
	const bool bStartEdgeYConstant = AreAssemblyAreaBridgeCoordsConstant(Bridges, true, false);
	const bool bEndEdgeXConstant = AreAssemblyAreaBridgeCoordsConstant(Bridges, false, true);
	const bool bEndEdgeYConstant = AreAssemblyAreaBridgeCoordsConstant(Bridges, false, false);
	if ((!bStartEdgeXConstant && !bStartEdgeYConstant) || (!bEndEdgeXConstant && !bEndEdgeYConstant))
	{
		return false;
	}

	const bool bStartBridgeAxisUseY = bStartEdgeXConstant;
	const bool bEndBridgeAxisUseY = bEndEdgeXConstant;
	const int32 StartAxisCoord = GetAssemblyAreaBridgeAxisCoord(StartCellId, bStartBridgeAxisUseY);
	const int32 EndAxisCoord = GetAssemblyAreaBridgeAxisCoord(EndCellId, bEndBridgeAxisUseY);

	const FSRAssemblyAreaFaceBridge* BridgeForStartAxis = FindClosestAssemblyAreaBridgeByAxisCoord(
		Bridges,
		true,
		bStartBridgeAxisUseY,
		StartAxisCoord);
	const FSRAssemblyAreaFaceBridge* BridgeForEndAxis = FindClosestAssemblyAreaBridgeByAxisCoord(
		Bridges,
		false,
		bEndBridgeAxisUseY,
		EndAxisCoord);
	if (!BridgeForStartAxis || !BridgeForEndAxis)
	{
		return false;
	}

	AppendAssemblyAreaRectCellIds(SurfaceGrid, StartCellId, BridgeForEndAxis->StartFaceCellId, OutCellIds);
	AppendAssemblyAreaRectCellIds(SurfaceGrid, BridgeForStartAxis->EndFaceCellId, EndCellId, OutCellIds);
	return !OutCellIds.IsEmpty();
}

bool USRAssemblyComponent::UpdateAreaSelection(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& TargetCell)
{
	if (!IsValid(SurfaceGrid) || !bHasAreaSelectionStartCell)
	{
		return false;
	}

	if (bHasLastAreaSelectionTargetCell && LastAreaSelectionTargetCellId == TargetCell.CellId)
	{
		return true;
	}

	TArray<FSRPlanetSurfaceGridCellId> NewAreaSelectionCellIds;
	if (!BuildAreaSelectionCellIds(SurfaceGrid, AreaSelectionStartCellId, TargetCell.CellId, NewAreaSelectionCellIds))
	{
		return false;
	}

	AreaSelectionCellIds = MoveTemp(NewAreaSelectionCellIds);
	SurfaceGrid->SetAreaSelectionCells(AreaSelectionCellIds);
	ApplyAreaSelectionGhosts(SurfaceGrid, AreaSelectionCellIds);
	LastAreaSelectionTargetCellId = TargetCell.CellId;
	bHasLastAreaSelectionTargetCell = true;
	return true;
}

void USRAssemblyComponent::ApplyAreaSelectionGhosts(USRPlanetSurfaceGrid* SurfaceGrid, const TArray<FSRPlanetSurfaceGridCellId>& CellIds)
{
	if (!IsValid(SurfaceGrid))
	{
		return;
	}

	TSet<FName> StructureOccupantIds;
	for (const FSRPlanetSurfaceGridCellId& CellId : CellIds)
	{
		FSRPlanetSurfaceGridCellInfo CellInfo;
		if (SurfaceGrid->GetCellInfoById(CellId, CellInfo) && CellInfo.bOccupied && !CellInfo.OccupantId.IsNone())
		{
			StructureOccupantIds.Add(CellInfo.OccupantId);
		}
	}

	if (AActor* SurfaceOwner = SurfaceGrid->GetOwner())
	{
		if (USRStructureInstanceManagerComponent* StructureInstanceManager = SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>())
		{
			StructureInstanceManager->SetGhostedStructures(StructureOccupantIds);
		}
	}
}

bool USRAssemblyComponent::ShouldHandleAreaDeletionDrag() const
{
	const ASRPlayerController* PlayerController = GetOwnerController();
	return bAssemblyModeActive
		&& !bIsAreaCopyPlacementActive
		&& IsValid(PlayerController)
		&& !PlayerController->IsPointerOverBlockingUi();
}

bool USRAssemblyComponent::IsAreaSelectionDragActive() const
{
	return bIsAreaSelectionDragActive;
}

bool USRAssemblyComponent::IsAreaDeletionDragActive() const
{
	return bIsAreaDeletionDragActive;
}

bool USRAssemblyComponent::BeginAreaDeletionDrag(AActor*& OutSelectedActor)
{
	OutSelectedActor = nullptr;
	if (!ShouldHandleAreaDeletionDrag())
	{
		return false;
	}

	EndStructurePlacementDrag(false);
	ClearAreaSelection();
	ClearAreaDeletion();
	ClearConveyorBulkDeletionPreview();

	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* SurfaceGrid = nullptr;
	FSRPlanetSurfaceGridCell TargetCell;
	FVector HitLocation = FVector::ZeroVector;
	if (!TryGetFocusedSurfaceGrid(FocusedActor, SurfaceGrid)
		|| !TryProjectCursorToSurfaceCell(SurfaceGrid, TargetCell, HitLocation))
	{
		return false;
	}

	bIsAreaDeletionDragActive = true;
	AreaDeletionSurfaceGrid = SurfaceGrid;
	AreaDeletionStartCellId = TargetCell.CellId;
	LastAreaDeletionTargetCellId = FSRPlanetSurfaceGridCellId();
	bHasAreaDeletionStartCell = true;
	bHasLastAreaDeletionTargetCell = false;
	OutSelectedActor = FocusedActor;
	return UpdateAreaDeletion(SurfaceGrid, TargetCell);
}

bool USRAssemblyComponent::ContinueAreaDeletionDrag(AActor*& OutSelectedActor)
{
	OutSelectedActor = nullptr;
	if (!bIsAreaDeletionDragActive || !bHasAreaDeletionStartCell)
	{
		return false;
	}

	AActor* FocusedActor = nullptr;
	USRPlanetSurfaceGrid* SurfaceGrid = nullptr;
	FSRPlanetSurfaceGridCell TargetCell;
	FVector HitLocation = FVector::ZeroVector;
	if (!TryGetFocusedSurfaceGrid(FocusedActor, SurfaceGrid)
		|| SurfaceGrid != AreaDeletionSurfaceGrid
		|| !TryProjectCursorToSurfaceCell(SurfaceGrid, TargetCell, HitLocation))
	{
		return false;
	}

	OutSelectedActor = FocusedActor;
	return UpdateAreaDeletion(SurfaceGrid, TargetCell);
}

void USRAssemblyComponent::EndAreaDeletionDrag()
{
	if (!bIsAreaDeletionDragActive)
	{
		return;
	}

	bIsAreaDeletionDragActive = false;
	CommitAreaDeletion();
	ClearAreaDeletion();
}

void USRAssemblyComponent::ClearAreaDeletion()
{
	bIsAreaDeletionDragActive = false;

	if (IsValid(AreaDeletionSurfaceGrid))
	{
		AreaDeletionSurfaceGrid->ClearDeletionPreviewCells();

		if (AActor* SurfaceOwner = AreaDeletionSurfaceGrid->GetOwner())
		{
			if (USRStructureInstanceManagerComponent* StructureInstanceManager = SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>())
			{
				StructureInstanceManager->ClearDeletePreviewedStructures();
			}
		}
	}

	AreaDeletionSurfaceGrid = nullptr;
	AreaDeletionStartCellId = FSRPlanetSurfaceGridCellId();
	LastAreaDeletionTargetCellId = FSRPlanetSurfaceGridCellId();
	bHasAreaDeletionStartCell = false;
	bHasLastAreaDeletionTargetCell = false;
	AreaDeletionCellIds.Reset();
}

bool USRAssemblyComponent::UpdateAreaDeletion(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& TargetCell)
{
	if (!IsValid(SurfaceGrid) || !bHasAreaDeletionStartCell)
	{
		return false;
	}

	if (bHasLastAreaDeletionTargetCell && LastAreaDeletionTargetCellId == TargetCell.CellId)
	{
		return true;
	}

	TArray<FSRPlanetSurfaceGridCellId> NewAreaDeletionCellIds;
	if (!BuildAreaSelectionCellIds(SurfaceGrid, AreaDeletionStartCellId, TargetCell.CellId, NewAreaDeletionCellIds))
	{
		return false;
	}

	AreaDeletionCellIds = MoveTemp(NewAreaDeletionCellIds);
	SurfaceGrid->SetDeletionPreviewCells(AreaDeletionCellIds);
	ApplyAreaDeletionPreview(SurfaceGrid, AreaDeletionCellIds);
	LastAreaDeletionTargetCellId = TargetCell.CellId;
	bHasLastAreaDeletionTargetCell = true;
	return true;
}

void USRAssemblyComponent::ApplyAreaDeletionPreview(USRPlanetSurfaceGrid* SurfaceGrid, const TArray<FSRPlanetSurfaceGridCellId>& CellIds)
{
	if (!IsValid(SurfaceGrid))
	{
		return;
	}

	TSet<FName> StructureOccupantIds;
	CollectAreaDeletionTargetOccupantIds(SurfaceGrid, CellIds, StructureOccupantIds);
	if (AActor* SurfaceOwner = SurfaceGrid->GetOwner())
	{
		if (USRStructureInstanceManagerComponent* StructureInstanceManager = SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>())
		{
			StructureInstanceManager->SetDeletePreviewedStructures(StructureOccupantIds);
		}
	}
}

bool USRAssemblyComponent::CommitAreaDeletion()
{
	if (!IsValid(AreaDeletionSurfaceGrid) || AreaDeletionCellIds.IsEmpty())
	{
		return false;
	}

	return DeleteAreaCells(AreaDeletionSurfaceGrid, AreaDeletionCellIds);
}

bool USRAssemblyComponent::DeleteAreaCells(USRPlanetSurfaceGrid* SurfaceGrid, const TArray<FSRPlanetSurfaceGridCellId>& CellIds)
{
	if (!IsValid(SurfaceGrid) || CellIds.IsEmpty())
	{
		return false;
	}

	bool bDeletedAny = false;
	AActor* SurfaceOwner = SurfaceGrid->GetOwner();

	if (USRConveyorNetworkComponent* ConveyorNetwork = IsValid(SurfaceOwner)
		? SurfaceOwner->FindComponentByClass<USRConveyorNetworkComponent>()
		: nullptr)
	{
		bDeletedAny |= ConveyorNetwork->TryRemoveConveyorsAtCells(SurfaceGrid, CellIds);
	}

	TSet<FName> StructureOccupantIds;
	CollectAreaDeletionTargetOccupantIds(SurfaceGrid, CellIds, StructureOccupantIds);
	USRStructureInstanceManagerComponent* StructureInstanceManager = IsValid(SurfaceOwner)
		? SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>()
		: nullptr;
	if (!StructureOccupantIds.IsEmpty()
		&& StructureInstanceManager
		&& StructureInstanceManager->RemoveNonResourceStructuresByOccupantIds(SurfaceGrid, StructureOccupantIds))
	{
		bDeletedAny = true;
	}

	if (!bDeletedAny)
	{
		return false;
	}

	ClearPendingConveyorPathStart();
	PendingStructurePlacementQueue.Reset();
	DestroyStructureGhostPreview();
	ClearSelectedStructureInfo();
	bHasLastPublishedHoveredCellInfo = false;
	LastPublishedHoveredSurfaceGrid = nullptr;
	LastPublishedHoveredCellId = FSRPlanetSurfaceGridCellId();
	return true;
}

void USRAssemblyComponent::CollectAreaDeletionTargetOccupantIds(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TArray<FSRPlanetSurfaceGridCellId>& CellIds,
	TSet<FName>& OutOccupantIds) const
{
	OutOccupantIds.Reset();
	if (!IsValid(SurfaceGrid))
	{
		return;
	}

	AActor* SurfaceOwner = SurfaceGrid->GetOwner();
	USRStructureInstanceManagerComponent* StructureInstanceManager = IsValid(SurfaceOwner)
		? SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>()
		: nullptr;
	if (!StructureInstanceManager)
	{
		return;
	}

	for (const FSRPlanetSurfaceGridCellId& CellId : CellIds)
	{
		FSRPlanetSurfaceGridCellInfo CellInfo;
		if (!SurfaceGrid->GetCellInfoById(CellId, CellInfo) || !CellInfo.bOccupied || CellInfo.OccupantId.IsNone())
		{
			continue;
		}

		FSRPlacedStructureInstance PlacedStructure;
		if (!StructureInstanceManager->GetPlacedStructure(CellInfo.OccupantId, PlacedStructure)
			|| !IsValid(PlacedStructure.StructureDataAsset.Get())
			|| PlacedStructure.StructureDataAsset->BuildData().bIsResourceDeposit)
		{
			continue;
		}

		OutOccupantIds.Add(CellInfo.OccupantId);
	}
}
