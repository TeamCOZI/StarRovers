#include "Assembly/SRAssemblyStructureDragPreviewUpdater.h"

#include "Assembly/SRAssemblyPlacementDragState.h"
#include "Assembly/SRAssemblyPreviewState.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Structure/SRBuildableStructureInterface.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Structure/SRStructurePlacementLibrary.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace StarRovers::Assembly
{
	bool FSRAssemblyStructureDragPreviewUpdater::Update(
		UWorld* World,
		AActor* PreviewOwner,
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRStructureDataAsset* StructureDataAsset,
		const FSRStructureData& StructureData,
		const TArray<FSRPlanetSurfaceGridCellId>& CandidateCellIds,
		int32 PlacementRotationSteps,
		float AdditionalYawDegrees,
		FSRAssemblyPlacementDragState& PlacementDrag,
		FSRAssemblyStructurePreviewState& StructurePreview)
	{
		if (!IsValid(World)
			|| !IsValid(PreviewOwner)
			|| !IsValid(SurfaceGrid)
			|| !IsValid(StructureDataAsset))
		{
			return false;
		}

		TMap<FSRPlanetSurfaceGridCellId, AActor*> ExistingPreviewActorsByCellId;
		for (const FSRStructurePlacementDragPreviewActor& PreviewInfo : StructurePreview.StructurePlacementDragPreviewActors)
		{
			if (AActor* PreviewActor = PreviewInfo.PreviewActor.Get())
			{
				ExistingPreviewActorsByCellId.Add(PreviewInfo.CellId, PreviewActor);
			}
		}

		const int32 SafePlacementRotationSteps = StarRovers::Structure::NormalizePlacementRotationSteps(PlacementRotationSteps);
		const int32 FootprintCellsX = StarRovers::Structure::GetRotatedFootprintCellsX(StructureData, SafePlacementRotationSteps);
		const int32 FootprintCellsY = StarRovers::Structure::GetRotatedFootprintCellsY(StructureData, SafePlacementRotationSteps);
		AActor* SurfaceOwner = SurfaceGrid->GetOwner();
		USRStructureInstanceManagerComponent* StructureInstanceManager = IsValid(SurfaceOwner)
			? SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>()
			: nullptr;

		TSet<FSRPlanetSurfaceGridCellId> ReservedFootprintCellIds;
		TSet<FName> ReplacementPreviewOccupantIds;
		TArray<FSRPlanetSurfaceGridCellId> NewPlacementCellIds;
		TArray<FSRStructurePlacementDragPreviewActor> NewPreviewActors;
		NewPlacementCellIds.Reserve(CandidateCellIds.Num());
		NewPreviewActors.Reserve(CandidateCellIds.Num());

		for (const FSRPlanetSurfaceGridCellId& CandidateCellId : CandidateCellIds)
		{
			FSRPlanetSurfaceGridCellInfo CandidateCellInfo;
			TArray<FSRPlanetSurfaceGridCellId> FootprintCellIds;
			TSet<FName> CandidateReplaceableOccupantIds;
			if (!SurfaceGrid->GetCellInfoById(CandidateCellId, CandidateCellInfo)
				|| !CandidateCellInfo.bCanConstruct
				|| !SurfaceGrid->GetFootprintCellIds(CandidateCellId, FootprintCellsX, FootprintCellsY, FootprintCellIds))
			{
				continue;
			}

			const bool bCanBuildOverCells = IsValid(StructureInstanceManager)
				? StructureInstanceManager->CanBuildOverCellsForConstruction(SurfaceGrid, FootprintCellIds, CandidateReplaceableOccupantIds)
				: SurfaceGrid->CanOccupyCells(FootprintCellIds);
			if (!bCanBuildOverCells)
			{
				continue;
			}

			bool bOverlapsReservedFootprint = false;
			for (const FSRPlanetSurfaceGridCellId& FootprintCellId : FootprintCellIds)
			{
				if (ReservedFootprintCellIds.Contains(FootprintCellId))
				{
					bOverlapsReservedFootprint = true;
					break;
				}
			}
			if (bOverlapsReservedFootprint)
			{
				continue;
			}

			AActor* PreviewActor = nullptr;
			if (AActor** ExistingPreviewActor = ExistingPreviewActorsByCellId.Find(CandidateCellId))
			{
				PreviewActor = *ExistingPreviewActor;
				ExistingPreviewActorsByCellId.Remove(CandidateCellId);
			}
			if (!IsValid(PreviewActor))
			{
				PreviewActor = StructurePreview.SpawnPlacementDragPreviewActor(World, PreviewOwner, SurfaceGrid, StructureDataAsset);
			}
			if (!IsValid(PreviewActor))
			{
				continue;
			}

			FTransform PreviewTransform;
			if (!USRStructurePlacementLibrary::BuildStructurePlacementTransform(
				SurfaceGrid,
				CandidateCellId,
				StructureDataAsset,
				PreviewTransform,
				AdditionalYawDegrees))
			{
				PreviewActor->SetActorHiddenInGame(true);
				continue;
			}

			FSRPlanetSurfaceGridCellInfo PreviewCellInfo = CandidateCellInfo;
			if (PreviewCellInfo.bOccupied
				&& !PreviewCellInfo.OccupantId.IsNone()
				&& CandidateReplaceableOccupantIds.Contains(PreviewCellInfo.OccupantId))
			{
				PreviewCellInfo.bOccupied = false;
				PreviewCellInfo.OccupantId = NAME_None;
			}

			if (!ISRBuildableStructureInterface::Execute_CanPlaceOnSurfaceCell(PreviewActor, PreviewCellInfo))
			{
				PreviewActor->SetActorHiddenInGame(true);
				continue;
			}

			for (const FSRPlanetSurfaceGridCellId& FootprintCellId : FootprintCellIds)
			{
				ReservedFootprintCellIds.Add(FootprintCellId);
			}
			ReplacementPreviewOccupantIds.Append(CandidateReplaceableOccupantIds);

			ISRBuildableStructureInterface::Execute_SetStructureGhostMode(PreviewActor, true);
			PreviewActor->SetActorTransform(PreviewTransform);
			PreviewActor->SetActorEnableCollision(false);
			PreviewActor->SetActorHiddenInGame(false);

			FSRStructurePlacementDragPreviewActor PreviewInfo;
			PreviewInfo.CellId = CandidateCellId;
			PreviewInfo.PreviewActor = PreviewActor;
			NewPreviewActors.Add(PreviewInfo);
			NewPlacementCellIds.Add(CandidateCellId);
		}

		for (const TPair<FSRPlanetSurfaceGridCellId, AActor*>& RemovedPreviewPair : ExistingPreviewActorsByCellId)
		{
			if (IsValid(RemovedPreviewPair.Value))
			{
				RemovedPreviewPair.Value->Destroy();
			}
		}

		PlacementDrag.StructurePlacementDragCellIds = MoveTemp(NewPlacementCellIds);
		StructurePreview.StructurePlacementDragPreviewActors = MoveTemp(NewPreviewActors);
		if (IsValid(StructureInstanceManager))
		{
			StructureInstanceManager->SetConstructionReplacementPreviewedStructures(ReplacementPreviewOccupantIds);
		}
		return true;
	}
}
