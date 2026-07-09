#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"

namespace StarRovers::SurfaceGridWireGeometry
{
	inline uint32 HashGridPoint(const FVector& LocalPoint)
	{
		constexpr double QuantizeScale = 100.0;
		const int64 QuantizedX = FMath::RoundToInt64(LocalPoint.X * QuantizeScale);
		const int64 QuantizedY = FMath::RoundToInt64(LocalPoint.Y * QuantizeScale);
		const int64 QuantizedZ = FMath::RoundToInt64(LocalPoint.Z * QuantizeScale);
		auto HashInt64 = [](int64 Value)
		{
			const uint64 Bits = static_cast<uint64>(Value);
			return HashCombine(
				::GetTypeHash(static_cast<uint32>(Bits & 0xffffffff)),
				::GetTypeHash(static_cast<uint32>(Bits >> 32)));
		};
		return HashCombine(HashCombine(HashInt64(QuantizedX), HashInt64(QuantizedY)), HashInt64(QuantizedZ));
	}

	inline uint64 BuildGridEdgeKey(const FVector& LocalPointA, const FVector& LocalPointB)
	{
		const uint32 EndpointA = HashGridPoint(LocalPointA);
		const uint32 EndpointB = HashGridPoint(LocalPointB);
		const uint32 MinEndpoint = FMath::Min(EndpointA, EndpointB);
		const uint32 MaxEndpoint = FMath::Max(EndpointA, EndpointB);
		return (static_cast<uint64>(MinEndpoint) << 32) | static_cast<uint64>(MaxEndpoint);
	}

	inline bool GetGridCellEdgePoints(const FSRPlanetSurfaceGridCell& Cell, int32 EdgeIndex, FVector& OutPointA, FVector& OutPointB)
	{
		switch (EdgeIndex)
		{
		case 0:
			OutPointA = Cell.Corner00;
			OutPointB = Cell.Corner10;
			return true;
		case 1:
			OutPointA = Cell.Corner10;
			OutPointB = Cell.Corner11;
			return true;
		case 2:
			OutPointA = Cell.Corner11;
			OutPointB = Cell.Corner01;
			return true;
		case 3:
			OutPointA = Cell.Corner01;
			OutPointB = Cell.Corner00;
			return true;
		default:
			OutPointA = FVector::ZeroVector;
			OutPointB = FVector::ZeroVector;
			return false;
		}
	}

	inline FSRPlanetSurfaceGridCellId GetGridCellEdgeNeighborId(const FSRPlanetSurfaceGridCell& Cell, int32 EdgeIndex)
	{
		switch (EdgeIndex)
		{
		case 0:
			return Cell.Neighbors.NegativeV;
		case 1:
			return Cell.Neighbors.PositiveU;
		case 2:
			return Cell.Neighbors.PositiveV;
		case 3:
			return Cell.Neighbors.NegativeU;
		default:
			return Cell.CellId;
		}
	}

	inline float GetGridEdgeDirectionMatchScore(
		const FVector& PointA,
		const FVector& PointB,
		const FVector& CandidatePointA,
		const FVector& CandidatePointB)
	{
		const FVector DirectionA = PointA.GetSafeNormal();
		const FVector DirectionB = PointB.GetSafeNormal();
		const FVector CandidateDirectionA = CandidatePointA.GetSafeNormal();
		const FVector CandidateDirectionB = CandidatePointB.GetSafeNormal();
		if (DirectionA.IsNearlyZero() || DirectionB.IsNearlyZero() || CandidateDirectionA.IsNearlyZero() || CandidateDirectionB.IsNearlyZero())
		{
			return TNumericLimits<float>::Max();
		}

		return static_cast<float>(
			FVector::DistSquared(DirectionA, CandidateDirectionA)
			+ FVector::DistSquared(DirectionB, CandidateDirectionB));
	}

	inline bool TryGetMatchingGridCellEdgePoints(
		const FSRPlanetSurfaceGridCell& Cell,
		int32 EdgeIndex,
		const FSRPlanetSurfaceGridCell& NeighborCell,
		FVector& OutCellPointA,
		FVector& OutCellPointB,
		FVector& OutNeighborPointA,
		FVector& OutNeighborPointB)
	{
		if (!GetGridCellEdgePoints(Cell, EdgeIndex, OutCellPointA, OutCellPointB))
		{
			return false;
		}

		float BestScore = TNumericLimits<float>::Max();
		for (int32 NeighborEdgeIndex = 0; NeighborEdgeIndex < 4; ++NeighborEdgeIndex)
		{
			FVector CandidatePointA;
			FVector CandidatePointB;
			if (!GetGridCellEdgePoints(NeighborCell, NeighborEdgeIndex, CandidatePointA, CandidatePointB))
			{
				continue;
			}

			const float ForwardScore = GetGridEdgeDirectionMatchScore(OutCellPointA, OutCellPointB, CandidatePointA, CandidatePointB);
			if (ForwardScore < BestScore)
			{
				BestScore = ForwardScore;
				OutNeighborPointA = CandidatePointA;
				OutNeighborPointB = CandidatePointB;
			}

			const float ReverseScore = GetGridEdgeDirectionMatchScore(OutCellPointA, OutCellPointB, CandidatePointB, CandidatePointA);
			if (ReverseScore < BestScore)
			{
				BestScore = ReverseScore;
				OutNeighborPointA = CandidatePointB;
				OutNeighborPointB = CandidatePointA;
			}
		}

		return BestScore <= 0.0001f;
	}

