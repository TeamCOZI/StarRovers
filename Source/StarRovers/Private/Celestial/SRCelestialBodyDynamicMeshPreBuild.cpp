#include "Celestial/SRCelestialBodyDynamicMeshPipeline.h"

#include "Utility/SRLog.h"
#include "SRCelestialBodyLog.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Surface/SRPlanetSurfaceGridLibrary.h"
#include "Surface/SRPlanetTerrainGenerator.h"

namespace StarRovers::Celestial::DynamicMesh
{
bool ValidatePreparedDynamicMeshBaseDataAsset(
	const USRDynamicMeshBaseDataAsset* DynamicMeshBaseDataAsset,
	const FString& BodyName)
{
	if (!IsValid(DynamicMeshBaseDataAsset))
	{
		return false;
	}

	if (DynamicMeshBaseDataAsset->BaseShape != ESRDynamicMeshBaseShape::CubeSphere)
	{
		SR_LOG(DynamicMesh, LogStarRoversCelestial, Warning, TEXT("Celestial body '%s' has unsupported DynamicMeshBase shape."), *BodyName);
		return false;
	}
	return true;
}

FSRCelestialBodyDynamicMeshBuildConfig MakePreparedDynamicMeshBuildConfig(
	const USRDynamicMeshBaseDataAsset& DynamicMeshBaseDataAsset,
	float FallbackRadius)
{
	FSRCelestialBodyDynamicMeshBuildConfig Config;
	Config.FaceResolution = DynamicMeshBaseDataAsset.GetClampedFaceResolution();
	Config.SourceBodyRadius = DynamicMeshBaseDataAsset.GetSafeBaseRadius(FallbackRadius);
	Config.CellCount = CubeSphereFaceComponentCount * Config.FaceResolution * Config.FaceResolution;
	Config.TerrainHeightStep = ComputeRegularCubeFaceCellEdgeLength(Config.SourceBodyRadius, Config.FaceResolution);
	return Config;
}

TMap<FName, int32> BuildDynamicMeshBiomeMaterialSlotIndexById(
	const TArray<FSRBiomeMaterialEntry>& BiomeMaterials)
{
	TMap<FName, int32> BiomeMaterialSlotIndexById;
	BiomeMaterialSlotIndexById.Reserve(BiomeMaterials.Num());
	for (int32 MaterialIndex = 0; MaterialIndex < BiomeMaterials.Num(); ++MaterialIndex)
	{
		const FSRBiomeMaterialEntry& BiomeMaterialEntry = BiomeMaterials[MaterialIndex];
		if (!BiomeMaterialEntry.BiomeId.IsNone() && IsValid(BiomeMaterialEntry.Material.Get()))
		{
			BiomeMaterialSlotIndexById.Add(BiomeMaterialEntry.BiomeId, MaterialIndex + 1);
		}
	}
	return BiomeMaterialSlotIndexById;
}

FSRCelestialBodyDynamicMeshOceanLevelClamp ResolveDynamicMeshOceanLevelClamp(
	const FSRDynamicMeshGenerationSnapshot& DynamicMeshGeneration,
	int32 SampleCount,
	float TerrainHeightStep)
{
	FSRCelestialBodyDynamicMeshOceanLevelClamp Result;
	Result.bApplyOceanLevelHeightClamp =
		DynamicMeshGeneration.bClampTerrainHeightToOceanLevel
		&& FSRPlanetTerrainGenerator::TryResolveOceanLevelHeightOffset(
			DynamicMeshGeneration,
			SampleCount,
			TerrainHeightStep,
			Result.OceanLevelHeightOffset);
	Result.OceanLevelClampHeightOffset = Result.bApplyOceanLevelHeightClamp
		? Result.OceanLevelHeightOffset + TerrainHeightStep
		: Result.OceanLevelHeightOffset;
	return Result;
}

FSRCelestialBodyDynamicMeshBaseCellSource PrepareDynamicMeshBaseCellSource(
	const USRDynamicMeshBaseDataAsset& DynamicMeshBaseDataAsset,
	int32 FaceResolution,
	float SourceBodyRadius,
	double& BaseCellsMs,
	double& BaseSourceMetadataMs)
{
	double StageStart = GetDynamicMeshTimingSeconds();
	FSRCelestialBodyDynamicMeshBaseCellSource Result;
	Result.PrecomputedBaseCells = DynamicMeshBaseDataAsset.GetValidPrecomputedCells();
	Result.bUsingPrecomputedBaseCells = Result.PrecomputedBaseCells != nullptr;
	Result.PrecomputedBaseCellScale = Result.bUsingPrecomputedBaseCells
		? DynamicMeshBaseDataAsset.GetPrecomputedCellScale(SourceBodyRadius)
		: 1.0f;
	if (!Result.bUsingPrecomputedBaseCells)
	{
		Result.GeneratedBaseCells = USRPlanetSurfaceGridLibrary::GenerateCubeSphereCells(FaceResolution, SourceBodyRadius);
	}
	BaseCellsMs = GetDynamicMeshTimingElapsedMilliseconds(StageStart);

	StageStart = GetDynamicMeshTimingSeconds();
	Result.BaseSourceMetadataCells = Result.bUsingPrecomputedBaseCells
		? DynamicMeshBaseDataAsset.GetValidPrecomputedSourceMetadata()
		: nullptr;
	Result.bUsingPrecomputedSourceMetadata = Result.BaseSourceMetadataCells != nullptr;
	if (!Result.BaseSourceMetadataCells && !Result.bUsingPrecomputedBaseCells)
	{
		Result.BaseSourceMetadataCells = &GetDynamicMeshBaseSourceMetadataCacheEntry(FaceResolution, Result.GeneratedBaseCells).Cells;
	}
	BaseSourceMetadataMs = GetDynamicMeshTimingElapsedMilliseconds(StageStart);
	return Result;
}

void InitializePreparedDynamicMeshFaceMeshes(TArray<UE::Geometry::FDynamicMesh3>& FaceDynamicMeshes)
{
	FaceDynamicMeshes.SetNum(CubeSphereFaceComponentCount);
	for (UE::Geometry::FDynamicMesh3& FaceDynamicMesh : FaceDynamicMeshes)
	{
		FaceDynamicMesh.EnableAttributes();
		FaceDynamicMesh.Attributes()->EnablePrimaryColors();
		FaceDynamicMesh.Attributes()->SetNumUVLayers(2);
		FaceDynamicMesh.Attributes()->EnableMaterialID();
	}
}

void ReservePreparedDynamicMeshBuildContainers(
	int32 FaceResolution,
	int32 CellCount,
	TMap<FSRCelestialBodyDynamicMeshTerrainVertexKey, int32>& WeldedVertexIds,
	TArray<int32>& CachedCellIndexByFlatId,
	TArray<FSRCelestialBodyDynamicMeshCellColorData>& PreparedColorDataByFlatId,
	TArray<FSRPlanetSurfaceGridCell>& PreparedSurfaceGridCells)
{
	WeldedVertexIds.Reserve((FaceResolution + 1) * (FaceResolution + 1) * CubeSphereFaceComponentCount);
	CachedCellIndexByFlatId.Init(INDEX_NONE, CellCount);
	PreparedColorDataByFlatId.SetNum(CellCount);
	PreparedSurfaceGridCells.SetNum(CellCount);
}

bool TryInitializePreparedDynamicMeshBuild(
	const USRDynamicMeshBaseDataAsset* DynamicMeshBaseDataAsset,
	const FString& BodyName,
	float FallbackRadius,
	FSRCelestialBodyDynamicMeshBuildConfig& OutBuildConfig,
	FSRCelestialBodyDynamicMeshPreparedBuildStorage& OutStorage,
	FSRCelestialBodyDynamicMeshPreBuildTimings& OutTimings)
{
	OutBuildConfig = FSRCelestialBodyDynamicMeshBuildConfig();
	OutStorage = FSRCelestialBodyDynamicMeshPreparedBuildStorage();
	OutTimings = FSRCelestialBodyDynamicMeshPreBuildTimings();

	const double StageStart = GetDynamicMeshTimingSeconds();
	if (!ValidatePreparedDynamicMeshBaseDataAsset(DynamicMeshBaseDataAsset, BodyName))
	{
		return false;
	}

	OutBuildConfig = MakePreparedDynamicMeshBuildConfig(*DynamicMeshBaseDataAsset, FallbackRadius);
	OutTimings.ValidationMs = GetDynamicMeshTimingElapsedMilliseconds(StageStart);
	LogPreparedDynamicMeshBuildBegin(
		BodyName,
		GetNameSafe(DynamicMeshBaseDataAsset),
		OutBuildConfig.FaceResolution,
		OutBuildConfig.CellCount,
		OutBuildConfig.SourceBodyRadius);

	InitializePreparedDynamicMeshBuildStorage(OutBuildConfig, OutStorage, OutTimings);
	return true;
}

void InitializePreparedDynamicMeshBuildStorage(
	const FSRCelestialBodyDynamicMeshBuildConfig& BuildConfig,
	FSRCelestialBodyDynamicMeshPreparedBuildStorage& Storage,
	FSRCelestialBodyDynamicMeshPreBuildTimings& Timings)
{
	double StageStart = GetDynamicMeshTimingSeconds();
	InitializePreparedDynamicMeshFaceMeshes(Storage.FaceDynamicMeshes);
	Timings.MeshSetupMs = GetDynamicMeshTimingElapsedMilliseconds(StageStart);

	StageStart = GetDynamicMeshTimingSeconds();
	ReservePreparedDynamicMeshBuildContainers(
		BuildConfig.FaceResolution,
		BuildConfig.CellCount,
		Storage.WeldedVertexIds,
		Storage.CachedCellIndexByFlatId,
		Storage.PreparedColorDataByFlatId,
		Storage.PreparedSurfaceGridCells);
	Timings.ContainerReserveMs = GetDynamicMeshTimingElapsedMilliseconds(StageStart);
}

FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator MakePreparedDynamicMeshTerrainEdgeAccumulator(
	FSRCelestialBodyDynamicMeshPreparedBuildStorage& BuildStorage,
	const FSRToonOutlineSettings& ToonOutlineSettings,
	const FSRCelestialBodyDynamicMeshBuildConfig& BuildConfig,
	float BodyScale,
	bool bMinecraft,
	float MinecraftSideWallMinHeightStepRatio)
{
	return FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator(
		BuildStorage.FaceDynamicMeshes,
		BuildStorage.WeldedVertexIds,
		BuildStorage.PreparedSurfaceGridCells,
		BuildStorage.CachedCellIndexByFlatId,
		BuildStorage.PreparedColorDataByFlatId,
		ToonOutlineSettings,
		BuildConfig.FaceResolution,
		BodyScale,
		BuildConfig.TerrainHeightStep,
		bMinecraft,
		MinecraftSideWallMinHeightStepRatio);
}

int32 ReservePreparedDynamicMeshTerrainEdges(
	FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator& TerrainEdgeAccumulator,
	int32 CellCount)
{
	const int32 PendingTerrainEdgeReserveCount = FMath::Min(CellCount * 2, 8192);
	TerrainEdgeAccumulator.ReservePendingEdges(PendingTerrainEdgeReserveCount);
	return PendingTerrainEdgeReserveCount;
}

int32 ReservePreparedDynamicMeshTerrainEdgesWithTiming(
	FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator& TerrainEdgeAccumulator,
	int32 CellCount,
	FSRCelestialBodyDynamicMeshPreBuildTimings& Timings)
{
	const double StageStart = GetDynamicMeshTimingSeconds();
	const int32 PendingTerrainEdgeReserveCount = ReservePreparedDynamicMeshTerrainEdges(TerrainEdgeAccumulator, CellCount);
	Timings.EdgeReserveMs = GetDynamicMeshTimingElapsedMilliseconds(StageStart);
	return PendingTerrainEdgeReserveCount;
}

FSRCelestialBodyDynamicMeshPreBuildData PreparePreparedDynamicMeshPreBuildData(
	const USRDynamicMeshBaseDataAsset& DynamicMeshBaseDataAsset,
	const FSRDynamicMeshGeneration& DynamicMeshGeneration,
	const FSRCelestialBodyDynamicMeshBuildConfig& BuildConfig,
	int32 OceanLevelSampleCount,
	FSRCelestialBodyDynamicMeshPreBuildTimings& Timings)
{
	FSRCelestialBodyDynamicMeshPreBuildData Result;

	double StageStart = GetDynamicMeshTimingSeconds();
	Result.BiomeMaterialSlotIndexById = BuildDynamicMeshBiomeMaterialSlotIndexById(DynamicMeshGeneration.BiomeMaterials);
	Timings.BiomeMaterialMapMs = GetDynamicMeshTimingElapsedMilliseconds(StageStart);

	StageStart = GetDynamicMeshTimingSeconds();
	Result.DynamicMeshGenerationSnapshot = DynamicMeshGeneration.MakeThreadSafeSnapshot();
	Timings.SnapshotMs = GetDynamicMeshTimingElapsedMilliseconds(StageStart);

	StageStart = GetDynamicMeshTimingSeconds();
	Result.OceanLevelClamp = ResolveDynamicMeshOceanLevelClamp(
		Result.DynamicMeshGenerationSnapshot,
		OceanLevelSampleCount,
		BuildConfig.TerrainHeightStep);
	Timings.OceanLevelMs = GetDynamicMeshTimingElapsedMilliseconds(StageStart);

	Result.BaseCellSource = PrepareDynamicMeshBaseCellSource(
		DynamicMeshBaseDataAsset,
		BuildConfig.FaceResolution,
		BuildConfig.SourceBodyRadius,
		Timings.BaseCellsMs,
		Timings.BaseSourceMetadataMs);

	return Result;
}

void CompletePreparedDynamicMeshPreBuild(
	const FString& BodyName,
	const FSRCelestialBodyDynamicMeshPreBuildData& PreBuildData,
	int32 OceanLevelSampleCount,
	double TotalStart,
	FSRCelestialBodyDynamicMeshPreBuildTimings& Timings)
{
	Timings.TotalMs = GetDynamicMeshTimingElapsedMilliseconds(TotalStart);
	LogPreparedDynamicMeshPreBuild(
		BodyName,
		PreBuildData.DynamicMeshGenerationSnapshot,
		PreBuildData.OceanLevelClamp,
		Timings,
		OceanLevelSampleCount);
}
}
