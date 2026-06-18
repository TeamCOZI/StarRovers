#include "Surface/SRPlanetSurfaceGridCubeSphereHelpers.h"
#include "Math/UnrealMathUtility.h"
namespace StarRovers::SurfaceGrid::CubeSphere
{
FVector MatrixVectorMultiply(const double Matrix[3][3], const FVector& Vector)
{
	return FVector(
		Matrix[0][0] * Vector.X + Matrix[0][1] * Vector.Y + Matrix[0][2] * Vector.Z,
		Matrix[1][0] * Vector.X + Matrix[1][1] * Vector.Y + Matrix[1][2] * Vector.Z,
		Matrix[2][0] * Vector.X + Matrix[2][1] * Vector.Y + Matrix[2][2] * Vector.Z);
}

bool InvertMatrix3x3(double Matrix[3][3])
{
	const double A00 = Matrix[0][0];
	const double A01 = Matrix[0][1];
	const double A02 = Matrix[0][2];
	const double A10 = Matrix[1][0];
	const double A11 = Matrix[1][1];
	const double A12 = Matrix[1][2];
	const double A20 = Matrix[2][0];
	const double A21 = Matrix[2][1];
	const double A22 = Matrix[2][2];

	const double Det =
		A00 * (A11 * A22 - A12 * A21)
		- A01 * (A10 * A22 - A12 * A20)
		+ A02 * (A10 * A21 - A11 * A20);
	if (FMath::Abs(Det) <= UE_DOUBLE_SMALL_NUMBER)
	{
		return false;
	}

	const double InvDet = 1.0 / Det;
	Matrix[0][0] = (A11 * A22 - A12 * A21) * InvDet;
	Matrix[0][1] = (A02 * A21 - A01 * A22) * InvDet;
	Matrix[0][2] = (A01 * A12 - A02 * A11) * InvDet;
	Matrix[1][0] = (A12 * A20 - A10 * A22) * InvDet;
	Matrix[1][1] = (A00 * A22 - A02 * A20) * InvDet;
	Matrix[1][2] = (A02 * A10 - A00 * A12) * InvDet;
	Matrix[2][0] = (A10 * A21 - A11 * A20) * InvDet;
	Matrix[2][1] = (A01 * A20 - A00 * A21) * InvDet;
	Matrix[2][2] = (A00 * A11 - A01 * A10) * InvDet;
	return true;
}

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

FSRCCAMGridTransform BuildCCAMGridTransform()
{
	FSRCCAMGridTransform Transform;

	const int32 KDA[48] =
	{
		0, 1, 1, 2, 2, 0, 0, 1, 1, 5, 5, 0,
		3, 1, 1, 2, 2, 3, 3, 1, 1, 5, 5, 3,
		0, 4, 4, 2, 2, 0, 0, 4, 4, 5, 5, 0,
		3, 4, 4, 2, 2, 3, 3, 4, 4, 5, 5, 3
	};
	const int32 KDB[48] =
	{
		3, 10, 4, 11, 5, 9, 6, 7, 1, 11, 5, 0,
		3, 1, 7, 8, 2, 9, 6, 4, 10, 8, 2, 0,
		0, 10, 4, 2, 8, 6, 9, 7, 1, 2, 8, 3,
		0, 1, 7, 5, 11, 6, 9, 4, 10, 5, 11, 3
	};
	const int32 KDC[48] =
	{
		5, 11, 3, 9, 4, 10, 5, 11, 6, 0, 1, 7,
		2, 8, 3, 9, 7, 1, 2, 8, 6, 0, 10, 4,
		8, 2, 0, 6, 4, 10, 8, 2, 9, 3, 1, 7,
		11, 5, 0, 6, 7, 1, 11, 5, 9, 3, 10, 4
	};
	const int32 KNA[48] =
	{
		12, 25, 26, 9, 10, 17, 18, 31, 32, 3, 4, 23,
		0, 37, 38, 21, 22, 5, 6, 43, 44, 15, 16, 11,
		36, 1, 2, 33, 34, 41, 42, 7, 8, 27, 28, 47,
		24, 13, 14, 45, 46, 29, 30, 19, 20, 39, 40, 35
	};
	const int32 KNC[48] =
	{
		1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10,
		13, 12, 15, 14, 17, 16, 19, 18, 21, 20, 23, 22,
		25, 24, 27, 26, 29, 28, 31, 30, 33, 32, 35, 34,
		37, 36, 39, 38, 41, 40, 43, 42, 45, 44, 47, 46
	};
	const int32 IG1[6] = { 22, 13, 40, 43, 9, 45 };

	for (int32 PanelIndex = 0; PanelIndex < 6; ++PanelIndex)
	{
		int32 IgK[8] = {};
		IgK[0] = IG1[PanelIndex];
		IgK[1] = KNA[IgK[0]];
		IgK[4] = KNC[IgK[0]];
		IgK[5] = KNC[IgK[1]];
		IgK[6] = KNA[IgK[4]];
		IgK[7] = KNA[IgK[5]];
		IgK[2] = KNC[IgK[6]];
		IgK[3] = KNC[IgK[7]];
		for (int32 KgIndex = 0; KgIndex < 8; ++KgIndex)
		{
			Transform.IgOfKg[KgIndex][PanelIndex] = IgK[KgIndex];
		}
	}

	const double R2 = FMath::Sqrt(2.0);
	const double R3 = FMath::Sqrt(3.0);
	const double R6 = R2 * R3;
	const double R2O2 = R2 / 2.0;
	const double R3O2 = R3 / 2.0;
	const double R3O3 = R3 / 3.0;
	const double R6O3 = R6 / 3.0;
	const double R6O6 = R6 / 6.0;
	const double R3O6 = R3 / 6.0;

	double Rotation[3][3] =
	{
		{ R6 / 6.0, R2 / 2.0, R3 / 3.0 },
		{ -R6 / 6.0, R2 / 2.0, -R3 / 3.0 },
		{ -R6 / 3.0, 0.0, R3 / 3.0 }
	};

	double F[3][6] = {};
	double E[3][12] = {};
	F[0][0] = -R6O3; F[1][0] = 0.0; F[2][0] = R3O3;
	F[0][1] = R6O6; F[1][1] = -R2O2; F[2][1] = R3O3;
	F[0][2] = R6O6; F[1][2] = R2O2; F[2][2] = R3O3;
	E[0][0] = R3O3; E[1][0] = 0.0; E[2][0] = R6O3;
	E[0][1] = -R3O6; E[1][1] = 0.5; E[2][1] = R6O3;
	E[0][2] = -R3O6; E[1][2] = -0.5; E[2][2] = R6O3;
	E[0][3] = 0.0; E[1][3] = 1.0; E[2][3] = 0.0;
	E[0][4] = -R3O2; E[1][4] = -0.5; E[2][4] = 0.0;
	E[0][5] = R3O2; E[1][5] = -0.5; E[2][5] = 0.0;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			F[Axis][Index + 3] = -F[Axis][Index];
		}
	}
	for (int32 Index = 0; Index < 6; ++Index)
	{
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			E[Axis][Index + 6] = -E[Axis][Index];
		}
	}

	double Txe[3][3] =
	{
		{ F[0][0], F[1][0], F[2][0] },
		{ E[0][3], E[1][3], E[2][3] },
		{ E[0][5], E[1][5], E[2][5] }
	};
	InvertMatrix3x3(Txe);

	double RotatedF[3][6] = {};
	double RotatedE[3][12] = {};
	for (int32 Index = 0; Index < 6; ++Index)
	{
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			RotatedF[Axis][Index] = Rotation[Axis][0] * F[0][Index] + Rotation[Axis][1] * F[1][Index] + Rotation[Axis][2] * F[2][Index];
		}
	}
	for (int32 Index = 0; Index < 12; ++Index)
	{
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			RotatedE[Axis][Index] = Rotation[Axis][0] * E[0][Index] + Rotation[Axis][1] * E[1][Index] + Rotation[Axis][2] * E[2][Index];
		}
	}

	for (int32 Lg = 0; Lg < 48; ++Lg)
	{
		for (int32 Row = 0; Row < 3; ++Row)
		{
			for (int32 Column = 0; Column < 3; ++Column)
			{
				Transform.RotG[Lg][Row][Column] =
					RotatedF[Row][KDA[Lg]] * Txe[Column][0]
					+ RotatedE[Row][KDB[Lg]] * Txe[Column][1]
					+ RotatedE[Row][KDC[Lg]] * Txe[Column][2];
			}
		}
	}

	Transform.bInitialized = true;
	return Transform;
}

