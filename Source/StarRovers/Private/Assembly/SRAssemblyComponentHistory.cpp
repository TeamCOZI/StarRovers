#include "Assembly/SRAssemblyComponent.h"

#include "Assembly/SRAssemblyPlacementHistory.h"

void USRAssemblyComponent::ClearAssemblyPlacementHistory()
{
	PlacementHistory.Clear();
}

void USRAssemblyComponent::PushAssemblyPlacementHistoryEntry(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRAssemblyPlacementHistoryEntry& Entry)
{
	PlacementHistory.Push(*this, SurfaceGrid, Entry);
}

void USRAssemblyComponent::RecordAssemblyPlacementHistoryBatch(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TArray<FSRAssemblyPlacementHistoryEntry>& Entries)
{
	PlacementHistory.RecordBatch(*this, SurfaceGrid, Entries);
}

void USRAssemblyComponent::RecordStructurePlacementHistory(
	USRPlanetSurfaceGrid* SurfaceGrid,
	USRStructureInstanceManagerComponent* StructureInstanceManager,
	USRStructureDataAsset* StructureDataAsset,
	const FSRPlanetSurfaceGridCellId& OriginCellId,
	int32 PlacementRotationSteps,
	FName OccupantId)
{
	PlacementHistory.RecordStructure(
		*this,
		SurfaceGrid,
		StructureInstanceManager,
		StructureDataAsset,
		OriginCellId,
		PlacementRotationSteps,
		OccupantId);
}

void USRAssemblyComponent::RecordConveyorPlacementHistory(
	USRPlanetSurfaceGrid* SurfaceGrid,
	USRConveyorNetworkComponent* ConveyorNetwork,
	const FSRConveyorVisualPath& VisualPath,
	const TArray<FSRPlanetSurfaceGridCellId>& PlacedCellIds,
	const TArray<FSRRestorableNaturalStructure>& RemovedNaturalStructures)
{
	PlacementHistory.RecordConveyor(
		*this,
		SurfaceGrid,
		ConveyorNetwork,
		VisualPath,
		PlacedCellIds,
		RemovedNaturalStructures);
}

void USRAssemblyComponent::BuildConveyorPlacementHistoryPayload(
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
	PlacementHistory.BuildConveyorPlacementPayload(
		*this,
		SurfaceGrid,
		ConveyorNetwork,
		StructureDataAsset,
		PathCellIds,
		Layer,
		LayerHeight,
		NetworkId,
		OutVisualPath,
		OutPlacedCellIds,
		OutRemovedNaturalStructures);
}

bool USRAssemblyComponent::TryUndoAssemblyPlacement()
{
	return PlacementHistory.TryUndo(*this);
}

bool USRAssemblyComponent::TryRedoAssemblyPlacement()
{
	return PlacementHistory.TryRedo(*this);
}
