#include "Surface/SRPlanetSurfaceGridCubeSphereHelpers.h"
#include "Surface/SRPlanetSurfaceGridLibrary.h"
namespace StarRovers::SurfaceGrid::CubeSphere
{
bool DetermineFaceFromDirection(const FVector& Direction, ESRCubeSphereFace& OutFace, float& OutMajorAxis)
{
	const FVector AbsDirection = Direction.GetAbs();
	OutMajorAxis = FMath::Max3(AbsDirection.X, AbsDirection.Y, AbsDirection.Z);
	if (OutMajorAxis <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	if (AbsDirection.X >= AbsDirection.Y && AbsDirection.X >= AbsDirection.Z)
	{
		OutFace = Direction.X >= 0.0f ? ESRCubeSphereFace::PositiveX : ESRCubeSphereFace::NegativeX;
		return true;
	}

	if (AbsDirection.Y >= AbsDirection.X && AbsDirection.Y >= AbsDirection.Z)
	{
		OutFace = Direction.Y >= 0.0f ? ESRCubeSphereFace::PositiveY : ESRCubeSphereFace::NegativeY;
		return true;
	}

	OutFace = Direction.Z >= 0.0f ? ESRCubeSphereFace::PositiveZ : ESRCubeSphereFace::NegativeZ;
	return true;
}

FSRPlanetSurfaceGridCellId BuildCellIdFromFaceCoordinates(ESRCubeSphereFace Face, float FaceU, float FaceV, int32 Resolution)
{
	const float NormalizedU = FMath::Clamp((FaceU + 1.0f) * 0.5f, 0.0f, 0.999999f);
	const float NormalizedV = FMath::Clamp((FaceV + 1.0f) * 0.5f, 0.0f, 0.999999f);

	FSRPlanetSurfaceGridCellId Result;
	Result.Face = Face;
	Result.CellX = FMath::Clamp(FMath::FloorToInt(NormalizedU * Resolution), 0, Resolution - 1);
	Result.CellY = FMath::Clamp(FMath::FloorToInt(NormalizedV * Resolution), 0, Resolution - 1);
	return Result;
}

FSRPlanetSurfaceGridCellId GetNeighborCellId(const FSRPlanetSurfaceGridCellId& CellId, int32 Resolution, float DeltaUCells, float DeltaVCells)
{
	FSRPlanetSurfaceGridCellId Result = CellId;
	if (!Result.IsValid(Resolution))
	{
		return Result;
	}

	const float FaceU = GetFaceCoordinateCenter(CellId.CellX, Resolution) + (GetCellStep(Resolution) * DeltaUCells);
	const float FaceV = GetFaceCoordinateCenter(CellId.CellY, Resolution) + (GetCellStep(Resolution) * DeltaVCells);
	const FVector ProbeDirection = GetConformalCubeDirection(CellId.Face, FaceU, FaceV);

	FVector2D UnusedFaceCoordinates = FVector2D::ZeroVector;
	USRPlanetSurfaceGridLibrary::ProjectDirectionToCubeSphereCellId(ProbeDirection, Resolution, Result, UnusedFaceCoordinates);
	return Result;
}
}