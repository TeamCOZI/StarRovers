#include "Surface/SRPlanetSurfaceGridLibrary.h"

#include "Math/UnrealMathUtility.h"

namespace
{
	struct FSRCubeSphereFaceBasis
	{
		FVector Normal;
		FVector AxisU;
		FVector AxisV;
	};

	FSRCubeSphereFaceBasis GetFaceBasis(ESRCubeSphereFace Face)
	{
		switch (Face)
		{
		case ESRCubeSphereFace::PositiveX:
			return { FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f) };
		case ESRCubeSphereFace::NegativeX:
			return { FVector(-1.0f, 0.0f, 0.0f), FVector(0.0f, -1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f) };
		case ESRCubeSphereFace::PositiveY:
			return { FVector(0.0f, 1.0f, 0.0f), FVector(-1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f) };
		case ESRCubeSphereFace::NegativeY:
			return { FVector(0.0f, -1.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f) };
		case ESRCubeSphereFace::PositiveZ:
			return { FVector(0.0f, 0.0f, 1.0f), FVector(0.0f, 1.0f, 0.0f), FVector(-1.0f, 0.0f, 0.0f) };
		case ESRCubeSphereFace::NegativeZ:
		default:
			return { FVector(0.0f, 0.0f, -1.0f), FVector(0.0f, 1.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f) };
		}
	}

	FVector BuildCubePoint(ESRCubeSphereFace Face, float FaceU, float FaceV)
	{
		const FSRCubeSphereFaceBasis Basis = GetFaceBasis(Face);
		return Basis.Normal + (Basis.AxisU * FaceU) + (Basis.AxisV * FaceV);
	}

	FVector SpherifyCubePoint(const FVector& CubePoint)
	{
		const double X = CubePoint.X;
		const double Y = CubePoint.Y;
		const double Z = CubePoint.Z;
		const double X2 = X * X;
		const double Y2 = Y * Y;
		const double Z2 = Z * Z;

		return FVector(
			static_cast<float>(X * FMath::Sqrt(FMath::Max(0.0, 1.0 - (Y2 * 0.5) - (Z2 * 0.5) + ((Y2 * Z2) / 3.0)))),
			static_cast<float>(Y * FMath::Sqrt(FMath::Max(0.0, 1.0 - (Z2 * 0.5) - (X2 * 0.5) + ((Z2 * X2) / 3.0)))),
			static_cast<float>(Z * FMath::Sqrt(FMath::Max(0.0, 1.0 - (X2 * 0.5) - (Y2 * 0.5) + ((X2 * Y2) / 3.0)))));
	}

	float GetCellStep(int32 Resolution)
	{
		return 2.0f / static_cast<float>(Resolution);
	}

	float GetFaceCoordinateMin(int32 CellIndex, int32 Resolution)
	{
		return -1.0f + (GetCellStep(Resolution) * static_cast<float>(CellIndex));
	}

	float GetFaceCoordinateMax(int32 CellIndex, int32 Resolution)
	{
		return GetFaceCoordinateMin(CellIndex, Resolution) + GetCellStep(Resolution);
	}

	float GetFaceCoordinateCenter(int32 CellIndex, int32 Resolution)
	{
		return GetFaceCoordinateMin(CellIndex, Resolution) + (GetCellStep(Resolution) * 0.5f);
	}

	float ComputeQuadArea(const FVector& Corner00, const FVector& Corner10, const FVector& Corner11, const FVector& Corner01)
	{
		const float TriangleA = FVector::CrossProduct(Corner10 - Corner00, Corner11 - Corner00).Size() * 0.5f;
		const float TriangleB = FVector::CrossProduct(Corner11 - Corner00, Corner01 - Corner00).Size() * 0.5f;
		return TriangleA + TriangleB;
	}

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

	FSRPlanetSurfaceGridCellId BuildProjectedCellId(ESRCubeSphereFace Face, float FaceU, float FaceV, int32 Resolution)
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
		const FVector ProbeDirection = BuildCubePoint(CellId.Face, FaceU, FaceV).GetSafeNormal();

		FVector2D UnusedFaceCoordinates = FVector2D::ZeroVector;
		USRPlanetSurfaceGridLibrary::ProjectDirectionToCubeSphereCellId(ProbeDirection, Resolution, Result, UnusedFaceCoordinates);
		return Result;
	}
}

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

FVector USRPlanetSurfaceGridLibrary::GetSpherifiedCubeDirection(ESRCubeSphereFace Face, float FaceU, float FaceV)
{
	return SpherifyCubePoint(BuildCubePoint(Face, FaceU, FaceV)).GetSafeNormal();
}

bool USRPlanetSurfaceGridLibrary::BuildCubeSphereCell(int32 Resolution, float Radius, const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCell& OutCell)
{
	OutCell = FSRPlanetSurfaceGridCell();
	if (Resolution <= 0 || Radius <= 0.0f || !CellId.IsValid(Resolution))
	{
		return false;
	}

	const float MinU = GetFaceCoordinateMin(CellId.CellX, Resolution);
	const float MaxU = GetFaceCoordinateMax(CellId.CellX, Resolution);
	const float MinV = GetFaceCoordinateMin(CellId.CellY, Resolution);
	const float MaxV = GetFaceCoordinateMax(CellId.CellY, Resolution);
	const float CenterU = GetFaceCoordinateCenter(CellId.CellX, Resolution);
	const float CenterV = GetFaceCoordinateCenter(CellId.CellY, Resolution);

	OutCell.CellId = CellId;
	OutCell.FaceUVMin = FVector2D(MinU, MinV);
	OutCell.FaceUVMax = FVector2D(MaxU, MaxV);
	OutCell.LocalCenter = GetSpherifiedCubeDirection(CellId.Face, CenterU, CenterV) * Radius;
	OutCell.LocalNormal = OutCell.LocalCenter.GetSafeNormal();
	OutCell.Corner00 = GetSpherifiedCubeDirection(CellId.Face, MinU, MinV) * Radius;
	OutCell.Corner10 = GetSpherifiedCubeDirection(CellId.Face, MaxU, MinV) * Radius;
	OutCell.Corner11 = GetSpherifiedCubeDirection(CellId.Face, MaxU, MaxV) * Radius;
	OutCell.Corner01 = GetSpherifiedCubeDirection(CellId.Face, MinU, MaxV) * Radius;
	OutCell.ApproxSurfaceArea = ComputeQuadArea(OutCell.Corner00, OutCell.Corner10, OutCell.Corner11, OutCell.Corner01);
	OutCell.Neighbors = GetCubeSphereNeighborIds(CellId, Resolution);
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
	const float FaceU = FMath::Clamp(FVector::DotProduct(CubePoint, Basis.AxisU), -1.0f, 1.0f);
	const float FaceV = FMath::Clamp(FVector::DotProduct(CubePoint, Basis.AxisV), -1.0f, 1.0f);

	OutFaceCoordinates = FVector2D(FaceU, FaceV);
	OutCellId = BuildProjectedCellId(Face, FaceU, FaceV, Resolution);
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

	Cells.Reserve(6 * Resolution * Resolution);

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

				if (BuildCubeSphereCell(Resolution, Radius, CellId, Cell))
				{
					Cells.Add(Cell);
				}
			}
		}
	}

	return Cells;
}
