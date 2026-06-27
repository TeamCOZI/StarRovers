#include "Automation/SRFacilityNetworkComponent.h"

#include "Conveyor/SRConveyorNetworkComponent.h"
#include "GameFramework/Actor.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureSurfacePortHelpers.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	bool GetFacilityNeighborCellIdByStructurePortDirection(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& CellId,
		ESRStructurePortDirection Direction,
		FSRPlanetSurfaceGridCellId& OutNeighborCellId)
	{
		return StarRovers::Structure::SurfacePorts::TryGetPortConnectionCellId(
			SurfaceGrid,
			CellId,
			Direction,
			OutNeighborCellId);
	}

	bool GetStructurePortFootprintCellId(
		const FSRFacilityInstance& FacilityInstance,
		const FSRStructureData& StructureData,
		const FSRStructurePortSpec& PortSpec,
		FSRPlanetSurfaceGridCellId& OutCellId)
	{
		OutCellId = FSRPlanetSurfaceGridCellId();
		if (FacilityInstance.FootprintCellIds.IsEmpty())
		{
			return false;
		}

		const int32 FootprintCellsX = StarRovers::Structure::GetRotatedFootprintCellsX(StructureData, FacilityInstance.PlacementRotationSteps);
		const int32 FootprintCellsY = StarRovers::Structure::GetRotatedFootprintCellsY(StructureData, FacilityInstance.PlacementRotationSteps);
		if (PortSpec.CellOffsetX < 0
			|| PortSpec.CellOffsetY < 0
			|| PortSpec.CellOffsetX >= FootprintCellsX
			|| PortSpec.CellOffsetY >= FootprintCellsY)
		{
			return false;
		}

		const int32 FootprintIndex = PortSpec.CellOffsetY * FootprintCellsX + PortSpec.CellOffsetX;
		if (!FacilityInstance.FootprintCellIds.IsValidIndex(FootprintIndex))
		{
			return false;
		}

		OutCellId = FacilityInstance.FootprintCellIds[FootprintIndex];
		return true;
	}

}

bool USRFacilityNetworkComponent::TryAcceptInputResourceFromConveyorCell(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& ConveyorCellId,
	const FSRResourceInstance& ResourceInstance,
	FName SourceFacilityOccupantId)
{
	if (!IsValid(SurfaceGrid) || ResourceInstance.ResourceId.IsNone() || ResourceInstance.StackCount <= 0)
	{
		return false;
	}

	for (TPair<FName, FSRFacilityInstance>& FacilityPair : FacilityInstancesByOccupantId)
	{
		FSRFacilityInstance& FacilityInstance = FacilityPair.Value;
		const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
		FSRFacilityPortInventory* InputPortInventory = FindConnectedInputPortInventory(SurfaceGrid, FacilityInstance, ConveyorCellId);
		if (!IsValid(FacilityDataAsset)
			|| !InputPortInventory
			|| (!SourceFacilityOccupantId.IsNone() && FacilityInstance.OccupantId == SourceFacilityOccupantId)
			|| InputPortInventory->Inventory.Num() >= FMath::Max(1, InputPortInventory->Capacity))
		{
			continue;
		}

		InputPortInventory->Inventory.Add(ResourceInstance);
		RefreshFacilityAggregateInventories(FacilityInstance);
		SetComponentTickEnabled(bAutoProcessFacilities);
		if (bLogFacilityNetworkEvents)
		{
			UE_LOG(
				LogTemp,
				Display,
				TEXT("[FacilityNetwork] Transfer accepted input: OccupantId=%s Port=%s ResourceId=%s PortInputCount=%d InputCount=%d Owner=%s"),
				*FacilityInstance.OccupantId.ToString(),
				*InputPortInventory->PortId.ToString(),
				*ResourceInstance.ResourceId.ToString(),
				InputPortInventory->Inventory.Num(),
				FacilityInstance.InputInventory.Num(),
				*GetNameSafe(GetOwner()));
		}
		return true;
	}

	return false;
}

