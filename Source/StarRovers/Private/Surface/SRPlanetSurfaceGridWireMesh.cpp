#include "Surface/SRPlanetSurfaceGrid.h"

#include "Celestial/SRCelestialBody.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "Surface/SRPlanetSurfaceGridVisualHelpers.h"
#include "UDynamicMesh.h"

using namespace StarRovers::SurfaceGridVisual;

void USRPlanetSurfaceGrid::AppendGeneratedGridCell(
	UE::Geometry::FDynamicMesh3& GridMesh,
	const FSRPlanetSurfaceGridCell& Cell,
	TSet<uint64>& DrawnEdges) const
{
	const FLinearColor DefaultLineColor(DebugLineColor.R, DebugLineColor.G, DebugLineColor.B, DebugLineOpacity);
	auto AppendDedupedSegment = [this, &GridMesh, &DefaultLineColor, &DrawnEdges](const FVector& PointA, const FVector& PointB)
	{
		const uint64 EdgeKey = BuildGridEdgeKey(PointA, PointB);
		if (DrawnEdges.Contains(EdgeKey))
		{
			return;
		}

		DrawnEdges.Add(EdgeKey);
		AppendGridWireSegment(GridMesh, PointA, PointB, DefaultLineColor, DebugLineThickness);
	};

	for (int32 EdgeIndex = 0; EdgeIndex < 4; ++EdgeIndex)
	{
		FVector EdgePointA;
		FVector EdgePointB;
		if (GetGridCellEdgePoints(Cell, EdgeIndex, EdgePointA, EdgePointB))
		{
			AppendDedupedSegment(
				OffsetGeneratedGridWirePoint(EdgePointA, GridSurfaceOffset),
				OffsetGeneratedGridWirePoint(EdgePointB, GridSurfaceOffset));
		}
	}

	for (const FSRPlanetSurfaceGridLineSegment& SideLineSegment : Cell.SideLineSegments)
	{
		AppendDedupedSegment(
			OffsetGeneratedGridWirePoint(SideLineSegment.LocalPointA, GridSurfaceOffset),
			OffsetGeneratedGridWirePoint(SideLineSegment.LocalPointB, GridSurfaceOffset));
	}
}

void USRPlanetSurfaceGrid::RebuildGridMesh()
{
	UE::Geometry::FDynamicMesh3 GridMesh;
	GridMesh.EnableAttributes();
	GridMesh.Attributes()->EnablePrimaryColors();

	if (!Cells.IsEmpty())
	{
		const FLinearColor DefaultLineColor(DebugLineColor.R, DebugLineColor.G, DebugLineColor.B, DebugLineOpacity);

		const bool bAppendedOwnerWire = !bUsingGeneratedGridCells
			&& AppendOwnerDynamicMeshWire(GridMesh, DefaultLineColor, DebugLineThickness);
		if (!bAppendedOwnerWire)
		{
			TSet<uint64> DrawnEdges;
			DrawnEdges.Reserve(Cells.Num() * 3);
			for (const FSRPlanetSurfaceGridCell& Cell : Cells)
			{
				AppendGridWireCell(GridMesh, Cell, DefaultLineColor, DebugLineThickness, true, &DrawnEdges);
			}
		}
	}

	SetMesh(MoveTemp(GridMesh));
	SetVisibility(bGridVisible);
	SetHiddenInGame(!bGridVisible);
	bGridMeshDirty = false;
}

bool USRPlanetSurfaceGrid::AppendOwnerDynamicMeshWire(
	UE::Geometry::FDynamicMesh3& GridMesh,
	const FLinearColor& LineColor,
	float LineThickness) const
{
	const ASRCelestialBody* OwnerBody = Cast<ASRCelestialBody>(GetOwner());
	if (!IsValid(OwnerBody))
	{
		return false;
	}

	UDynamicMeshComponent* OwnerDynamicMeshComponent = OwnerBody->GetCelestialBodyDynamicMesh();
	UDynamicMesh* OwnerDynamicMeshObject = IsValid(OwnerDynamicMeshComponent)
		? OwnerDynamicMeshComponent->GetDynamicMesh()
		: nullptr;
	if (!IsValid(OwnerDynamicMeshObject))
	{
		return false;
	}

	bool bAppendedAnyEdge = false;
	const FTransform OwnerDynamicMeshRelativeTransform = OwnerDynamicMeshComponent->GetRelativeTransform();
	OwnerDynamicMeshObject->ProcessMesh([this, &GridMesh, &LineColor, LineThickness, &bAppendedAnyEdge, &OwnerDynamicMeshRelativeTransform](const UE::Geometry::FDynamicMesh3& OwnerMesh)
	{
		for (const int32 EdgeId : OwnerMesh.EdgeIndicesItr())
		{
			const auto EdgeTriangles = OwnerMesh.GetEdgeT(EdgeId);
			if (EdgeTriangles.A >= 0 && EdgeTriangles.B >= 0)
			{
				continue;
			}

			const auto EdgeVertices = OwnerMesh.GetEdgeV(EdgeId);
			const FVector LocalPointA = OwnerDynamicMeshRelativeTransform.TransformPosition(FVector(OwnerMesh.GetVertex(EdgeVertices.A)));
			const FVector LocalPointB = OwnerDynamicMeshRelativeTransform.TransformPosition(FVector(OwnerMesh.GetVertex(EdgeVertices.B)));
			AppendGridWireSegment(GridMesh, LocalPointA, LocalPointB, LineColor, LineThickness);
			bAppendedAnyEdge = true;
		}
	});

	return bAppendedAnyEdge;
}

