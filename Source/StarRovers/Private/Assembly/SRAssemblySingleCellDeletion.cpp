#include "Assembly/SRAssemblySingleCellDeletion.h"

#include "Automation/SRFacilityNetworkComponent.h"
#include "Conveyor/SRConveyorNetworkComponent.h"
#include "GameFramework/Actor.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"

void StarRovers::Assembly::FSRAssemblySingleCellDeletion::BuildCandidateConveyorLayers(
	const USRStructureDataAsset* SelectedStructureDataAsset,
	TArray<int32>& OutLayers)
{
	OutLayers.Reset();
	if (IsValid(SelectedStructureDataAsset))
	{
		const FSRStructureData SelectedStructureData = SelectedStructureDataAsset->BuildData();
		if (SelectedStructureData.BuildKind == ESRStructureBuildKind::Conveyor)
		{
			OutLayers.Add(FMath::Max(0, SelectedStructureData.ConveyorLayer));
		}
	}
	OutLayers.AddUnique(0);
}

bool StarRovers::Assembly::FSRAssemblySingleCellDeletion::TryDeleteStructureAtCell(
	AActor* FocusedActor,
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& TargetCellId,
	const TArray<int32>& CandidateConveyorLayers)
{
	if (!IsValid(FocusedActor) || !IsValid(SurfaceGrid))
	{
		return false;
	}

	USRConveyorNetworkComponent* ConveyorNetwork = FocusedActor->FindComponentByClass<USRConveyorNetworkComponent>();
	if (IsValid(ConveyorNetwork))
	{
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
		FSRPlacedStructureInstance PlacedStructure;
		if (StructureInstanceManager->GetPlacedStructure(TargetCellInfo.OccupantId, PlacedStructure)
			&& PlacedStructure.bNaturalStructure)
		{
			return false;
		}

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
	if (USRFacilityNetworkComponent* FacilityNetwork = FocusedActor->FindComponentByClass<USRFacilityNetworkComponent>())
	{
		FacilityNetwork->UnregisterFacility(TargetCellInfo.OccupantId);
	}
	return SurfaceGrid->SetCellsOccupied(OccupantCellIds, false, NAME_None);
}

bool StarRovers::Assembly::FSRAssemblySingleCellDeletion::TryDeleteConnectedConveyorsAtCell(
	AActor* FocusedActor,
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& TargetCellId,
	const TArray<int32>& CandidateConveyorLayers)
{
	if (!IsValid(FocusedActor) || !IsValid(SurfaceGrid))
	{
		return false;
	}

	USRConveyorNetworkComponent* ConveyorNetwork = FocusedActor->FindComponentByClass<USRConveyorNetworkComponent>();
	if (!IsValid(ConveyorNetwork))
	{
		return false;
	}

	for (const int32 CandidateLayer : CandidateConveyorLayers)
	{
		if (ConveyorNetwork->TryRemoveConnectedConveyorsAtCell(SurfaceGrid, TargetCellId, CandidateLayer))
		{
			return true;
		}
	}

	return false;
}

bool StarRovers::Assembly::FSRAssemblySingleCellDeletion::TryDestroyAttachedOccupantActor(AActor* SurfaceOwner, FName OccupantId)
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