bool USRFacilityNetworkComponent::TryPullOutputResourceToConveyorCell(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& ConveyorCellId,
	FSRResourceInstance& OutResourceInstance,
	FName& OutSourceFacilityOccupantId)
{
	if (!IsValid(SurfaceGrid))
	{
		OutResourceInstance = FSRResourceInstance();
		OutSourceFacilityOccupantId = NAME_None;
		return false;
	}

	for (TPair<FName, FSRFacilityInstance>& FacilityPair : FacilityInstancesByOccupantId)
	{
		FSRFacilityInstance& FacilityInstance = FacilityPair.Value;
		FSRFacilityPortInventory* OutputPortInventory = FindConnectedOutputPortInventory(SurfaceGrid, FacilityInstance, ConveyorCellId);
		if (!OutputPortInventory
			|| OutputPortInventory->Inventory.IsEmpty()
			|| !FacilityInstance.bDeliverEnabled
			|| !IsConveyorCellConnectedToPortInventory(SurfaceGrid, FacilityInstance, *OutputPortInventory, ConveyorCellId))
		{
			continue;
		}

		OutResourceInstance = OutputPortInventory->Inventory[0];
		OutSourceFacilityOccupantId = FacilityInstance.OccupantId;
		OutputPortInventory->Inventory.RemoveAt(0);
		RefreshFacilityAggregateInventories(FacilityInstance);
		if (bLogFacilityNetworkEvents)
		{
			UE_LOG(
				LogTemp,
				Display,
				TEXT("[FacilityNetwork] Transfer provided output: OccupantId=%s Port=%s ResourceId=%s RemainingPortOutput=%d RemainingOutput=%d Owner=%s"),
				*FacilityInstance.OccupantId.ToString(),
				*OutputPortInventory->PortId.ToString(),
				*OutResourceInstance.ResourceId.ToString(),
				OutputPortInventory->Inventory.Num(),
				FacilityInstance.OutputInventory.Num(),
				*GetNameSafe(GetOwner()));
		}
		return true;
	}

	OutResourceInstance = FSRResourceInstance();
	OutSourceFacilityOccupantId = NAME_None;
	return false;
}

bool USRFacilityNetworkComponent::HasConnectedConveyorForFacilityPort(FName OccupantId, ESRFacilityPortKind PortKind) const
{
	const FSRFacilityInstance* FacilityInstance = FacilityInstancesByOccupantId.Find(OccupantId);
	if (!FacilityInstance)
	{
		return false;
	}

	AActor* Owner = GetOwner();
	USRPlanetSurfaceGrid* SurfaceGrid = IsValid(Owner) ? Owner->FindComponentByClass<USRPlanetSurfaceGrid>() : nullptr;
	const USRConveyorNetworkComponent* ConveyorNetwork = IsValid(Owner) ? Owner->FindComponentByClass<USRConveyorNetworkComponent>() : nullptr;
	if (!IsValid(SurfaceGrid) || !IsValid(ConveyorNetwork))
	{
		return false;
	}

	for (const FSRPlanetSurfaceGridCellId& FootprintCellId : FacilityInstance->FootprintCellIds)
	{
		TArray<FSRPlanetSurfaceGridCellId> NeighborCellIds;
		if (!GetNeighborCellIds(SurfaceGrid, FootprintCellId, NeighborCellIds))
		{
			continue;
		}

		for (const FSRPlanetSurfaceGridCellId& NeighborCellId : NeighborCellIds)
		{
			if (ConveyorNetwork->HasConveyorSegmentAtCell(NeighborCellId)
				&& IsConveyorCellConnectedToFacilityPort(SurfaceGrid, *FacilityInstance, NeighborCellId, PortKind))
			{
				return true;
			}
		}
	}

	return false;
}

FSRFacilityPortInventory* USRFacilityNetworkComponent::FindConnectedInputPortInventory(
	USRPlanetSurfaceGrid* SurfaceGrid,
	FSRFacilityInstance& FacilityInstance,
	const FSRPlanetSurfaceGridCellId& ConveyorCellId)
{
	FSRFacilityPortInventory* FirstConnectedPortInventory = nullptr;
	for (FSRFacilityPortInventory& InputPortInventory : FacilityInstance.InputPortInventories)
	{
		if (IsConveyorCellConnectedToPortInventory(SurfaceGrid, FacilityInstance, InputPortInventory, ConveyorCellId))
		{
			if (!FirstConnectedPortInventory)
			{
				FirstConnectedPortInventory = &InputPortInventory;
			}
			if (InputPortInventory.Inventory.Num() < FMath::Max(1, InputPortInventory.Capacity))
			{
				return &InputPortInventory;
			}
		}
	}

	return FirstConnectedPortInventory;
}

