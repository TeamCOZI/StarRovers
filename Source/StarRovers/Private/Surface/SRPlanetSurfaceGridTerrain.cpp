#include "Surface/SRPlanetSurfaceGrid.h"

#include "SRPlanetSurfaceGridSurfaceGeometry.h"
#include "SRPlanetSurfaceGridTerrainState.h"

namespace SurfaceGridSurfaceGeometry = StarRovers::SurfaceGridSurfaceGeometry;
namespace SurfaceGridTerrainState = StarRovers::SurfaceGridTerrainState;

float USRPlanetSurfaceGrid::GetSurfaceHeightOffsetAtDirection_Implementation(FVector LocalUnitDirection) const
{
	return ComputeProceduralDynamicMeshHeight(LocalUnitDirection);
}

void USRPlanetSurfaceGrid::ConfigureProceduralTerrain(
	bool bNewDynamicMeshGeneration,
	int32 NewGenerationSeed,
	float NewDynamicMeshHeight,
	float NewDetailFrequency,
	int32 NewNoiseOctaves,
	float NewNoisePersistence)
{
	ConfigureTerrain(SurfaceGridTerrainState::BuildProceduralTerrainConfig(
		DynamicMeshGeneration,
		bNewDynamicMeshGeneration,
		NewGenerationSeed,
		NewDynamicMeshHeight,
		NewDetailFrequency,
		NewNoiseOctaves,
		NewNoisePersistence));
}

void USRPlanetSurfaceGrid::ConfigureTerrain(const FSRDynamicMeshGeneration& NewDynamicMeshGeneration)
{
	SurfaceGridTerrainState::ApplyTerrainConfig(NewDynamicMeshGeneration, DynamicMeshGeneration, bCellsDirty, bGridMeshDirty);
	if (bGridVisible)
	{
		RebuildGrid();
	}
}

FSRPlanetTerrainSample USRPlanetSurfaceGrid::GetTerrainSampleAtDirection(FVector LocalUnitDirection) const
{
	return SurfaceGridTerrainState::SampleTerrainAtDirection(
		LocalUnitDirection,
		DynamicMeshGeneration,
		PlanetRadius,
		FaceResolution);
}

float USRPlanetSurfaceGrid::GetTerrainHeightStep() const
{
	return SurfaceGridTerrainState::CalculateTerrainHeightStep(DynamicMeshGeneration, PlanetRadius, FaceResolution);
}

FVector USRPlanetSurfaceGrid::ResolveLocalSurfacePoint(const FVector& LocalUnitDirection, float HeightOffset) const
{
	return SurfaceGridSurfaceGeometry::ResolveLocalSurfacePoint(
		LocalUnitDirection,
		PlanetRadius,
		DynamicMeshGeneration.bDynamicMeshGeneration,
		[this](const FVector& LocalDirection)
		{
			return GetSurfaceHeightOffsetAtDirection(LocalDirection);
		},
		HeightOffset);
}

FVector USRPlanetSurfaceGrid::ResolveWorldSurfacePoint(const FVector& LocalUnitDirection, float HeightOffset) const
{
	return SurfaceGridSurfaceGeometry::ResolveWorldSurfacePoint(
		LocalUnitDirection,
		GetComponentTransform(),
		PlanetRadius,
		DynamicMeshGeneration.bDynamicMeshGeneration,
		[this](const FVector& LocalDirection)
		{
			return GetSurfaceHeightOffsetAtDirection(LocalDirection);
		},
		HeightOffset);
}

float USRPlanetSurfaceGrid::ComputeProceduralDynamicMeshHeight(FVector LocalUnitDirection) const
{
	return GetTerrainSampleAtDirection(LocalUnitDirection).HeightOffset;
}

FVector USRPlanetSurfaceGrid::ComputeProceduralSurfaceNormal(FVector LocalUnitDirection) const
{
	return SurfaceGridSurfaceGeometry::ComputeProceduralSurfaceNormal(
		LocalUnitDirection,
		PlanetRadius,
		[this](const FVector& LocalDirection)
		{
			return GetSurfaceHeightOffsetAtDirection(LocalDirection);
		});
}
