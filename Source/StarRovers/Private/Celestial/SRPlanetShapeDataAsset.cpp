#include "Celestial/SRPlanetShapeDataAsset.h"

USRDynamicMeshBaseDataAsset* USRPlanetShapeDataAsset::GetDynamicMeshBaseDataAsset() const
{
	return DynamicMeshBaseDataAsset.Get();
}

USRDynamicMeshBaseDataAsset* USRPlanetShapeDataAsset::GetOceanDynamicMeshBaseDataAsset() const
{
	return IsValid(OceanDynamicMeshBaseDataAsset.Get())
		? OceanDynamicMeshBaseDataAsset.Get()
		: DynamicMeshBaseDataAsset.Get();
}

USRDynamicMeshBaseDataAsset* USRPlanetShapeDataAsset::GetAtmosphereDynamicMeshBaseDataAsset() const
{
	return IsValid(AtmosphereDynamicMeshBaseDataAsset.Get())
		? AtmosphereDynamicMeshBaseDataAsset.Get()
		: DynamicMeshBaseDataAsset.Get();
}

bool USRPlanetShapeDataAsset::IsDynamicMeshBaseShapeCompatible() const
{
	const USRDynamicMeshBaseDataAsset* TerrainBase = DynamicMeshBaseDataAsset.Get();
	const USRDynamicMeshBaseDataAsset* OceanBase = OceanDynamicMeshBaseDataAsset.Get();
	const USRDynamicMeshBaseDataAsset* AtmosphereBase = AtmosphereDynamicMeshBaseDataAsset.Get();
	return (!IsValid(TerrainBase) || TerrainBase->BaseShape == Shape)
		&& (!IsValid(OceanBase) || OceanBase->BaseShape == Shape)
		&& (!IsValid(AtmosphereBase) || AtmosphereBase->BaseShape == Shape);
}

#if WITH_EDITOR
void USRPlanetShapeDataAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!IsDynamicMeshBaseShapeCompatible())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("PlanetShape '%s' has DynamicMeshBaseDataAsset '%s' with a different base shape."),
			*GetName(),
			*GetNameSafe(DynamicMeshBaseDataAsset.Get()));
	}
}
#endif
