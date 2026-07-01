#include "Assembly/SRAssemblyPlacementHistory.h"

#include "Assembly/SRAssemblyComponent.h"
#include "Camera/SRPlayerController.h"
#include "Conveyor/SRConveyorNetworkComponent.h"
#include "GameFramework/Actor.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace StarRovers::Assembly
{
	namespace
	{
		constexpr int32 MaxAssemblyPlacementHistoryEntries = 128;
	}

	void FSRAssemblyPlacementHistory::Clear()
	{
		HistoryBySurfaceGrid.Reset();
	}

	USRPlanetSurfaceGrid* FSRAssemblyPlacementHistory::ResolveSurfaceGrid(const USRAssemblyComponent& Owner) const
	{
		AActor* FocusedActor = nullptr;
		USRPlanetSurfaceGrid* FocusedSurfaceGrid = nullptr;
		if (Owner.TryGetFocusedSurfaceGrid(FocusedActor, FocusedSurfaceGrid) && IsValid(FocusedSurfaceGrid))
		{
			return FocusedSurfaceGrid;
		}

		return IsValid(Owner.ActiveAssemblySurfaceGrid) ? Owner.ActiveAssemblySurfaceGrid.Get() : nullptr;
	}

	FSRAssemblyPlacementHistoryState* FSRAssemblyPlacementHistory::FindState(USRPlanetSurfaceGrid* SurfaceGrid)
	{
		if (!IsValid(SurfaceGrid))
		{
			return nullptr;
		}

		return HistoryBySurfaceGrid.Find(TObjectKey<USRPlanetSurfaceGrid>(SurfaceGrid));
	}

	FSRAssemblyPlacementHistoryState& FSRAssemblyPlacementHistory::FindOrAddState(USRPlanetSurfaceGrid* SurfaceGrid)
	{
		return HistoryBySurfaceGrid.FindOrAdd(TObjectKey<USRPlanetSurfaceGrid>(SurfaceGrid));
	}

	void FSRAssemblyPlacementHistory::Push(
		USRAssemblyComponent& Owner,
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRAssemblyPlacementHistoryEntry& Entry)
	{
		if (!Owner.bAssemblyModeActive || !IsValid(SurfaceGrid))
		{
			return;
		}

		FSRAssemblyPlacementHistoryState& HistoryState = FindOrAddState(SurfaceGrid);
		HistoryState.UndoStack.Add(Entry);
		if (HistoryState.UndoStack.Num() > MaxAssemblyPlacementHistoryEntries)
		{
			HistoryState.UndoStack.RemoveAt(0, HistoryState.UndoStack.Num() - MaxAssemblyPlacementHistoryEntries, EAllowShrinking::No);
		}
		HistoryState.RedoStack.Reset();
	}

	void FSRAssemblyPlacementHistory::RecordBatch(
		USRAssemblyComponent& Owner,
		USRPlanetSurfaceGrid* SurfaceGrid,
		const TArray<FSRAssemblyPlacementHistoryEntry>& Entries)
	{
		if (!Owner.bAssemblyModeActive || !IsValid(SurfaceGrid) || Entries.IsEmpty())
		{
			return;
		}

		if (Entries.Num() == 1)
		{
			Push(Owner, SurfaceGrid, Entries[0]);
			return;
		}

		FSRAssemblyPlacementHistoryEntry BatchEntry;
		BatchEntry.Kind = ESRAssemblyPlacementHistoryKind::Batch;
		BatchEntry.SurfaceGrid = SurfaceGrid;
		BatchEntry.ChildEntries = Entries;
		Push(Owner, SurfaceGrid, BatchEntry);
	}

	void FSRAssemblyPlacementHistory::RecordStructure(
		USRAssemblyComponent& Owner,
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRStructureInstanceManagerComponent* StructureInstanceManager,
		USRStructureDataAsset* StructureDataAsset,
		const FSRPlanetSurfaceGridCellId& OriginCellId,
		int32 PlacementRotationSteps,
		FName OccupantId)
	{
		if (!Owner.bAssemblyModeActive
			|| !IsValid(SurfaceGrid)
			|| !IsValid(StructureInstanceManager)
			|| !IsValid(StructureDataAsset)
			|| OccupantId.IsNone())
		{
			return;
		}

		FSRAssemblyPlacementHistoryEntry Entry;
		Entry.Kind = ESRAssemblyPlacementHistoryKind::Structure;
		Entry.SurfaceGrid = SurfaceGrid;
		Entry.StructureInstanceManager = StructureInstanceManager;
		Entry.StructureDataAsset = StructureDataAsset;
		Entry.OriginCellId = OriginCellId;
		Entry.PlacementRotationSteps = PlacementRotationSteps;
		Entry.OccupantId = OccupantId;

		Push(Owner, SurfaceGrid, Entry);
	}

	void FSRAssemblyPlacementHistory::RecordConveyor(
		USRAssemblyComponent& Owner,
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRConveyorNetworkComponent* ConveyorNetwork,
		const FSRConveyorVisualPath& VisualPath,
		const TArray<FSRPlanetSurfaceGridCellId>& PlacedCellIds,
		const TArray<FSRRestorableNaturalStructure>& RemovedNaturalStructures)
	{
		if (!Owner.bAssemblyModeActive
			|| !IsValid(SurfaceGrid)
			|| !IsValid(ConveyorNetwork)
			|| VisualPath.CellIds.IsEmpty()
			|| PlacedCellIds.IsEmpty()
			|| !IsValid(VisualPath.StructureDataAsset.Get()))
		{
			return;
		}

		FSRAssemblyPlacementHistoryEntry Entry;
		Entry.Kind = ESRAssemblyPlacementHistoryKind::Conveyor;
		Entry.SurfaceGrid = SurfaceGrid;
		Entry.ConveyorNetwork = ConveyorNetwork;
		Entry.StructureDataAsset = VisualPath.StructureDataAsset.Get();
		Entry.ConveyorVisualPath = VisualPath;
		Entry.ConveyorPlacedCellIds = PlacedCellIds;
		Entry.RemovedNaturalStructures = RemovedNaturalStructures;

		Push(Owner, SurfaceGrid, Entry);
	}

	void FSRAssemblyPlacementHistory::BuildConveyorPlacementPayload(
		const USRAssemblyComponent& Owner,
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRConveyorNetworkComponent* ConveyorNetwork,
		USRStructureDataAsset* StructureDataAsset,
		const TArray<FSRPlanetSurfaceGridCellId>& PathCellIds,
		int32 Layer,
		float LayerHeight,
		FName NetworkId,
		FSRConveyorVisualPath& OutVisualPath,
		TArray<FSRPlanetSurfaceGridCellId>& OutPlacedCellIds,
		TArray<FSRRestorableNaturalStructure>& OutRemovedNaturalStructures) const
	{
		OutVisualPath = FSRConveyorVisualPath();
		OutPlacedCellIds.Reset();
		OutRemovedNaturalStructures.Reset();
		if (!IsValid(SurfaceGrid)
			|| !IsValid(ConveyorNetwork)
			|| !IsValid(StructureDataAsset)
			|| PathCellIds.IsEmpty())
		{
			return;
		}

		const int32 SafeLayer = FMath::Max(0, Layer);
		OutVisualPath.CellIds = PathCellIds;
		OutVisualPath.Layer = SafeLayer;
		OutVisualPath.LayerHeight = LayerHeight;
		OutVisualPath.NetworkId = NetworkId;
		OutVisualPath.StructureDataAsset = StructureDataAsset;

		for (const FSRPlanetSurfaceGridCellId& CellId : PathCellIds)
		{
			FSRConveyorLaneKey LaneKey;
			LaneKey.CellId = CellId;
			LaneKey.Layer = SafeLayer;
			if (!ConveyorNetwork->HasConveyorSegment(LaneKey))
			{
				OutPlacedCellIds.AddUnique(CellId);
			}
		}

		if (SafeLayer != 0 || OutPlacedCellIds.IsEmpty())
		{
			return;
		}

		AActor* SurfaceOwner = SurfaceGrid->GetOwner();
		USRStructureInstanceManagerComponent* StructureInstanceManager = IsValid(SurfaceOwner)
			? SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>()
			: nullptr;
		CollectConstructionDestructibleNaturalStructures(
			SurfaceGrid,
			StructureInstanceManager,
			OutPlacedCellIds,
			OutRemovedNaturalStructures);
	}

	bool FSRAssemblyPlacementHistory::TryUndo(USRAssemblyComponent& Owner)
	{
		if (!Owner.bAssemblyModeActive
			|| Owner.bIsStructurePlacementDragActive
			|| Owner.bIsConveyorPlacementDragActive)
		{
			return false;
		}

		USRPlanetSurfaceGrid* SurfaceGrid = ResolveSurfaceGrid(Owner);
		FSRAssemblyPlacementHistoryState* HistoryState = FindState(SurfaceGrid);
		if (!HistoryState || HistoryState->UndoStack.IsEmpty())
		{
			return false;
		}

		Owner.PlacementQueue.Reset();
		FSRAssemblyPlacementHistoryEntry Entry = HistoryState->UndoStack.Last();
		HistoryState->UndoStack.RemoveAt(HistoryState->UndoStack.Num() - 1, 1, EAllowShrinking::No);
		if (!TryUndoEntry(Owner, Entry))
		{
			return false;
		}

		HistoryState->RedoStack.Add(Entry);
		return true;
	}

	bool FSRAssemblyPlacementHistory::TryRedo(USRAssemblyComponent& Owner)
	{
		if (!Owner.bAssemblyModeActive
			|| Owner.bIsStructurePlacementDragActive
			|| Owner.bIsConveyorPlacementDragActive)
		{
			return false;
		}

		USRPlanetSurfaceGrid* SurfaceGrid = ResolveSurfaceGrid(Owner);
		FSRAssemblyPlacementHistoryState* HistoryState = FindState(SurfaceGrid);
		if (!HistoryState || HistoryState->RedoStack.IsEmpty())
		{
			return false;
		}

		Owner.PlacementQueue.Reset();
		FSRAssemblyPlacementHistoryEntry Entry = HistoryState->RedoStack.Last();
		HistoryState->RedoStack.RemoveAt(HistoryState->RedoStack.Num() - 1, 1, EAllowShrinking::No);
		if (!TryRedoEntry(Owner, Entry))
		{
			return false;
		}

		HistoryState->UndoStack.Add(Entry);
		return true;
	}

	bool FSRAssemblyPlacementHistory::TryUndoEntry(
		USRAssemblyComponent& Owner,
		FSRAssemblyPlacementHistoryEntry& Entry)
	{
		bool bUndone = false;
		switch (Entry.Kind)
		{
		case ESRAssemblyPlacementHistoryKind::Structure:
			bUndone = TryUndoStructurePlacement(Entry);
			break;
		case ESRAssemblyPlacementHistoryKind::Conveyor:
			bUndone = TryUndoConveyorPlacement(Entry);
			break;
		case ESRAssemblyPlacementHistoryKind::Batch:
			bUndone = TryUndoBatch(Entry);
			break;
		default:
			break;
		}
		if (bUndone)
		{
			RefreshSurfaceState(Owner, Entry.SurfaceGrid.Get());
		}
		return bUndone;
	}

	bool FSRAssemblyPlacementHistory::TryRedoEntry(
		USRAssemblyComponent& Owner,
		FSRAssemblyPlacementHistoryEntry& Entry)
	{
		bool bRedone = false;
		switch (Entry.Kind)
		{
		case ESRAssemblyPlacementHistoryKind::Structure:
			bRedone = TryRedoStructurePlacement(Entry);
			break;
		case ESRAssemblyPlacementHistoryKind::Conveyor:
			bRedone = TryRedoConveyorPlacement(Owner, Entry);
			break;
		case ESRAssemblyPlacementHistoryKind::Batch:
			bRedone = TryRedoBatch(Owner, Entry);
			break;
		default:
			break;
		}
		if (bRedone)
		{
			RefreshSurfaceState(Owner, Entry.SurfaceGrid.Get());
		}
		return bRedone;
	}

	bool FSRAssemblyPlacementHistory::TryUndoStructurePlacement(FSRAssemblyPlacementHistoryEntry& Entry)
	{
		USRPlanetSurfaceGrid* SurfaceGrid = Entry.SurfaceGrid.Get();
		USRStructureInstanceManagerComponent* StructureInstanceManager = Entry.StructureInstanceManager.Get();
		if (!IsValid(StructureInstanceManager) && IsValid(SurfaceGrid))
		{
			AActor* SurfaceOwner = SurfaceGrid->GetOwner();
			StructureInstanceManager = IsValid(SurfaceOwner)
				? SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>()
				: nullptr;
		}

		if (!IsValid(SurfaceGrid) || !IsValid(StructureInstanceManager) || Entry.OccupantId.IsNone())
		{
			return false;
		}

		FSRPlacedStructureInstance RemovedStructure;
		if (!StructureInstanceManager->TryRemoveStructureByOccupantId(SurfaceGrid, Entry.OccupantId, RemovedStructure))
		{
			return false;
		}

		Entry.SurfaceGrid = SurfaceGrid;
		Entry.StructureInstanceManager = StructureInstanceManager;
		Entry.StructureDataAsset = RemovedStructure.StructureDataAsset.Get();
		Entry.OriginCellId = RemovedStructure.OriginCellId;
		Entry.PlacementRotationSteps = RemovedStructure.PlacementRotationSteps;
		Entry.OccupantId = RemovedStructure.OccupantId;
		return true;
	}

	bool FSRAssemblyPlacementHistory::TryRedoStructurePlacement(FSRAssemblyPlacementHistoryEntry& Entry)
	{
		USRPlanetSurfaceGrid* SurfaceGrid = Entry.SurfaceGrid.Get();
		USRStructureInstanceManagerComponent* StructureInstanceManager = Entry.StructureInstanceManager.Get();
		if (!IsValid(StructureInstanceManager) && IsValid(SurfaceGrid))
		{
			AActor* SurfaceOwner = SurfaceGrid->GetOwner();
			StructureInstanceManager = IsValid(SurfaceOwner)
				? SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>()
				: nullptr;
		}

		USRStructureDataAsset* StructureDataAsset = Entry.StructureDataAsset.Get();
		if (!IsValid(SurfaceGrid) || !IsValid(StructureInstanceManager) || !IsValid(StructureDataAsset))
		{
			return false;
		}

		FName NewOccupantId = NAME_None;
		if (!StructureInstanceManager->TryPlaceStructureOnSurfaceGrid(
			SurfaceGrid,
			Entry.OriginCellId,
			StructureDataAsset,
			NewOccupantId,
			false,
			false,
			Entry.PlacementRotationSteps))
		{
			return false;
		}

		Entry.SurfaceGrid = SurfaceGrid;
		Entry.StructureInstanceManager = StructureInstanceManager;
		Entry.StructureDataAsset = StructureDataAsset;
		Entry.OccupantId = NewOccupantId;
		return true;
	}

	bool FSRAssemblyPlacementHistory::TryUndoConveyorPlacement(FSRAssemblyPlacementHistoryEntry& Entry)
	{
		USRPlanetSurfaceGrid* SurfaceGrid = Entry.SurfaceGrid.Get();
		USRConveyorNetworkComponent* ConveyorNetwork = Entry.ConveyorNetwork.Get();
		if (!IsValid(ConveyorNetwork) && IsValid(SurfaceGrid))
		{
			AActor* SurfaceOwner = SurfaceGrid->GetOwner();
			ConveyorNetwork = IsValid(SurfaceOwner)
				? SurfaceOwner->FindComponentByClass<USRConveyorNetworkComponent>()
				: nullptr;
		}

		if (!IsValid(SurfaceGrid)
			|| !IsValid(ConveyorNetwork)
			|| Entry.ConveyorVisualPath.CellIds.IsEmpty()
			|| Entry.ConveyorPlacedCellIds.IsEmpty())
		{
			return false;
		}

		bool bRemoved = ConveyorNetwork->TryRemoveConveyorVisualPath(
			SurfaceGrid,
			Entry.ConveyorVisualPath,
			Entry.ConveyorPlacedCellIds);
		if (!bRemoved)
		{
			for (const FSRPlanetSurfaceGridCellId& CellId : Entry.ConveyorPlacedCellIds)
			{
				bRemoved |= ConveyorNetwork->TryRemoveConveyorAtCell(SurfaceGrid, CellId, Entry.ConveyorVisualPath.Layer);
			}
		}

		if (!bRemoved)
		{
			return false;
		}

		AActor* SurfaceOwner = SurfaceGrid->GetOwner();
		USRStructureInstanceManagerComponent* StructureInstanceManager = IsValid(SurfaceOwner)
			? SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>()
			: nullptr;
		RestoreNaturalStructures(SurfaceGrid, StructureInstanceManager, Entry.RemovedNaturalStructures);

		Entry.SurfaceGrid = SurfaceGrid;
		Entry.ConveyorNetwork = ConveyorNetwork;
		Entry.StructureDataAsset = Entry.ConveyorVisualPath.StructureDataAsset.Get();
		return true;
	}

	bool FSRAssemblyPlacementHistory::TryRedoConveyorPlacement(
		const USRAssemblyComponent& Owner,
		FSRAssemblyPlacementHistoryEntry& Entry)
	{
		USRPlanetSurfaceGrid* SurfaceGrid = Entry.SurfaceGrid.Get();
		USRConveyorNetworkComponent* ConveyorNetwork = Entry.ConveyorNetwork.Get();
		if (!IsValid(ConveyorNetwork) && IsValid(SurfaceGrid))
		{
			AActor* SurfaceOwner = SurfaceGrid->GetOwner();
			ConveyorNetwork = IsValid(SurfaceOwner)
				? SurfaceOwner->FindComponentByClass<USRConveyorNetworkComponent>()
				: nullptr;
		}

		USRStructureDataAsset* StructureDataAsset = Entry.StructureDataAsset.Get();
		if (!IsValid(StructureDataAsset))
		{
			StructureDataAsset = Entry.ConveyorVisualPath.StructureDataAsset.Get();
		}

		if (!IsValid(SurfaceGrid)
			|| !IsValid(ConveyorNetwork)
			|| !IsValid(StructureDataAsset)
			|| Entry.ConveyorVisualPath.CellIds.IsEmpty())
		{
			return false;
		}

		FSRConveyorVisualPath VisualPath;
		TArray<FSRPlanetSurfaceGridCellId> PlacedCellIds;
		TArray<FSRRestorableNaturalStructure> RemovedNaturalStructures;
		BuildConveyorPlacementPayload(
			Owner,
			SurfaceGrid,
			ConveyorNetwork,
			StructureDataAsset,
			Entry.ConveyorVisualPath.CellIds,
			Entry.ConveyorVisualPath.Layer,
			Entry.ConveyorVisualPath.LayerHeight,
			Entry.ConveyorVisualPath.NetworkId,
			VisualPath,
			PlacedCellIds,
			RemovedNaturalStructures);

		if (PlacedCellIds.IsEmpty()
			|| !ConveyorNetwork->TryPlaceConveyorPath(
				SurfaceGrid,
				VisualPath.CellIds,
				VisualPath.Layer,
				VisualPath.LayerHeight,
				StructureDataAsset,
				VisualPath.NetworkId))
		{
			return false;
		}

		Entry.SurfaceGrid = SurfaceGrid;
		Entry.ConveyorNetwork = ConveyorNetwork;
		Entry.StructureDataAsset = StructureDataAsset;
		Entry.ConveyorVisualPath = VisualPath;
		Entry.ConveyorPlacedCellIds = MoveTemp(PlacedCellIds);
		Entry.RemovedNaturalStructures = MoveTemp(RemovedNaturalStructures);
		return true;
	}

	bool FSRAssemblyPlacementHistory::TryUndoBatch(FSRAssemblyPlacementHistoryEntry& Entry)
	{
		if (Entry.ChildEntries.IsEmpty())
		{
			return false;
		}

		bool bUndoneAny = false;
		for (int32 ChildIndex = Entry.ChildEntries.Num() - 1; ChildIndex >= 0; --ChildIndex)
		{
			FSRAssemblyPlacementHistoryEntry& ChildEntry = Entry.ChildEntries[ChildIndex];
			bool bChildUndone = false;
			switch (ChildEntry.Kind)
			{
			case ESRAssemblyPlacementHistoryKind::Structure:
				bChildUndone = TryUndoStructurePlacement(ChildEntry);
				break;
			case ESRAssemblyPlacementHistoryKind::Conveyor:
				bChildUndone = TryUndoConveyorPlacement(ChildEntry);
				break;
			case ESRAssemblyPlacementHistoryKind::Batch:
				bChildUndone = TryUndoBatch(ChildEntry);
				break;
			default:
				break;
			}
			bUndoneAny |= bChildUndone;
		}

		if (!bUndoneAny)
		{
			return false;
		}

		for (const FSRAssemblyPlacementHistoryEntry& ChildEntry : Entry.ChildEntries)
		{
			if (USRPlanetSurfaceGrid* ChildSurfaceGrid = ChildEntry.SurfaceGrid.Get())
			{
				Entry.SurfaceGrid = ChildSurfaceGrid;
				break;
			}
		}
		return true;
	}

	bool FSRAssemblyPlacementHistory::TryRedoBatch(
		const USRAssemblyComponent& Owner,
		FSRAssemblyPlacementHistoryEntry& Entry)
	{
		if (Entry.ChildEntries.IsEmpty())
		{
			return false;
		}

		bool bRedoneAny = false;
		for (FSRAssemblyPlacementHistoryEntry& ChildEntry : Entry.ChildEntries)
		{
			bool bChildRedone = false;
			switch (ChildEntry.Kind)
			{
			case ESRAssemblyPlacementHistoryKind::Structure:
				bChildRedone = TryRedoStructurePlacement(ChildEntry);
				break;
			case ESRAssemblyPlacementHistoryKind::Conveyor:
				bChildRedone = TryRedoConveyorPlacement(Owner, ChildEntry);
				break;
			case ESRAssemblyPlacementHistoryKind::Batch:
				bChildRedone = TryRedoBatch(Owner, ChildEntry);
				break;
			default:
				break;
			}
			bRedoneAny |= bChildRedone;
		}

		if (!bRedoneAny)
		{
			return false;
		}

		for (const FSRAssemblyPlacementHistoryEntry& ChildEntry : Entry.ChildEntries)
		{
			if (USRPlanetSurfaceGrid* ChildSurfaceGrid = ChildEntry.SurfaceGrid.Get())
			{
				Entry.SurfaceGrid = ChildSurfaceGrid;
				break;
			}
		}
		return true;
	}

	void FSRAssemblyPlacementHistory::CollectConstructionDestructibleNaturalStructures(
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRStructureInstanceManagerComponent* StructureInstanceManager,
		const TArray<FSRPlanetSurfaceGridCellId>& CellIds,
		TArray<FSRRestorableNaturalStructure>& OutNaturalStructures) const
	{
		OutNaturalStructures.Reset();
		if (!IsValid(SurfaceGrid) || !IsValid(StructureInstanceManager))
		{
			return;
		}

		TSet<FName> AddedOccupantIds;
		for (const FSRPlanetSurfaceGridCellId& CellId : CellIds)
		{
			FSRPlanetSurfaceGridCellInfo CellInfo;
			if (!SurfaceGrid->GetCellInfoById(CellId, CellInfo)
				|| !CellInfo.bOccupied
				|| CellInfo.OccupantId.IsNone()
				|| AddedOccupantIds.Contains(CellInfo.OccupantId)
				|| !StructureInstanceManager->CanDestroyNaturalStructureForConstruction(CellInfo.OccupantId))
			{
				continue;
			}

			FSRPlacedStructureInstance PlacedStructure;
			if (!StructureInstanceManager->GetPlacedStructure(CellInfo.OccupantId, PlacedStructure)
				|| !PlacedStructure.bNaturalStructure
				|| !IsValid(PlacedStructure.StructureDataAsset.Get()))
			{
				continue;
			}

			FSRRestorableNaturalStructure NaturalStructure;
			NaturalStructure.StructureDataAsset = PlacedStructure.StructureDataAsset.Get();
			NaturalStructure.OriginCellId = PlacedStructure.OriginCellId;
			NaturalStructure.PlacementRotationSteps = PlacedStructure.PlacementRotationSteps;
			OutNaturalStructures.Add(NaturalStructure);
			AddedOccupantIds.Add(CellInfo.OccupantId);
		}
	}

	void FSRAssemblyPlacementHistory::RestoreNaturalStructures(
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRStructureInstanceManagerComponent* StructureInstanceManager,
		const TArray<FSRRestorableNaturalStructure>& NaturalStructures) const
	{
		if (!IsValid(SurfaceGrid) || !IsValid(StructureInstanceManager))
		{
			return;
		}

		for (const FSRRestorableNaturalStructure& NaturalStructure : NaturalStructures)
		{
			USRStructureDataAsset* StructureDataAsset = NaturalStructure.StructureDataAsset.Get();
			if (!IsValid(StructureDataAsset))
			{
				continue;
			}

			FName RestoredOccupantId = NAME_None;
			if (!StructureInstanceManager->TryPlaceStructureOnSurfaceGrid(
				SurfaceGrid,
				NaturalStructure.OriginCellId,
				StructureDataAsset,
				RestoredOccupantId,
				true,
				true,
				NaturalStructure.PlacementRotationSteps))
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to restore natural structure '%s' while undoing conveyor placement."), *GetNameSafe(StructureDataAsset));
			}
		}
	}

	void FSRAssemblyPlacementHistory::RefreshSurfaceState(
		USRAssemblyComponent& Owner,
		USRPlanetSurfaceGrid* SurfaceGrid)
	{
		Owner.ClearConveyorBulkDeletionPreview();
		Owner.ClearPendingConveyorPathStart();
		Owner.ClearSelectedStructureInfo();
		Owner.DestroyStructureGhostPreview();
		Owner.DestroyConveyorGhostPreview();
		Owner.DestroyConveyorDeletionGhostPreview();

		ASRPlayerController* PlayerController = Owner.GetOwnerController();
		if (IsValid(SurfaceGrid) && PlayerController)
		{
			SurfaceGrid->SetHoveredInteractionGridPatchVisible(IsValid(PlayerController->GetSelectedStructureDataAsset()));
		}

		Owner.bHasLastPublishedHoveredCellInfo = false;
		Owner.LastPublishedHoveredSurfaceGrid = nullptr;
		Owner.LastPublishedHoveredCellId = FSRPlanetSurfaceGridCellId();

		if (IsValid(SurfaceGrid))
		{
			FSRPlanetSurfaceGridCell HoveredCell;
			if (SurfaceGrid->GetHoveredCell(HoveredCell))
			{
				Owner.PublishHoveredCellInfo(SurfaceGrid, HoveredCell);
			}
		}

		Owner.UpdateConveyorPlacementPortPreview();
		Owner.UpdateStructureGhostPreview();
	}
}
