#pragma once

#include "CoreMinimal.h"

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
		bool& bGridMeshDirty);
}
