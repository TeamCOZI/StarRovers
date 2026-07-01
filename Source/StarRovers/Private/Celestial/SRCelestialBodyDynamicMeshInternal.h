#pragma once

#include "CoreMinimal.h"
#include "Celestial/SRCelestialBody.h"
#include "Celestial/SRDynamicMeshBaseDataAsset.h"

namespace StarRovers::Celestial::DynamicMesh
{
	inline constexpr int32 CubeSphereFaceComponentCount = 6;
	inline constexpr bool bEnableGlobalDynamicMeshRuntimeCache = false;
	inline constexpr int32 MaxRuntimeDynamicMeshCacheEntries = 16;

	inline double SRCelestialNowSeconds()
	{
		return FPlatformTime::Seconds();
	}

	inline double SRCelestialElapsedMilliseconds(double StartSeconds)
	{
		return (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
	}

	struct FSRCelestialBodyDynamicMeshRuntimeCacheEntry
	{
		TArray<UE::Geometry::FDynamicMesh3> FaceDynamicMeshes;
		TArray<FSRPlanetSurfaceGridCell> SurfaceGridCells;
		TArray<FSRCelestialBodyDynamicMeshCellColorData> ColorDataByFlatId;
	};

	struct FSRCelestialBodyBaseSourceMetadataCacheEntry
	{
		int32 FaceResolution = 0;
		TArray<FSRDynamicMeshBaseSourceMetadataCell> Cells;
	};

	struct FSRCelestialBodyDynamicMeshBaseCellView
	{
		FSRPlanetSurfaceGridCellId CellId;
		FVector LocalCenter = FVector::ZeroVector;
		FVector LocalNormal = FVector::UpVector;
		FVector Corner00 = FVector::ZeroVector;
		FVector Corner10 = FVector::ZeroVector;
		FVector Corner11 = FVector::ZeroVector;
		FVector Corner01 = FVector::ZeroVector;
		FVector2D FaceUVMin = FVector2D::ZeroVector;
		FVector2D FaceUVMax = FVector2D::ZeroVector;
		float ApproxSurfaceArea = 0.0f;
		FSRPlanetSurfaceGridCellNeighbors Neighbors;
	};

	struct FSRTerrainVertexKey
	{
		int32 A = 0;
		int32 B = 0;
		int32 C = 0;
		int32 D = 0;

		bool operator==(const FSRTerrainVertexKey& Other) const
		{
			return A == Other.A && B == Other.B && C == Other.C && D == Other.D;
		}
	};

	struct FSRCelestialBodyDynamicMeshSurfaceCellGeometry
	{
		FVector TargetPositions[4] = { FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector };
		uint32 SourcePositionHashes[4] = { 0, 0, 0, 0 };
		FSRTerrainVertexKey SurfaceVertexKeys[4];
		FVector CellNormal = FVector::UpVector;
		FVector TargetCellCenter = FVector::ZeroVector;
		bool bSwappedCellWinding = false;
	};

	struct FSRCelestialBodyDynamicMeshTerrainEdge
	{
		FVector PointA = FVector::ZeroVector;
		FVector PointB = FVector::ZeroVector;
		FVector CellCenter = FVector::ZeroVector;
		FVector CellNormal = FVector::UpVector;
		uint32 SourceHashA = 0;
		uint32 SourceHashB = 0;
		float HeightOffset = 0.0f;
		FLinearColor SurfaceColor = FLinearColor::White;
		int32 MaterialId = 0;
		FSRPlanetSurfaceGridCellId CellId;
		FSRCelestialBodyDynamicMeshQuadFeatureMaskRef SurfaceFeatureMaskRef;
		int32 SurfaceFeatureMaskEdgeIndex = INDEX_NONE;
	};

	struct FSRCelestialBodyDynamicMeshSideWallFeatureMaskEdge
	{
		FSRCelestialBodyDynamicMeshQuadFeatureMaskRef FeatureMaskRef;
		int32 EdgeIndex = INDEX_NONE;
		FVector WallNormal = FVector::ForwardVector;
	};

	struct FSRCelestialBodyDynamicMeshTerrainEdgeStats
	{
		int32 RegisterCount = 0;
		int32 MatchCount = 0;
		int32 SideWallCount = 0;
		int32 FeatureEdgeMaskCount = 0;
		int32 SideWallFailedTriangleCount = 0;
		int32 SideWallFallbackTriangleCount = 0;
		int32 MaxPendingEdgeCount = 0;
	};

	struct FSRCelestialBodyDynamicMeshValidationStats
	{
		int32 WeldedBoundaryEdgeCount = 0;
		int32 NonEmptyDynamicMeshCount = 0;
		int32 FirstMeshVertexCount = 0;
		int32 FirstMeshTriangleCount = 0;
	};

	class FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator
	{
	public:
		FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator(
			TArray<UE::Geometry::FDynamicMesh3>& InFaceDynamicMeshes,
			TMap<FSRTerrainVertexKey, int32>& InWeldedVertexIds,
			TArray<FSRPlanetSurfaceGridCell>& InPreparedSurfaceGridCells,
			const TArray<int32>& InCachedCellIndexByFlatId,
			TArray<FSRCelestialBodyDynamicMeshCellColorData>& InPreparedColorDataByFlatId,
			const FSRToonOutlineSettings& InToonOutlineSettings,
			int32 InFaceResolution,
			float InBodyScale,
			float InTerrainHeightStep,
			bool bInMinecraft,
			float InMinecraftSideWallMinHeightStepRatio);

		void ReservePendingEdges(int32 ReserveCount);
		void RegisterEdge(
			uint32 EndpointHashA,
			uint32 EndpointHashB,
			const FVector& PointA,
			const FVector& PointB,
			const FVector& CellCenter,
			const FVector& CellNormal,
			float HeightOffset,
			const FLinearColor& SurfaceColor,
			const FSRCelestialBodyDynamicMeshQuadRenderData& SurfaceRenderData,
			int32 SurfaceFeatureMaskEdgeIndex,
			int32 MaterialId,
			const FSRPlanetSurfaceGridCellId& CellId);
		void RegisterCellEdges(
			const FSRCelestialBodyDynamicMeshSurfaceCellGeometry& CellGeometry,
			const FSRPlanetTerrainSample& TerrainSample,
			const FSRCelestialBodyDynamicMeshQuadRenderData& SurfaceRenderData,
			int32 MaterialId,
			const FSRPlanetSurfaceGridCellId& CellId,
			bool bProfileBuildBreakdown,
			double& TerrainEdgeRegisterMs);
		void FlushPendingSideWallFeatureMaskEdges();
		int32 GetPendingEdgeCount() const;
		const FSRCelestialBodyDynamicMeshTerrainEdgeStats& GetStats() const;

	private:
		bool ApplyFeatureEdgeMask(
			const FSRCelestialBodyDynamicMeshQuadFeatureMaskRef& FeatureMaskRef,
			int32 EdgeIndex);
		void AppendSideWallFeatureMaskBoundaryEdge(
			const FSRCelestialBodyDynamicMeshQuadFeatureMaskRef& FeatureMaskRef,
			int32 EdgeIndex);
		void RegisterSideWallFeatureMaskVerticalEdge(
			uint32 SourceHash,
			float HeightOffsetA,
			float HeightOffsetB,
			const FSRCelestialBodyDynamicMeshQuadFeatureMaskRef& FeatureMaskRef,
			int32 EdgeIndex,
			const FVector& WallNormal);

		TArray<UE::Geometry::FDynamicMesh3>& FaceDynamicMeshes;
		TMap<FSRTerrainVertexKey, int32>& WeldedVertexIds;
		TArray<FSRPlanetSurfaceGridCell>& PreparedSurfaceGridCells;
		const TArray<int32>& CachedCellIndexByFlatId;
		TArray<FSRCelestialBodyDynamicMeshCellColorData>& PreparedColorDataByFlatId;
		const FSRToonOutlineSettings& ToonOutlineSettings;
		TMap<uint64, FSRCelestialBodyDynamicMeshTerrainEdge> PendingEdges;
		TMap<uint64, FSRCelestialBodyDynamicMeshSideWallFeatureMaskEdge> PendingSideWallFeatureMaskEdges;
		FSRCelestialBodyDynamicMeshTerrainEdgeStats Stats;
		int32 FaceResolution = 0;
		float BodyScale = 1.0f;
		float TerrainHeightStep = 0.0f;
		bool bMinecraft = false;
		float MinecraftSideWallMinHeightStepRatio = 0.0f;
	};

	uint32 GetTypeHash(const FSRTerrainVertexKey& Key);

	FSRCelestialBodyDynamicMeshRuntimeCacheEntry* FindCelestialBodyDynamicMeshRuntimeCache(uint32 BuildHash);
	FSRCelestialBodyDynamicMeshRuntimeCacheEntry& StoreCelestialBodyDynamicMeshRuntimeCache(
		uint32 BuildHash,
		FSRCelestialBodyDynamicMeshRuntimeCacheEntry&& Entry);
	void ClearCelestialBodyDynamicMeshRuntimeCache();
	int32 GetCelestialBodyDynamicMeshRuntimeCacheEntryCount();
	UWorld* GetCelestialBodyDynamicMeshRuntimeCacheWorld();
	void SetCelestialBodyDynamicMeshRuntimeCacheWorld(UWorld* World);

	int32 GetCubeSphereFaceComponentIndex(ESRCubeSphereFace Face);
	uint32 HashSourcePosition(const FVector& Position);
	uint64 BuildSourcePositionEdgeKey(uint32 EndpointA, uint32 EndpointB);
	const FSRCelestialBodyBaseSourceMetadataCacheEntry& GetBaseSourceMetadataCacheEntry(
		int32 FaceResolution,
		const TArray<FSRPlanetSurfaceGridCell>& BaseCells);
	FSRTerrainVertexKey MakeTerrainVertexKey(const FVector& Position);
	FSRTerrainVertexKey MakeTerrainVertexKey(uint32 SourcePositionHash, float HeightOffset);
	int32 CountDynamicMeshBoundaryEdges(const UE::Geometry::FDynamicMesh3& Mesh);
	float ComputeRegularCubeFaceCellEdgeLength(float SourceRadius, int32 FaceResolution);
	FSRPlanetTerrainSample SampleTerrainForDynamicMesh(
		const FSRBiomeSampleContext& Context,
		const FSRDynamicMeshGenerationSnapshot& DynamicMeshGeneration,
		float HeightStep);
	FSRCelestialBodyDynamicMeshBaseCellView MakeDynamicMeshBaseCellView(
		int32 BaseCellIndex,
		const TArray<FSRDynamicMeshBasePrecomputedCell>* PrecomputedBaseCells,
		float PrecomputedBaseCellScale,
		const TArray<FSRPlanetSurfaceGridCell>& GeneratedBaseCells);
	FSRCelestialBodyDynamicMeshSurfaceCellGeometry BuildDynamicMeshSurfaceCellGeometry(
		const FSRCelestialBodyDynamicMeshBaseCellView& BaseCell,
		const FSRPlanetTerrainSample& TerrainSample,
		const FVector& CellDirection,
		const TArray<FSRDynamicMeshBaseSourceMetadataCell>* BaseSourceMetadataCells,
		int32 BaseCellIndex,
		bool bProfileBuildBreakdown,
		double& CellTransformMs,
		double& SourceHashMs);
	FSRCelestialBodyDynamicMeshQuadRenderData AppendDynamicMeshSurfaceCellQuad(
		TArray<UE::Geometry::FDynamicMesh3>& FaceDynamicMeshes,
		TMap<FSRTerrainVertexKey, int32>& WeldedVertexIds,
		const FSRPlanetSurfaceGridCellId& CellId,
		const FSRCelestialBodyDynamicMeshSurfaceCellGeometry& CellGeometry,
		const FSRPlanetTerrainSample& TerrainSample,
		int32 MaterialId,
		bool bProfileBuildBreakdown,
		double& SurfaceAppendMs);
	void RecordDynamicMeshSurfaceColorData(
		TArray<FSRCelestialBodyDynamicMeshCellColorData>& PreparedColorDataByFlatId,
		int32 CellFlatIndex,
		const FSRCelestialBodyDynamicMeshQuadRenderData& SurfaceRenderData,
		bool bProfileBuildBreakdown,
		double& ColorDataMs);
	void CacheDynamicMeshSurfaceGridCell(
		TArray<FSRPlanetSurfaceGridCell>& PreparedSurfaceGridCells,
		TArray<int32>& CachedCellIndexByFlatId,
		int32 CellFlatIndex,
		const FSRCelestialBodyDynamicMeshBaseCellView& BaseCell,
		const FSRCelestialBodyDynamicMeshSurfaceCellGeometry& CellGeometry,
		const FSRPlanetTerrainSample& TerrainSample,
		float BodyScale,
		bool bProfileBuildBreakdown,
		double& CacheCellMs);
	void CompactPreparedSurfaceGridCells(
		TArray<FSRPlanetSurfaceGridCell>& PreparedSurfaceGridCells,
		TArray<int32>& CachedCellIndexByFlatId,
		int32 ValidCellCount);
	FSRCelestialBodyDynamicMeshValidationStats ValidatePreparedDynamicMeshBuild(
		const TArray<UE::Geometry::FDynamicMesh3>& FaceDynamicMeshes);
	FSRCelestialBodyDynamicMeshQuadRenderData AppendFlatColoredDynamicMeshQuad(
		TArray<UE::Geometry::FDynamicMesh3>& FaceDynamicMeshes,
		TMap<FSRTerrainVertexKey, int32>& WeldedVertexIds,
		int32 MeshComponentIndex,
		const FVector& Point0,
		const FVector& Point1,
		const FVector& Point2,
		const FVector& Point3,
		const FLinearColor& SurfaceColor,
		int32 MaterialId,
		bool bDoubleSided = false,
		const FSRTerrainVertexKey* VertexKeys = nullptr,
		const FVector* NormalReferenceDirectionOverride = nullptr,
		bool bAllowUnweldedFallbackForFailedTriangles = false);
	int32 ResolvePreparedSurfaceGridCellIndex(
		const TArray<int32>& CachedCellIndexByFlatId,
		int32 FaceResolution,
		const FSRPlanetSurfaceGridCellId& CellId);
	void AddPreparedSurfaceGridSideLineSegment(
		TArray<FSRPlanetSurfaceGridCell>& PreparedSurfaceGridCells,
		const TArray<int32>& CachedCellIndexByFlatId,
		int32 FaceResolution,
		float BodyScale,
		const FSRPlanetSurfaceGridCellId& CellId,
		const FSRPlanetSurfaceGridCellId& AdjacentCellId,
		bool bHasAdjacentCell,
		const FVector& PointA,
		const FVector& PointB);
	void AddPreparedSurfaceGridSideFace(
		TArray<FSRPlanetSurfaceGridCell>& PreparedSurfaceGridCells,
		const TArray<int32>& CachedCellIndexByFlatId,
		int32 FaceResolution,
		float BodyScale,
		const FSRPlanetSurfaceGridCellId& CellId,
		const FSRPlanetSurfaceGridCellId& AdjacentCellId,
		bool bHasAdjacentCell,
		const FVector& Point0,
		const FVector& Point1,
		const FVector& Point2,
		const FVector& Point3);
	void AddPreparedSurfaceGridSideWallOutline(
		TArray<FSRPlanetSurfaceGridCell>& PreparedSurfaceGridCells,
		const TArray<int32>& CachedCellIndexByFlatId,
		int32 FaceResolution,
		float BodyScale,
		const FSRPlanetSurfaceGridCellId& CellId,
		const FSRPlanetSurfaceGridCellId& AdjacentCellId,
		bool bHasAdjacentCell,
		const FVector& Point0,
		const FVector& Point1,
		const FVector& Point2,
		const FVector& Point3);
}
