#include "Assembly/SRAssemblyStructurePlacement.h"

#include "Assembly/SRAssemblyPlacementRestoration.h"
#include "GameFramework/Actor.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Structure/SRStructurePlacementLibrary.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace StarRovers::Assembly
{
	bool FSRAssemblyStructurePlacement::TryPlace(
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRStructureDataAsset* StructureDataAsset,
		const FSRStructureData& StructureData,
		const FSRPlanetSurfaceGridCellId& TargetCellId,
		int32 PlacementRotationSteps,
		FSRAssemblyStructurePlacementResult& OutResult)
	{
		OutResult = FSRAssemblyStructurePlacementResult();
		if (!IsValid(SurfaceGrid) || !IsValid(StructureDataAsset))
		{
			return false;
		}

		TArray<FSRPlanetSurfaceGridCellId> FootprintCellIds;
		if (!SurfaceGrid->GetFootprintCellIds(
			TargetCellId,
			StarRovers::Structure::GetRotatedFootprintCellsX(StructureData, PlacementRotationSteps),
			StarRovers::Structure::GetRotatedFootprintCellsY(StructureData, PlacementRotationSteps),
			FootprintCellIds))
		{
			OutResult.bShouldDestroyPreviewOnFailure = true;
			return false;
		}

		if (AActor* SurfaceOwner = SurfaceGrid->GetOwner())
		{
			if (USRStructureInstanceManagerComponent* StructureInstanceManager = SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>())
			{
				TSet<FName> DestructibleOccupantIds;
				if (!StructureInstanceManager->CanBuildOverCellsForConstruction(
					SurfaceGrid,
					FootprintCellIds,
					DestructibleOccupantIds))
				{
					OutResult.bShouldDestroyPreviewOnFailure = true;
					return false;
				}

				TArray<FSRPlacedStructureInstance> RemovedStructures;
				if (!DestructibleOccupantIds.IsEmpty()
					&& !StructureInstanceManager->RemoveConstructionDestructibleStructuresByOccupantIds(
						SurfaceGrid,
						DestructibleOccupantIds,
						&RemovedStructures))
				{
					OutResult.bShouldDestroyPreviewOnFailure = true;
					return false;
				}

				FName OccupantId = NAME_None;
				if (StructureInstanceManager->TryPlaceStructureOnSurfaceGrid(
					SurfaceGrid,
					TargetCellId,
					StructureDataAsset,
					OccupantId,
					false,
					false,
					PlacementRotationSteps))
				{
					OutResult.StructureInstanceManager = StructureInstanceManager;
					OutResult.OccupantId = OccupantId;
					OutResult.bPlacedWithStructureInstanceManager = true;
					AppendRestorableStructures(RemovedStructures, OutResult.RemovedNaturalStructures);
					return true;
				}

				RestoreRemovedStructures(SurfaceGrid, StructureInstanceManager, RemovedStructures);
			}
		}

		if (!SurfaceGrid->CanOccupyCells(FootprintCellIds))
		{
			return false;
		}

		AActor* PlacedStructureActor = nullptr;
		return USRStructurePlacementLibrary::TryPlaceStructureOnSurfaceGrid(
			SurfaceGrid,
			TargetCellId,
			StructureDataAsset,
			PlacedStructureActor,
			false,
			PlacementRotationSteps);
	}

	FSRAssemblyPlacementHistoryEntry FSRAssemblyStructurePlacement::BuildHistoryEntry(
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRStructureInstanceManagerComponent* StructureInstanceManager,
		USRStructureDataAsset* StructureDataAsset,
		const FSRPlanetSurfaceGridCellId& OriginCellId,
		int32 PlacementRotationSteps,
		const FSRAssemblyStructurePlacementResult& PlacementResult)
	{
		FSRAssemblyPlacementHistoryEntry HistoryEntry;
		HistoryEntry.Kind = ESRAssemblyPlacementHistoryKind::Structure;
		HistoryEntry.SurfaceGrid = SurfaceGrid;
		HistoryEntry.StructureInstanceManager = StructureInstanceManager;
		HistoryEntry.StructureDataAsset = StructureDataAsset;
		HistoryEntry.OriginCellId = OriginCellId;
		HistoryEntry.PlacementRotationSteps = PlacementRotationSteps;
		HistoryEntry.OccupantId = PlacementResult.OccupantId;
		HistoryEntry.RemovedNaturalStructures = PlacementResult.RemovedNaturalStructures;
		return HistoryEntry;
	}
}
