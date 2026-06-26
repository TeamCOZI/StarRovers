#include "Surface/SRPlanetSurfaceGrid.h"

#include "Celestial/SRCelestialBody.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Materials/MaterialInterface.h"

void USRPlanetSurfaceGrid::SetGridVisible(bool bNewGridVisible)
{
	bGridVisible = bNewGridVisible;
	SetVisibility(false);
	SetHiddenInGame(true);
	SetInteractionOverlayVisible(bGridVisible && (bHasHoveredCell || bHasSelectedCell || !AreaSelectionCellIds.IsEmpty() || !OccupiedPreviewCellIds.IsEmpty() || !InputPortPreviewCellIds.IsEmpty() || !OutputPortPreviewCellIds.IsEmpty() || !DeletionPreviewCellIds.IsEmpty() || !InvalidPreviewCellIds.IsEmpty()));
	if (!bGridVisible)
	{
		ClearHoveredCell();
		ClearSelectedCell();
		ClearAreaSelectionCells();
		ClearOccupiedPreviewCells();
		ClearFacilityPortPreviewCells();
		ClearDeletionPreviewCells();
		ClearInvalidPreviewCells();
	}
	else
	{
		if (ASRCelestialBody* OwnerBody = Cast<ASRCelestialBody>(GetOwner()))
		{
			OwnerBody->PrepareCelestialBodyDynamicMesh();
		}

		if (bCellsDirty)
		{
			RebuildGrid();
		}

		RequestInteractionHighlightRefresh();
	}
	UpdateDebugTickState();
}

bool USRPlanetSurfaceGrid::IsGridVisible() const
{
	return bGridVisible;
}

void USRPlanetSurfaceGrid::PrepareGridForAssembly()
{
	if (ASRCelestialBody* OwnerBody = Cast<ASRCelestialBody>(GetOwner()))
	{
		OwnerBody->PrepareCelestialBodyDynamicMesh();
	}

	if (Cells.IsEmpty() || bCellsDirty)
	{
		RebuildGrid();
	}

	if (!bCellsDirty && bGridMeshDirty)
	{
		UE::Geometry::FDynamicMesh3 EmptyGridMesh;
		EmptyGridMesh.EnableAttributes();
		EmptyGridMesh.Attributes()->EnablePrimaryColors();
		SetMesh(MoveTemp(EmptyGridMesh));
		bGridMeshDirty = false;
	}
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
	DebugLineColor = NewGridLineColor;
	DebugLineOpacity = FMath::Clamp(NewGridLineOpacity, 0.0f, 1.0f);
	DebugLineThickness = FMath::Clamp(NewLineThickness, 0.0f, 2.0f);
	HoveredCellColor = NewHoveredCellColor;
	SelectedCellColor = NewSelectedCellColor;
	OccupiedCellColor = NewOccupiedCellColor;
	GridSurfaceOffset = FMath::Clamp(NewSurfaceOffset, 0.0f, 1.0f);
	bGridMeshDirty = true;
	if (bGridVisible)
	{
		RequestInteractionHighlightRefresh();
	}
}

void USRPlanetSurfaceGrid::SetGridOverlayMaterial(UMaterialInterface* NewGridOverlayMaterial)
{
	GridOverlayMaterial = NewGridOverlayMaterial;
	UMaterialInterface* EffectiveGridMaterial = GridOverlayMaterial ? GridOverlayMaterial.Get() : GetMaterial(0);
	if (EffectiveGridMaterial)
	{
		SetMaterial(0, EffectiveGridMaterial);
		if (InteractionOverlayMesh)
		{
			InteractionOverlayMesh->SetMaterial(0, EffectiveGridMaterial);
		}
	}
}

void USRPlanetSurfaceGrid::UpdateDebugTickState()
{
	SetComponentTickEnabled(false);
}
