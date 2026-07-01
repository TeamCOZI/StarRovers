#include "Surface/SRPlanetSurfaceGrid.h"

#include "Celestial/SRCelestialBody.h"
#include "Materials/MaterialInterface.h"
#include "Surface/SRPlanetSurfaceGridLibrary.h"
#include "UObject/ConstructorHelpers.h"

USRPlanetSurfaceGrid::USRPlanetSurfaceGrid()
{
	PrimaryComponentTick.bCanEverTick = true;

	FaceResolution = 8;
	PlanetRadius = 1000.0f;
	bRebuildGridOnRegister = false;
	bGridVisible = false;
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
	DynamicMeshGeneration = FSRDynamicMeshGeneration();
	DynamicMeshGeneration.bDynamicMeshGeneration = false;
	DynamicMeshGeneration.DynamicMeshHeight = 0.0f;
	bHasHoveredCell = false;
	bHoveredInteractionGridPatchVisible = true;
	bHasSelectedCell = false;
	bUsingGeneratedGridCells = false;
	bGridMeshDirty = true;
	bCellsDirty = true;

	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCastShadow(false);
	SetVisibility(false);
	SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> VertexColorMaterialFinder(
		TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	if (VertexColorMaterialFinder.Succeeded())
	{
		GridOverlayMaterial = VertexColorMaterialFinder.Object;
		SetMaterial(0, GridOverlayMaterial);
	}
}

void USRPlanetSurfaceGrid::OnRegister()
{
	Super::OnRegister();
	if (GridOverlayMaterial)
	{
		SetMaterial(0, GridOverlayMaterial);
	}
	UpdateDebugTickState();

	if (bRebuildGridOnRegister && !IsTemplate())
	{
		RebuildGrid();
	}
}

void USRPlanetSurfaceGrid::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void USRPlanetSurfaceGrid::RebuildGrid()
{
	if (!RebuildCellsFromOwnerGeneratedGrid())
	{
		bUsingGeneratedGridCells = false;
		Cells = USRPlanetSurfaceGridLibrary::GenerateCubeSphereCells(FMath::Max(1, FaceResolution), FMath::Max(1.0f, PlanetRadius));
		for (FSRPlanetSurfaceGridCell& Cell : Cells)
		{
			const FSRPlanetTerrainSample TerrainSample = GetTerrainSampleAtDirection(Cell.LocalCenter.GetSafeNormal());
			Cell.Biome = TerrainSample.Biome;
			Cell.BiomeId = TerrainSample.BiomeId;
			Cell.WaterRole = TerrainSample.WaterRole;
			Cell.SurfaceTemperature = TerrainSample.Temperature;
			Cell.TemperatureState = ResolveTemperatureStateFromSurfaceTemperature(TerrainSample.Temperature);
		}
	}
	if (ASRCelestialBody* OwnerBody = Cast<ASRCelestialBody>(GetOwner()))
	{
		OwnerBody->ClearSurfaceCellHighlights();
	}
	bHasHoveredCell = false;
	HoveredCellId = FSRPlanetSurfaceGridCellId();
	bHoveredInteractionGridPatchVisible = true;
	bHasSelectedCell = false;
	SelectedCellId = FSRPlanetSurfaceGridCellId();
	AreaSelectionCellIds.Reset();
	OccupiedPreviewCellIds.Reset();
	InputPortPreviewCellIds.Reset();
	OutputPortPreviewCellIds.Reset();
	DeletionPreviewCellIds.Reset();
	InvalidPreviewCellIds.Reset();
	SetInteractionOverlayVisible(false);
	RebuildCellIndex();
	RebuildCellInfoIndex();
	RebuildRaycastIndex();
	bCellsDirty = false;
	bGridMeshDirty = true;
	if (bGridVisible)
	{
		RequestInteractionHighlightRefresh();
	}
}

void USRPlanetSurfaceGrid::SetPlanetRadius(float NewPlanetRadius)
{
	PlanetRadius = FMath::Max(1.0f, NewPlanetRadius);
	bCellsDirty = true;
	bGridMeshDirty = true;
	if (bGridVisible)
	{
		RebuildGrid();
	}
}

void USRPlanetSurfaceGrid::SetFaceResolution(int32 NewFaceResolution)
{
	FaceResolution = FMath::Max(1, NewFaceResolution);
	bCellsDirty = true;
	bGridMeshDirty = true;
	if (bGridVisible)
	{
		RebuildGrid();
	}
}

int32 USRPlanetSurfaceGrid::GetFaceResolution() const
{
	return FaceResolution;
}

float USRPlanetSurfaceGrid::GetPlanetRadius() const
{
	return PlanetRadius;
}
