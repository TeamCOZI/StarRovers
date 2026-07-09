#include "SRPlanetSurfaceGridMaterialState.h"

#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace StarRovers::SurfaceGridMaterialState
{
	UMaterialInterface* LoadDefaultGridOverlayMaterial()
	{
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> VertexColorMaterialFinder(
			TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
		return VertexColorMaterialFinder.Succeeded() ? VertexColorMaterialFinder.Object : nullptr;
	}

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
