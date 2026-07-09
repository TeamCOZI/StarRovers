#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridRuntimeState.h"

namespace StarRovers::SurfaceGridCellIndex
{
	int32 GetFlatCellIndex(const FSRPlanetSurfaceGridCellId& CellId, int32 FaceResolution);

	bool GetCellIndex(
		const FSRPlanetSurfaceGridCellIndexState& CellIndexState,
		const TArray<FSRPlanetSurfaceGridCell>& Cells,
		int32 FaceResolution,
		const FSRPlanetSurfaceGridCellId& CellId,
		int32& OutIndex);

	void RebuildCellIndex(
		FSRPlanetSurfaceGridCellIndexState& CellIndexState,
		const TArray<FSRPlanetSurfaceGridCell>& Cells,
		int32 FaceResolution);

	TArray<int32> BuildFlatCellIndexFromMap(
		const TMap<FSRPlanetSurfaceGridCellId, int32>& CellIndexById,
		int32 FaceResolution);

	bool TryAssignFlatCellIndex(
		FSRPlanetSurfaceGridCellIndexState& CellIndexState,
		TArray<int32>&& NewCellIndexByFlatId,
		int32 FaceResolution);

	FSRPlanetSurfaceGridCellInfo BuildCellInfo(const FSRPlanetSurfaceGridCell& Cell, int32 FaceResolution);

	FSRPlanetSurfaceGridCellInfo ResolveRuntimeCellInfo(
		const FSRPlanetSurfaceGridCellInfo& CellInfo,
		const FTransform& ComponentTransform);

	void RebuildCellInfoIndex(
		FSRPlanetSurfaceGridCellIndexState& CellIndexState,
		const TArray<FSRPlanetSurfaceGridCell>& Cells,
		int32 FaceResolution);

	bool GetStoredCellInfoById(
		const FSRPlanetSurfaceGridCellIndexState& CellIndexState,
		const TArray<FSRPlanetSurfaceGridCell>& Cells,
		int32 FaceResolution,
		const FSRPlanetSurfaceGridCellId& CellId,
		FSRPlanetSurfaceGridCellInfo& OutCellInfo);

	void StoreCellInfo(
		FSRPlanetSurfaceGridCellIndexState& CellIndexState,
		int32 FaceResolution,
		const FSRPlanetSurfaceGridCellInfo& CellInfo);
}
