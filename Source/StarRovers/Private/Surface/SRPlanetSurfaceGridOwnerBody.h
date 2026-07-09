#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

class AActor;
struct FSRPlanetSurfaceGridCell;
struct FSRPlanetSurfaceGridCellId;

namespace StarRovers::SurfaceGridOwnerBody
{
	using FDynamicMeshBoundarySegmentAppender = TFunctionRef<void(const FVector& LocalPointA, const FVector& LocalPointB)>;

	void PrepareDynamicMesh(AActor* Owner);
	bool GetCachedSurfaceGridCells(const AActor* Owner, TArray<FSRPlanetSurfaceGridCell>& OutCells);
	bool AppendDynamicMeshBoundaryWire(const AActor* Owner, FDynamicMeshBoundarySegmentAppender AppendSegment);
	bool ApplySurfaceCellHighlights(
		AActor* Owner,
		const FSRPlanetSurfaceGridCellId& HoveredCellId,
		bool bHasHoveredCell,
		const FSRPlanetSurfaceGridCellId& SelectedCellId,
		bool bHasSelectedCell,
		const FLinearColor& HoveredCellColor,
		const FLinearColor& SelectedCellColor);
	void ClearSurfaceCellHighlights(AActor* Owner);
}
