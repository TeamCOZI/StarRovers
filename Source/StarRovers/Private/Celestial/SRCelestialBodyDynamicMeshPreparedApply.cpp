#include "Celestial/SRCelestialBody.h"

#include "Celestial/SRCelestialBodyDynamicMeshPipeline.h"
#include "Utility/SRTimingLog.h"

using namespace StarRovers::Celestial::DynamicMesh;

bool ASRCelestialBody::ApplyPreparedCelestialBodyDynamicMesh(FSRCelestialBodyPreparedDynamicMesh&& PreparedMesh, double TotalStart)
{
	if (!PreparedMesh.bValid)
	{
		return false;
	}

	ResetDynamicMeshCellColorData();
	DynamicMeshState.ColorDataByFlatId = MoveTemp(PreparedMesh.ColorDataByFlatId);
	DynamicMeshState.SurfaceGridCells = MoveTemp(PreparedMesh.SurfaceGridCells);

	const double SetMeshMs = ApplyPreparedDynamicMeshFaceMeshes(
		CelestialBodyDynamicMesh.Get(),
		CelestialBodyDynamicMeshFaces,
		PreparedMesh.FaceDynamicMeshes);
	const double SurfaceGridApplyMs = ApplyPreparedDynamicMeshSurfaceGridBuild(
		GetSurfaceGrid(),
		DynamicMeshState.SurfaceGridCells,
		MoveTemp(PreparedMesh.CellIndexByFlatId));

	DynamicMeshState.MarkBuilt(PreparedMesh.BuildHash);
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("DynamicMesh '%s' BaseMetadata.Total %.2f ms Build=%.2f ms RuntimeCache=0.00 ms SetMesh=%.2f ms SurfaceGrid=%.2f ms"),
		*GetName(),
		GetDynamicMeshTimingElapsedMilliseconds(TotalStart),
		PreparedMesh.BuildMilliseconds,
		SetMeshMs,
		SurfaceGridApplyMs));
	return true;
}
