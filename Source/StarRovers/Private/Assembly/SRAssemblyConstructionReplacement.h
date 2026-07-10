#pragma once

#include "CoreMinimal.h"
#include "Assembly/SRAssemblyPreviewState.h"
#include "Conveyor/SRConveyorNetworkComponent.h"
#include "Conveyor/SRConveyorTypes.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace StarRovers::Assembly::ConstructionReplacement
{
	inline constexpr int32 GroundConveyorLayer = 0;

	struct FSRConstructionReplacementTargets
	{
		TSet<FName> StructureOccupantIds;
		TArray<FSRPlanetSurfaceGridCellId> ConveyorCellIds;
		TArray<FSRConveyorBeltPath> ConveyorBeltPaths;

		bool HasAny() const
		{
			return !StructureOccupantIds.IsEmpty() || !ConveyorCellIds.IsEmpty();
		}
	};

	inline USRConveyorNetworkComponent* FindConveyorNetwork(USRPlanetSurfaceGrid* SurfaceGrid)
	{
		AActor* SurfaceOwner = IsValid(SurfaceGrid) ? SurfaceGrid->GetOwner() : nullptr;
		return IsValid(SurfaceOwner)
			? SurfaceOwner->FindComponentByClass<USRConveyorNetworkComponent>()
			: nullptr;
	}

	inline bool HasGroundConveyorAtCell(
		USRConveyorNetworkComponent* ConveyorNetwork,
		const FSRPlanetSurfaceGridCellId& CellId)
	{
		if (!IsValid(ConveyorNetwork))
		{
			return false;
		}

		FSRConveyorLaneKey LaneKey;
		LaneKey.CellId = CellId;
		LaneKey.Layer = GroundConveyorLayer;
		return ConveyorNetwork->HasConveyorSegment(LaneKey);
	}

	inline void CollectGroundConveyorBeltPaths(
		USRConveyorNetworkComponent* ConveyorNetwork,
		const TArray<FSRPlanetSurfaceGridCellId>& CellIds,
		TArray<FSRConveyorBeltPath>& OutBeltPaths)
	{
		OutBeltPaths.Reset();
		if (!IsValid(ConveyorNetwork) || CellIds.IsEmpty())
		{
			return;
		}

		TSet<FSRPlanetSurfaceGridCellId> CellIdSet;
		CellIdSet.Reserve(CellIds.Num());
		for (const FSRPlanetSurfaceGridCellId& CellId : CellIds)
		{
			CellIdSet.Add(CellId);
		}

		TArray<FSRConveyorBeltPath> BeltPathsInCells;
		ConveyorNetwork->GetConveyorBeltPathsInCells(CellIdSet, BeltPathsInCells);
		for (const FSRConveyorBeltPath& BeltPath : BeltPathsInCells)
		{
			if (FMath::Max(0, BeltPath.Layer) == GroundConveyorLayer)
			{
				OutBeltPaths.Add(BeltPath);
			}
		}
	}

	inline void CollectConstructionReplacementPreviewCellIds(
		USRStructureInstanceManagerComponent* StructureInstanceManager,
		const FSRConstructionReplacementTargets& Targets,
		TArray<FSRPlanetSurfaceGridCellId>& OutCellIds)
	{
		OutCellIds.Reset();
		for (const FSRPlanetSurfaceGridCellId& CellId : Targets.ConveyorCellIds)
		{
			OutCellIds.AddUnique(CellId);
		}

		if (!IsValid(StructureInstanceManager))
		{
			return;
		}

		for (const FName OccupantId : Targets.StructureOccupantIds)
		{
			FSRPlacedStructureInstance PlacedStructure;
			if (!StructureInstanceManager->GetPlacedStructure(OccupantId, PlacedStructure))
			{
				continue;
			}

			for (const FSRPlanetSurfaceGridCellId& CellId : PlacedStructure.FootprintCellIds)
			{
				OutCellIds.AddUnique(CellId);
			}
		}
	}

	inline void ApplyConstructionReplacementPreview(
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRStructureInstanceManagerComponent* StructureInstanceManager,
		const FSRConstructionReplacementTargets& Targets)
	{
		if (!IsValid(SurfaceGrid))
		{
			return;
		}

		TArray<FSRPlanetSurfaceGridCellId> ReplacementPreviewCellIds;
		CollectConstructionReplacementPreviewCellIds(
			StructureInstanceManager,
			Targets,
			ReplacementPreviewCellIds);
		if (ReplacementPreviewCellIds.IsEmpty())
		{
			SurfaceGrid->ClearConstructionReplacementPreviewCells();
			return;
		}

		SurfaceGrid->SetConstructionReplacementPreviewCells(ReplacementPreviewCellIds);
	}

	inline bool CanBuildOverCellsForStructureConstruction(
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRStructureInstanceManagerComponent* StructureInstanceManager,
		USRConveyorNetworkComponent* ConveyorNetwork,
		const TArray<FSRPlanetSurfaceGridCellId>& CellIds,
		FSRConstructionReplacementTargets& OutTargets)
	{
		OutTargets = FSRConstructionReplacementTargets();
		if (!IsValid(SurfaceGrid))
		{
			return false;
		}

		for (const FSRPlanetSurfaceGridCellId& CellId : CellIds)
		{
			FSRPlanetSurfaceGridCellInfo CellInfo;
			if (!SurfaceGrid->GetCellInfoById(CellId, CellInfo))
			{
				return false;
			}

			if (!CellInfo.bOccupied)
			{
				if (!CellInfo.bCanConstruct)
				{
					return false;
				}
				continue;
			}

			if (!CellInfo.OccupantId.IsNone()
				&& IsValid(StructureInstanceManager)
				&& StructureInstanceManager->CanDestroyStructureForConstruction(CellInfo.OccupantId))
			{
				OutTargets.StructureOccupantIds.Add(CellInfo.OccupantId);
				continue;
			}

			if (HasGroundConveyorAtCell(ConveyorNetwork, CellId))
			{
				OutTargets.ConveyorCellIds.AddUnique(CellId);
				continue;
			}

			return false;
		}

		CollectGroundConveyorBeltPaths(ConveyorNetwork, OutTargets.ConveyorCellIds, OutTargets.ConveyorBeltPaths);
		return true;
	}

	inline void RestoreConveyorBeltPaths(
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRConveyorNetworkComponent* ConveyorNetwork,
		const TArray<FSRConveyorBeltPath>& BeltPaths);

	inline void ApplyConveyorReplacementPreview(
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRConveyorNetworkComponent* ConveyorNetwork,
		FSRAssemblyConveyorPreviewState& ConveyorPreview,
		const TArray<FSRPlanetSurfaceGridCellId>& CellIds,
		const TArray<FSRConveyorBeltPath>& BeltPaths,
		const FSRPlanetSurfaceGridCellId& TargetCellId)
	{
		if (!IsValid(SurfaceGrid) || CellIds.IsEmpty())
		{
			ConveyorPreview.ClearBulkDeletionPreview();
			return;
		}

		ConveyorPreview.SetBulkDeletionPreview(SurfaceGrid, CellIds);
		if (!IsValid(ConveyorNetwork) || BeltPaths.IsEmpty())
		{
			ConveyorPreview.DestroyDeletionGhostActor();
			return;
		}

		USRStructureDataAsset* ConveyorDataAsset = BeltPaths[0].StructureDataAsset.Get();
		if (!IsValid(ConveyorDataAsset))
		{
			ConveyorPreview.DestroyDeletionGhostActor();
			return;
		}

		const FSRStructureData ConveyorData = ConveyorDataAsset->BuildData();
		ConveyorPreview.UpdateDeletionGhostActor(
			SurfaceGrid,
			ConveyorDataAsset,
			ConveyorData,
			BeltPaths,
			ConveyorNetwork->GetConveyorActorSplineComponentTag(),
			ConveyorNetwork->GetConveyorActorSurfaceOffset(),
			TargetCellId,
			GroundConveyorLayer);
	}

	inline bool RemoveReplacementTargets(
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRStructureInstanceManagerComponent* StructureInstanceManager,
		USRConveyorNetworkComponent* ConveyorNetwork,
		const FSRConstructionReplacementTargets& Targets,
		TArray<FSRPlacedStructureInstance>* OutRemovedStructures = nullptr)
	{
		if (OutRemovedStructures)
		{
			OutRemovedStructures->Reset();
		}

		if (!IsValid(SurfaceGrid))
		{
			return false;
		}

		bool bRemovedAnyConveyor = false;
		if (!Targets.ConveyorCellIds.IsEmpty())
		{
			if (!IsValid(ConveyorNetwork))
			{
				return false;
			}

			for (const FSRPlanetSurfaceGridCellId& CellId : Targets.ConveyorCellIds)
			{
				bRemovedAnyConveyor |= ConveyorNetwork->TryRemoveConveyorAtCell(SurfaceGrid, CellId, GroundConveyorLayer);
			}

			if (!bRemovedAnyConveyor)
			{
				return false;
			}
		}

		if (Targets.StructureOccupantIds.IsEmpty())
		{
			return true;
		}

		if (!IsValid(StructureInstanceManager)
			|| !StructureInstanceManager->RemoveConstructionDestructibleStructuresByOccupantIds(
				SurfaceGrid,
				Targets.StructureOccupantIds,
				OutRemovedStructures))
		{
			if (bRemovedAnyConveyor)
			{
				RestoreConveyorBeltPaths(SurfaceGrid, ConveyorNetwork, Targets.ConveyorBeltPaths);
			}
			return false;
		}

		return true;
	}

	inline void RestoreConveyorBeltPaths(
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRConveyorNetworkComponent* ConveyorNetwork,
		const TArray<FSRConveyorBeltPath>& BeltPaths)
	{
		if (!IsValid(SurfaceGrid) || !IsValid(ConveyorNetwork))
		{
			return;
		}

		for (const FSRConveyorBeltPath& BeltPath : BeltPaths)
		{
			USRStructureDataAsset* StructureDataAsset = BeltPath.StructureDataAsset.Get();
			if (!IsValid(StructureDataAsset) || BeltPath.CellIds.IsEmpty())
			{
				continue;
			}

			ConveyorNetwork->TryPlaceConveyorPath(
				SurfaceGrid,
				BeltPath.CellIds,
				FMath::Max(0, BeltPath.Layer),
				BeltPath.LayerHeight,
				StructureDataAsset,
				BeltPath.NetworkId);
		}
	}
}
