#include "Celestial/SRCelestialBodyDynamicMeshInternal.h"

#include "Surface/SRPlanetTerrainGenerator.h"

namespace StarRovers::Celestial::DynamicMesh
{
int32 GetCubeSphereFaceComponentIndex(ESRCubeSphereFace Face)
{
	const int32 FaceIndex = static_cast<int32>(Face);
	return FaceIndex >= 0 && FaceIndex < CubeSphereFaceComponentCount ? FaceIndex : 0;
}

uint64 BuildSourcePositionEdgeKey(uint32 EndpointA, uint32 EndpointB)
{
	const uint32 MinEndpoint = FMath::Min(EndpointA, EndpointB);
	const uint32 MaxEndpoint = FMath::Max(EndpointA, EndpointB);
	return (static_cast<uint64>(MinEndpoint) << 32) | static_cast<uint64>(MaxEndpoint);
}

uint32 GetTypeHash(const FSRTerrainVertexKey& Key)
{
	uint32 Hash = ::GetTypeHash(Key.A);
	Hash = HashCombine(Hash, ::GetTypeHash(Key.B));
	Hash = HashCombine(Hash, ::GetTypeHash(Key.C));
	Hash = HashCombine(Hash, ::GetTypeHash(Key.D));
	return Hash;
}

FSRTerrainVertexKey MakeTerrainVertexKey(const FVector& Position)
{
	constexpr double PositionQuantizationScale = 100000.0;
	FSRTerrainVertexKey Key;
	Key.A = FMath::RoundToInt(Position.X * PositionQuantizationScale);
	Key.B = FMath::RoundToInt(Position.Y * PositionQuantizationScale);
	Key.C = FMath::RoundToInt(Position.Z * PositionQuantizationScale);
	Key.D = 0;
	return Key;
}

FSRTerrainVertexKey MakeTerrainVertexKey(uint32 SourcePositionHash, float HeightOffset)
{
	constexpr float HeightQuantizationScale = 10000.0f;
	FSRTerrainVertexKey Key;
	Key.A = static_cast<int32>(SourcePositionHash);
	Key.B = FMath::RoundToInt(HeightOffset * HeightQuantizationScale);
	Key.C = 0;
	Key.D = 1;
	return Key;
}

int32 CountDynamicMeshBoundaryEdges(const UE::Geometry::FDynamicMesh3& Mesh)
{
	int32 BoundaryEdgeCount = 0;
	for (const int32 EdgeId : Mesh.EdgeIndicesItr())
	{
		if (Mesh.IsBoundaryEdge(EdgeId))
		{
			++BoundaryEdgeCount;
		}
	}
	return BoundaryEdgeCount;
}

float ComputeRegularCubeFaceCellEdgeLength(float SourceRadius, int32 FaceResolution)
{
	return (2.0f * FMath::Max(1.0f, SourceRadius)) / static_cast<float>(FMath::Max(1, FaceResolution));
}

FSRPlanetTerrainSample SampleTerrainForDynamicMesh(
	const FSRBiomeSampleContext& Context,
	const FSRDynamicMeshGenerationSnapshot& DynamicMeshGeneration,
	float HeightStep)
{
	FSRPlanetTerrainSample Sample = FSRPlanetTerrainGenerator::SampleTerrain(Context, DynamicMeshGeneration);
	const float SafeDynamicMeshHeight = FMath::Max(0.0f, DynamicMeshGeneration.DynamicMeshHeight);
	if (!DynamicMeshGeneration.bMinecraft
		|| !DynamicMeshGeneration.bDynamicMeshGeneration
		|| SafeDynamicMeshHeight <= KINDA_SMALL_NUMBER
		|| HeightStep <= KINDA_SMALL_NUMBER)
	{
		return Sample;
	}

	Sample.HeightOffset = FMath::RoundToFloat(Sample.HeightOffset / HeightStep) * HeightStep;
	return Sample;
}
}
