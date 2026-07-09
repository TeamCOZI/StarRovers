#include "SRPlanetSurfaceGridRaycastQuery.h"

namespace
{
	bool IntersectRayTriangle(
		const FVector& RayOrigin,
		const FVector& RayDirection,
		const FVector& TriangleA,
		const FVector& TriangleB,
		const FVector& TriangleC,
		float& OutHitDistance)
	{
		OutHitDistance = 0.0f;

		const FVector Edge1 = TriangleB - TriangleA;
		const FVector Edge2 = TriangleC - TriangleA;
		const FVector P = FVector::CrossProduct(RayDirection, Edge2);
		const float Determinant = FVector::DotProduct(Edge1, P);
		if (FMath::Abs(Determinant) <= UE_SMALL_NUMBER)
		{
			return false;
		}

		const float InvDeterminant = 1.0f / Determinant;
		const FVector T = RayOrigin - TriangleA;
		const float U = FVector::DotProduct(T, P) * InvDeterminant;
		if (U < 0.0f || U > 1.0f)
		{
			return false;
		}

		const FVector Q = FVector::CrossProduct(T, Edge1);
		const float V = FVector::DotProduct(RayDirection, Q) * InvDeterminant;
		if (V < 0.0f || U + V > 1.0f)
		{
			return false;
		}

		const float HitDistance = FVector::DotProduct(Edge2, Q) * InvDeterminant;
		if (HitDistance < 0.0f)
		{
			return false;
		}

		OutHitDistance = HitDistance;
		return true;
	}

	bool IntersectRayBox(const FVector& RayOrigin, const FVector& RayDirection, const FBox& Box, float& OutHitDistance)
	{
		OutHitDistance = 0.0f;
		if (!Box.IsValid)
		{
			return false;
		}

		float MinDistance = 0.0f;
		float MaxDistance = BIG_NUMBER;

		auto ClipAxis = [&MinDistance, &MaxDistance](float Origin, float Direction, float MinValue, float MaxValue)
		{
			if (FMath::Abs(Direction) <= UE_SMALL_NUMBER)
			{
				return Origin >= MinValue && Origin <= MaxValue;
			}

			float Distance0 = (MinValue - Origin) / Direction;
			float Distance1 = (MaxValue - Origin) / Direction;
			if (Distance0 > Distance1)
			{
				Swap(Distance0, Distance1);
			}

			MinDistance = FMath::Max(MinDistance, Distance0);
			MaxDistance = FMath::Min(MaxDistance, Distance1);
			return MinDistance <= MaxDistance;
		};

		if (!ClipAxis(RayOrigin.X, RayDirection.X, Box.Min.X, Box.Max.X)
			|| !ClipAxis(RayOrigin.Y, RayDirection.Y, Box.Min.Y, Box.Max.Y)
			|| !ClipAxis(RayOrigin.Z, RayDirection.Z, Box.Min.Z, Box.Max.Z)
			|| MaxDistance < 0.0f)
		{
			return false;
		}

		OutHitDistance = FMath::Max(0.0f, MinDistance);
		return true;
	}

	struct FSRPlanetSurfaceGridRaycastStats
	{
		int32 BucketTests = 0;
		int32 BucketHits = 0;
		int32 BucketSkippedByBestHit = 0;
		int32 CellTests = 0;
		int32 TopTriangleTests = 0;
		int32 SideTriangleTests = 0;
		int32 TriangleHits = 0;
		int32 BestHitUpdates = 0;
	};

	constexpr int32 SurfaceGridRaycastBucketResolution = 16;
}

