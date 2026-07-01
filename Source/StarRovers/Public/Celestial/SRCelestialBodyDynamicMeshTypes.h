#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"

struct FSRCelestialBodyDynamicMeshColorElement
{
	int32 MeshComponentIndex = INDEX_NONE;
	int32 ElementId = INDEX_NONE;
	FLinearColor BaseColor = FLinearColor::White;
};

struct FSRCelestialBodyDynamicMeshQuadFeatureMaskRef
{
	int32 MeshComponentIndex = INDEX_NONE;
	int32 FeatureMaskUVElementIds[4] = { INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE };
	int32 InputEdgeToFeatureMaskEdgeIndex[4] = { 0, 1, 2, 3 };
};

struct FSRCelestialBodyDynamicMeshQuadRenderData
{
	TArray<FSRCelestialBodyDynamicMeshColorElement, TInlineAllocator<8>> ColorElements;
	FSRCelestialBodyDynamicMeshQuadFeatureMaskRef FeatureMaskRef;
	int32 FailedTriangleCount = 0;
	int32 FallbackTriangleCount = 0;
};

struct FSRCelestialBodyDynamicMeshCellColorData
{
	TArray<FSRCelestialBodyDynamicMeshColorElement, TInlineAllocator<4>> SurfaceColorElements;
	TArray<FSRCelestialBodyDynamicMeshColorElement, TInlineAllocator<8>> SideColorElements;
};

struct FSRCelestialBodyPreparedDynamicMesh
{
	bool bValid = false;
	uint32 BuildHash = 0;
	TArray<UE::Geometry::FDynamicMesh3> FaceDynamicMeshes;
	int32 FeatureEdgeMaskCount = 0;
	TArray<FSRPlanetSurfaceGridCell> SurfaceGridCells;
	TMap<FSRPlanetSurfaceGridCellId, int32> CellIndexById;
	TArray<int32> CellIndexByFlatId;
	TArray<FSRCelestialBodyDynamicMeshCellColorData> ColorDataByFlatId;
	TArray<FString> DetailLines;
	double BuildMilliseconds = 0.0;
};

class STARROVERS_API FSRCelestialBodyDynamicMeshRuntimeState
{
public:
	void Reset();
	bool HasBuild() const;
	bool HasBuildHash(uint32 InBuildHash) const;
	void MarkBuilt(uint32 InBuildHash);
	bool GetSurfaceGridCells(TArray<FSRPlanetSurfaceGridCell>& OutCells) const;
	const FSRCelestialBodyDynamicMeshCellColorData* FindCellColorData(
		const FSRPlanetSurfaceGridCellId& CellId,
		int32 FaceResolution) const;

	TArray<FSRCelestialBodyDynamicMeshCellColorData> ColorDataByFlatId;
	TSet<uint64> HighlightedColorElements;
	TMap<uint64, FLinearColor> HighlightedBaseColorByElement;
	TArray<FSRPlanetSurfaceGridCell> SurfaceGridCells;
	uint32 BuildHash = 0;
	bool bHasBuildHash = false;
};
