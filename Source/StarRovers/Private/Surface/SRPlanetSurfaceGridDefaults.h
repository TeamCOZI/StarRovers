#pragma once

#include "CoreMinimal.h"

struct FSRDynamicMeshGeneration;

namespace StarRovers::SurfaceGridDefaults
{
	void ApplyGridConfigDefaults(
		int32& FaceResolution,
		float& PlanetRadius,
		bool& bRebuildGridOnRegister,
		bool& bGridVisible);

	void ApplyGridDisplayDefaults(
		FLinearColor& DebugLineColor,
		float& DebugLineOpacity,
		FLinearColor& HoveredCellColor,
		FLinearColor& SelectedCellColor,
		FLinearColor& OccupiedCellColor,
		FLinearColor& AreaSelectionCellColor,
		FLinearColor& InputPortPreviewCellColor,
		FLinearColor& OutputPortPreviewCellColor,
		FLinearColor& DeletionPreviewCellColor,
		FLinearColor& InvalidPreviewCellColor,
		float& DebugLineThickness,
		float& GridSurfaceOffset);

	void ApplyTerrainDefaults(FSRDynamicMeshGeneration& DynamicMeshGeneration);

	void ApplyRuntimeFlagDefaults(
		bool& bUsingGeneratedGridCells,
		bool& bGridMeshDirty,
		bool& bCellsDirty);
}
