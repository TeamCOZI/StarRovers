#include "SRPlanetSurfaceGridWirePrimitives.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"

void StarRovers::SurfaceGridWirePrimitives::AppendGridWireEdge(
	UE::Geometry::FDynamicMesh3& GridMesh,
	const FVector& LocalDirectionA,
	const FVector& LocalDirectionB,
	const FLinearColor& LineColor,
	float LineThickness,
	float GridSurfaceOffset,
	FSurfacePointResolver ResolveLocalSurfacePoint)
{
	const FVector DirectionA = LocalDirectionA.GetSafeNormal();
	const FVector DirectionB = LocalDirectionB.GetSafeNormal();
	if (DirectionA.IsNearlyZero() || DirectionB.IsNearlyZero())
	{
		return;
	}

	constexpr int32 SegmentCount = 8;
	FVector PreviousPoint = ResolveLocalSurfacePoint(DirectionA, GridSurfaceOffset);
	for (int32 SegmentIndex = 1; SegmentIndex <= SegmentCount; ++SegmentIndex)
	{
		const float Alpha = static_cast<float>(SegmentIndex) / static_cast<float>(SegmentCount);
		const FVector SampleDirection = FMath::Lerp(DirectionA, DirectionB, Alpha).GetSafeNormal();
		if (SampleDirection.IsNearlyZero())
		{
			continue;
		}

		const FVector CurrentPoint = ResolveLocalSurfacePoint(SampleDirection, GridSurfaceOffset);
		AppendGridWireSegment(GridMesh, PreviousPoint, CurrentPoint, LineColor, LineThickness);
		PreviousPoint = CurrentPoint;
	}
}

void StarRovers::SurfaceGridWirePrimitives::AppendGridWireSegment(
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

	const FVector Midpoint = (LocalPointA + LocalPointB) * 0.5f;
	FVector SurfaceNormal = Midpoint.GetSafeNormal();
	if (SurfaceNormal.IsNearlyZero())
	{
		SurfaceNormal = FVector::UpVector;
	}

	FVector SideDirection = FVector::CrossProduct(SurfaceNormal, SegmentDirection).GetSafeNormal();
	if (SideDirection.IsNearlyZero())
	{
		SideDirection = FVector::CrossProduct(FVector::UpVector, SegmentDirection).GetSafeNormal();
	}
	if (SideDirection.IsNearlyZero())
	{
		return;
	}

	const float HalfThickness = FMath::Max(0.01f, LineThickness) * 0.5f;
	const FVector Offset = SideDirection * HalfThickness;
	const int32 Vertex0 = GridMesh.AppendVertex(FVector3d(LocalPointA - Offset));
	const int32 Vertex1 = GridMesh.AppendVertex(FVector3d(LocalPointA + Offset));
	const int32 Vertex2 = GridMesh.AppendVertex(FVector3d(LocalPointB + Offset));
	const int32 Vertex3 = GridMesh.AppendVertex(FVector3d(LocalPointB - Offset));

	const int32 Triangle0 = GridMesh.AppendTriangle(Vertex0, Vertex1, Vertex2);
	const int32 Triangle1 = GridMesh.AppendTriangle(Vertex0, Vertex2, Vertex3);

	UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay = GridMesh.Attributes()->PrimaryNormals();
	auto* ColorOverlay = GridMesh.Attributes()->PrimaryColors();
	if (!NormalOverlay || !ColorOverlay)
	{
		return;
	}

	const int32 Normal0 = NormalOverlay->AppendElement(FVector3f(SurfaceNormal));
	const int32 Normal1 = NormalOverlay->AppendElement(FVector3f(SurfaceNormal));
	const int32 Normal2 = NormalOverlay->AppendElement(FVector3f(SurfaceNormal));
	const int32 Normal3 = NormalOverlay->AppendElement(FVector3f(SurfaceNormal));
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
}
