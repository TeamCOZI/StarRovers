#include "Surface/SRPlanetSurfaceGridCubeSphereHelpers.h"
#include "Surface/SRPlanetSurfaceGridLibrary.h"
#include "HAL/CriticalSection.h"
#include "Utility/SRTimingLog.h"
namespace StarRovers::SurfaceGrid::CubeSphere
{
TSharedRef<const FSRConformalCubeVertexDirectionGrid> BuildConformalVertexDirectionGrid(int32 Resolution)
{
	TSharedRef<FSRConformalCubeVertexDirectionGrid> Grid = MakeShared<FSRConformalCubeVertexDirectionGrid>();
	Grid->Resolution = Resolution;
	Grid->VertexResolution = Resolution + 1;
	Grid->Directions.SetNum(6 * Grid->VertexResolution * Grid->VertexResolution);

	for (uint8 FaceValue = static_cast<uint8>(ESRCubeSphereFace::PositiveX); FaceValue <= static_cast<uint8>(ESRCubeSphereFace::NegativeZ); ++FaceValue)
	{
		const ESRCubeSphereFace Face = static_cast<ESRCubeSphereFace>(FaceValue);
		for (int32 VertexY = 0; VertexY < Grid->VertexResolution; ++VertexY)
		{
			const float FaceV = -1.0f + (2.0f * static_cast<float>(VertexY) / static_cast<float>(Resolution));
			for (int32 VertexX = 0; VertexX < Grid->VertexResolution; ++VertexX)
			{
				const float FaceU = -1.0f + (2.0f * static_cast<float>(VertexX) / static_cast<float>(Resolution));
				Grid->Directions[Grid->GetDirectionIndex(Face, VertexX, VertexY)] =
					GetConformalCubeDirection(Face, FaceU, FaceV);
			}
		}
	}

	return Grid;
}

const FSRConformalCubeVertexDirectionGrid& GetConformalVertexDirectionGrid(int32 Resolution)
{
	static TMap<int32, TSharedPtr<const FSRConformalCubeVertexDirectionGrid>> CacheByResolution;
	static FCriticalSection CacheCriticalSection;

	const double LockStart = FPlatformTime::Seconds();
	CacheCriticalSection.Lock();
	const double LockWaitMs = (FPlatformTime::Seconds() - LockStart) * 1000.0;
	if (const TSharedPtr<const FSRConformalCubeVertexDirectionGrid>* CachedGrid = CacheByResolution.Find(Resolution))
	{
		if (CachedGrid->IsValid())
		{
			const FSRConformalCubeVertexDirectionGrid* Result = CachedGrid->Get();
			CacheCriticalSection.Unlock();
			if (LockWaitMs > 1.0)
			{
				FSRTimingLog::AddLine(FString::Printf(
					TEXT("ConformalCubeVertexGrid CacheHit Resolution=%d LockWait=%.2f ms"),
					Resolution,
					LockWaitMs));
			}
			return *Result;
		}
	}

	const double BuildStart = FPlatformTime::Seconds();
	const TSharedRef<const FSRConformalCubeVertexDirectionGrid> NewGrid = BuildConformalVertexDirectionGrid(Resolution);
	const double BuildMs = (FPlatformTime::Seconds() - BuildStart) * 1000.0;
	CacheByResolution.Add(Resolution, NewGrid);
	CacheCriticalSection.Unlock();
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("ConformalCubeVertexGrid CacheMissBuild Resolution=%d LockWait=%.2f ms Build=%.2f ms Vertices=%d"),
		Resolution,
		LockWaitMs,
		BuildMs,
		NewGrid->Directions.Num()));
	return NewGrid.Get();
}

bool BuildConformalCubeCell(
	int32 Resolution,
	float Radius,
	const FSRPlanetSurfaceGridCellId& CellId,
	const FSRConformalCubeVertexDirectionGrid& VertexDirectionGrid,
	FSRPlanetSurfaceGridCell& OutCell)
{
	OutCell = FSRPlanetSurfaceGridCell();
	if (Resolution <= 0
		|| Radius <= 0.0f
		|| !CellId.IsValid(Resolution)
		|| VertexDirectionGrid.Resolution != Resolution)
	{
		return false;
	}

	const FVector& Corner00Direction = VertexDirectionGrid.GetDirection(CellId.Face, CellId.CellX, CellId.CellY);
	const FVector& Corner10Direction = VertexDirectionGrid.GetDirection(CellId.Face, CellId.CellX + 1, CellId.CellY);
	const FVector& Corner11Direction = VertexDirectionGrid.GetDirection(CellId.Face, CellId.CellX + 1, CellId.CellY + 1);
	const FVector& Corner01Direction = VertexDirectionGrid.GetDirection(CellId.Face, CellId.CellX, CellId.CellY + 1);

	OutCell.CellId = CellId;
	OutCell.FaceUVMin = FVector2D(GetFaceCoordinateMin(CellId.CellX, Resolution), GetFaceCoordinateMin(CellId.CellY, Resolution));
	OutCell.FaceUVMax = FVector2D(GetFaceCoordinateMax(CellId.CellX, Resolution), GetFaceCoordinateMax(CellId.CellY, Resolution));
	OutCell.Corner00 = Corner00Direction * Radius;
	OutCell.Corner10 = Corner10Direction * Radius;
	OutCell.Corner11 = Corner11Direction * Radius;
	OutCell.Corner01 = Corner01Direction * Radius;
	OutCell.LocalCenter = (Corner00Direction + Corner10Direction + Corner11Direction + Corner01Direction).GetSafeNormal() * Radius;
	OutCell.LocalNormal = OutCell.LocalCenter.GetSafeNormal();
	OutCell.ApproxSurfaceArea = ComputeQuadArea(OutCell.Corner00, OutCell.Corner10, OutCell.Corner11, OutCell.Corner01);
	OutCell.Neighbors = USRPlanetSurfaceGridLibrary::GetCubeSphereNeighborIds(CellId, Resolution);
	return true;
}

