#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

class UDynamicMeshComponent;

namespace StarRovers::SurfaceGridGeneratedGridFinalize
{
	using FDebugTickUpdater = TFunctionRef<void()>;

	void FinalizeGeneratedGridMesh(
		UDynamicMeshComponent& GridComponent,
		bool& bCellsDirty,
		bool& bGridMeshDirty,
		FDebugTickUpdater UpdateDebugTickState);
}