bool StarRovers::SurfaceGridRaycast::RaycastGeneratedGrid(
	const TArray<FSRPlanetSurfaceGridCell>& Cells,
	const FSRPlanetSurfaceGridRaycastState& RaycastState,
	const FTransform& ComponentTransform,
	const FVector& RayOrigin,
	const FVector& RayDirection,
	int32& OutCellIndex,
	FVector& OutHitLocation,
	bool& bOutAttempted)
{
	OutCellIndex = INDEX_NONE;
	OutHitLocation = FVector::ZeroVector;
	bOutAttempted = false;
	if (Cells.IsEmpty())
	{
		return false;
	}

	const FVector LocalRayOrigin = ComponentTransform.InverseTransformPosition(RayOrigin);
	const FVector LocalRayDirection = ComponentTransform.InverseTransformVectorNoScale(RayDirection).GetSafeNormal();
	if (LocalRayDirection.IsNearlyZero())
	{
		return false;
	}

	bOutAttempted = true;
	FSRPlanetSurfaceGridRaycastStats RaycastStats;
	float BestHitDistance = BIG_NUMBER;
	int32 BestCellIndex = INDEX_NONE;

	auto ConsiderCell = [&Cells, &LocalRayOrigin, &LocalRayDirection, &BestHitDistance, &BestCellIndex, &RaycastStats](int32 CellIndex)
	{
		if (!Cells.IsValidIndex(CellIndex))
		{
			return;
		}

		++RaycastStats.CellTests;

		auto ConsiderTriangleHit = [&LocalRayOrigin, &LocalRayDirection, &BestHitDistance, &BestCellIndex, &RaycastStats, CellIndex](
			const FVector& Point0,
			const FVector& Point1,
			const FVector& Point2,
			bool bSideTriangle)
		{
			if (bSideTriangle)
			{
				++RaycastStats.SideTriangleTests;
			}
			else
			{
				++RaycastStats.TopTriangleTests;
			}

			float HitDistance = 0.0f;
			if (!IntersectRayTriangle(LocalRayOrigin, LocalRayDirection, Point0, Point1, Point2, HitDistance))
			{
				return;
			}

			++RaycastStats.TriangleHits;
			if (HitDistance < BestHitDistance)
			{
				BestHitDistance = HitDistance;
				BestCellIndex = CellIndex;
				++RaycastStats.BestHitUpdates;
			}
		};

		const FSRPlanetSurfaceGridCell& Cell = Cells[CellIndex];
		ConsiderTriangleHit(Cell.Corner00, Cell.Corner10, Cell.Corner11, false);
		ConsiderTriangleHit(Cell.Corner00, Cell.Corner11, Cell.Corner01, false);
		for (const FSRPlanetSurfaceGridSideFace& SideFace : Cell.SideFaces)
		{
			ConsiderTriangleHit(SideFace.LocalPoint0, SideFace.LocalPoint1, SideFace.LocalPoint2, true);
			ConsiderTriangleHit(SideFace.LocalPoint0, SideFace.LocalPoint2, SideFace.LocalPoint3, true);
		}
	};

	for (const FSRPlanetSurfaceGridRaycastBucket& Bucket : RaycastState.Buckets)
	{
		++RaycastStats.BucketTests;
		float BucketHitDistance = 0.0f;
		if (!IntersectRayBox(LocalRayOrigin, LocalRayDirection, Bucket.LocalBounds, BucketHitDistance))
		{
			continue;
		}
		++RaycastStats.BucketHits;
		if (BucketHitDistance > BestHitDistance)
		{
			++RaycastStats.BucketSkippedByBestHit;
			continue;
		}

		for (const int32 CellIndex : Bucket.CellIndices)
		{
			ConsiderCell(CellIndex);
		}
	}

	if (!Cells.IsValidIndex(BestCellIndex))
	{
		return false;
	}

	OutCellIndex = BestCellIndex;
	OutHitLocation = ComponentTransform.TransformPosition(LocalRayOrigin + (LocalRayDirection * BestHitDistance));
	return true;
}

