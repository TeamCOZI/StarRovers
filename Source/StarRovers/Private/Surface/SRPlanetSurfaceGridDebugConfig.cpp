#include "SRPlanetSurfaceGridDebugConfig.h"

namespace StarRovers::SurfaceGridDebugConfig
{
	void ApplyDebugGridConfig(
		FLinearColor NewGridLineColor,
		float NewGridLineOpacity,
		float NewLineThickness,
		FLinearColor NewHoveredCellColor,
		FLinearColor NewSelectedCellColor,
		FLinearColor NewOccupiedCellColor,
		float NewSurfaceOffset,
		FLinearColor& DebugLineColor,
		float& DebugLineOpacity,
		float& DebugLineThickness,
		FLinearColor& HoveredCellColor,
		FLinearColor& SelectedCellColor,
		FLinearColor& OccupiedCellColor,
		float& GridSurfaceOffset,
		bool& bGridMeshDirty)
	{
		DebugLineColor = NewGridLineColor;
		DebugLineOpacity = FMath::Clamp(NewGridLineOpacity, 0.0f, 1.0f);
		DebugLineThickness = FMath::Clamp(NewLineThickness, 0.0f, 2.0f);
		HoveredCellColor = NewHoveredCellColor;
		SelectedCellColor = NewSelectedCellColor;
		OccupiedCellColor = NewOccupiedCellColor;
		GridSurfaceOffset = FMath::Clamp(NewSurfaceOffset, 0.0f, 1.0f);
		bGridMeshDirty = true;
	}
}
