#include "Surface/SRPlanetSurfaceGrid.h"

#include "SRPlanetSurfaceGridCellIndex.h"
#include "SRPlanetSurfaceGridGeneratedGridState.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "Utility/SRTimingLog.h"

namespace SurfaceGridCellIndex = StarRovers::SurfaceGridCellIndex;
namespace SurfaceGridGeneratedGridState = StarRovers::SurfaceGridGeneratedGridState;

namespace
{
	double SRSurfaceGridNowSeconds()
	{
		return FPlatformTime::Seconds();
	}

	double SRSurfaceGridElapsedMilliseconds(double StartSeconds)
	{
		return (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
	}
}

void USRPlanetSurfaceGrid::ApplyGeneratedGridBuild(
	TArray<FSRPlanetSurfaceGridCell>&& NewCells,
	UE::Geometry::FDynamicMesh3&& NewGridMesh)
{
	TArray<int32> EmptyCellIndexByFlatId;
	ApplyGeneratedGridBuild(MoveTemp(NewCells), MoveTemp(NewGridMesh), MoveTemp(EmptyCellIndexByFlatId));
}

void USRPlanetSurfaceGrid::ApplyGeneratedGridBuild(
	TArray<FSRPlanetSurfaceGridCell>&& NewCells,
	UE::Geometry::FDynamicMesh3&& NewGridMesh,
	TMap<FSRPlanetSurfaceGridCellId, int32>&& NewCellIndexById)
{
	TArray<int32> NewCellIndexByFlatId = SurfaceGridCellIndex::BuildFlatCellIndexFromMap(NewCellIndexById, FaceResolution);
	ApplyGeneratedGridBuild(MoveTemp(NewCells), MoveTemp(NewGridMesh), MoveTemp(NewCellIndexByFlatId));
}

void USRPlanetSurfaceGrid::ApplyGeneratedGridBuild(
	TArray<FSRPlanetSurfaceGridCell>&& NewCells,
	UE::Geometry::FDynamicMesh3&& NewGridMesh,
	TArray<int32>&& NewCellIndexByFlatId)
{
	FSRTimingLogSession TimingLogSession(FString::Printf(TEXT("SurfaceGrid.ApplyGeneratedGridBuild Body=%s"), *GetNameSafe(GetOwner())));
	const double TotalStart = SRSurfaceGridNowSeconds();
	const int32 IncomingCellCount = NewCells.Num();
	double StageStart = SRSurfaceGridNowSeconds();
	ClearGeneratedGridBuildHighlights();
	const double ClearHighlightMs = SRSurfaceGridElapsedMilliseconds(StageStart);

	StageStart = SRSurfaceGridNowSeconds();
	AssignGeneratedGridBuildCells(MoveTemp(NewCells));
	const double AssignCellsMs = SRSurfaceGridElapsedMilliseconds(StageStart);

	StageStart = SRSurfaceGridNowSeconds();
	ApplyGeneratedGridCellIndex(MoveTemp(NewCellIndexByFlatId));
	const double RebuildCellIndexMs = SRSurfaceGridElapsedMilliseconds(StageStart);

	StageStart = SRSurfaceGridNowSeconds();
	RebuildGeneratedGridCellInfoIndex();
	const double RebuildCellInfoIndexMs = SRSurfaceGridElapsedMilliseconds(StageStart);

	StageStart = SRSurfaceGridNowSeconds();
	RebuildGeneratedGridRaycastIndex();
	const double RebuildRaycastIndexMs = SRSurfaceGridElapsedMilliseconds(StageStart);

	StageStart = SRSurfaceGridNowSeconds();
	FinalizeGeneratedGridBuildMesh();
	const double FinalizeMs = SRSurfaceGridElapsedMilliseconds(StageStart);

	FSRTimingLog::AddLine(FString::Printf(TEXT("SurfaceGrid.ApplyGeneratedGridBuild Body=%s Total=%.2fms Cells=%d FaceResolution=%d ClearHighlights=%.2fms AssignCells=%.2fms CellIndex=%.2fms CellInfoIndex=%.2fms RaycastIndex=%.2fms Finalize=%.2fms"),
		*GetNameSafe(GetOwner()),
		SRSurfaceGridElapsedMilliseconds(TotalStart),
		IncomingCellCount,
		FaceResolution,
		ClearHighlightMs,
		AssignCellsMs,
		RebuildCellIndexMs,
		RebuildCellInfoIndexMs,
		RebuildRaycastIndexMs,
		FinalizeMs));
}

bool USRPlanetSurfaceGrid::RebuildCellsFromOwnerGeneratedGrid()
{
	return SurfaceGridGeneratedGridState::TryLoadOwnerCachedCells(GetOwner(), Cells, bUsingGeneratedGridCells);
}
