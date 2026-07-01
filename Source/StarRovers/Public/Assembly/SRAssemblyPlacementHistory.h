#pragma once

#include "CoreMinimal.h"
#include "Conveyor/SRConveyorTypes.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"

class USRAssemblyComponent;
class USRConveyorNetworkComponent;
class USRPlanetSurfaceGrid;
class USRStructureDataAsset;
class USRStructureInstanceManagerComponent;

namespace StarRovers::Assembly
{
	enum class ESRAssemblyPlacementHistoryKind : uint8
	{
		Structure,
		Conveyor,
		Batch,
	};

	struct FSRRestorableNaturalStructure
	{
		TWeakObjectPtr<USRStructureDataAsset> StructureDataAsset;
		FSRPlanetSurfaceGridCellId OriginCellId;
		int32 PlacementRotationSteps = 0;
	};

	struct FSRAssemblyPlacementHistoryEntry
	{
		ESRAssemblyPlacementHistoryKind Kind = ESRAssemblyPlacementHistoryKind::Structure;
		TWeakObjectPtr<USRPlanetSurfaceGrid> SurfaceGrid;
		TWeakObjectPtr<USRStructureInstanceManagerComponent> StructureInstanceManager;
		TWeakObjectPtr<USRConveyorNetworkComponent> ConveyorNetwork;
		TWeakObjectPtr<USRStructureDataAsset> StructureDataAsset;
		FSRPlanetSurfaceGridCellId OriginCellId;
		int32 PlacementRotationSteps = 0;
		FName OccupantId = NAME_None;
		FSRConveyorVisualPath ConveyorVisualPath;
		TArray<FSRPlanetSurfaceGridCellId> ConveyorPlacedCellIds;
		TArray<FSRRestorableNaturalStructure> RemovedNaturalStructures;
		TArray<FSRAssemblyPlacementHistoryEntry> ChildEntries;
	};

	struct FSRAssemblyPlacementHistoryState
	{
		TArray<FSRAssemblyPlacementHistoryEntry> UndoStack;
		TArray<FSRAssemblyPlacementHistoryEntry> RedoStack;
	};

	class STARROVERS_API FSRAssemblyPlacementHistory
	{
	public:
		void Clear();
		void Push(USRAssemblyComponent& Owner, USRPlanetSurfaceGrid* SurfaceGrid, const FSRAssemblyPlacementHistoryEntry& Entry);
		void RecordBatch(USRAssemblyComponent& Owner, USRPlanetSurfaceGrid* SurfaceGrid, const TArray<FSRAssemblyPlacementHistoryEntry>& Entries);
		void RecordStructure(
			USRAssemblyComponent& Owner,
			USRPlanetSurfaceGrid* SurfaceGrid,
			USRStructureInstanceManagerComponent* StructureInstanceManager,
			USRStructureDataAsset* StructureDataAsset,
			const FSRPlanetSurfaceGridCellId& OriginCellId,
			int32 PlacementRotationSteps,
			FName OccupantId);
		void RecordConveyor(
			USRAssemblyComponent& Owner,
			USRPlanetSurfaceGrid* SurfaceGrid,
			USRConveyorNetworkComponent* ConveyorNetwork,
			const FSRConveyorVisualPath& VisualPath,
			const TArray<FSRPlanetSurfaceGridCellId>& PlacedCellIds,
			const TArray<FSRRestorableNaturalStructure>& RemovedNaturalStructures);
		void BuildConveyorPlacementPayload(
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
			TArray<FSRRestorableNaturalStructure>& OutRemovedNaturalStructures) const;
		bool TryUndo(USRAssemblyComponent& Owner);
		bool TryRedo(USRAssemblyComponent& Owner);

	private:
		USRPlanetSurfaceGrid* ResolveSurfaceGrid(const USRAssemblyComponent& Owner) const;
		FSRAssemblyPlacementHistoryState* FindState(USRPlanetSurfaceGrid* SurfaceGrid);
		FSRAssemblyPlacementHistoryState& FindOrAddState(USRPlanetSurfaceGrid* SurfaceGrid);
		bool TryUndoEntry(USRAssemblyComponent& Owner, FSRAssemblyPlacementHistoryEntry& Entry);
		bool TryRedoEntry(USRAssemblyComponent& Owner, FSRAssemblyPlacementHistoryEntry& Entry);
		bool TryUndoStructurePlacement(FSRAssemblyPlacementHistoryEntry& Entry);
		bool TryRedoStructurePlacement(FSRAssemblyPlacementHistoryEntry& Entry);
		bool TryUndoConveyorPlacement(FSRAssemblyPlacementHistoryEntry& Entry);
		bool TryRedoConveyorPlacement(const USRAssemblyComponent& Owner, FSRAssemblyPlacementHistoryEntry& Entry);
		bool TryUndoBatch(FSRAssemblyPlacementHistoryEntry& Entry);
		bool TryRedoBatch(const USRAssemblyComponent& Owner, FSRAssemblyPlacementHistoryEntry& Entry);
		void CollectConstructionDestructibleNaturalStructures(
			USRPlanetSurfaceGrid* SurfaceGrid,
			USRStructureInstanceManagerComponent* StructureInstanceManager,
			const TArray<FSRPlanetSurfaceGridCellId>& CellIds,
			TArray<FSRRestorableNaturalStructure>& OutNaturalStructures) const;
		void RestoreNaturalStructures(
			USRPlanetSurfaceGrid* SurfaceGrid,
			USRStructureInstanceManagerComponent* StructureInstanceManager,
			const TArray<FSRRestorableNaturalStructure>& NaturalStructures) const;
		void RefreshSurfaceState(USRAssemblyComponent& Owner, USRPlanetSurfaceGrid* SurfaceGrid);

		TMap<TObjectKey<USRPlanetSurfaceGrid>, FSRAssemblyPlacementHistoryState> HistoryBySurfaceGrid;
	};
}
