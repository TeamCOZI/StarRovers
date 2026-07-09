#include "Surface/SRPlanetSurfaceGrid.h"

void USRPlanetSurfaceGrid::NotifyInteractionStateChanged()
{
	RequestInteractionHighlightRefresh();
	UpdateDebugTickState();
}
