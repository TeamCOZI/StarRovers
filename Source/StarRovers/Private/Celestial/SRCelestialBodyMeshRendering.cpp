#include "Celestial/SRCelestialBody.h"

#include "SRCelestialBodyLog.h"
#include "Celestial/SRDynamicMeshBaseDataAsset.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

bool ASRCelestialBody::PrepareCelestialBodyDynamicMesh()
{
	if (HasCelestialBodyDynamicMeshBuild())
	{
		return true;
	}

	EnsureCelestialBodyMeshRendering(true);
	return HasCelestialBodyDynamicMeshBuild();
}

bool ASRCelestialBody::HasCelestialBodyDynamicMeshBuild() const
{
	return DynamicMeshState.HasBuild();
}

void ASRCelestialBody::EnsureCelestialBodyMeshRendering(bool bBuildDynamicMesh)
{
	if (!IsValid(CelestialBodyDynamicMesh.Get()))
	{
		return;
	}

	UStaticMesh* DesiredMesh = nullptr;
	if (IsValid(StaticMesh))
	{
		DesiredMesh = StaticMesh.Get();
	}
	if (!IsValid(DesiredMesh) && !IsValid(DynamicMeshBaseDataAsset.Get()))
	{
		UE_LOG(LogStarRoversCelestial, Error, TEXT("Celestial body '%s' requires StaticMesh or DynamicMeshBaseDataAsset."), *GetName());
		return;
	}

	if (IsValid(CelestialBodyStaticMesh.Get()) && CelestialBodyStaticMesh->GetStaticMesh() != DesiredMesh)
	{
		CelestialBodyStaticMesh->SetStaticMesh(DesiredMesh);
	}

	if (bBuildDynamicMesh)
	{
		BuildCelestialBodyDynamicMesh();
	}

	SyncDynamicMeshFaceComponentSettings();
	ApplyCelestialBodyMeshMaterials();
}
