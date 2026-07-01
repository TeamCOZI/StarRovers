#include "Celestial/SRCelestialBodyDataTypes.h"

FSRCelestialBodyData::FSRCelestialBodyData()
{
	VariableName = FText::FromString(TEXT("Primary Star"));
	BodyCategory = ESRCelestialBodyCategory::Star;
	OrbitPeriod = 0.0f;
	DynamicMeshGeneration = FSRDynamicMeshGeneration();
	DynamicMeshGeneration.bDynamicMeshGeneration = false;
	DynamicMeshGeneration.DynamicMeshHeight = 0.0f;
	bHasOcean = false;
	OceanMaterial = nullptr;
	OceanScaleMultiplier = 1.0f;
	bHasAtmosphere = false;
	AtmosphereMaterial = nullptr;
	AtmosphereScaleMultiplier = 1.0f;
}
