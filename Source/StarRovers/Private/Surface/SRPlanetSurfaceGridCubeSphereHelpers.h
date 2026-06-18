#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"

namespace StarRovers::SurfaceGrid::CubeSphere
{
	struct FSRCubeSphereFaceBasis
	{
		FVector Normal = FVector::ForwardVector;
		FVector AxisU = FVector::RightVector;
		FVector AxisV = FVector::UpVector;
	};

	struct FSRComplex
	{
		double Real = 0.0;
		double Imaginary = 0.0;

		FSRComplex() = default;
		FSRComplex(double InReal, double InImaginary)
			: Real(InReal)
			, Imaginary(InImaginary)
		{
		}
	};

	inline FSRComplex operator+(const FSRComplex& Left, const FSRComplex& Right)
	{
		return FSRComplex(Left.Real + Right.Real, Left.Imaginary + Right.Imaginary);
	}

	inline FSRComplex operator-(const FSRComplex& Left, const FSRComplex& Right)
	{
		return FSRComplex(Left.Real - Right.Real, Left.Imaginary - Right.Imaginary);
	}

	inline FSRComplex operator*(const FSRComplex& Left, const FSRComplex& Right)
	{
		return FSRComplex(
			Left.Real * Right.Real - Left.Imaginary * Right.Imaginary,
			Left.Real * Right.Imaginary + Left.Imaginary * Right.Real);
	}

	inline FSRComplex operator*(const FSRComplex& Value, double Scalar)
	{
		return FSRComplex(Value.Real * Scalar, Value.Imaginary * Scalar);
	}

	inline FSRComplex operator/(const FSRComplex& Value, double Scalar)
	{
		return FSRComplex(Value.Real / Scalar, Value.Imaginary / Scalar);
	}

	inline double AbsSquared(const FSRComplex& Value)
	{
		return Value.Real * Value.Real + Value.Imaginary * Value.Imaginary;
	}

	inline FSRComplex ComplexPow(const FSRComplex& Base, double Exponent)
	{
		const double Radius = FMath::Sqrt(AbsSquared(Base));
		const double Angle = FMath::Atan2(Base.Imaginary, Base.Real);
		const double PoweredRadius = FMath::Pow(Radius, Exponent);
		const double PoweredAngle = Angle * Exponent;
		return FSRComplex(PoweredRadius * FMath::Cos(PoweredAngle), PoweredRadius * FMath::Sin(PoweredAngle));
	}

	struct FSRConformalCubeVertexDirectionGrid
	{
		int32 Resolution = 0;
		int32 VertexResolution = 0;
		TArray<FVector> Directions;

		int32 GetDirectionIndex(ESRCubeSphereFace Face, int32 VertexX, int32 VertexY) const
		{
			const int32 FaceIndex = static_cast<int32>(Face);
			const int32 FaceVertexCount = VertexResolution * VertexResolution;
			return (FaceIndex * FaceVertexCount) + (VertexY * VertexResolution) + VertexX;
		}

		const FVector& GetDirection(ESRCubeSphereFace Face, int32 VertexX, int32 VertexY) const
		{
			return Directions[GetDirectionIndex(Face, VertexX, VertexY)];
		}
	};

	struct FSRCCAMGridTransform
	{
		int32 IgOfKg[8][6] = {};
		double RotG[48][3][3] = {};
		bool bInitialized = false;
	};

	struct FSRConformalCubeBaseCellGrid
	{
		int32 Resolution = 0;
		TArray<FSRPlanetSurfaceGridCell> UnitCells;

		int32 GetCellIndex(const FSRPlanetSurfaceGridCellId& CellId) const
		{
			return ((static_cast<int32>(CellId.Face) * Resolution) + CellId.CellY) * Resolution + CellId.CellX;
		}
	};

	FSRCubeSphereFaceBasis GetFaceBasis(ESRCubeSphereFace Face);
	FVector BuildCubePoint(ESRCubeSphereFace Face, float FaceU, float FaceV);
	FVector GetConformalCubeDirection(ESRCubeSphereFace Face, float FaceU, float FaceV);
	bool InvertCCAMPanelCoordinates(double TargetX, double TargetY, double& OutFaceU, double& OutFaceV);

	float GetCellStep(int32 Resolution);
	float GetFaceCoordinateMin(int32 CellIndex, int32 Resolution);
	float GetFaceCoordinateMax(int32 CellIndex, int32 Resolution);
	float GetFaceCoordinateCenter(int32 CellIndex, int32 Resolution);
	float ComputeQuadArea(const FVector& A, const FVector& B, const FVector& C, const FVector& D);

	bool DetermineFaceFromDirection(const FVector& Direction, ESRCubeSphereFace& OutFace, float& OutMajorAxis);
	FSRPlanetSurfaceGridCellId BuildCellIdFromFaceCoordinates(ESRCubeSphereFace Face, float FaceU, float FaceV, int32 Resolution);
	FSRPlanetSurfaceGridCellId GetNeighborCellId(const FSRPlanetSurfaceGridCellId& CellId, int32 Resolution, float DeltaUCells, float DeltaVCells);

	const FSRConformalCubeBaseCellGrid& GetConformalBaseCellGrid(int32 Resolution);
	void ScaleBaseCell(FSRPlanetSurfaceGridCell& Cell, float Radius);
}
