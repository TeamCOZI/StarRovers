#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UMeshComponent;

namespace StarRovers::SurfaceGridMaterialState
{
	UMaterialInterface* LoadDefaultGridOverlayMaterial();

	void ApplyGridOverlayMaterial(
		UMeshComponent& GridComponent,
		UMeshComponent* InteractionOverlayMesh,
		UMaterialInterface* GridOverlayMaterial);
}
