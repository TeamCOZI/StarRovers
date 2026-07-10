#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UMeshComponent;

namespace StarRovers::SurfaceGridMaterialState
{
	void ApplyGridOverlayMaterial(
		UMeshComponent& GridComponent,
		UMeshComponent* InteractionOverlayMesh,
		UMaterialInterface* GridOverlayMaterial);
}
