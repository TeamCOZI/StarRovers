#include "SRPlanetSurfaceGridInteractionOverlayGeometry.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"

FString StarRovers::SurfaceGridInteractionOverlayGeometry::FormatSurfacePatchCellId(const FSRPlanetSurfaceGridCellId& CellId)
{
	return FString::Printf(
		TEXT("Face=%d X=%d Y=%d"),
		static_cast<int32>(CellId.Face),
		CellId.CellX,
		CellId.CellY);
}

void StarRovers::SurfaceGridInteractionOverlayGeometry::AppendInteractionFilledQuad(
	UE::Geometry::FDynamicMesh3& OverlayMesh,
	const FVector& Point0,
	const FVector& Point1,
	const FVector& Point2,
	const FVector& Point3,
	FLinearColor FillColor)
{
	FVector QuadPoints[4] = { Point0, Point1, Point2, Point3 };
	FVector QuadNormal = FVector::CrossProduct(QuadPoints[1] - QuadPoints[0], QuadPoints[2] - QuadPoints[0]).GetSafeNormal();
	const FVector OutwardDirection = ((Point0 + Point1 + Point2 + Point3) * 0.25f).GetSafeNormal();
	if (!OutwardDirection.IsNearlyZero() && FVector::DotProduct(QuadNormal, OutwardDirection) < 0.0f)
	{
		Swap(QuadPoints[1], QuadPoints[3]);
		QuadNormal *= -1.0f;
	}
	if (QuadNormal.IsNearlyZero())
	{
		QuadNormal = OutwardDirection.IsNearlyZero() ? FVector::UpVector : OutwardDirection;
	}

	FillColor.A = FMath::Clamp(FillColor.A * 0.55f, 0.0f, 1.0f);
	const int32 Vertex0 = OverlayMesh.AppendVertex(FVector3d(QuadPoints[0]));
	const int32 Vertex1 = OverlayMesh.AppendVertex(FVector3d(QuadPoints[1]));
	const int32 Vertex2 = OverlayMesh.AppendVertex(FVector3d(QuadPoints[2]));
	const int32 Vertex3 = OverlayMesh.AppendVertex(FVector3d(QuadPoints[3]));

	const int32 Triangle0 = OverlayMesh.AppendTriangle(Vertex0, Vertex2, Vertex1);
	const int32 Triangle1 = OverlayMesh.AppendTriangle(Vertex0, Vertex3, Vertex2);

	UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay = OverlayMesh.Attributes()->PrimaryNormals();
	auto* ColorOverlay = OverlayMesh.Attributes()->PrimaryColors();
	if (!NormalOverlay || !ColorOverlay)
	{
		return;
	}

	const int32 Normal0 = NormalOverlay->AppendElement(FVector3f(QuadNormal));
	const int32 Normal1 = NormalOverlay->AppendElement(FVector3f(QuadNormal));
	const int32 Normal2 = NormalOverlay->AppendElement(FVector3f(QuadNormal));
	const int32 Normal3 = NormalOverlay->AppendElement(FVector3f(QuadNormal));
	const int32 Color0 = ColorOverlay->AppendElement(FVector4f(FillColor.R, FillColor.G, FillColor.B, FillColor.A));
	const int32 Color1 = ColorOverlay->AppendElement(FVector4f(FillColor.R, FillColor.G, FillColor.B, FillColor.A));
	const int32 Color2 = ColorOverlay->AppendElement(FVector4f(FillColor.R, FillColor.G, FillColor.B, FillColor.A));
	const int32 Color3 = ColorOverlay->AppendElement(FVector4f(FillColor.R, FillColor.G, FillColor.B, FillColor.A));

	if (Triangle0 >= 0)
	{
		NormalOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(Normal0, Normal2, Normal1));
		ColorOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(Color0, Color2, Color1));
	}
	if (Triangle1 >= 0)
	{
		NormalOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(Normal0, Normal3, Normal2));
		ColorOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(Color0, Color3, Color2));
	}
}

bool StarRovers::SurfaceGridInteractionOverlayGeometry::IsContiguousRectangularCellRegion(const TArray<FSRPlanetSurfaceGridCellId>& CellIds)
{
	if (CellIds.IsEmpty())
	{
		return true;
	}

	const ESRCubeSphereFace Face = CellIds[0].Face;
	int32 MinCellX = CellIds[0].CellX;
	int32 MaxCellX = CellIds[0].CellX;
	int32 MinCellY = CellIds[0].CellY;
	int32 MaxCellY = CellIds[0].CellY;
	TSet<FSRPlanetSurfaceGridCellId> RegionCellIdSet;
	RegionCellIdSet.Reserve(CellIds.Num());

	for (const FSRPlanetSurfaceGridCellId& CellId : CellIds)
	{
		if (CellId.Face != Face)
		{
			return false;
		}

		RegionCellIdSet.Add(CellId);
		MinCellX = FMath::Min(MinCellX, CellId.CellX);
		MaxCellX = FMath::Max(MaxCellX, CellId.CellX);
		MinCellY = FMath::Min(MinCellY, CellId.CellY);
		MaxCellY = FMath::Max(MaxCellY, CellId.CellY);
	}

	const int64 RegionCellsX = static_cast<int64>(MaxCellX) - static_cast<int64>(MinCellX) + 1;
	const int64 RegionCellsY = static_cast<int64>(MaxCellY) - static_cast<int64>(MinCellY) + 1;
	return RegionCellsX > 0
		&& RegionCellsY > 0
		&& static_cast<int64>(RegionCellIdSet.Num()) == RegionCellsX * RegionCellsY;
}
