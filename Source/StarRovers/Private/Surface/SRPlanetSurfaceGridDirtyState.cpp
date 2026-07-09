#include "Surface/SRPlanetSurfaceGrid.h"

void USRPlanetSurfaceGrid::MarkGridMeshDirtyAndRefreshIfVisible()
{
	bGridMeshDirty = true;
	RefreshInteractionIfGridVisible();
}

void USRPlanetSurfaceGrid::RefreshInteractionIfGridVisible()
{
	if (bGridVisible)
	{
		RequestInteractionHighlightRefresh();
	}
}
