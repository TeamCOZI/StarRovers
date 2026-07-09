#include "Celestial/SRCelestialBody.h"

#include "Components/DynamicMeshComponent.h"
#include "Components/StaticMeshComponent.h"

void ASRCelestialBody::SetCelestialBodyMesh(bool bUseDynamicMesh)
{
	if (bUseDynamicMesh && !HasCelestialBodyDynamicMeshBuild())
	{
		bUseDynamicMesh = false;
	}

	bool bDynamicMeshAlreadyVisible = true;
	for (UDynamicMeshComponent* DynamicMeshComponent : CelestialBodyDynamicMeshFaces)
	{
		if (IsValid(DynamicMeshComponent) && DynamicMeshComponent->IsVisible() != bUseDynamicMesh)
		{
			bDynamicMeshAlreadyVisible = false;
			break;
		}
	}

	const bool bStaticMeshAlreadyVisible = IsValid(CelestialBodyStaticMesh.Get())
		&& CelestialBodyStaticMesh->IsVisible() != bUseDynamicMesh;
	if (bDynamicMeshAlreadyVisible && bStaticMeshAlreadyVisible)
	{
		return;
	}

	for (UDynamicMeshComponent* DynamicMeshComponent : CelestialBodyDynamicMeshFaces)
	{
		if (!IsValid(DynamicMeshComponent))
		{
			continue;
		}

		if (DynamicMeshComponent->IsVisible() != bUseDynamicMesh)
		{
			DynamicMeshComponent->SetVisibility(bUseDynamicMesh);
		}
		DynamicMeshComponent->SetHiddenInGame(!bUseDynamicMesh);
	}

	if (IsValid(CelestialBodyStaticMesh.Get()))
	{
		if (CelestialBodyStaticMesh->IsVisible() == bUseDynamicMesh)
		{
			CelestialBodyStaticMesh->SetVisibility(!bUseDynamicMesh);
		}
		CelestialBodyStaticMesh->SetHiddenInGame(bUseDynamicMesh);
	}
}
