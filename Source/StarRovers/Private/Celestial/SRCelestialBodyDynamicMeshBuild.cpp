#include "Celestial/SRCelestialBody.h"

#include "SRCelestialBodyLog.h"
#include "Celestial/SRDynamicMeshBaseDataAsset.h"
#include "Celestial/SRCelestialBodyDynamicMeshInternal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Engine/StaticMesh.h"
#include "Surface/SRPlanetBiomeDataAsset.h"
#include "Surface/SRPlanetSurfaceGrid.h"
#include "Surface/SRPlanetSurfaceGridLibrary.h"
#include "Surface/SRPlanetTerrainGenerator.h"
#include "Utility/SRTimingLog.h"

namespace
{
	TAutoConsoleVariable<float> CVarSRDynamicMeshMinecraftSideWallMinHeightStepRatio(
		TEXT("sr.DynamicMesh.MinecraftSideWallMinHeightStepRatio"),
		0.25f,
		TEXT("Minimum Minecraft height-step ratio required to generate an internal terrain side wall. Set 0 to only skip exact same-height edges."));

	TAutoConsoleVariable<int32> CVarSRDynamicMeshBuildBreakdownTimings(
		TEXT("sr.DynamicMesh.BuildBreakdownTimings"),
		1,
		TEXT("Log detailed dynamic mesh build stage timings. Set 0 to disable per-cell timing probes."));

}

using namespace StarRovers::Celestial::DynamicMesh;