FSRFacilityPortInventory* USRFacilityNetworkComponent::FindConnectedOutputPortInventory(
	USRPlanetSurfaceGrid* SurfaceGrid,
	FSRFacilityInstance& FacilityInstance,
	const FSRPlanetSurfaceGridCellId& ConveyorCellId)
{
	FSRFacilityPortInventory* FirstConnectedPortInventory = nullptr;
	for (FSRFacilityPortInventory& OutputPortInventory : FacilityInstance.OutputPortInventories)
	{
		if (IsConveyorCellConnectedToPortInventory(SurfaceGrid, FacilityInstance, OutputPortInventory, ConveyorCellId))
		{
			if (!FirstConnectedPortInventory)
			{
				FirstConnectedPortInventory = &OutputPortInventory;
			}
			if (!OutputPortInventory.Inventory.IsEmpty())
			{
				return &OutputPortInventory;
			}
		}
	}

	return FirstConnectedPortInventory;
}

bool USRFacilityNetworkComponent::IsConveyorCellConnectedToPortInventory(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRFacilityInstance& FacilityInstance,
	const FSRFacilityPortInventory& PortInventory,
	const FSRPlanetSurfaceGridCellId& ConveyorCellId) const
{
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	return IsConveyorCellConnectedToExplicitPort(SurfaceGrid, FacilityInstance, PortInventory.PortSpec, ConveyorCellId);
}

bool USRFacilityNetworkComponent::IsConveyorCellConnectedToFacilityPort(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRFacilityInstance& FacilityInstance,
	const FSRPlanetSurfaceGridCellId& ConveyorCellId,
	ESRFacilityPortKind PortKind) const
{
	const TArray<FSRFacilityPortInventory>& PortInventories = PortKind == ESRFacilityPortKind::Output
		? FacilityInstance.OutputPortInventories
		: FacilityInstance.InputPortInventories;
	for (const FSRFacilityPortInventory& PortInventory : PortInventories)
	{
		if (IsConveyorCellConnectedToPortInventory(SurfaceGrid, FacilityInstance, PortInventory, ConveyorCellId))
		{
			return true;
		}
	}

	return false;
}

bool USRFacilityNetworkComponent::IsConveyorCellConnectedToExplicitPort(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRFacilityInstance& FacilityInstance,
	const FSRStructurePortSpec& PortSpec,
	const FSRPlanetSurfaceGridCellId& ConveyorCellId) const
{
	if (!IsValid(FacilityInstance.StructureDataAsset.Get()))
	{
		return false;
	}

	const FSRStructureData StructureData = FacilityInstance.StructureDataAsset->BuildData();
	FSRPlanetSurfaceGridCellId PortCellId;
	if (!GetStructurePortFootprintCellId(FacilityInstance, StructureData, PortSpec, PortCellId))
	{
		return false;
	}

	FSRPlanetSurfaceGridCellId NeighborCellId;
	if (!GetFacilityNeighborCellIdByStructurePortDirection(SurfaceGrid, PortCellId, PortSpec.Direction, NeighborCellId))
	{
		return false;
	}

	return NeighborCellId == ConveyorCellId;
}

bool USRFacilityNetworkComponent::GetNeighborCellIds(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& CellId,
	TArray<FSRPlanetSurfaceGridCellId>& OutNeighborCellIds) const
{
	OutNeighborCellIds.Reset();
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	FSRPlanetSurfaceGridCellNeighbors Neighbors;
	if (!SurfaceGrid->GetCellNeighbors(CellId, Neighbors))
	{
		return false;
	}

	OutNeighborCellIds.Add(Neighbors.NegativeU);
	OutNeighborCellIds.Add(Neighbors.PositiveU);
	OutNeighborCellIds.Add(Neighbors.NegativeV);
	OutNeighborCellIds.Add(Neighbors.PositiveV);

	return !OutNeighborCellIds.IsEmpty();
}
