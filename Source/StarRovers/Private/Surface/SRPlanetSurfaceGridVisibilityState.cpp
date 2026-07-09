#include "SRPlanetSurfaceGridVisibilityState.h"

#include "Components/PrimitiveComponent.h"

namespace StarRovers::SurfaceGridVisibilityState
{
	void ConfigurePrimaryGridComponent(UPrimitiveComponent& GridComponent)
	{
		GridComponent.SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GridComponent.SetGenerateOverlapEvents(false);
		GridComponent.SetCastShadow(false);
		HidePrimaryGridComponent(GridComponent);
	}

	void HidePrimaryGridComponent(UPrimitiveComponent& GridComponent)
	{
		GridComponent.SetVisibility(false);
		GridComponent.SetHiddenInGame(true);
	}

	void ApplyPrimaryGridVisibility(UPrimitiveComponent& GridComponent, bool bGridVisible)
	{
		GridComponent.SetVisibility(bGridVisible);
		GridComponent.SetHiddenInGame(!bGridVisible);
	}
}
