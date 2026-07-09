#include "SRFacilityConveyorPortConnector.h"

#include "SRFacilityResourceOperations.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureSurfacePortConnection.h"
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

	bool IsConveyorCellConnectedToExplicitPort(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRFacilityInstance& FacilityInstance,
		const FSRStructurePortSpec& PortSpec,
		const FSRPlanetSurfaceGridCellId& ConveyorCellId)
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
}

FSRFacilityPortInventory* FSRFacilityConveyorPortConnector::FindConnectedInputPortInventory(
	USRPlanetSurfaceGrid* SurfaceGrid,
	FSRFacilityInstance& FacilityInstance,
	const FSRPlanetSurfaceGridCellId& ConveyorCellId,
	const FSRResourceInstance& ResourceInstance)
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
			if (StarRovers::FacilityResources::CanInventorySlotAcceptResource(InputPortInventory, ResourceInstance))
			{
				return &InputPortInventory;
			}
		}
	}

	return FirstConnectedPortInventory;
}

FSRFacilityPortInventory* FSRFacilityConveyorPortConnector::FindConnectedOutputPortInventory(
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
			if (StarRovers::FacilityResources::GetInventorySlotStackCount(OutputPortInventory) > 0)
			{
				return &OutputPortInventory;
			}
		}
	}

	return FirstConnectedPortInventory;
}

bool FSRFacilityConveyorPortConnector::IsConveyorCellConnectedToPortInventory(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRFacilityInstance& FacilityInstance,
	const FSRFacilityPortInventory& PortInventory,
	const FSRPlanetSurfaceGridCellId& ConveyorCellId)
{
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	return IsConveyorCellConnectedToExplicitPort(SurfaceGrid, FacilityInstance, PortInventory.PortSpec, ConveyorCellId);
}

bool FSRFacilityConveyorPortConnector::IsConveyorCellConnectedToFacilityPort(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRFacilityInstance& FacilityInstance,
	const FSRPlanetSurfaceGridCellId& ConveyorCellId,
	ESRFacilityPortKind PortKind)
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

bool FSRFacilityConveyorPortConnector::GetNeighborCellIds(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& CellId,
	TArray<FSRPlanetSurfaceGridCellId>& OutNeighborCellIds)
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
