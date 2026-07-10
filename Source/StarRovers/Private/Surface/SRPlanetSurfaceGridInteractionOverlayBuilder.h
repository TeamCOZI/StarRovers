#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "Templates/Function.h"

namespace UE::Geometry
{
	class FDynamicMesh3;
}

namespace StarRovers::SurfaceGridInteractionOverlayBuilder
{
	struct FSRInteractionOverlayBuildInput
	{
		bool bGridVisible = false;
		bool bIncludeCellHighlightOverlay = false;
		bool bHasHoveredCell = false;
		bool bHasSelectedCell = false;
		bool bHoveredInteractionGridPatchVisible = false;
		FSRPlanetSurfaceGridCellId HoveredCellId;
		FSRPlanetSurfaceGridCellId SelectedCellId;
		FLinearColor HoveredCellColor;
		FLinearColor SelectedCellColor;
		FLinearColor OccupiedCellColor;
		FLinearColor AreaSelectionCellColor;
		FLinearColor InputPortPreviewCellColor;
		FLinearColor OutputPortPreviewCellColor;
		FLinearColor DeletionPreviewCellColor;
		FLinearColor InvalidPreviewCellColor;
		float DebugLineThickness = 1.0f;
		const TArray<FSRPlanetSurfaceGridCellId>* AreaSelectionCellIds = nullptr;
		const TArray<FSRPlanetSurfaceGridCellId>* InputPortPreviewCellIds = nullptr;
		const TArray<FSRPlanetSurfaceGridCellId>* OutputPortPreviewCellIds = nullptr;
		const TArray<FSRPlanetSurfaceGridCellId>* OccupiedPreviewCellIds = nullptr;
		const TArray<FSRPlanetSurfaceGridCellId>* DeletionPreviewCellIds = nullptr;
		const TArray<FSRPlanetSurfaceGridCellId>* ConstructionReplacementPreviewCellIds = nullptr;
		const TArray<FSRPlanetSurfaceGridCellId>* InvalidPreviewCellIds = nullptr;
	};

	using FCellLookup = TFunctionRef<bool(const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCell& OutCell)>;
	using FAppendInteractionCell = TFunctionRef<void(UE::Geometry::FDynamicMesh3& OverlayMesh, const FSRPlanetSurfaceGridCell& Cell, const FLinearColor& LineColor, float LineThickness)>;
	using FAppendInteractionRegion = TFunctionRef<void(UE::Geometry::FDynamicMesh3& OverlayMesh, const TArray<FSRPlanetSurfaceGridCellId>& CellIds, const FLinearColor& LineColor, float LineThickness, bool bPreferCompactRectangles)>;
	using FAppendInteractionPatch = TFunctionRef<void(UE::Geometry::FDynamicMesh3& OverlayMesh, const FSRPlanetSurfaceGridCellId& CenterCellId, const FLinearColor& BaseLineColor, float LineThickness, TSet<uint64>& DrawnEdges)>;

	bool BuildInteractionOverlayMesh(
		UE::Geometry::FDynamicMesh3& OverlayMesh,
		const FSRInteractionOverlayBuildInput& Input,
		FCellLookup GetCellById,
		FAppendInteractionCell AppendInteractionCell,
		FAppendInteractionRegion AppendInteractionRegion,
		FAppendInteractionPatch AppendInteractionPatch);
}
