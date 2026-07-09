#include "Surface/SRPlanetSurfaceGrid.h"

#include "Materials/MaterialInterface.h"
#include "SRPlanetSurfaceGridDebugConfig.h"
#include "SRPlanetSurfaceGridMaterialState.h"
#include "SRPlanetSurfaceGridOwnerBody.h"

namespace SurfaceGridDebugConfig = StarRovers::SurfaceGridDebugConfig;
namespace SurfaceGridMaterialState = StarRovers::SurfaceGridMaterialState;
namespace SurfaceGridOwnerBody = StarRovers::SurfaceGridOwnerBody;

void USRPlanetSurfaceGrid::SetGridVisible(bool bNewGridVisible)
{
	bGridVisible = bNewGridVisible;
	ApplyGridVisibilityState();
	UpdateDebugTickState();
}

bool USRPlanetSurfaceGrid::IsGridVisible() const
{
	return bGridVisible;
}

void USRPlanetSurfaceGrid::PrepareGridForAssembly()
{
	SurfaceGridOwnerBody::PrepareDynamicMesh(GetOwner());
	EnsureAssemblyGridCellsReady();
	ApplyEmptyPrimaryGridMeshIfNeeded();
}

void USRPlanetSurfaceGrid::ConfigureDebugGrid(
	FLinearColor NewGridLineColor,
	float NewGridLineOpacity,
	float NewLineThickness,
	FLinearColor NewHoveredCellColor,
	FLinearColor NewSelectedCellColor,
	FLinearColor NewOccupiedCellColor,
	float NewSurfaceOffset)
{
	SurfaceGridDebugConfig::ApplyDebugGridConfig(
		NewGridLineColor,
		NewGridLineOpacity,
		NewLineThickness,
		NewHoveredCellColor,
		NewSelectedCellColor,
		NewOccupiedCellColor,
		NewSurfaceOffset,
		DebugLineColor,
		DebugLineOpacity,
		DebugLineThickness,
		HoveredCellColor,
		SelectedCellColor,
		OccupiedCellColor,
		GridSurfaceOffset,
		bGridMeshDirty);
	RefreshInteractionIfGridVisible();
}

void USRPlanetSurfaceGrid::SetGridOverlayMaterial(UMaterialInterface* NewGridOverlayMaterial)
{
	GridOverlayMaterial = NewGridOverlayMaterial;
	SurfaceGridMaterialState::ApplyGridOverlayMaterial(*this, InteractionOverlayMesh, GridOverlayMaterial.Get());
}

void USRPlanetSurfaceGrid::UpdateDebugTickState()
{
	SetComponentTickEnabled(false);
}
