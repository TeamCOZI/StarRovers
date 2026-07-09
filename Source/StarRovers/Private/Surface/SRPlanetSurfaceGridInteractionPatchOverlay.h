#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "Templates/Function.h"

namespace UE::Geometry
{
	class FDynamicMesh3;
}

namespace StarRovers::SurfaceGridInteractionPatchOverlay
{
	using FPatchCellIdQuery = TFunctionRef<bool(const FSRPlanetSurfaceGridCellId& CenterCellId, TArray<FSRPlanetSurfaceGridCellId>& OutCellIds)>;
	using FAppendPatchRegion = TFunctionRef<void(UE::Geometry::FDynamicMesh3& OverlayMesh, const TArray<FSRPlanetSurfaceGridCellId>& CellIds, const FLinearColor& LineColor, float LineThickness, TSet<uint64>& DrawnEdges)>;

	void AppendInteractionGridPatch(
		UE::Geometry::FDynamicMesh3& OverlayMesh,
		const FSRPlanetSurfaceGridCellId& CenterCellId,
		const FLinearColor& BaseLineColor,
		float LineThickness,
		float DebugLineOpacity,
		TSet<uint64>& DrawnEdges,
		FPatchCellIdQuery GetPatchCellIds,
		FAppendPatchRegion AppendPatchRegion);
}
