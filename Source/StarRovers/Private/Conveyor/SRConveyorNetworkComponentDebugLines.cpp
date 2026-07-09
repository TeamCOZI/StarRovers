#include "Conveyor/SRConveyorNetworkComponent.h"

#include "Components/LineBatchComponent.h"
#include "Conveyor/SRConveyorComponentPool.h"
#include "Conveyor/SRConveyorDebugLineRenderer.h"
#include "Conveyor/SRConveyorPlacementValidator.h"
#include "Surface/SRPlanetSurfaceGrid.h"

void USRConveyorNetworkComponent::RefreshPathDebugLines(USRPlanetSurfaceGrid* SurfaceGrid)
{
	if (IsValid(PathDebugLineBatchComponent))
	{
		PathDebugLineBatchComponent->Flush();
	}

	if (!IsValid(SurfaceGrid) || (!bShowPathDebugLine && !bShowConnectionDebugLine))
	{
		return;
	}

	StarRovers::Conveyor::FSRConveyorComponentPool::EnsurePathDebugLineBatchComponent(GetOwner(), this, PathDebugLineBatchComponent);
	if (!IsValid(PathDebugLineBatchComponent))
	{
		return;
	}

	StarRovers::Conveyor::FSRConveyorDebugLineSettings Settings;
	Settings.bShowPathDebugLine = bShowPathDebugLine;
	Settings.bShowConnectionDebugLine = bShowConnectionDebugLine;
	Settings.PathDebugLineColor = PathDebugLineColor;
	Settings.ConnectionDebugLineColor = ConnectionDebugLineColor;
	Settings.BrokenConnectionDebugLineColor = BrokenConnectionDebugLineColor;
	Settings.EndpointDebugLineColor = EndpointDebugLineColor;
	Settings.PathDebugLineThickness = PathDebugLineThickness;
	Settings.ConnectionDebugLineThickness = ConnectionDebugLineThickness;
	Settings.ConnectionDebugLineHeightOffset = ConnectionDebugLineHeightOffset;
	Settings.BeltSurfaceOffset = BeltSurfaceOffset;
	Settings.ConnectionLayerHeight = StarRovers::Conveyor::FSRConveyorPlacementValidator::ResolveLayerHeight(SurfaceGrid, 0.0f, DefaultLayerHeight);
	StarRovers::Conveyor::FSRConveyorDebugLineRenderer::Draw(
		SurfaceGrid,
		PathDebugLineBatchComponent,
		BeltPaths,
		Segments,
		Settings);
}
