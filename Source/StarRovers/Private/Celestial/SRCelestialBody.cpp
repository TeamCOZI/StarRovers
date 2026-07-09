#include "Celestial/SRCelestialBody.h"

#include "Engine/World.h"

ASRCelestialBody::ASRCelestialBody()
{
	InitializeCelestialBodyComponents();
	InitializeCelestialBodyDefaults();
}

void ASRCelestialBody::SetData(const FSRCelestialBodyData& NewData)
{
	bHasAppliedData = true;
	bHasLoggedMissingDataError = false;

	CopyBodyDataFields(NewData);
	ApplyTerrainProfileData();

	if (ShouldAutoApplyDataAfterSet())
	{
		ApplyData();
	}
}

void ASRCelestialBody::ApplyData()
{
	if (!bHasAppliedData && GetWorld() && GetWorld()->IsGameWorld())
	{
		LogMissingDataErrorOnce(TEXT("ApplyData"));
		return;
	}

	SanitizeBodyRuntimeValues();
	SetActorScale3D(FVector::OneVector);

	ApplyBodyMeshTransforms();
	UpdateDynamicMeshBuildStateForCurrentData();
	EnsureCelestialBodyMeshRendering(ShouldBuildDynamicMeshForCurrentWorld());
	ApplyClickCollisionForCurrentBody();
	ApplyGravityLineSettings();
	ApplyToonOutlineSettings();
}

FSRCelestialBodyData ASRCelestialBody::GetData() const
{
	return BuildBodyDataSnapshot();
}

UDynamicMeshComponent* ASRCelestialBody::GetCelestialBodyDynamicMesh() const
{
	return CelestialBodyDynamicMesh;
}

ESRCelestialBodyCategory ASRCelestialBody::GetBodyCategory() const
{
	return BodyCategory;
}

USRGravityParent* ASRCelestialBody::GetGravityParent() const
{
	return GravityParent;
}

USROrbit* ASRCelestialBody::GetOrbit() const
{
	return nullptr;
}

USRPlanetSurfaceGrid* ASRCelestialBody::GetSurfaceGrid() const
{
	return nullptr;
}
