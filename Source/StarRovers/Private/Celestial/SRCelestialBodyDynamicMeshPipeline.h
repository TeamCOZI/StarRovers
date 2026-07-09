#pragma once

#include "CoreMinimal.h"
#include "Celestial/SRCelestialBody.h"
#include "Celestial/SRDynamicMeshBaseDataAsset.h"

namespace StarRovers::Celestial::DynamicMesh
{
	inline constexpr int32 CubeSphereFaceComponentCount = 6;
	inline constexpr bool bEnableGlobalDynamicMeshRuntimeCache = false;
	inline constexpr int32 MaxRuntimeDynamicMeshCacheEntries = 16;

	inline double GetDynamicMeshTimingSeconds()
	{
		return FPlatformTime::Seconds();
	}

	inline double GetDynamicMeshTimingElapsedMilliseconds(double StartSeconds)
	{
		return (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
	}

	struct FSRCelestialBodyDynamicMeshRuntimeCacheEntry
	{
		TArray<UE::Geometry::FDynamicMesh3> FaceDynamicMeshes;
		TArray<FSRPlanetSurfaceGridCell> SurfaceGridCells;
		TArray<FSRCelestialBodyDynamicMeshCellColorData> ColorDataByFlatId;
	};

	struct FSRCelestialBodyDynamicMeshBaseSourceMetadataCacheEntry
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

	struct FSRCelestialBodyDynamicMeshTerrainVertexKey
	{
		int32 A = 0;
		int32 B = 0;
		int32 C = 0;
		int32 D = 0;

		bool operator==(const FSRCelestialBodyDynamicMeshTerrainVertexKey& Other) const
		{
			return A == Other.A && B == Other.B && C == Other.C && D == Other.D;
		}
	};

	uint32 GetTypeHash(const FSRCelestialBodyDynamicMeshTerrainVertexKey& Key);

	struct FSRCelestialBodyDynamicMeshSurfaceCellGeometry
	{
		FVector TargetPositions[4] = { FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector };
		uint32 SourcePositionHashes[4] = { 0, 0, 0, 0 };
		FSRCelestialBodyDynamicMeshTerrainVertexKey SurfaceVertexKeys[4];
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

	struct FSRCelestialBodyDynamicMeshOceanLevelClamp
	{
		float OceanLevelHeightOffset = 0.0f;
		float OceanLevelClampHeightOffset = 0.0f;
		bool bApplyOceanLevelHeightClamp = false;
	};

	struct FSRCelestialBodyDynamicMeshBaseCellSource
	{
		TArray<FSRPlanetSurfaceGridCell> GeneratedBaseCells;
		const TArray<FSRDynamicMeshBasePrecomputedCell>* PrecomputedBaseCells = nullptr;
		const TArray<FSRDynamicMeshBaseSourceMetadataCell>* BaseSourceMetadataCells = nullptr;
		float PrecomputedBaseCellScale = 1.0f;
		bool bUsingPrecomputedBaseCells = false;
		bool bUsingPrecomputedSourceMetadata = false;
	};

	struct FSRCelestialBodyDynamicMeshSurfaceCellBuildMetrics
	{
		int32 ValidCellCount = 0;
		double CellLoopMs = 0.0;
		double TerrainSampleMs = 0.0;
		double CellTransformMs = 0.0;
		double SourceHashMs = 0.0;
		double SurfaceAppendMs = 0.0;
		double ColorDataMs = 0.0;
		double CacheCellMs = 0.0;
		double TerrainEdgeRegisterMs = 0.0;
	};

	struct FSRCelestialBodyDynamicMeshPreBuildTimings
	{
		double ValidationMs = 0.0;
		double MeshSetupMs = 0.0;
		double ContainerReserveMs = 0.0;
		double EdgeReserveMs = 0.0;
		double BiomeMaterialMapMs = 0.0;
		double SnapshotMs = 0.0;
		double OceanLevelMs = 0.0;
		double BaseCellsMs = 0.0;
		double BaseSourceMetadataMs = 0.0;
		double TotalMs = 0.0;
	};

	struct FSRCelestialBodyDynamicMeshBuildConfig
	{
		int32 FaceResolution = 0;
		int32 CellCount = 0;
		float SourceBodyRadius = 1.0f;
		float TerrainHeightStep = 0.0f;
	};

	struct FSRCelestialBodyDynamicMeshPreparedBuildStorage
	{
		TArray<UE::Geometry::FDynamicMesh3> FaceDynamicMeshes;
		TMap<FSRCelestialBodyDynamicMeshTerrainVertexKey, int32> WeldedVertexIds;
		TArray<int32> CachedCellIndexByFlatId;
		TArray<FSRCelestialBodyDynamicMeshCellColorData> PreparedColorDataByFlatId;
		TArray<FSRPlanetSurfaceGridCell> PreparedSurfaceGridCells;
	};

	struct FSRCelestialBodyDynamicMeshPreBuildData
	{
		TMap<FName, int32> BiomeMaterialSlotIndexById;
		FSRDynamicMeshGenerationSnapshot DynamicMeshGenerationSnapshot;
		FSRCelestialBodyDynamicMeshOceanLevelClamp OceanLevelClamp;
		FSRCelestialBodyDynamicMeshBaseCellSource BaseCellSource;
	};

	class FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator
	{
	public:
		FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator(
			TArray<UE::Geometry::FDynamicMesh3>& InFaceDynamicMeshes,
			TMap<FSRCelestialBodyDynamicMeshTerrainVertexKey, int32>& InWeldedVertexIds,
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
		TMap<FSRCelestialBodyDynamicMeshTerrainVertexKey, int32>& WeldedVertexIds;
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
	FSRCelestialBodyDynamicMeshRuntimeCacheEntry* FindCelestialBodyDynamicMeshRuntimeCache(uint32 BuildHash);
	FSRCelestialBodyDynamicMeshRuntimeCacheEntry& StoreCelestialBodyDynamicMeshRuntimeCache(
		uint32 BuildHash,
		FSRCelestialBodyDynamicMeshRuntimeCacheEntry&& Entry);
	void ClearCelestialBodyDynamicMeshRuntimeCache();
	int32 GetCelestialBodyDynamicMeshRuntimeCacheEntryCount();
	UWorld* GetCelestialBodyDynamicMeshRuntimeCacheWorld();
	void SetCelestialBodyDynamicMeshRuntimeCacheWorld(UWorld* World);
	double ApplyPreparedDynamicMeshFaceMeshes(
		UDynamicMeshComponent* PrimaryDynamicMeshComponent,
		const TArray<TObjectPtr<UDynamicMeshComponent>>& FaceDynamicMeshComponents,
		TArray<UE::Geometry::FDynamicMesh3>& FaceDynamicMeshes);
	double ApplyCachedDynamicMeshFaceMeshes(
		UDynamicMeshComponent* PrimaryDynamicMeshComponent,
		const TArray<TObjectPtr<UDynamicMeshComponent>>& FaceDynamicMeshComponents,
		const TArray<UE::Geometry::FDynamicMesh3>& FaceDynamicMeshes);
	double ApplyPreparedDynamicMeshSurfaceGridBuild(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const TArray<FSRPlanetSurfaceGridCell>& SurfaceGridCells,
		TArray<int32>&& CellIndexByFlatId);
	double ApplyCachedDynamicMeshSurfaceGridBuild(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const TArray<FSRPlanetSurfaceGridCell>& SurfaceGridCells);
	bool ApplyDynamicMeshRuntimeCacheEntry(
		const FString& BodyName,
		FSRCelestialBodyDynamicMeshRuntimeState& DynamicMeshState,
		UDynamicMeshComponent* PrimaryDynamicMeshComponent,
		const TArray<TObjectPtr<UDynamicMeshComponent>>& FaceDynamicMeshComponents,
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRCelestialBodyDynamicMeshRuntimeCacheEntry& CacheEntry,
		uint32 DynamicMeshBuildHash);

	int32 GetCubeSphereFaceComponentIndex(ESRCubeSphereFace Face);
	uint32 HashSourcePosition(const FVector& Position);
	uint64 BuildSourcePositionEdgeKey(uint32 EndpointA, uint32 EndpointB);
	const FSRCelestialBodyDynamicMeshBaseSourceMetadataCacheEntry& GetDynamicMeshBaseSourceMetadataCacheEntry(
		int32 FaceResolution,
		const TArray<FSRPlanetSurfaceGridCell>& BaseCells);
	FSRCelestialBodyDynamicMeshTerrainVertexKey MakeCelestialBodyDynamicMeshTerrainVertexKey(const FVector& Position);
	FSRCelestialBodyDynamicMeshTerrainVertexKey MakeCelestialBodyDynamicMeshTerrainVertexKey(uint32 SourcePositionHash, float HeightOffset);
	int32 CountDynamicMeshBoundaryEdges(const UE::Geometry::FDynamicMesh3& Mesh);
	float ComputeRegularCubeFaceCellEdgeLength(float SourceRadius, int32 FaceResolution);
	FSRPlanetTerrainSample SampleTerrainForDynamicMesh(
		const FSRBiomeSampleContext& Context,
		const FSRDynamicMeshGenerationSnapshot& DynamicMeshGeneration,
		float HeightStep,
		bool bApplyOceanLevelHeightClamp,
		float OceanLevelHeightOffset);
	bool ValidatePreparedDynamicMeshBaseDataAsset(
		const USRDynamicMeshBaseDataAsset* DynamicMeshBaseDataAsset,
		const FString& BodyName);
	FSRCelestialBodyDynamicMeshBuildConfig MakePreparedDynamicMeshBuildConfig(
		const USRDynamicMeshBaseDataAsset& DynamicMeshBaseDataAsset,
		float FallbackRadius);
	TMap<FName, int32> BuildDynamicMeshBiomeMaterialSlotIndexById(
		const TArray<FSRBiomeMaterialEntry>& BiomeMaterials);
	FSRCelestialBodyDynamicMeshOceanLevelClamp ResolveDynamicMeshOceanLevelClamp(
		const FSRDynamicMeshGenerationSnapshot& DynamicMeshGeneration,
		int32 SampleCount,
		float TerrainHeightStep);
	FSRCelestialBodyDynamicMeshBaseCellSource PrepareDynamicMeshBaseCellSource(
		const USRDynamicMeshBaseDataAsset& DynamicMeshBaseDataAsset,
		int32 FaceResolution,
		float SourceBodyRadius,
		double& BaseCellsMs,
		double& BaseSourceMetadataMs);
	void InitializePreparedDynamicMeshFaceMeshes(TArray<UE::Geometry::FDynamicMesh3>& FaceDynamicMeshes);
	void ReservePreparedDynamicMeshBuildContainers(
		int32 FaceResolution,
		int32 CellCount,
		TMap<FSRCelestialBodyDynamicMeshTerrainVertexKey, int32>& WeldedVertexIds,
		TArray<int32>& CachedCellIndexByFlatId,
		TArray<FSRCelestialBodyDynamicMeshCellColorData>& PreparedColorDataByFlatId,
		TArray<FSRPlanetSurfaceGridCell>& PreparedSurfaceGridCells);
	bool TryInitializePreparedDynamicMeshBuild(
		const USRDynamicMeshBaseDataAsset* DynamicMeshBaseDataAsset,
		const FString& BodyName,
		float FallbackRadius,
		FSRCelestialBodyDynamicMeshBuildConfig& OutBuildConfig,
		FSRCelestialBodyDynamicMeshPreparedBuildStorage& OutStorage,
		FSRCelestialBodyDynamicMeshPreBuildTimings& OutTimings);
	void InitializePreparedDynamicMeshBuildStorage(
		const FSRCelestialBodyDynamicMeshBuildConfig& BuildConfig,
		FSRCelestialBodyDynamicMeshPreparedBuildStorage& Storage,
		FSRCelestialBodyDynamicMeshPreBuildTimings& Timings);
	FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator MakePreparedDynamicMeshTerrainEdgeAccumulator(
		FSRCelestialBodyDynamicMeshPreparedBuildStorage& BuildStorage,
		const FSRToonOutlineSettings& ToonOutlineSettings,
		const FSRCelestialBodyDynamicMeshBuildConfig& BuildConfig,
		float BodyScale,
		bool bMinecraft,
		float MinecraftSideWallMinHeightStepRatio);
	int32 ReservePreparedDynamicMeshTerrainEdges(
		FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator& TerrainEdgeAccumulator,
		int32 CellCount);
	int32 ReservePreparedDynamicMeshTerrainEdgesWithTiming(
		FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator& TerrainEdgeAccumulator,
		int32 CellCount,
		FSRCelestialBodyDynamicMeshPreBuildTimings& Timings);
	FSRCelestialBodyDynamicMeshPreBuildData PreparePreparedDynamicMeshPreBuildData(
		const USRDynamicMeshBaseDataAsset& DynamicMeshBaseDataAsset,
		const FSRDynamicMeshGeneration& DynamicMeshGeneration,
		const FSRCelestialBodyDynamicMeshBuildConfig& BuildConfig,
		int32 OceanLevelSampleCount,
		FSRCelestialBodyDynamicMeshPreBuildTimings& Timings);
	void CompletePreparedDynamicMeshPreBuild(
		const FString& BodyName,
		const FSRCelestialBodyDynamicMeshPreBuildData& PreBuildData,
		int32 OceanLevelSampleCount,
		double TotalStart,
		FSRCelestialBodyDynamicMeshPreBuildTimings& Timings);
	void LogPreparedDynamicMeshBuildBegin(
		const FString& BodyName,
		const FString& SourceName,
		int32 FaceResolution,
		int32 CellCount,
		float SourceBodyRadius);
	void LogPreparedDynamicMeshPreBuild(
		const FString& BodyName,
		const FSRDynamicMeshGenerationSnapshot& DynamicMeshGeneration,
		const FSRCelestialBodyDynamicMeshOceanLevelClamp& OceanLevelClamp,
		const FSRCelestialBodyDynamicMeshPreBuildTimings& Timings,
		int32 OceanLevelSampleCount);
	void LogPreparedDynamicMeshBuildSummary(
		const FString& BodyName,
		const FSRToonOutlineSettings& ToonOutlineSettings,
		const FSRCelestialBodyDynamicMeshBaseCellSource& BaseCellSource,
		const FSRCelestialBodyDynamicMeshSurfaceCellBuildMetrics& SurfaceCellBuildMetrics,
		const FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator& TerrainEdgeAccumulator,
		const FSRCelestialBodyDynamicMeshPreBuildTimings& Timings,
		int32 PendingTerrainEdgeReserveCount,
		bool bProfileBuildBreakdown);
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
		TMap<FSRCelestialBodyDynamicMeshTerrainVertexKey, int32>& WeldedVertexIds,
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
	FSRCelestialBodyDynamicMeshSurfaceCellBuildMetrics BuildPreparedDynamicMeshSurfaceCells(
		const FSRCelestialBodyDynamicMeshBaseCellSource& BaseCellSource,
		const FSRDynamicMeshGenerationSnapshot& DynamicMeshGeneration,
		const FSRCelestialBodyDynamicMeshOceanLevelClamp& OceanLevelClamp,
		const TMap<FName, int32>& BiomeMaterialSlotIndexById,
		int32 FaceResolution,
		float TerrainHeightStep,
		float BodyScale,
		bool bProfileBuildBreakdown,
		TArray<UE::Geometry::FDynamicMesh3>& FaceDynamicMeshes,
		TMap<FSRCelestialBodyDynamicMeshTerrainVertexKey, int32>& WeldedVertexIds,
		TArray<FSRPlanetSurfaceGridCell>& PreparedSurfaceGridCells,
		TArray<int32>& CachedCellIndexByFlatId,
		TArray<FSRCelestialBodyDynamicMeshCellColorData>& PreparedColorDataByFlatId,
		FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator& TerrainEdgeAccumulator);
	void BuildPreparedDynamicMeshCellsAndFinalize(
		const FString& BodyName,
		const FSRToonOutlineSettings& ToonOutlineSettings,
		const FSRCelestialBodyDynamicMeshBuildConfig& BuildConfig,
		const FSRCelestialBodyDynamicMeshPreBuildData& PreBuildData,
		const FSRCelestialBodyDynamicMeshPreBuildTimings& PreBuildTimings,
		int32 PendingTerrainEdgeReserveCount,
		bool bProfileBuildBreakdown,
		float BodyScale,
		uint32 DynamicMeshBuildHash,
		double TotalStart,
		FSRCelestialBodyDynamicMeshPreparedBuildStorage& BuildStorage,
		FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator& TerrainEdgeAccumulator,
		FSRCelestialBodyPreparedDynamicMesh& OutPreparedMesh);
	void CompactPreparedSurfaceGridCells(
		TArray<FSRPlanetSurfaceGridCell>& PreparedSurfaceGridCells,
		TArray<int32>& CachedCellIndexByFlatId,
		int32 ValidCellCount);
	FSRCelestialBodyDynamicMeshValidationStats ValidatePreparedDynamicMeshBuild(
		const TArray<UE::Geometry::FDynamicMesh3>& FaceDynamicMeshes);
	void FinalizePreparedDynamicMeshBuild(
		const FString& BodyName,
		uint32 DynamicMeshBuildHash,
		double TotalStart,
		const FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator& TerrainEdgeAccumulator,
		TArray<UE::Geometry::FDynamicMesh3>& FaceDynamicMeshes,
		TArray<FSRPlanetSurfaceGridCell>& PreparedSurfaceGridCells,
		TArray<int32>& CachedCellIndexByFlatId,
		TArray<FSRCelestialBodyDynamicMeshCellColorData>& PreparedColorDataByFlatId,
		FSRCelestialBodyPreparedDynamicMesh& OutPreparedMesh);
	FSRCelestialBodyDynamicMeshQuadRenderData AppendFlatColoredDynamicMeshQuad(
		TArray<UE::Geometry::FDynamicMesh3>& FaceDynamicMeshes,
		TMap<FSRCelestialBodyDynamicMeshTerrainVertexKey, int32>& WeldedVertexIds,
		int32 MeshComponentIndex,
		const FVector& Point0,
		const FVector& Point1,
		const FVector& Point2,
		const FVector& Point3,
		const FLinearColor& SurfaceColor,
		int32 MaterialId,
		bool bDoubleSided = false,
		const FSRCelestialBodyDynamicMeshTerrainVertexKey* VertexKeys = nullptr,
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