const FSRCCAMGridTransform& GetCCAMGridTransform()
{
	static const FSRCCAMGridTransform Transform = BuildCCAMGridTransform();
	return Transform;
}

FSRComplex EvaluateCCAMTaylor(const FSRComplex& Z)
{
	static constexpr double Coefficients[30] =
	{
		1.47713062600964, -0.38183510510174, -0.05573058001191,
		-0.00895883606818, -0.00791315785221, -0.00486625437708,
		-0.00329251751279, -0.00235481488325, -0.00175870527475,
		-0.00135681133278, -0.00107459847699, -0.00086944475948,
		-0.00071607115121, -0.00059867100093, -0.00050699063239,
		-0.00043415191279, -0.00037541003286, -0.00032741060100,
		-0.00028773091482, -0.00025458777519, -0.00022664642371,
		-0.00020289261022, -0.00018254510830, -0.00016499474460,
		-0.00014976117167, -0.00013646173947, -0.00012478875822,
		-0.00011449267279, -0.00010536946150, -0.00009725109376
	};

	FSRComplex W(0.0, 0.0);
	for (int32 Index = 29; Index >= 0; --Index)
	{
		W = (W + FSRComplex(Coefficients[Index], 0.0)) * Z;
	}
	return W;
}

bool GetCCAMPanelCoordinates(float FaceU, float FaceV, double& OutX, double& OutY)
{
	double X = (static_cast<double>(FMath::Clamp(FaceU, -1.0f, 1.0f)) + 1.0) * 0.5;
	double Y = (static_cast<double>(FMath::Clamp(FaceV, -1.0f, 1.0f)) + 1.0) * 0.5;
	int32 Kg = 0;
	if (X > 0.5)
	{
		Kg += 1;
		X = 1.0 - X;
	}
	if (Y > 0.5)
	{
		Kg += 2;
		Y = 1.0 - Y;
	}
	if (Y > X)
	{
		Kg += 4;
		Swap(X, Y);
	}

	FSRComplex Z(X, Y);
	Z = Z * Z;
	Z = Z * Z;
	const FSRComplex W = EvaluateCCAMTaylor(Z);
	const FSRComplex MinusIW(W.Imaginary, -W.Real);
	FSRComplex Projected(0.0, 0.0);
	if (AbsSquared(MinusIW) > 1.e-40)
	{
		constexpr double SqrtTwo = 1.41421356237309504880;
		const FSRComplex CiToThird(FMath::Cos(PI / 6.0), FMath::Sin(PI / 6.0));
		Projected = CiToThird * ComplexPow(MinusIW, 1.0 / 3.0) / SqrtTwo;
	}

	const double XW = Projected.Real;
	const double YW = Projected.Imaginary;
	const double H = 2.0 / (1.0 + XW * XW + YW * YW);
	const FVector Xv(XW * H, YW * H, H - 1.0);
	const FSRCCAMGridTransform& Transform = GetCCAMGridTransform();
	constexpr int32 CCAMPanelIndex = 1; // Fortran ipanel = 2.
	const int32 Ig = Transform.IgOfKg[Kg][CCAMPanelIndex];
	const FVector PanelPoint = MatrixVectorMultiply(Transform.RotG[Ig], Xv);
	if (FMath::Abs(PanelPoint.X) <= UE_DOUBLE_SMALL_NUMBER)
	{
		OutX = 0.0;
		OutY = 0.0;
		return false;
	}

	OutX = PanelPoint.Y / PanelPoint.X;
	OutY = PanelPoint.Z / PanelPoint.X;
	return true;
}