bool ASRCelestialBody::BuildPreparedCelestialBodyDynamicMesh(FSRCelestialBodyPreparedDynamicMesh& OutPreparedMesh)
{
	OutPreparedMesh = FSRCelestialBodyPreparedDynamicMesh();
	const double TotalStart = SRCelestialNowSeconds();
	double PreBuildValidationMs = 0.0;
	double PreBuildMeshSetupMs = 0.0;
	double PreBuildContainerReserveMs = 0.0;
	double PreBuildEdgeReserveMs = 0.0;
	double PreBuildSetupTotalMs = 0.0;
	double PreBuildStageStart = SRCelestialNowSeconds();
	const uint32 DynamicMeshBuildHash = ComputeDynamicMeshBuildHash();

	if (!IsValid(DynamicMeshBaseDataAsset.Get()))
	{
		return false;
	}

	if (DynamicMeshBaseDataAsset->BaseShape != ESRDynamicMeshBaseShape::CubeSphere)
	{
		UE_LOG(LogStarRoversCelestial, Warning, TEXT("Celestial body '%s' has unsupported DynamicMeshBase shape."), *GetName());
		return false;
	}

	const int32 FaceResolution = DynamicMeshBaseDataAsset->GetClampedFaceResolution();
	const float SourceBodyRadius = DynamicMeshBaseDataAsset->GetSafeBaseRadius(IsValid(StaticMesh.Get()) ? StaticMesh->GetBounds().SphereRadius : 1.0f);
	const int32 CellCount = CubeSphereFaceComponentCount * FaceResolution * FaceResolution;
	const float TerrainHeightStep = ComputeRegularCubeFaceCellEdgeLength(SourceBodyRadius, FaceResolution);
	PreBuildValidationMs = SRCelestialElapsedMilliseconds(PreBuildStageStart);
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("DynamicMesh '%s' BaseMetadata Begin Source='%s' Shape=CubeSphere Resolution=%d Cells=%d Radius=%.3f"),
		*GetName(),
		*GetNameSafe(DynamicMeshBaseDataAsset.Get()),
		FaceResolution,
		CellCount,
		SourceBodyRadius));

	PreBuildStageStart = SRCelestialNowSeconds();
	TArray<UE::Geometry::FDynamicMesh3> FaceDynamicMeshes;
	FaceDynamicMeshes.SetNum(CubeSphereFaceComponentCount);
	for (UE::Geometry::FDynamicMesh3& FaceDynamicMesh : FaceDynamicMeshes)
	{
		FaceDynamicMesh.EnableAttributes();
		FaceDynamicMesh.Attributes()->EnablePrimaryColors();
		FaceDynamicMesh.Attributes()->SetNumUVLayers(1);
		FaceDynamicMesh.Attributes()->EnableMaterialID();
	}
	PreBuildMeshSetupMs = SRCelestialElapsedMilliseconds(PreBuildStageStart);

	PreBuildStageStart = SRCelestialNowSeconds();
	TMap<FSRTerrainVertexKey, int32> WeldedVertexIds;
	WeldedVertexIds.Reserve((FaceResolution + 1) * (FaceResolution + 1) * CubeSphereFaceComponentCount);
	TArray<int32> CachedCellIndexByFlatId;
	CachedCellIndexByFlatId.Init(INDEX_NONE, CellCount);
	TArray<FSRCelestialBodyDynamicMeshCellColorData> PreparedColorDataByFlatId;
	PreparedColorDataByFlatId.SetNum(CellCount);
	TArray<FSRPlanetSurfaceGridCell> PreparedSurfaceGridCells;
	PreparedSurfaceGridCells.SetNum(CellCount);
	PreBuildContainerReserveMs = SRCelestialElapsedMilliseconds(PreBuildStageStart);

	FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator TerrainEdgeAccumulator(
		FaceDynamicMeshes,
		WeldedVertexIds,
		PreparedSurfaceGridCells,
		CachedCellIndexByFlatId,
		PreparedColorDataByFlatId,
		FaceResolution,
		Scale,
		TerrainHeightStep,
		DynamicMeshGeneration.bMinecraft,
		CVarSRDynamicMeshMinecraftSideWallMinHeightStepRatio.GetValueOnGameThread());

	const int32 PendingTerrainEdgeReserveCount = FMath::Min(CellCount * 2, 8192);
	PreBuildStageStart = SRCelestialNowSeconds();
	TerrainEdgeAccumulator.ReservePendingEdges(PendingTerrainEdgeReserveCount);
	PreBuildEdgeReserveMs = SRCelestialElapsedMilliseconds(PreBuildStageStart);

	double StageStart = SRCelestialNowSeconds();
	TMap<FName, int32> BiomeMaterialSlotIndexById;
	BiomeMaterialSlotIndexById.Reserve(DynamicMeshGeneration.BiomeMaterials.Num());
	for (int32 MaterialIndex = 0; MaterialIndex < DynamicMeshGeneration.BiomeMaterials.Num(); ++MaterialIndex)
	{
		const FSRBiomeMaterialEntry& BiomeMaterialEntry = DynamicMeshGeneration.BiomeMaterials[MaterialIndex];
		if (!BiomeMaterialEntry.BiomeId.IsNone() && IsValid(BiomeMaterialEntry.Material.Get()))
		{
			BiomeMaterialSlotIndexById.Add(BiomeMaterialEntry.BiomeId, MaterialIndex + 1);
		}
	}
	const double BiomeMaterialMapMs = SRCelestialElapsedMilliseconds(StageStart);

	StageStart = SRCelestialNowSeconds();
	const FSRDynamicMeshGenerationSnapshot DynamicMeshGenerationSnapshot = DynamicMeshGeneration.MakeThreadSafeSnapshot();
	const double SnapshotMs = SRCelestialElapsedMilliseconds(StageStart);

	StageStart = SRCelestialNowSeconds();
	TArray<FSRPlanetSurfaceGridCell> GeneratedBaseCells;
	const TArray<FSRDynamicMeshBasePrecomputedCell>* PrecomputedBaseCells =
		DynamicMeshBaseDataAsset->GetValidPrecomputedCells();
	const bool bUsingPrecomputedBaseCells = PrecomputedBaseCells != nullptr;
	const float PrecomputedBaseCellScale =
		bUsingPrecomputedBaseCells ? DynamicMeshBaseDataAsset->GetPrecomputedCellScale(SourceBodyRadius) : 1.0f;
	if (!bUsingPrecomputedBaseCells)
	{
		GeneratedBaseCells = USRPlanetSurfaceGridLibrary::GenerateCubeSphereCells(FaceResolution, SourceBodyRadius);
	}
	const double BaseCellsMs = SRCelestialElapsedMilliseconds(StageStart);

	StageStart = SRCelestialNowSeconds();
	const TArray<FSRDynamicMeshBaseSourceMetadataCell>* BaseSourceMetadataCells =
		bUsingPrecomputedBaseCells ? DynamicMeshBaseDataAsset->GetValidPrecomputedSourceMetadata() : nullptr;
	const bool bUsingPrecomputedSourceMetadata = BaseSourceMetadataCells != nullptr;
	if (!BaseSourceMetadataCells && !bUsingPrecomputedBaseCells)
	{
		BaseSourceMetadataCells = &GetBaseSourceMetadataCacheEntry(FaceResolution, GeneratedBaseCells).Cells;
	}
	const double BaseSourceMetadataMs = SRCelestialElapsedMilliseconds(StageStart);

	const bool bProfileBuildBreakdown = CVarSRDynamicMeshBuildBreakdownTimings.GetValueOnGameThread() != 0;
	double TerrainSampleMs = 0.0;
	double CellTransformMs = 0.0;
	double SourceHashMs = 0.0;
	double SurfaceAppendMs = 0.0;
	double ColorDataMs = 0.0;
	double CacheCellMs = 0.0;
	double TerrainEdgeRegisterMs = 0.0;

	PreBuildSetupTotalMs = SRCelestialElapsedMilliseconds(TotalStart);
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("DynamicMesh '%s' BaseMetadata.PreBuildBreakdown Validation=%.2f ms MeshSetup=%.2f ms ContainerReserve=%.2f ms EdgeReserve=%.2f ms BiomeMap=%.2f ms Snapshot=%.2f ms BaseGrid=%.2f ms BaseSourceMeta=%.2f ms PreBuildTotal=%.2f ms"),
		*GetName(),
		PreBuildValidationMs,
		PreBuildMeshSetupMs,
		PreBuildContainerReserveMs,
		PreBuildEdgeReserveMs,
		BiomeMaterialMapMs,
		SnapshotMs,
		BaseCellsMs,
		BaseSourceMetadataMs,
		PreBuildSetupTotalMs));

	const double BuildCellsStart = SRCelestialNowSeconds();
	int32 ValidCellCount = 0;

	const int32 BaseCellCount = bUsingPrecomputedBaseCells ? PrecomputedBaseCells->Num() : GeneratedBaseCells.Num();
	for (int32 BaseCellIndex = 0; BaseCellIndex < BaseCellCount; ++BaseCellIndex)
	{
		const FSRCelestialBodyDynamicMeshBaseCellView BaseCell = MakeDynamicMeshBaseCellView(
			BaseCellIndex,
			PrecomputedBaseCells,
			PrecomputedBaseCellScale,
			GeneratedBaseCells);
		const FSRPlanetSurfaceGridCellId CellId = BaseCell.CellId;
		if (!CellId.IsValid(FaceResolution))
		{
			continue;
		}
		const int32 CellFlatIndex = ((static_cast<int32>(CellId.Face) * FaceResolution) + CellId.CellY) * FaceResolution + CellId.CellX;
		if (!PreparedSurfaceGridCells.IsValidIndex(CellFlatIndex) || !CachedCellIndexByFlatId.IsValidIndex(CellFlatIndex))
		{
			continue;
		}

		const FVector CellDirection = BaseCell.LocalCenter.GetSafeNormal();
		if (CellDirection.IsNearlyZero())
		{
			continue;
		}

		FSRBiomeSampleContext TerrainSampleContext;
		TerrainSampleContext.LocalUnitDirection = CellDirection;
		TerrainSampleContext.Face = CellId.Face;
		TerrainSampleContext.CellX = CellId.CellX;
		TerrainSampleContext.CellY = CellId.CellY;
		TerrainSampleContext.FaceResolution = FaceResolution;
		TerrainSampleContext.FaceUV = (BaseCell.FaceUVMin + BaseCell.FaceUVMax) * 0.5f;

		FSRPlanetTerrainSample TerrainSample;
		if (bProfileBuildBreakdown)
		{
			const double InnerStart = SRCelestialNowSeconds();
			TerrainSample = SampleTerrainForDynamicMesh(TerrainSampleContext, DynamicMeshGenerationSnapshot, TerrainHeightStep);
			TerrainSampleMs += SRCelestialElapsedMilliseconds(InnerStart);
		}
		else
		{
			TerrainSample = SampleTerrainForDynamicMesh(TerrainSampleContext, DynamicMeshGenerationSnapshot, TerrainHeightStep);
		}

		const FSRCelestialBodyDynamicMeshSurfaceCellGeometry CellGeometry = BuildDynamicMeshSurfaceCellGeometry(
			BaseCell,
			TerrainSample,
			CellDirection,
			BaseSourceMetadataCells,
			BaseCellIndex,
			bProfileBuildBreakdown,
			CellTransformMs,
			SourceHashMs);
		const int32 MaterialId = BiomeMaterialSlotIndexById.FindRef(TerrainSample.BiomeId);
		const FSRCelestialBodyDynamicMeshQuadRenderData SurfaceRenderData = AppendDynamicMeshSurfaceCellQuad(
			FaceDynamicMeshes,
			WeldedVertexIds,
			CellId,
			CellGeometry,
			TerrainSample,
			MaterialId,
			bProfileBuildBreakdown,
			SurfaceAppendMs);
		RecordDynamicMeshSurfaceColorData(
			PreparedColorDataByFlatId,
			CellFlatIndex,
			SurfaceRenderData,
			bProfileBuildBreakdown,
			ColorDataMs);
		CacheDynamicMeshSurfaceGridCell(
			PreparedSurfaceGridCells,
			CachedCellIndexByFlatId,
			CellFlatIndex,
			BaseCell,
			CellGeometry,
			TerrainSample,
			Scale,
			bProfileBuildBreakdown,
			CacheCellMs);
		TerrainEdgeAccumulator.RegisterCellEdges(
			CellGeometry,
			TerrainSample,
			MaterialId,
			CellId,
			bProfileBuildBreakdown,
			TerrainEdgeRegisterMs);
		++ValidCellCount;
	}
	CompactPreparedSurfaceGridCells(PreparedSurfaceGridCells, CachedCellIndexByFlatId, ValidCellCount);
	const double CellLoopMs = SRCelestialElapsedMilliseconds(BuildCellsStart);
	const FSRCelestialBodyDynamicMeshTerrainEdgeStats& TerrainEdgeStats = TerrainEdgeAccumulator.GetStats();

	FSRTimingLog::AddLine(FString::Printf(
		TEXT("DynamicMesh '%s' BaseMetadata.BuildCells %.2f ms BaseCells=%d Cells=%d EdgeRegisters=%d EdgeMatches=%d SideWalls=%d PendingEdges=%d MaxPendingEdges=%d PendingReserve=%d"),
		*GetName(),
		BaseCellsMs + BaseSourceMetadataMs + CellLoopMs,
		GeneratedBaseCells.Num(),
		ValidCellCount,
		TerrainEdgeStats.RegisterCount,
		TerrainEdgeStats.MatchCount,
		TerrainEdgeStats.SideWallCount,
		TerrainEdgeAccumulator.GetPendingEdgeCount(),
		TerrainEdgeStats.MaxPendingEdgeCount,
		PendingTerrainEdgeReserveCount));
	const int32 SideWallUnpatchedTriangleCount = FMath::Max(
		0,
		TerrainEdgeStats.SideWallFailedTriangleCount - TerrainEdgeStats.SideWallFallbackTriangleCount);
	const TCHAR* SideWallPatchStatus = SideWallUnpatchedTriangleCount > 0
		? TEXT("UnpatchedTriangles")
		: (TerrainEdgeStats.SideWallFailedTriangleCount > 0 ? TEXT("FallbackPatched") : TEXT("NoFallbackNeeded"));
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("DynamicMesh '%s' BaseMetadata.SideWallPatch FailedTriangles=%d FallbackTriangles=%d UnpatchedTriangles=%d Status=%s"),
		*GetName(),
		TerrainEdgeStats.SideWallFailedTriangleCount,
		TerrainEdgeStats.SideWallFallbackTriangleCount,
		SideWallUnpatchedTriangleCount,
		SideWallPatchStatus));
	if (SideWallUnpatchedTriangleCount > 0)
	{
		UE_LOG(
			LogStarRoversCelestial,
			Warning,
			TEXT("Dynamic mesh '%s' left %d side-wall triangles unpatched after fallback."),
			*GetName(),
			SideWallUnpatchedTriangleCount);
	}
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("DynamicMesh '%s' BaseMetadata.BuildBreakdown Profile=%s BaseGridSource=%s BaseSourceMetaSource=%s BiomeMap=%.2f ms Snapshot=%.2f ms BaseGrid=%.2f ms BaseSourceMeta=%.2f ms CellLoop=%.2f ms TerrainSample=%.2f ms Transform=%.2f ms SourceHash=%.2f ms SurfaceAppend=%.2f ms ColorData=%.2f ms CacheCell=%.2f ms EdgeRegister=%.2f ms"),
		*GetName(),
		bProfileBuildBreakdown ? TEXT("true") : TEXT("false"),
		bUsingPrecomputedBaseCells ? TEXT("Precomputed") : TEXT("Generated"),
		bUsingPrecomputedSourceMetadata ? TEXT("Precomputed") : TEXT("Generated"),
		BiomeMaterialMapMs,
		SnapshotMs,
		BaseCellsMs,
		BaseSourceMetadataMs,
		CellLoopMs,
		TerrainSampleMs,
		CellTransformMs,
		SourceHashMs,
		SurfaceAppendMs,
		ColorDataMs,
		CacheCellMs,
		TerrainEdgeRegisterMs));

	const double PostBuildStart = SRCelestialNowSeconds();
	double PendingEdgesCheckMs = 0.0;
	double WeldedMeshCheckMs = 0.0;
	double PreparedMoveMs = 0.0;

	double PostBuildStageStart = SRCelestialNowSeconds();
	if (TerrainEdgeAccumulator.GetPendingEdgeCount() > 0)
	{
		UE_LOG(
			LogStarRoversCelestial,
			Warning,
			TEXT("Dynamic mesh '%s' code-generated base has %d unmatched source edges."),
			*GetName(),
			TerrainEdgeAccumulator.GetPendingEdgeCount());
	}
	PendingEdgesCheckMs = SRCelestialElapsedMilliseconds(PostBuildStageStart);

	PostBuildStageStart = SRCelestialNowSeconds();
	const FSRCelestialBodyDynamicMeshValidationStats ValidationStats = ValidatePreparedDynamicMeshBuild(FaceDynamicMeshes);
	if (ValidationStats.WeldedBoundaryEdgeCount > 0)
	{
		UE_LOG(
			LogStarRoversCelestial,
			Warning,
			TEXT("Dynamic mesh '%s' generated with %d open boundary edges after code-generated base welding."),
			*GetName(),
			ValidationStats.WeldedBoundaryEdgeCount);
	}
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("DynamicMesh '%s' BaseMetadata.WeldedMeshCheck BoundaryEdges=%d Meshes=%d Vertices=%d Triangles=%d"),
		*GetName(),
		ValidationStats.WeldedBoundaryEdgeCount,
		ValidationStats.NonEmptyDynamicMeshCount,
		ValidationStats.FirstMeshVertexCount,
		ValidationStats.FirstMeshTriangleCount));
	WeldedMeshCheckMs = SRCelestialElapsedMilliseconds(PostBuildStageStart);

	PostBuildStageStart = SRCelestialNowSeconds();
	OutPreparedMesh.bValid = true;
	OutPreparedMesh.BuildHash = DynamicMeshBuildHash;
	OutPreparedMesh.FaceDynamicMeshes = MoveTemp(FaceDynamicMeshes);
	OutPreparedMesh.SurfaceGridCells = MoveTemp(PreparedSurfaceGridCells);
	OutPreparedMesh.CellIndexByFlatId = MoveTemp(CachedCellIndexByFlatId);
	OutPreparedMesh.ColorDataByFlatId = MoveTemp(PreparedColorDataByFlatId);
	PreparedMoveMs = SRCelestialElapsedMilliseconds(PostBuildStageStart);
	OutPreparedMesh.BuildMilliseconds = SRCelestialElapsedMilliseconds(TotalStart);
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("DynamicMesh '%s' BaseMetadata.PostBuildBreakdown PendingEdges=%.2f ms WeldedMeshCheck=%.2f ms PreparedMove=%.2f ms PostBuildTotal=%.2f ms"),
		*GetName(),
		PendingEdgesCheckMs,
		WeldedMeshCheckMs,
		PreparedMoveMs,
		SRCelestialElapsedMilliseconds(PostBuildStart)));
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("DynamicMesh '%s' BaseMetadata.Prepared %.2f ms"),
		*GetName(),
		OutPreparedMesh.BuildMilliseconds));
	return true;
}
