#include "SRPlanetSurfaceGridMaterialState.h"

#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"

namespace StarRovers::SurfaceGridMaterialState
{
	void ApplyGridOverlayMaterial(
		UMeshComponent& GridComponent,
		UMeshComponent* InteractionOverlayMesh,
		UMaterialInterface* GridOverlayMaterial)
	{
		UMaterialInterface* EffectiveGridMaterial = GridOverlayMaterial ? GridOverlayMaterial : GridComponent.GetMaterial(0);
		if (!EffectiveGridMaterial)
		{
			return;
		}

		GridComponent.SetMaterial(0, EffectiveGridMaterial);
		if (InteractionOverlayMesh)
		{
			InteractionOverlayMesh->SetMaterial(0, EffectiveGridMaterial);
		}
	}
}
