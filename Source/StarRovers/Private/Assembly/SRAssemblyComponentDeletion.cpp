#include "Assembly/SRAssemblyComponent.h"

#include "Automation/SRFacilityNetworkComponent.h"
#include "Camera/SRPlayerController.h"
#include "Conveyor/SRConveyorNetworkComponent.h"
#include "GameFramework/Actor.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"

bool USRAssemblyComponent::TryDeleteStructureAtCell(AActor* FocusedActor, USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& TargetCellId)
{
	if (!IsValid(FocusedActor) || !IsValid(SurfaceGrid))
	{
		return false;
	}

	USRConveyorNetworkComponent* ConveyorNetwork = FocusedActor->FindComponentByClass<USRConveyorNetworkComponent>();
	if (IsValid(ConveyorNetwork))
	{
		TArray<int32> CandidateConveyorLayers;
		if (const ASRPlayerController* PlayerController = GetOwnerController())
		{
			if (USRStructureDataAsset* SelectedStructureDataAsset = PlayerController->GetSelectedStructureDataAsset())
			{
				const FSRStructureData SelectedStructureData = SelectedStructureDataAsset->BuildData();
				if (SelectedStructureData.BuildKind == ESRStructureBuildKind::Conveyor)
				{
					CandidateConveyorLayers.Add(FMath::Max(0, SelectedStructureData.ConveyorLayer));
				}
			}
		}
		CandidateConveyorLayers.AddUnique(0);

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

bool USRAssemblyComponent::TryDestroyAttachedOccupantActor(AActor* SurfaceOwner, FName OccupantId) const
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
