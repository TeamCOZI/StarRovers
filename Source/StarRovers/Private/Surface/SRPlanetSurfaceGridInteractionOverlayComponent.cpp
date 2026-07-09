#include "SRPlanetSurfaceGridInteractionOverlayComponent.h"

#include "Components/DynamicMeshComponent.h"
#include "Materials/MaterialInterface.h"

UDynamicMeshComponent* StarRovers::SurfaceGridInteractionOverlayComponent::EnsureInteractionOverlay(
	UDynamicMeshComponent* ExistingOverlayMesh,
	UDynamicMeshComponent* ParentComponent,
	AActor* OwnerActor,
	UMaterialInterface* PreferredMaterial,
	UMaterialInterface* FallbackMaterial)
{
	if (ExistingOverlayMesh || !ParentComponent || !OwnerActor)
	{
		return ExistingOverlayMesh;
	}

	UDynamicMeshComponent* OverlayMesh = NewObject<UDynamicMeshComponent>(OwnerActor, TEXT("SurfaceGridInteractionOverlay"));
	if (!OverlayMesh)
	{
		return nullptr;
	}

	OverlayMesh->SetupAttachment(ParentComponent);
	OverlayMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OverlayMesh->SetGenerateOverlapEvents(false);
	OverlayMesh->SetCastShadow(false);
	SetInteractionOverlayVisible(OverlayMesh, false);
	OverlayMesh->RegisterComponent();

	UMaterialInterface* GridMaterial = PreferredMaterial ? PreferredMaterial : FallbackMaterial;
	if (GridMaterial)
	{
		OverlayMesh->SetMaterial(0, GridMaterial);
	}

	return OverlayMesh;
}

void StarRovers::SurfaceGridInteractionOverlayComponent::SetInteractionOverlayVisible(
	UDynamicMeshComponent* OverlayMesh,
	bool bNewVisible)
{
	if (!OverlayMesh)
	{
		return;
	}

	OverlayMesh->SetVisibility(bNewVisible);
	OverlayMesh->SetHiddenInGame(!bNewVisible);
}
