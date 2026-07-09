#include "SRPlanetSurfaceGridGeneratedGridFinalize.h"

#include "Components/DynamicMeshComponent.h"
#include "SRPlanetSurfaceGridEmptyMesh.h"
#include "SRPlanetSurfaceGridVisibilityState.h"

namespace SurfaceGridEmptyMesh = StarRovers::SurfaceGridEmptyMesh;
namespace SurfaceGridVisibilityState = StarRovers::SurfaceGridVisibilityState;

void StarRovers::SurfaceGridGeneratedGridFinalize::FinalizeGeneratedGridMesh(
	UDynamicMeshComponent& GridComponent,
	bool& bCellsDirty,
	bool& bGridMeshDirty,
	FDebugTickUpdater UpdateDebugTickState)
{
	SurfaceGridEmptyMesh::ApplyEmptyPrimaryColorMesh(GridComponent);
	SurfaceGridVisibilityState::HidePrimaryGridComponent(GridComponent);
	bCellsDirty = false;
	bGridMeshDirty = false;
	UpdateDebugTickState();
}
