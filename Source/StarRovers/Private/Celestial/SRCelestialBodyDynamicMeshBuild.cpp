#include "Celestial/SRCelestialBody.h"

#include "Celestial/SRDynamicMeshBaseDataAsset.h"
#include "Celestial/SRCelestialBodyDynamicMeshPipeline.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Engine/StaticMesh.h"
#include "Surface/SRPlanetSurfaceGrid.h"

using namespace StarRovers::Celestial::DynamicMesh;

namespace
{
	TAutoConsoleVariable<float> CVarSRDynamicMeshMinecraftSideWallMinHeightStepRatio(
		TEXT("sr.DynamicMesh.MinecraftSideWallMinHeightStepRatio"),
		0.25f,
		TEXT("Minimum Minecraft height-step ratio required to generate an internal terrain side wall. Set 0 to only skip exact same-height edges."));

	TAutoConsoleVariable<int32> CVarSRDynamicMeshBuildBreakdownTimings(
		TEXT("sr.DynamicMesh.BuildBreakdownTimings"),
		0,
		TEXT("Log detailed dynamic mesh build stage timings. Set 0 to disable per-cell timing probes."));

	constexpr int32 DynamicMeshOceanLevelSampleCount = 512;
}

bool ASRCelestialBody::BuildPreparedCelestialBodyDynamicMesh(FSRCelestialBodyPreparedDynamicMesh& OutPreparedMesh)
{
	OutPreparedMesh = FSRCelestialBodyPreparedDynamicMesh();
	const double TotalStart = GetDynamicMeshTimingSeconds();
	FSRCelestialBodyDynamicMeshPreBuildTimings PreBuildTimings;
	const uint32 DynamicMeshBuildHash = ComputeDynamicMeshBuildHash();

	const float FallbackRadius = IsValid(StaticMesh.Get()) ? StaticMesh->GetBounds().SphereRadius : 1.0f;
	FSRCelestialBodyDynamicMeshBuildConfig BuildConfig;
	FSRCelestialBodyDynamicMeshPreparedBuildStorage BuildStorage;
	if (!TryInitializePreparedDynamicMeshBuild(
		DynamicMeshBaseDataAsset.Get(),
		GetName(),
		FallbackRadius,
		BuildConfig,
		BuildStorage,
		PreBuildTimings))
	{
		return false;
	}

	FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator TerrainEdgeAccumulator = MakePreparedDynamicMeshTerrainEdgeAccumulator(
		BuildStorage,
		ToonOutlineSettings,
		BuildConfig,
		Scale,
		DynamicMeshGeneration.bMinecraft,
		CVarSRDynamicMeshMinecraftSideWallMinHeightStepRatio.GetValueOnAnyThread());

	const int32 PendingTerrainEdgeReserveCount = ReservePreparedDynamicMeshTerrainEdgesWithTiming(
		TerrainEdgeAccumulator,
		BuildConfig.CellCount,
		PreBuildTimings);

	const FSRCelestialBodyDynamicMeshPreBuildData PreBuildData = PreparePreparedDynamicMeshPreBuildData(
		*DynamicMeshBaseDataAsset.Get(),
		DynamicMeshGeneration,
		BuildConfig,
		DynamicMeshOceanLevelSampleCount,
		PreBuildTimings);

	const bool bProfileBuildBreakdown = CVarSRDynamicMeshBuildBreakdownTimings.GetValueOnAnyThread() != 0;

	CompletePreparedDynamicMeshPreBuild(
		GetName(),
		PreBuildData,
		DynamicMeshOceanLevelSampleCount,
		TotalStart,
		PreBuildTimings);

	BuildPreparedDynamicMeshCellsAndFinalize(
		GetName(),
		ToonOutlineSettings,
		BuildConfig,
		PreBuildData,
		PreBuildTimings,
		PendingTerrainEdgeReserveCount,
		bProfileBuildBreakdown,
		Scale,
		DynamicMeshBuildHash,
		TotalStart,
		BuildStorage,
		TerrainEdgeAccumulator,
		OutPreparedMesh);
	return true;
}
