#include "SRPlanetSurfaceGridSurfaceGeometry.h"

FVector StarRovers::SurfaceGridSurfaceGeometry::ResolveLocalSurfacePoint(
	const FVector& LocalUnitDirection,
	float PlanetRadius,
	bool bUseProceduralSurfaceNormal,
	FSurfaceHeightSampler SampleSurfaceHeightOffset,
	float HeightOffset)
{
	const FVector LocalDirection = LocalUnitDirection.GetSafeNormal();
	if (LocalDirection.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	const float SurfaceHeightOffset = SampleSurfaceHeightOffset(LocalDirection);
	const FVector LocalBasePoint = LocalDirection * FMath::Max(1.0f, PlanetRadius + SurfaceHeightOffset);
	const FVector LocalSurfaceNormal = bUseProceduralSurfaceNormal
		? ComputeProceduralSurfaceNormal(LocalDirection, PlanetRadius, SampleSurfaceHeightOffset)
		: LocalDirection;
	return LocalBasePoint + (LocalSurfaceNormal.GetSafeNormal() * HeightOffset);
}

FVector StarRovers::SurfaceGridSurfaceGeometry::ResolveWorldSurfacePoint(
	const FVector& LocalUnitDirection,
	const FTransform& ComponentTransform,
	float PlanetRadius,
	bool bUseProceduralSurfaceNormal,
	FSurfaceHeightSampler SampleSurfaceHeightOffset,
	float HeightOffset)
{
	return ComponentTransform.TransformPosition(ResolveLocalSurfacePoint(
		LocalUnitDirection,
		PlanetRadius,
		bUseProceduralSurfaceNormal,
		SampleSurfaceHeightOffset,
		HeightOffset));
}

FVector StarRovers::SurfaceGridSurfaceGeometry::ComputeProceduralSurfaceNormal(
	FVector LocalUnitDirection,
	float PlanetRadius,
	FSurfaceHeightSampler SampleSurfaceHeightOffset)
{
	const FVector Direction = LocalUnitDirection.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return FVector::UpVector;
	}

	FVector TangentA = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
	if (TangentA.IsNearlyZero())
	{
		TangentA = FVector::CrossProduct(Direction, FVector::RightVector).GetSafeNormal();
	}
	const FVector TangentB = FVector::CrossProduct(Direction, TangentA).GetSafeNormal();
	if (TangentA.IsNearlyZero() || TangentB.IsNearlyZero())
	{
		return Direction;
	}

	auto ResolveBasePoint = [PlanetRadius, SampleSurfaceHeightOffset](const FVector& SampleDirection)
	{
		const FVector SafeDirection = SampleDirection.GetSafeNormal();
		const float SurfaceHeightOffset = SampleSurfaceHeightOffset(SafeDirection);
		return SafeDirection * FMath::Max(1.0f, PlanetRadius + SurfaceHeightOffset);
	};

	constexpr float NormalSampleStep = 0.003f;
	const FVector PointA0 = ResolveBasePoint(Direction - TangentA * NormalSampleStep);
	const FVector PointA1 = ResolveBasePoint(Direction + TangentA * NormalSampleStep);
	const FVector PointB0 = ResolveBasePoint(Direction - TangentB * NormalSampleStep);
	const FVector PointB1 = ResolveBasePoint(Direction + TangentB * NormalSampleStep);

	FVector Normal = FVector::CrossProduct(PointA1 - PointA0, PointB1 - PointB0).GetSafeNormal();
	if (Normal.IsNearlyZero())
	{
		return Direction;
	}

	if (FVector::DotProduct(Normal, Direction) < 0.0f)
	{
		Normal *= -1.0f;
	}
	return Normal;
}
