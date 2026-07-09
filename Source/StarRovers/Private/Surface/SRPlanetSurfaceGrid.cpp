#include "Surface/SRPlanetSurfaceGrid.h"

#include "SRPlanetSurfaceGridConfigState.h"

namespace SurfaceGridConfigState = StarRovers::SurfaceGridConfigState;

USRPlanetSurfaceGrid::USRPlanetSurfaceGrid()
{
	ConfigureSurfaceGridComponentDefaults();
	InitializeSurfaceGridDefaults();
	ApplyDefaultGridOverlayMaterial();
}

void USRPlanetSurfaceGrid::OnRegister()
{
	Super::OnRegister();
	ApplyRegisteredGridOverlayMaterial();
	UpdateDebugTickState();
	RebuildGridOnRegisterIfNeeded();
}

void USRPlanetSurfaceGrid::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void USRPlanetSurfaceGrid::RebuildGrid()
{
	if (!RebuildCellsFromOwnerGeneratedGrid())
	{
		RebuildDefaultSurfaceCells();
	}
	FinalizeGridRebuild();
}

void USRPlanetSurfaceGrid::SetPlanetRadius(float NewPlanetRadius)
{
	SurfaceGridConfigState::ApplyPlanetRadius(NewPlanetRadius, PlanetRadius, bCellsDirty, bGridMeshDirty);
	RebuildGridIfVisible();
}

void USRPlanetSurfaceGrid::SetFaceResolution(int32 NewFaceResolution)
{
	SurfaceGridConfigState::ApplyFaceResolution(NewFaceResolution, FaceResolution, bCellsDirty, bGridMeshDirty);
	RebuildGridIfVisible();
}

int32 USRPlanetSurfaceGrid::GetFaceResolution() const
{
	return FaceResolution;
}

float USRPlanetSurfaceGrid::GetPlanetRadius() const
{
	return PlanetRadius;
}
