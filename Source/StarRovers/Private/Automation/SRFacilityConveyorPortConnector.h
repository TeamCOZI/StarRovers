#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityRuntimeData.h"

class USRPlanetSurfaceGrid;

class FSRFacilityConveyorPortConnector
{
public:
	static FSRFacilityPortInventory* FindConnectedInputPortInventory(
		USRPlanetSurfaceGrid* SurfaceGrid,
		FSRFacilityInstance& FacilityInstance,
		const FSRPlanetSurfaceGridCellId& ConveyorCellId,
		const FSRResourceInstance& ResourceInstance);

	static FSRFacilityPortInventory* FindConnectedOutputPortInventory(
		USRPlanetSurfaceGrid* SurfaceGrid,
		FSRFacilityInstance& FacilityInstance,
		const FSRPlanetSurfaceGridCellId& ConveyorCellId);

	static bool IsConveyorCellConnectedToPortInventory(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRFacilityInstance& FacilityInstance,
		const FSRFacilityPortInventory& PortInventory,
		const FSRPlanetSurfaceGridCellId& ConveyorCellId);

	static bool IsConveyorCellConnectedToFacilityPort(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRFacilityInstance& FacilityInstance,
		const FSRPlanetSurfaceGridCellId& ConveyorCellId,
		ESRFacilityPortKind PortKind);

	static bool GetNeighborCellIds(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& CellId,
		TArray<FSRPlanetSurfaceGridCellId>& OutNeighborCellIds);
};