TSharedRef<const FSRConformalCubeBaseCellGrid> BuildConformalBaseCellGrid(int32 Resolution)
{
	TSharedRef<FSRConformalCubeBaseCellGrid> Grid = MakeShared<FSRConformalCubeBaseCellGrid>();
	Grid->Resolution = Resolution;
	Grid->UnitCells.Reserve(6 * Resolution * Resolution);
	const FSRConformalCubeVertexDirectionGrid& VertexDirectionGrid = GetConformalVertexDirectionGrid(Resolution);

	for (uint8 FaceValue = static_cast<uint8>(ESRCubeSphereFace::PositiveX); FaceValue <= static_cast<uint8>(ESRCubeSphereFace::NegativeZ); ++FaceValue)
	{
		const ESRCubeSphereFace Face = static_cast<ESRCubeSphereFace>(FaceValue);
		for (int32 CellY = 0; CellY < Resolution; ++CellY)
		{
			for (int32 CellX = 0; CellX < Resolution; ++CellX)
			{
				FSRPlanetSurfaceGridCell Cell;
				FSRPlanetSurfaceGridCellId CellId;
				CellId.Face = Face;
				CellId.CellX = CellX;
				CellId.CellY = CellY;
				if (BuildConformalCubeCell(Resolution, 1.0f, CellId, VertexDirectionGrid, Cell))
				{
					Grid->UnitCells.Add(Cell);
				}
			}
		}
	}

	return Grid;
}

const FSRConformalCubeBaseCellGrid& GetConformalBaseCellGrid(int32 Resolution)
{
	static TMap<int32, TSharedPtr<const FSRConformalCubeBaseCellGrid>> CacheByResolution;
	static FCriticalSection CacheCriticalSection;

	const double LockStart = FPlatformTime::Seconds();
	CacheCriticalSection.Lock();
	const double LockWaitMs = (FPlatformTime::Seconds() - LockStart) * 1000.0;
	if (const TSharedPtr<const FSRConformalCubeBaseCellGrid>* CachedGrid = CacheByResolution.Find(Resolution))
	{
		if (CachedGrid->IsValid())
		{
			const FSRConformalCubeBaseCellGrid* Result = CachedGrid->Get();
			CacheCriticalSection.Unlock();
			if (LockWaitMs > 1.0)
			{
				FSRTimingLog::AddLine(FString::Printf(
					TEXT("ConformalCubeBaseGrid CacheHit Resolution=%d LockWait=%.2f ms"),
					Resolution,
					LockWaitMs));
			}
			return *Result;
		}
	}

	const double BuildStart = FPlatformTime::Seconds();
	const TSharedRef<const FSRConformalCubeBaseCellGrid> NewGrid = BuildConformalBaseCellGrid(Resolution);
	const double BuildMs = (FPlatformTime::Seconds() - BuildStart) * 1000.0;
	CacheByResolution.Add(Resolution, NewGrid);
	CacheCriticalSection.Unlock();
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("ConformalCubeBaseGrid CacheMissBuild Resolution=%d LockWait=%.2f ms Build=%.2f ms Cells=%d"),
		Resolution,
		LockWaitMs,
		BuildMs,
		NewGrid->UnitCells.Num()));
	return NewGrid.Get();
}

void ScaleBaseCell(FSRPlanetSurfaceGridCell& Cell, float Radius)
{
	if (FMath::IsNearlyEqual(Radius, 1.0f))
	{
		return;
	}

	Cell.LocalCenter *= Radius;
	Cell.Corner00 *= Radius;
	Cell.Corner10 *= Radius;
	Cell.Corner11 *= Radius;
	Cell.Corner01 *= Radius;
	Cell.ApproxSurfaceArea *= Radius * Radius;
}
}