void StarRovers::SurfaceGridRaycast::RebuildRaycastIndex(
	FSRPlanetSurfaceGridRaycastState& RaycastState,
	const TArray<FSRPlanetSurfaceGridCell>& Cells,
	bool bUsingGeneratedGridCells,
	int32 FaceResolution,
	float GridSurfaceOffset)
{
	RaycastState.Buckets.Reset();
	if (!bUsingGeneratedGridCells || Cells.IsEmpty())
	{
		return;
	}

	constexpr int32 FaceCount = 6;
	constexpr int32 BucketResolution = SurfaceGridRaycastBucketResolution;
	const ESRCubeSphereFace Faces[FaceCount] =
	{
		ESRCubeSphereFace::PositiveX,
		ESRCubeSphereFace::NegativeX,
		ESRCubeSphereFace::PositiveY,
		ESRCubeSphereFace::NegativeY,
		ESRCubeSphereFace::PositiveZ,
		ESRCubeSphereFace::NegativeZ,
	};

	auto GetFaceBucketOffset = [&Faces](ESRCubeSphereFace Face) -> int32
	{
		for (int32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
		{
			if (Faces[FaceIndex] == Face)
			{
				return FaceIndex * BucketResolution * BucketResolution;
			}
		}
		return INDEX_NONE;
	};

	RaycastState.Buckets.SetNum(FaceCount * BucketResolution * BucketResolution);
	for (int32 FaceIndex = 0; FaceIndex < FaceCount; ++FaceIndex)
	{
		for (int32 BucketY = 0; BucketY < BucketResolution; ++BucketY)
		{
			for (int32 BucketX = 0; BucketX < BucketResolution; ++BucketX)
			{
				const int32 BucketIndex = FaceIndex * BucketResolution * BucketResolution + BucketY * BucketResolution + BucketX;
				FSRPlanetSurfaceGridRaycastBucket& Bucket = RaycastState.Buckets[BucketIndex];
				Bucket.Face = Faces[FaceIndex];
				Bucket.BucketX = BucketX;
				Bucket.BucketY = BucketY;
				Bucket.LocalBounds = FBox(ForceInit);
				Bucket.CellIndices.Reset();
			}
		}
	}

	const int32 BucketCellSize = FMath::Max(1, FMath::DivideAndRoundUp(FMath::Max(1, FaceResolution), BucketResolution));
	for (int32 CellIndex = 0; CellIndex < Cells.Num(); ++CellIndex)
	{
		const FSRPlanetSurfaceGridCell& Cell = Cells[CellIndex];
		const int32 FaceBucketOffset = GetFaceBucketOffset(Cell.CellId.Face);
		if (FaceBucketOffset == INDEX_NONE)
		{
			continue;
		}

		const int32 BucketX = FMath::Clamp(Cell.CellId.CellX / BucketCellSize, 0, BucketResolution - 1);
		const int32 BucketY = FMath::Clamp(Cell.CellId.CellY / BucketCellSize, 0, BucketResolution - 1);
		const int32 BucketIndex = FaceBucketOffset + BucketY * BucketResolution + BucketX;
		if (!RaycastState.Buckets.IsValidIndex(BucketIndex))
		{
			continue;
		}

		FSRPlanetSurfaceGridRaycastBucket& Bucket = RaycastState.Buckets[BucketIndex];
		Bucket.CellIndices.Add(CellIndex);
		Bucket.LocalBounds += Cell.Corner00;
		Bucket.LocalBounds += Cell.Corner10;
		Bucket.LocalBounds += Cell.Corner11;
		Bucket.LocalBounds += Cell.Corner01;
		for (const FSRPlanetSurfaceGridSideFace& SideFace : Cell.SideFaces)
		{
			Bucket.LocalBounds += SideFace.LocalPoint0;
			Bucket.LocalBounds += SideFace.LocalPoint1;
			Bucket.LocalBounds += SideFace.LocalPoint2;
			Bucket.LocalBounds += SideFace.LocalPoint3;
		}
	}

	const float BoundsPadding = FMath::Max(1.0f, GridSurfaceOffset + 1.0f);
	for (FSRPlanetSurfaceGridRaycastBucket& Bucket : RaycastState.Buckets)
	{
		if (Bucket.LocalBounds.IsValid)
		{
			Bucket.LocalBounds = Bucket.LocalBounds.ExpandBy(BoundsPadding);
		}
	}
}

bool StarRovers::SurfaceGridRaycast::IntersectRayWithSphere(
	const FVector& RayOrigin,
	const FVector& RayDirection,
	const FVector& SphereCenter,
	float SphereRadius,
	FVector& OutHitLocation)
{
	OutHitLocation = FVector::ZeroVector;

	const FVector Direction = RayDirection.GetSafeNormal();
	if (Direction.IsNearlyZero() || SphereRadius <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector OriginToCenter = RayOrigin - SphereCenter;
	const double HalfB = FVector::DotProduct(OriginToCenter, Direction);
	const double C = OriginToCenter.SizeSquared() - FMath::Square(static_cast<double>(SphereRadius));
	const double Discriminant = (HalfB * HalfB) - C;
	if (Discriminant < 0.0)
	{
		return false;
	}

	const double SqrtDiscriminant = FMath::Sqrt(Discriminant);
	double HitDistance = -HalfB - SqrtDiscriminant;
	if (HitDistance < 0.0)
	{
		HitDistance = -HalfB + SqrtDiscriminant;
	}

	if (HitDistance < 0.0)
	{
		return false;
	}

	OutHitLocation = RayOrigin + (Direction * static_cast<float>(HitDistance));
	return true;
}
