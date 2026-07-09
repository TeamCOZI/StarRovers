#include "SRPlanetSurfaceGridDefaults.h"

#include "Surface/SRPlanetTerrainTypes.h"

namespace StarRovers::SurfaceGridDefaults
{
	void ApplyGridConfigDefaults(
		int32& FaceResolution,
		float& PlanetRadius,
		bool& bRebuildGridOnRegister,
		bool& bGridVisible)
	{
		FaceResolution = 8;
		PlanetRadius = 1000.0f;
		bRebuildGridOnRegister = false;
		bGridVisible = false;
	}

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
		float& GridSurfaceOffset)
	{
		DebugLineColor = FLinearColor(0.15f, 0.85f, 1.0f, 1.0f);
		DebugLineOpacity = 1.0f;
		HoveredCellColor = FLinearColor(1.0f, 0.85f, 0.2f, 1.0f);
		SelectedCellColor = FLinearColor(0.25f, 1.0f, 0.35f, 1.0f);
		OccupiedCellColor = FLinearColor(1.0f, 0.35f, 0.35f, 1.0f);
		AreaSelectionCellColor = FLinearColor(0.1f, 0.55f, 1.0f, 1.0f);
		InputPortPreviewCellColor = FLinearColor(0.15f, 0.55f, 1.0f, 1.0f);
		OutputPortPreviewCellColor = FLinearColor(1.0f, 0.55f, 0.05f, 1.0f);
		DeletionPreviewCellColor = FLinearColor(1.0f, 0.05f, 0.02f, 1.0f);
		InvalidPreviewCellColor = FLinearColor(1.0f, 0.02f, 0.02f, 1.0f);
		DebugLineThickness = 1.0f;
		GridSurfaceOffset = 0.0f;
	}

	void ApplyTerrainDefaults(FSRDynamicMeshGeneration& DynamicMeshGeneration)
	{
		DynamicMeshGeneration = FSRDynamicMeshGeneration();
		DynamicMeshGeneration.bDynamicMeshGeneration = false;
		DynamicMeshGeneration.DynamicMeshHeight = 0.0f;
	}

	void ApplyRuntimeFlagDefaults(
		bool& bUsingGeneratedGridCells,
		bool& bGridMeshDirty,
		bool& bCellsDirty)
	{
		bUsingGeneratedGridCells = false;
		bGridMeshDirty = true;
		bCellsDirty = true;
	}
}
