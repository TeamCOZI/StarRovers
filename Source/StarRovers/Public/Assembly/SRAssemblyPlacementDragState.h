#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "SRAssemblyPlacementDragState.generated.h"

class USRPlanetSurfaceGrid;

USTRUCT()
struct STARROVERS_API FSRAssemblyPlacementDragState
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	bool bIsStructurePlacementDragActive = false;

	UPROPERTY(Transient)
	bool bIsConveyorPlacementDragActive = false;

	UPROPERTY(Transient)
	TObjectPtr<USRPlanetSurfaceGrid> LastStructurePlacementDragSurfaceGrid = nullptr;

	UPROPERTY(Transient)
	FSRPlanetSurfaceGridCellId LastStructurePlacementDragCellId;

	UPROPERTY(Transient)
	bool bHasLastStructurePlacementDragCellId = false;

	UPROPERTY(Transient)
	TObjectPtr<USRPlanetSurfaceGrid> StructurePlacementDragSurfaceGrid = nullptr;

	UPROPERTY(Transient)
	FSRPlanetSurfaceGridCellId StructurePlacementDragStartCellId;

	UPROPERTY(Transient)
	int32 StructurePlacementDragRotationSteps = 0;

	UPROPERTY(Transient)
	TArray<FSRPlanetSurfaceGridCellId> StructurePlacementDragCellIds;

	UPROPERTY(Transient)
	TObjectPtr<USRPlanetSurfaceGrid> PendingConveyorStartSurfaceGrid = nullptr;

	UPROPERTY(Transient)
	FSRPlanetSurfaceGridCellId PendingConveyorStartCellId;

	UPROPERTY(Transient)
	bool bHasPendingConveyorStartCell = false;

	UPROPERTY(Transient)
	TObjectPtr<USRPlanetSurfaceGrid> ConveyorDragStartSurfaceGrid = nullptr;

	UPROPERTY(Transient)
	TArray<FSRPlanetSurfaceGridCellId> ConveyorDragWaypointCellIds;

	UPROPERTY(Transient)
	FSRPlanetSurfaceGridCellId ConveyorDragStartCellId;

	UPROPERTY(Transient)
	bool bHasConveyorDragStartCell = false;

	bool HasLastStructurePlacementDragCell(USRPlanetSurfaceGrid* SurfaceGrid) const
	{
		return LastStructurePlacementDragSurfaceGrid == SurfaceGrid
			&& bHasLastStructurePlacementDragCellId;
	}

	bool IsLastStructurePlacementDragCell(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& CellId) const
	{
		return HasLastStructurePlacementDragCell(SurfaceGrid)
			&& LastStructurePlacementDragCellId == CellId;
	}

	void SetLastStructurePlacementDragCell(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& CellId)
	{
		LastStructurePlacementDragSurfaceGrid = SurfaceGrid;
		LastStructurePlacementDragCellId = CellId;
		bHasLastStructurePlacementDragCellId = true;
	}

	void BeginConveyorPlacementDrag(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& StartCellId)
	{
		bIsStructurePlacementDragActive = true;
		bIsConveyorPlacementDragActive = true;
		ConveyorDragStartSurfaceGrid = SurfaceGrid;
		ConveyorDragWaypointCellIds.Reset();
		ConveyorDragStartCellId = StartCellId;
		bHasConveyorDragStartCell = true;
		SetLastStructurePlacementDragCell(SurfaceGrid, StartCellId);
	}

	void BeginStructurePlacementDrag(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& StartCellId, int32 PlacementRotationSteps)
	{
		bIsStructurePlacementDragActive = true;
		bIsConveyorPlacementDragActive = false;
		StructurePlacementDragSurfaceGrid = SurfaceGrid;
		StructurePlacementDragStartCellId = StartCellId;
		StructurePlacementDragRotationSteps = PlacementRotationSteps;
		StructurePlacementDragCellIds.Reset();
	}

	void ResetPlacementDrag()
	{
		bIsStructurePlacementDragActive = false;
		bIsConveyorPlacementDragActive = false;
		LastStructurePlacementDragSurfaceGrid = nullptr;
		LastStructurePlacementDragCellId = FSRPlanetSurfaceGridCellId();
		bHasLastStructurePlacementDragCellId = false;
		StructurePlacementDragSurfaceGrid = nullptr;
		StructurePlacementDragStartCellId = FSRPlanetSurfaceGridCellId();
		StructurePlacementDragRotationSteps = 0;
		StructurePlacementDragCellIds.Reset();
		ConveyorDragStartSurfaceGrid = nullptr;
		ConveyorDragWaypointCellIds.Reset();
		ConveyorDragStartCellId = FSRPlanetSurfaceGridCellId();
		bHasConveyorDragStartCell = false;
	}

	void ResetPendingConveyorStart()
	{
		PendingConveyorStartSurfaceGrid = nullptr;
		PendingConveyorStartCellId = FSRPlanetSurfaceGridCellId();
		bHasPendingConveyorStartCell = false;
	}
};
