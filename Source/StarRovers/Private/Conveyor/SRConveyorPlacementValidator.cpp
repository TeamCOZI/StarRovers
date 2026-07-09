#include "Conveyor/SRConveyorPlacementValidator.h"

#include "GameFramework/Actor.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"

bool StarRovers::Conveyor::FSRConveyorPlacementValidator::CanPlaceNewLane(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& ExistingSegments,
	const FSRConveyorLaneKey& LaneKey)
{
	if (!IsValid(SurfaceGrid) || ExistingSegments.Contains(LaneKey))
	{
		return false;
	}

	FSRPlanetSurfaceGridCellInfo CellInfo;
	if (!SurfaceGrid->GetCellInfoById(LaneKey.CellId, CellInfo))
	{
		return false;
	}

	if (LaneKey.Layer > 0)
	{
		return true;
	}

	if (!CellInfo.bOccupied)
	{
		return CellInfo.bCanConstruct;
	}

	return CanDestroyStructureForPlacement(SurfaceGrid, CellInfo.OccupantId);
}

bool StarRovers::Conveyor::FSRConveyorPlacementValidator::CanPlaceOrReuseLane(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TMap<FSRConveyorLaneKey, FSRConveyorSegment>& ExistingSegments,
	const FSRConveyorLaneKey& LaneKey,
	const TSet<FSRPlanetSurfaceGridCellId>& IgnoredOccupiedCellIds)
{
	if (ExistingSegments.Contains(LaneKey))
	{
		return true;
	}

	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	FSRPlanetSurfaceGridCellInfo CellInfo;
	if (!SurfaceGrid->GetCellInfoById(LaneKey.CellId, CellInfo))
	{
		return false;
	}

	if (LaneKey.Layer > 0)
	{
		return true;
	}

	if (!CellInfo.bOccupied)
	{
		return CellInfo.bCanConstruct;
	}

	if (IgnoredOccupiedCellIds.Contains(LaneKey.CellId))
	{
		return true;
	}

	return CanDestroyStructureForPlacement(SurfaceGrid, CellInfo.OccupantId);
}

bool StarRovers::Conveyor::FSRConveyorPlacementValidator::CanDestroyStructureForPlacement(
	USRPlanetSurfaceGrid* SurfaceGrid,
	FName OccupantId)
{
	if (!IsValid(SurfaceGrid) || OccupantId.IsNone())
	{
		return false;
	}

	AActor* SurfaceOwner = SurfaceGrid->GetOwner();
	const USRStructureInstanceManagerComponent* StructureInstanceManager = IsValid(SurfaceOwner)
		? SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>()
		: nullptr;
	return IsValid(StructureInstanceManager)
		&& StructureInstanceManager->CanDestroyStructureForConstruction(OccupantId);
}

bool StarRovers::Conveyor::FSRConveyorPlacementValidator::TryRemoveDestructibleStructuresAtCells(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const TArray<FSRPlanetSurfaceGridCellId>& CellIds)
{
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	AActor* SurfaceOwner = SurfaceGrid->GetOwner();
	USRStructureInstanceManagerComponent* StructureInstanceManager = IsValid(SurfaceOwner)
		? SurfaceOwner->FindComponentByClass<USRStructureInstanceManagerComponent>()
		: nullptr;
	return IsValid(StructureInstanceManager)
		&& StructureInstanceManager->TryRemoveConstructionDestructibleStructuresAtCells(SurfaceGrid, CellIds);
}

float StarRovers::Conveyor::FSRConveyorPlacementValidator::ResolveLayerHeight(
	USRPlanetSurfaceGrid* SurfaceGrid,
	float RequestedLayerHeight,
	float DefaultLayerHeight)
{
	const float TerrainHeightStep = IsValid(SurfaceGrid) ? SurfaceGrid->GetTerrainHeightStep() : 0.0f;
	if (TerrainHeightStep > KINDA_SMALL_NUMBER)
	{
		return TerrainHeightStep;
	}

	if (RequestedLayerHeight > KINDA_SMALL_NUMBER)
	{
		return RequestedLayerHeight;
	}

	return DefaultLayerHeight;
}
