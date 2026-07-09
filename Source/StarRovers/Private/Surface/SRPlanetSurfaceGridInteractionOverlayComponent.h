#pragma once

#include "CoreMinimal.h"

class AActor;
class UDynamicMeshComponent;
class UMaterialInterface;

namespace StarRovers::SurfaceGridInteractionOverlayComponent
{
	UDynamicMeshComponent* EnsureInteractionOverlay(
		UDynamicMeshComponent* ExistingOverlayMesh,
		UDynamicMeshComponent* ParentComponent,
		AActor* OwnerActor,
		UMaterialInterface* PreferredMaterial,
		UMaterialInterface* FallbackMaterial);

	void SetInteractionOverlayVisible(UDynamicMeshComponent* OverlayMesh, bool bNewVisible);
}