void USRPlanetSurfaceGrid::AppendGridWireCell(
	UE::Geometry::FDynamicMesh3& GridMesh,
	const FSRPlanetSurfaceGridCell& Cell,
	const FLinearColor& LineColor,
	float LineThickness,
	bool bIncludeInEdgeSet,
	TSet<uint64>* DrawnEdges) const
{
	auto AppendDedupedSegment = [&GridMesh, &LineColor, LineThickness, bIncludeInEdgeSet, DrawnEdges, this](
		const FVector& PointA,
		const FVector& PointB)
	{
		if (bIncludeInEdgeSet && DrawnEdges)
		{
			const uint64 EdgeKey = BuildGridEdgeKey(PointA, PointB);
			if (DrawnEdges->Contains(EdgeKey))
			{
				return;
			}
			DrawnEdges->Add(EdgeKey);
		}

		AppendGridWireSegment(GridMesh, PointA, PointB, LineColor, LineThickness);
	};

	if (bUsingGeneratedGridCells)
	{
		for (int32 EdgeIndex = 0; EdgeIndex < 4; ++EdgeIndex)
		{
			FVector EdgePointA;
			FVector EdgePointB;
			if (GetGridCellEdgePoints(Cell, EdgeIndex, EdgePointA, EdgePointB))
			{
				AppendDedupedSegment(
					OffsetGeneratedGridWirePoint(EdgePointA, GridSurfaceOffset),
					OffsetGeneratedGridWirePoint(EdgePointB, GridSurfaceOffset));
			}
		}

		for (const FSRPlanetSurfaceGridLineSegment& SideLineSegment : Cell.SideLineSegments)
		{
			AppendDedupedSegment(
				OffsetGeneratedGridWirePoint(SideLineSegment.LocalPointA, GridSurfaceOffset),
				OffsetGeneratedGridWirePoint(SideLineSegment.LocalPointB, GridSurfaceOffset));
		}

		for (int32 EdgeIndex = 0; EdgeIndex < 4; ++EdgeIndex)
		{
			const FSRPlanetSurfaceGridCellId NeighborId = GetGridCellEdgeNeighborId(Cell, EdgeIndex);
			if (NeighborId == Cell.CellId)
			{
				continue;
			}

			FSRPlanetSurfaceGridCell NeighborCell;
			if (!GetCellById(NeighborId, NeighborCell))
			{
				continue;
			}

			FVector CellPointA;
			FVector CellPointB;
			FVector NeighborPointA;
			FVector NeighborPointB;
			if (!TryGetMatchingGridCellEdgePoints(Cell, EdgeIndex, NeighborCell, CellPointA, CellPointB, NeighborPointA, NeighborPointB))
			{
				continue;
			}

			if (FVector::DistSquared(CellPointA, NeighborPointA) > KINDA_SMALL_NUMBER)
			{
				AppendDedupedSegment(
					OffsetGeneratedGridWirePoint(CellPointA, GridSurfaceOffset),
					OffsetGeneratedGridWirePoint(NeighborPointA, GridSurfaceOffset));
			}
			if (FVector::DistSquared(CellPointB, NeighborPointB) > KINDA_SMALL_NUMBER)
			{
				AppendDedupedSegment(
					OffsetGeneratedGridWirePoint(CellPointB, GridSurfaceOffset),
					OffsetGeneratedGridWirePoint(NeighborPointB, GridSurfaceOffset));
			}
		}
		return;
	}

	auto AppendEdge = [this, &GridMesh, &LineColor, LineThickness, bIncludeInEdgeSet, DrawnEdges](
		const FVector& CornerA,
		const FVector& CornerB)
	{
		if (bIncludeInEdgeSet && DrawnEdges)
		{
			const uint64 EdgeKey = BuildGridEdgeKey(CornerA, CornerB);
			if (DrawnEdges->Contains(EdgeKey))
			{
				return;
			}
			DrawnEdges->Add(EdgeKey);
		}

		AppendGridWireEdge(GridMesh, CornerA, CornerB, LineColor, LineThickness);
	};

	for (int32 EdgeIndex = 0; EdgeIndex < 4; ++EdgeIndex)
	{
		FVector EdgePointA;
		FVector EdgePointB;
		if (GetGridCellEdgePoints(Cell, EdgeIndex, EdgePointA, EdgePointB))
		{
			AppendEdge(EdgePointA, EdgePointB);
		}
	}
}

void USRPlanetSurfaceGrid::AppendGridWireEdge(
	UE::Geometry::FDynamicMesh3& GridMesh,
	const FVector& LocalDirectionA,
	const FVector& LocalDirectionB,
	const FLinearColor& LineColor,
	float LineThickness) const
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

void USRPlanetSurfaceGrid::AppendGridWireSegment(
	UE::Geometry::FDynamicMesh3& GridMesh,
	const FVector& LocalPointA,
	const FVector& LocalPointB,
	const FLinearColor& LineColor,
	float LineThickness) const
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