	inline FVector OffsetGeneratedGridWirePoint(const FVector& LocalPoint, float SurfaceOffset)
	{
		if (SurfaceOffset <= KINDA_SMALL_NUMBER)
		{
			return LocalPoint;
		}

		FVector SurfaceNormal = LocalPoint.GetSafeNormal();
		if (SurfaceNormal.IsNearlyZero())
		{
			SurfaceNormal = FVector::UpVector;
		}
		return LocalPoint + (SurfaceNormal * SurfaceOffset);
	}

	inline void AppendGridWireVolumeSegment(
		UE::Geometry::FDynamicMesh3& GridMesh,
		const FVector& LocalPointA,
		const FVector& LocalPointB,
		const FLinearColor& LineColor,
		float LineThickness)
	{
		const FVector SegmentDirection = (LocalPointB - LocalPointA).GetSafeNormal();
		if (SegmentDirection.IsNearlyZero())
		{
			return;
		}

		FVector ReferenceDirection = ((LocalPointA + LocalPointB) * 0.5f).GetSafeNormal();
		if (ReferenceDirection.IsNearlyZero() || FMath::Abs(FVector::DotProduct(ReferenceDirection, SegmentDirection)) > 0.98f)
		{
			ReferenceDirection = FVector::UpVector;
		}
		if (FMath::Abs(FVector::DotProduct(ReferenceDirection, SegmentDirection)) > 0.98f)
		{
			ReferenceDirection = FVector::RightVector;
		}

		FVector AxisA = FVector::CrossProduct(SegmentDirection, ReferenceDirection).GetSafeNormal();
		if (AxisA.IsNearlyZero())
		{
			return;
		}

		FVector AxisB = FVector::CrossProduct(SegmentDirection, AxisA).GetSafeNormal();
		if (AxisB.IsNearlyZero())
		{
			return;
		}

		const float HalfThickness = FMath::Max(0.01f, LineThickness) * 0.5f;
		AxisA *= HalfThickness;
		AxisB *= HalfThickness;

		const FVector A0 = LocalPointA + AxisA + AxisB;
		const FVector A1 = LocalPointA - AxisA + AxisB;
		const FVector A2 = LocalPointA - AxisA - AxisB;
		const FVector A3 = LocalPointA + AxisA - AxisB;
		const FVector B0 = LocalPointB + AxisA + AxisB;
		const FVector B1 = LocalPointB - AxisA + AxisB;
		const FVector B2 = LocalPointB - AxisA - AxisB;
		const FVector B3 = LocalPointB + AxisA - AxisB;

		UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay = GridMesh.Attributes()->PrimaryNormals();
		auto* ColorOverlay = GridMesh.Attributes()->PrimaryColors();
		if (!NormalOverlay || !ColorOverlay)
		{
			return;
		}

		auto AppendColoredQuad = [&GridMesh, NormalOverlay, ColorOverlay, &LineColor](
			const FVector& Point0,
			const FVector& Point1,
			const FVector& Point2,
			const FVector& Point3)
		{
			const FVector FaceNormal = FVector::CrossProduct(Point1 - Point0, Point2 - Point0).GetSafeNormal();
			if (FaceNormal.IsNearlyZero())
			{
				return;
			}

			const int32 Vertex0 = GridMesh.AppendVertex(FVector3d(Point0));
			const int32 Vertex1 = GridMesh.AppendVertex(FVector3d(Point1));
			const int32 Vertex2 = GridMesh.AppendVertex(FVector3d(Point2));
			const int32 Vertex3 = GridMesh.AppendVertex(FVector3d(Point3));
			const int32 Triangle0 = GridMesh.AppendTriangle(Vertex0, Vertex1, Vertex2);
			const int32 Triangle1 = GridMesh.AppendTriangle(Vertex0, Vertex2, Vertex3);

			const int32 Normal0 = NormalOverlay->AppendElement(FVector3f(FaceNormal));
			const int32 Normal1 = NormalOverlay->AppendElement(FVector3f(FaceNormal));
			const int32 Normal2 = NormalOverlay->AppendElement(FVector3f(FaceNormal));
			const int32 Normal3 = NormalOverlay->AppendElement(FVector3f(FaceNormal));
			const int32 Color0 = ColorOverlay->AppendElement(FVector4f(LineColor.R, LineColor.G, LineColor.B, LineColor.A));
			const int32 Color1 = ColorOverlay->AppendElement(FVector4f(LineColor.R, LineColor.G, LineColor.B, LineColor.A));
			const int32 Color2 = ColorOverlay->AppendElement(FVector4f(LineColor.R, LineColor.G, LineColor.B, LineColor.A));
			const int32 Color3 = ColorOverlay->AppendElement(FVector4f(LineColor.R, LineColor.G, LineColor.B, LineColor.A));

			if (Triangle0 >= 0)
			{
				NormalOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(Normal0, Normal1, Normal2));
				ColorOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(Color0, Color1, Color2));
			}
			if (Triangle1 >= 0)
			{
				NormalOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(Normal0, Normal2, Normal3));
				ColorOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(Color0, Color2, Color3));
			}
		};

		AppendColoredQuad(A0, A1, B1, B0);
		AppendColoredQuad(A1, A2, B2, B1);
		AppendColoredQuad(A2, A3, B3, B2);
		AppendColoredQuad(A3, A0, B0, B3);
		AppendColoredQuad(A0, A3, A2, A1);
		AppendColoredQuad(B0, B1, B2, B3);
	}
}
