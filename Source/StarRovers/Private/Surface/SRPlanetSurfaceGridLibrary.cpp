#include "Surface/SRPlanetSurfaceGridLibrary.h"
#include "Surface/SRPlanetSurfaceGridCubeSphereHelpers.h"
using namespace StarRovers::SurfaceGrid::CubeSphere;
bool USRPlanetSurfaceGridLibrary::IsValidCubeSphereCellId(const FSRPlanetSurfaceGridCellId& CellId, int32 Resolution)
{
	return CellId.IsValid(Resolution);
}

FText USRPlanetSurfaceGridLibrary::GetCubeSphereFaceText(ESRCubeSphereFace Face)
{
	switch (Face)
	{
	case ESRCubeSphereFace::PositiveX:
		return FText::FromString(TEXT("+X"));
	case ESRCubeSphereFace::NegativeX:
		return FText::FromString(TEXT("-X"));
	case ESRCubeSphereFace::PositiveY:
		return FText::FromString(TEXT("+Y"));
	case ESRCubeSphereFace::NegativeY:
		return FText::FromString(TEXT("-Y"));
	case ESRCubeSphereFace::PositiveZ:
		return FText::FromString(TEXT("+Z"));
	case ESRCubeSphereFace::NegativeZ:
	default:
		return FText::FromString(TEXT("-Z"));
	}
}

FVector USRPlanetSurfaceGridLibrary::GetCubeSphereDirection(ESRCubeSphereFace Face, float FaceU, float FaceV)
{
	return GetConformalCubeDirection(Face, FaceU, FaceV);
}

bool USRPlanetSurfaceGridLibrary::BuildCubeSphereCell(int32 Resolution, float Radius, const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCell& OutCell)
{
	OutCell = FSRPlanetSurfaceGridCell();
	if (Resolution <= 0 || Radius <= 0.0f || !CellId.IsValid(Resolution))
	{
		return false;
	}

	const FSRConformalCubeBaseCellGrid& BaseCellGrid = GetConformalBaseCellGrid(Resolution);
	const int32 CellIndex = BaseCellGrid.GetCellIndex(CellId);
	if (!BaseCellGrid.UnitCells.IsValidIndex(CellIndex))
	{
		return false;
	}

	OutCell = BaseCellGrid.UnitCells[CellIndex];
	ScaleBaseCell(OutCell, Radius);
	return true;
}

bool USRPlanetSurfaceGridLibrary::ProjectDirectionToCubeSphereCellId(const FVector& UnitDirection, int32 Resolution, FSRPlanetSurfaceGridCellId& OutCellId, FVector2D& OutFaceCoordinates)
{
	OutCellId = FSRPlanetSurfaceGridCellId();
	OutFaceCoordinates = FVector2D::ZeroVector;

	if (Resolution <= 0)
	{
		return false;
	}

	const FVector Direction = UnitDirection.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return false;
	}

	ESRCubeSphereFace Face = ESRCubeSphereFace::PositiveX;
	float MajorAxis = 0.0f;
	if (!DetermineFaceFromDirection(Direction, Face, MajorAxis))
	{
		return false;
	}

	const FVector CubePoint = Direction / MajorAxis;
	const FSRCubeSphereFaceBasis Basis = GetFaceBasis(Face);
	const float ProjectionU = FVector::DotProduct(CubePoint, Basis.AxisU);
	const float ProjectionV = FVector::DotProduct(CubePoint, Basis.AxisV);
	double InvertedFaceU = 0.0;
	double InvertedFaceV = 0.0;
	float FaceU = FMath::Clamp(ProjectionU, -1.0f, 1.0f);
	float FaceV = FMath::Clamp(ProjectionV, -1.0f, 1.0f);
	if (InvertCCAMPanelCoordinates(ProjectionU, ProjectionV, InvertedFaceU, InvertedFaceV))
	{
		FaceU = static_cast<float>(InvertedFaceU);
		FaceV = static_cast<float>(InvertedFaceV);
	}

	OutFaceCoordinates = FVector2D(FaceU, FaceV);
	OutCellId = BuildCellIdFromFaceCoordinates(Face, FaceU, FaceV, Resolution);
	return true;
}

FSRPlanetSurfaceGridCellNeighbors USRPlanetSurfaceGridLibrary::GetCubeSphereNeighborIds(const FSRPlanetSurfaceGridCellId& CellId, int32 Resolution)
{
	FSRPlanetSurfaceGridCellNeighbors Result;
	if (!CellId.IsValid(Resolution))
	{
		return Result;
	}

	Result.NegativeU = GetNeighborCellId(CellId, Resolution, -1.0f, 0.0f);
	Result.PositiveU = GetNeighborCellId(CellId, Resolution, 1.0f, 0.0f);
	Result.NegativeV = GetNeighborCellId(CellId, Resolution, 0.0f, -1.0f);
	Result.PositiveV = GetNeighborCellId(CellId, Resolution, 0.0f, 1.0f);
	return Result;
}

TArray<FSRPlanetSurfaceGridCell> USRPlanetSurfaceGridLibrary::GenerateCubeSphereCells(int32 Resolution, float Radius)
{
	TArray<FSRPlanetSurfaceGridCell> Cells;
	if (Resolution <= 0 || Radius <= 0.0f)
	{
		return Cells;
	}

	const FSRConformalCubeBaseCellGrid& BaseCellGrid = GetConformalBaseCellGrid(Resolution);
	Cells = BaseCellGrid.UnitCells;
	for (FSRPlanetSurfaceGridCell& Cell : Cells)
	{
		ScaleBaseCell(Cell, Radius);
	}

	return Cells;
}