#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

class AActor;
struct FSRPlanetSurfaceGridCell;
struct FSRPlanetSurfaceGridCellId;
enum class ESRFacilityTemperatureState : uint8;

namespace StarRovers::SurfaceGridOwnerBody
{
	using FDynamicMeshBoundarySegmentAppender = TFunctionRef<void(const FVector& LocalPointA, const FVector& LocalPointB)>;

	void PrepareDynamicMesh(AActor* Owner);
	bool GetCachedSurfaceGridCells(const AActor* Owner, TArray<FSRPlanetSurfaceGridCell>& OutCells);
	bool AppendDynamicMeshBoundaryWire(const AActor* Owner, FDynamicMeshBoundarySegmentAppender AppendSegment);
	bool ApplySurfaceCellHighlights(
		AActor* Owner,
		const TArray<FSRPlanetSurfaceGridCellId>& HoveredCellIds,
		const TArray<FSRPlanetSurfaceGridCellId>& SelectedCellIds,
		const TArray<FSRPlanetSurfaceGridCellId>& OccupiedPreviewCellIds,
		const FLinearColor& HoveredCellColor,
		const FLinearColor& SelectedCellColor,
		const FLinearColor& OccupiedCellColor);
	void ClearSurfaceCellHighlights(AActor* Owner);
	bool ApplySurfaceTemperatureStateColor(
		AActor* Owner,
		const FSRPlanetSurfaceGridCellId& CellId,
		ESRFacilityTemperatureState TemperatureState);
}
