#include "Celestial/SRCelestialBodyDynamicMeshPipeline.h"
#include "Utility/SRTimingLog.h"

namespace StarRovers::Celestial::DynamicMesh
{
bool ApplyDynamicMeshRuntimeCacheEntry(
	const FString& BodyName,
	FSRCelestialBodyDynamicMeshRuntimeState& DynamicMeshState,
	UDynamicMeshComponent* PrimaryDynamicMeshComponent,
	const TArray<TObjectPtr<UDynamicMeshComponent>>& FaceDynamicMeshComponents,
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRCelestialBodyDynamicMeshRuntimeCacheEntry& CacheEntry,
	uint32 DynamicMeshBuildHash)
{
	const double ApplyStart = GetDynamicMeshTimingSeconds();
	DynamicMeshState.Reset();

	DynamicMeshState.ColorDataByFlatId = CacheEntry.ColorDataByFlatId;
	DynamicMeshState.SurfaceGridCells = CacheEntry.SurfaceGridCells;

	ApplyCachedDynamicMeshFaceMeshes(
		PrimaryDynamicMeshComponent,
		FaceDynamicMeshComponents,
		CacheEntry.FaceDynamicMeshes);
	const double SurfaceGridApplyMs = ApplyCachedDynamicMeshSurfaceGridBuild(
		SurfaceGrid,
		DynamicMeshState.SurfaceGridCells);

	DynamicMeshState.MarkBuilt(DynamicMeshBuildHash);
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("DynamicMesh.ApplyCache '%s' %.2f ms SurfaceGrid=%.2f ms Meshes=%d Cells=%d"),
		*BodyName,
		GetDynamicMeshTimingElapsedMilliseconds(ApplyStart),
		SurfaceGridApplyMs,
		CacheEntry.FaceDynamicMeshes.Num(),
		CacheEntry.SurfaceGridCells.Num()));
	return true;
}
}
