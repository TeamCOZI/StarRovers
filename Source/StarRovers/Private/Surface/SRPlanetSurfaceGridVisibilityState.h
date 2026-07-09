#pragma once

#include "CoreMinimal.h"

class UPrimitiveComponent;

namespace StarRovers::SurfaceGridVisibilityState
{
	void ConfigurePrimaryGridComponent(UPrimitiveComponent& GridComponent);
	void HidePrimaryGridComponent(UPrimitiveComponent& GridComponent);
	void ApplyPrimaryGridVisibility(UPrimitiveComponent& GridComponent, bool bGridVisible);
}