bool InvertCCAMPanelCoordinates(double TargetX, double TargetY, double& OutFaceU, double& OutFaceV)
{
	TargetX = FMath::Clamp(TargetX, -1.0, 1.0);
	TargetY = FMath::Clamp(TargetY, -1.0, 1.0);

	double FaceU = TargetX;
	double FaceV = TargetY;
	double BestFaceU = FaceU;
	double BestFaceV = FaceV;
	double BestErrorSquared = TNumericLimits<double>::Max();

	auto EvaluateResidual = [TargetX, TargetY](double TestFaceU, double TestFaceV, double& OutResidualX, double& OutResidualY)
	{
		double ProjectedX = 0.0;
		double ProjectedY = 0.0;
		if (!GetCCAMPanelCoordinates(static_cast<float>(TestFaceU), static_cast<float>(TestFaceV), ProjectedX, ProjectedY))
		{
			return false;
		}

		OutResidualX = ProjectedX - TargetX;
		OutResidualY = ProjectedY - TargetY;
		return true;
	};

	for (int32 Iteration = 0; Iteration < 12; ++Iteration)
	{
		double ResidualX = 0.0;
		double ResidualY = 0.0;
		if (!EvaluateResidual(FaceU, FaceV, ResidualX, ResidualY))
		{
			break;
		}

		const double ErrorSquared = (ResidualX * ResidualX) + (ResidualY * ResidualY);
		if (ErrorSquared < BestErrorSquared)
		{
			BestErrorSquared = ErrorSquared;
			BestFaceU = FaceU;
			BestFaceV = FaceV;
		}
		if (ErrorSquared <= 1.e-14)
		{
			OutFaceU = FaceU;
			OutFaceV = FaceV;
			return true;
		}

		constexpr double DerivativeStep = 1.e-4;
		const double U0 = FMath::Clamp(FaceU - DerivativeStep, -1.0, 1.0);
		const double U1 = FMath::Clamp(FaceU + DerivativeStep, -1.0, 1.0);
		const double V0 = FMath::Clamp(FaceV - DerivativeStep, -1.0, 1.0);
		const double V1 = FMath::Clamp(FaceV + DerivativeStep, -1.0, 1.0);
		if (FMath::Abs(U1 - U0) <= UE_DOUBLE_SMALL_NUMBER || FMath::Abs(V1 - V0) <= UE_DOUBLE_SMALL_NUMBER)
		{
			break;
		}

		double ResidualUX0 = 0.0;
		double ResidualUY0 = 0.0;
		double ResidualUX1 = 0.0;
		double ResidualUY1 = 0.0;
		double ResidualVX0 = 0.0;
		double ResidualVY0 = 0.0;
		double ResidualVX1 = 0.0;
		double ResidualVY1 = 0.0;
		if (!EvaluateResidual(U0, FaceV, ResidualUX0, ResidualUY0)
			|| !EvaluateResidual(U1, FaceV, ResidualUX1, ResidualUY1)
			|| !EvaluateResidual(FaceU, V0, ResidualVX0, ResidualVY0)
			|| !EvaluateResidual(FaceU, V1, ResidualVX1, ResidualVY1))
		{
			break;
		}

		const double J00 = (ResidualUX1 - ResidualUX0) / (U1 - U0);
		const double J10 = (ResidualUY1 - ResidualUY0) / (U1 - U0);
		const double J01 = (ResidualVX1 - ResidualVX0) / (V1 - V0);
		const double J11 = (ResidualVY1 - ResidualVY0) / (V1 - V0);
		const double Det = (J00 * J11) - (J01 * J10);
		if (FMath::Abs(Det) <= 1.e-12)
		{
			break;
		}

		double DeltaU = ((J11 * ResidualX) - (J01 * ResidualY)) / Det;
		double DeltaV = ((J00 * ResidualY) - (J10 * ResidualX)) / Det;
		DeltaU = FMath::Clamp(DeltaU, -0.25, 0.25);
		DeltaV = FMath::Clamp(DeltaV, -0.25, 0.25);

		const double NextFaceU = FMath::Clamp(FaceU - DeltaU, -1.0, 1.0);
		const double NextFaceV = FMath::Clamp(FaceV - DeltaV, -1.0, 1.0);
		if (FMath::Abs(NextFaceU - FaceU) <= 1.e-10 && FMath::Abs(NextFaceV - FaceV) <= 1.e-10)
		{
			break;
		}

		FaceU = NextFaceU;
		FaceV = NextFaceV;
	}

	OutFaceU = BestFaceU;
	OutFaceV = BestFaceV;
	return BestErrorSquared < TNumericLimits<double>::Max();
}

FVector GetConformalCubeDirection(ESRCubeSphereFace Face, float FaceU, float FaceV)
{
	double Xx = 0.0;
	double Yy = 0.0;
	if (!GetCCAMPanelCoordinates(FaceU, FaceV, Xx, Yy))
	{
		return BuildCubePoint(Face, FaceU, FaceV).GetSafeNormal();
	}

	return BuildCubePoint(Face, static_cast<float>(Xx), static_cast<float>(Yy)).GetSafeNormal();
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
}