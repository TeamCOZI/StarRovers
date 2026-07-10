#include "Surface/SRPlanetSurfaceGrid.h"

#include "Materials/MaterialInterface.h"
#include "SRPlanetSurfaceGridDefaults.h"
#include "SRPlanetSurfaceGridMaterialState.h"
#include "SRPlanetSurfaceGridVisibilityState.h"

namespace SurfaceGridDefaults = StarRovers::SurfaceGridDefaults;
namespace SurfaceGridMaterialState = StarRovers::SurfaceGridMaterialState;
namespace SurfaceGridVisibilityState = StarRovers::SurfaceGridVisibilityState;

void USRPlanetSurfaceGrid::InitializeSurfaceGridDefaults()
{
	SurfaceGridDefaults::ApplyGridConfigDefaults(FaceResolution, PlanetRadius, bRebuildGridOnRegister, bGridVisible);
	SurfaceGridDefaults::ApplyGridDisplayDefaults(
		DebugLineColor,
		DebugLineOpacity,
		HoveredCellColor,
		SelectedCellColor,
		OccupiedCellColor,
		AreaSelectionCellColor,
		InputPortPreviewCellColor,
		OutputPortPreviewCellColor,
		DeletionPreviewCellColor,
		InvalidPreviewCellColor,
		DebugLineThickness,
		GridSurfaceOffset);
	SurfaceGridDefaults::ApplyTerrainDefaults(DynamicMeshGeneration);
	ResetSurfaceInteractionState();
	SurfaceGridDefaults::ApplyRuntimeFlagDefaults(bUsingGeneratedGridCells, bGridMeshDirty, bCellsDirty);
}

void USRPlanetSurfaceGrid::ConfigureSurfaceGridComponentDefaults()
{
	PrimaryComponentTick.bCanEverTick = true;
	SurfaceGridVisibilityState::ConfigurePrimaryGridComponent(*this);
}

void USRPlanetSurfaceGrid::ApplyDefaultGridOverlayMaterial()
{
}

void USRPlanetSurfaceGrid::ApplyRegisteredGridOverlayMaterial()
{
	if (GridOverlayMaterial)
	{
		SurfaceGridMaterialState::ApplyGridOverlayMaterial(*this, nullptr, GridOverlayMaterial.Get());
	}
}

void USRPlanetSurfaceGrid::RebuildGridOnRegisterIfNeeded()
{
	if (bRebuildGridOnRegister && !IsTemplate())
	{
		RebuildGrid();
	}
}

void USRPlanetSurfaceGrid::RebuildGridIfVisible()
{
	if (bGridVisible)
	{
		RebuildGrid();
	}
}
