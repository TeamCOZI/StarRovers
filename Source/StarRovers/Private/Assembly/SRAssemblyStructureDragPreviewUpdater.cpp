#include "Assembly/SRAssemblyStructureDragPreviewUpdater.h"

#include "Assembly/SRAssemblyConstructionReplacement.h"
#include "Assembly/SRAssemblyPlacementDragState.h"
#include "Assembly/SRAssemblyPreviewMaterial.h"
#include "Assembly/SRAssemblyPreviewState.h"
#include "Conveyor/SRConveyorNetworkComponent.h"
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
		FSRAssemblyConveyorPreviewState& ConveyorPreview,
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
		USRConveyorNetworkComponent* ConveyorNetwork = IsValid(SurfaceOwner)
			? SurfaceOwner->FindComponentByClass<USRConveyorNetworkComponent>()
			: nullptr;

		TSet<FSRPlanetSurfaceGridCellId> ReservedFootprintCellIds;
		TSet<FName> ReplacementPreviewOccupantIds;
		TSet<FSRPlanetSurfaceGridCellId> ReplacementPreviewConveyorCellIdSet;
		TSet<FSRPlanetSurfaceGridCellId> ReplacementPreviewCellIdSet;
		TArray<FSRPlanetSurfaceGridCellId> ReplacementPreviewConveyorCellIds;
		TArray<FSRConveyorBeltPath> ReplacementPreviewConveyorBeltPaths;
		TArray<FSRPlanetSurfaceGridCellId> ReplacementPreviewCellIds;
		TArray<FSRPlanetSurfaceGridCellId> NewPlacementCellIds;
		TArray<FSRStructurePlacementDragPreviewActor> NewPreviewActors;
		NewPlacementCellIds.Reserve(CandidateCellIds.Num());
		NewPreviewActors.Reserve(CandidateCellIds.Num());
		ReservedFootprintCellIds.Reserve(CandidateCellIds.Num());
		ReplacementPreviewConveyorCellIdSet.Reserve(CandidateCellIds.Num());
		ReplacementPreviewCellIdSet.Reserve(CandidateCellIds.Num());
		ReplacementPreviewConveyorCellIds.Reserve(CandidateCellIds.Num());
		ReplacementPreviewCellIds.Reserve(CandidateCellIds.Num());

		for (const FSRPlanetSurfaceGridCellId& CandidateCellId : CandidateCellIds)
		{
			FSRPlanetSurfaceGridCellInfo CandidateCellInfo;
			TArray<FSRPlanetSurfaceGridCellId> FootprintCellIds;
			ConstructionReplacement::FSRConstructionReplacementTargets CandidateReplacementTargets;
			if (!SurfaceGrid->GetCellInfoById(CandidateCellId, CandidateCellInfo)
				|| !SurfaceGrid->GetFootprintCellIds(CandidateCellId, FootprintCellsX, FootprintCellsY, FootprintCellIds))
			{
				continue;
			}

			const bool bCanBuildOverCells = IsValid(StructureInstanceManager)
				? ConstructionReplacement::CanBuildOverCellsForStructureConstruction(
					SurfaceGrid,
					StructureInstanceManager,
					ConveyorNetwork,
					FootprintCellIds,
					CandidateReplacementTargets)
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
				&& ((!PreviewCellInfo.OccupantId.IsNone()
					&& CandidateReplacementTargets.StructureOccupantIds.Contains(PreviewCellInfo.OccupantId))
					|| CandidateReplacementTargets.ConveyorCellIds.Contains(CandidateCellId)))
			{
				PreviewCellInfo.bOccupied = false;
				PreviewCellInfo.bCanConstruct = true;
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
			ReplacementPreviewOccupantIds.Append(CandidateReplacementTargets.StructureOccupantIds);
			for (const FSRPlanetSurfaceGridCellId& ConveyorCellId : CandidateReplacementTargets.ConveyorCellIds)
			{
				const int32 PreviousCellCount = ReplacementPreviewConveyorCellIdSet.Num();
				ReplacementPreviewConveyorCellIdSet.Add(ConveyorCellId);
				if (ReplacementPreviewConveyorCellIdSet.Num() != PreviousCellCount)
				{
					ReplacementPreviewConveyorCellIds.Add(ConveyorCellId);
				}
			}
			TArray<FSRPlanetSurfaceGridCellId> CandidateReplacementPreviewCellIds;
			ConstructionReplacement::CollectConstructionReplacementPreviewCellIds(
				StructureInstanceManager,
				CandidateReplacementTargets,
				CandidateReplacementPreviewCellIds);
			for (const FSRPlanetSurfaceGridCellId& ReplacementCellId : CandidateReplacementPreviewCellIds)
			{
				const int32 PreviousCellCount = ReplacementPreviewCellIdSet.Num();
				ReplacementPreviewCellIdSet.Add(ReplacementCellId);
				if (ReplacementPreviewCellIdSet.Num() != PreviousCellCount)
				{
					ReplacementPreviewCellIds.Add(ReplacementCellId);
				}
			}
			ReplacementPreviewConveyorBeltPaths.Append(CandidateReplacementTargets.ConveyorBeltPaths);

			ISRBuildableStructureInterface::Execute_SetStructureGhostMode(PreviewActor, true);
			PreviewMaterials::ApplyToActor(
				PreviewActor,
				CandidateReplacementTargets.HasAny()
					? PreviewMaterials::ResolveReplaceableMaterial(StructureData)
					: PreviewMaterials::ResolveGhostMaterial(StructureData));
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
		if (ReplacementPreviewCellIds.IsEmpty())
		{
			SurfaceGrid->ClearConstructionReplacementPreviewCells();
		}
		else
		{
			SurfaceGrid->SetConstructionReplacementPreviewCells(ReplacementPreviewCellIds);
		}
		const FSRPlanetSurfaceGridCellId ConveyorReplacementPreviewTargetCellId = ReplacementPreviewConveyorCellIds.IsEmpty()
			? FSRPlanetSurfaceGridCellId()
			: ReplacementPreviewConveyorCellIds[0];
		ConstructionReplacement::ApplyConveyorReplacementPreview(
			SurfaceGrid,
			ConveyorNetwork,
			ConveyorPreview,
			ReplacementPreviewConveyorCellIds,
			ReplacementPreviewConveyorBeltPaths,
			ConveyorReplacementPreviewTargetCellId);
		return true;
	}
}
