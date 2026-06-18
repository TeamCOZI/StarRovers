#include "Surface/SRPlanetSurfaceGrid.h"

#include "Celestial/SRCelestialBody.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Utility/SRTimingLog.h"

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
	TArray<int32> NewCellIndexByFlatId;
	const int32 ExpectedFlatCellCount = 6 * FMath::Max(1, FaceResolution) * FMath::Max(1, FaceResolution);
	NewCellIndexByFlatId.Init(INDEX_NONE, ExpectedFlatCellCount);
	for (const TPair<FSRPlanetSurfaceGridCellId, int32>& CellIndexPair : NewCellIndexById)
	{
		const int32 FlatIndex = CellIndexPair.Key.IsValid(FaceResolution)
			? ((static_cast<int32>(CellIndexPair.Key.Face) * FaceResolution + CellIndexPair.Key.CellY) * FaceResolution + CellIndexPair.Key.CellX)
			: INDEX_NONE;
		if (NewCellIndexByFlatId.IsValidIndex(FlatIndex))
		{
			NewCellIndexByFlatId[FlatIndex] = CellIndexPair.Value;
		}
	}
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
	if (ASRCelestialBody* OwnerBody = Cast<ASRCelestialBody>(GetOwner()))
	{
		OwnerBody->ClearSurfaceCellHighlights();
	}
	const double ClearHighlightMs = SRSurfaceGridElapsedMilliseconds(StageStart);

	StageStart = SRSurfaceGridNowSeconds();
	Cells = MoveTemp(NewCells);
	bUsingGeneratedGridCells = true;
	TMap<ESRCubeSphereFace, int32> CellCountByFace;
	for (const FSRPlanetSurfaceGridCell& Cell : Cells)
	{
		CellCountByFace.FindOrAdd(Cell.CellId.Face)++;
	}
	for (const TPair<ESRCubeSphereFace, int32>& CellCountPair : CellCountByFace)
	{
		FaceResolution = FMath::Max(1, FMath::RoundToInt(FMath::Sqrt(static_cast<float>(CellCountPair.Value))));
		break;
	}
	bHasHoveredCell = false;
	HoveredCellId = FSRPlanetSurfaceGridCellId();
	bHasSelectedCell = false;
	SelectedCellId = FSRPlanetSurfaceGridCellId();
	InputPortPreviewCellIds.Reset();
	OutputPortPreviewCellIds.Reset();
	SetInteractionOverlayVisible(false);
	const double AssignCellsMs = SRSurfaceGridElapsedMilliseconds(StageStart);

	StageStart = SRSurfaceGridNowSeconds();
	const int32 ExpectedFlatCellCount = 6 * FaceResolution * FaceResolution;
	if (NewCellIndexByFlatId.Num() == ExpectedFlatCellCount)
	{
		CellIndexById.Reset();
		CellIndexByFlatId = MoveTemp(NewCellIndexByFlatId);
	}
	else
	{
		RebuildCellIndex();
	}
	const double RebuildCellIndexMs = SRSurfaceGridElapsedMilliseconds(StageStart);

	StageStart = SRSurfaceGridNowSeconds();
	RebuildCellInfoIndex();
	const double RebuildCellInfoIndexMs = SRSurfaceGridElapsedMilliseconds(StageStart);

	StageStart = SRSurfaceGridNowSeconds();
	RebuildRaycastIndex();
	const double RebuildRaycastIndexMs = SRSurfaceGridElapsedMilliseconds(StageStart);

	StageStart = SRSurfaceGridNowSeconds();
	UE::Geometry::FDynamicMesh3 EmptyGridMesh;
	EmptyGridMesh.EnableAttributes();
	EmptyGridMesh.Attributes()->EnablePrimaryColors();
	SetMesh(MoveTemp(EmptyGridMesh));
	SetVisibility(false);
	SetHiddenInGame(true);
	bCellsDirty = false;
	bGridMeshDirty = false;
	UpdateDebugTickState();
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
	const ASRCelestialBody* OwnerBody = Cast<ASRCelestialBody>(GetOwner());
	if (!IsValid(OwnerBody))
	{
		return false;
	}

	if (!OwnerBody->GetCachedSurfaceGridCells(Cells))
	{
		return false;
	}

	bUsingGeneratedGridCells = true;
	return true;
}
