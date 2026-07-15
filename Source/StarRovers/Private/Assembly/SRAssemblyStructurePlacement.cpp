#include "Assembly/SRAssemblyStructurePlacement.h"

#include "Assembly/SRAssemblyConstructionReplacement.h"
#include "Assembly/SRAssemblyPlacementRestoration.h"
#include "Conveyor/SRConveyorNetworkComponent.h"
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
				USRConveyorNetworkComponent* ConveyorNetwork = SurfaceOwner->FindComponentByClass<USRConveyorNetworkComponent>();
				ConstructionReplacement::FSRConstructionReplacementTargets ReplacementTargets;
				if (!ConstructionReplacement::CanBuildOverCellsForStructureConstruction(
					SurfaceGrid,
					StructureInstanceManager,
					ConveyorNetwork,
					FootprintCellIds,
					ReplacementTargets))
				{
					OutResult.bShouldDestroyPreviewOnFailure = true;
					return false;
				}

				TArray<FSRPlacedStructureInstance> RemovedStructures;
				if (ReplacementTargets.HasAny()
					&& !ConstructionReplacement::RemoveReplacementTargets(
						SurfaceGrid,
						StructureInstanceManager,
						ConveyorNetwork,
						ReplacementTargets,
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
					OutResult.ConveyorNetwork = ConveyorNetwork;
					OutResult.OccupantId = OccupantId;
					OutResult.bPlacedWithStructureInstanceManager = true;
					AppendRestorableStructures(RemovedStructures, OutResult.RemovedNaturalStructures);
					OutResult.RemovedConveyorBeltPaths = ReplacementTargets.ConveyorBeltPaths;
					return true;
				}

				RestoreRemovedStructures(SurfaceGrid, StructureInstanceManager, RemovedStructures);
				ConstructionReplacement::RestoreConveyorBeltPaths(SurfaceGrid, ConveyorNetwork, ReplacementTargets.ConveyorBeltPaths);
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
		HistoryEntry.ConveyorNetwork = PlacementResult.ConveyorNetwork;
		HistoryEntry.StructureDataAsset = StructureDataAsset;
		HistoryEntry.OriginCellId = OriginCellId;
		HistoryEntry.PlacementRotationSteps = PlacementRotationSteps;
		HistoryEntry.OccupantId = PlacementResult.OccupantId;
		HistoryEntry.RemovedNaturalStructures = PlacementResult.RemovedNaturalStructures;
		HistoryEntry.RemovedConveyorBeltPaths = PlacementResult.RemovedConveyorBeltPaths;
		return HistoryEntry;
	}
}
