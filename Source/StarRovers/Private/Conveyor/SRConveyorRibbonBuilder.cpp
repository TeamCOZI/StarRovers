#include "Conveyor/SRConveyorRibbonBuilder.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "Surface/SRPlanetSurfaceGrid.h"

bool StarRovers::Conveyor::FSRConveyorRibbonBuilder::BuildPathSplinePoints(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRConveyorBeltPath& BeltPath,
	const FSRConveyorRibbonBuildSettings& Settings,
	TArray<FVector>& OutWorldPoints,
	TArray<FVector>& OutWorldNormals)
{
	const float LayerOffset = static_cast<float>(FMath::Max(0, BeltPath.Layer)) * FMath::Max(0.0f, BeltPath.LayerHeight);
	const float HeightOffset = LayerOffset + FMath::Max(0.0f, Settings.BeltSurfaceOffset) + Settings.PCGSplineHeightOffset;
	return BuildPathPoints(SurfaceGrid, BeltPath, HeightOffset, Settings.BeltWidth, OutWorldPoints, OutWorldNormals);
}

bool StarRovers::Conveyor::FSRConveyorRibbonBuilder::BuildPathRibbon(
	UE::Geometry::FDynamicMesh3& BeltMesh,
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRConveyorBeltPath& BeltPath,
	const FSRConveyorRibbonBuildSettings& Settings)
{
	TArray<FVector> WorldPoints;
	TArray<FVector> WorldNormals;
	WorldPoints.Reserve(BeltPath.CellIds.Num());
	WorldNormals.Reserve(BeltPath.CellIds.Num());

	const float LayerOffset = static_cast<float>(FMath::Max(0, BeltPath.Layer)) * FMath::Max(0.0f, BeltPath.LayerHeight);
	if (!BuildPathPoints(SurfaceGrid, BeltPath, LayerOffset, Settings.BeltWidth, WorldPoints, WorldNormals))
	{
		return false;
	}

	UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay = BeltMesh.Attributes()->PrimaryNormals();
	UE::Geometry::FDynamicMeshUVOverlay* UVOverlay = BeltMesh.Attributes()->PrimaryUV();
	auto* ColorOverlay = BeltMesh.Attributes()->PrimaryColors();
	if (!NormalOverlay || !UVOverlay || !ColorOverlay)
	{
		return false;
	}

	const float HalfWidth = ResolveBeltHalfWidth(WorldPoints, Settings.BeltWidth);
	const float HalfThickness = ResolveBeltHalfThickness(HalfWidth, BeltPath.LayerHeight, Settings.BeltThickness);
	const float CenterSurfaceOffset = FMath::Max(0.0f, Settings.BeltSurfaceOffset) + HalfThickness;
	for (int32 PointIndex = 0; PointIndex < WorldPoints.Num(); ++PointIndex)
	{
		WorldPoints[PointIndex] += WorldNormals[PointIndex].GetSafeNormal() * CenterSurfaceOffset;
	}
	const FLinearColor BeltColor = FLinearColor::White;

	auto AppendQuad = [&BeltMesh, NormalOverlay, UVOverlay, ColorOverlay, &BeltColor](
		int32 Vertex0,
		int32 Vertex1,
		int32 Vertex2,
		int32 Vertex3,
		const FVector& LocalNormal,
		const FVector2f& UV0,
		const FVector2f& UV1,
		const FVector2f& UV2,
		const FVector2f& UV3)
	{
		const int32 Triangle0 = BeltMesh.AppendTriangle(Vertex0, Vertex1, Vertex2);
		const int32 Triangle1 = BeltMesh.AppendTriangle(Vertex0, Vertex2, Vertex3);
		if (Triangle0 < 0 || Triangle1 < 0)
		{
			return;
		}

		const int32 Normal0 = NormalOverlay->AppendElement(FVector3f(LocalNormal));
		const int32 Normal1 = NormalOverlay->AppendElement(FVector3f(LocalNormal));
		const int32 Normal2 = NormalOverlay->AppendElement(FVector3f(LocalNormal));
		const int32 Normal3 = NormalOverlay->AppendElement(FVector3f(LocalNormal));
		const int32 Color0 = ColorOverlay->AppendElement(FVector4f(BeltColor.R, BeltColor.G, BeltColor.B, BeltColor.A));
		const int32 Color1 = ColorOverlay->AppendElement(FVector4f(BeltColor.R, BeltColor.G, BeltColor.B, BeltColor.A));
		const int32 Color2 = ColorOverlay->AppendElement(FVector4f(BeltColor.R, BeltColor.G, BeltColor.B, BeltColor.A));
		const int32 Color3 = ColorOverlay->AppendElement(FVector4f(BeltColor.R, BeltColor.G, BeltColor.B, BeltColor.A));
		const int32 UVElement0 = UVOverlay->AppendElement(UV0);
		const int32 UVElement1 = UVOverlay->AppendElement(UV1);
		const int32 UVElement2 = UVOverlay->AppendElement(UV2);
		const int32 UVElement3 = UVOverlay->AppendElement(UV3);

		NormalOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(Normal0, Normal1, Normal2));
		NormalOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(Normal0, Normal2, Normal3));
		UVOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(UVElement0, UVElement1, UVElement2));
		UVOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(UVElement0, UVElement2, UVElement3));
		ColorOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(Color0, Color1, Color2));
		ColorOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(Color0, Color2, Color3));
	};

	float AccumulatedDistance = 0.0f;
	for (int32 SegmentIndex = 0; SegmentIndex + 1 < WorldPoints.Num(); ++SegmentIndex)
	{
		const FVector SegmentStart = WorldPoints[SegmentIndex];
		const FVector SegmentEnd = WorldPoints[SegmentIndex + 1];
		const float SegmentLength = FVector::Distance(SegmentStart, SegmentEnd);
		if (SegmentLength <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		FVector Normal = (WorldNormals[SegmentIndex] + WorldNormals[SegmentIndex + 1]).GetSafeNormal();
		if (Normal.IsNearlyZero())
		{
			Normal = WorldNormals[SegmentIndex].GetSafeNormal();
		}

		FVector Tangent = SegmentEnd - SegmentStart;
		Tangent = Tangent - Normal * FVector::DotProduct(Tangent, Normal);
		if (!Tangent.Normalize())
		{
			continue;
		}

		const FVector Side = FVector::CrossProduct(Normal, Tangent).GetSafeNormal();
		if (Side.IsNearlyZero())
		{
			continue;
		}

		const FVector TopOffset = Normal * HalfThickness;
		const FVector BottomOffset = -TopOffset;
		const FVector WidthOffset = Side * HalfWidth;
		const FVector WorldTopLeft0 = SegmentStart - WidthOffset + TopOffset;
		const FVector WorldTopRight0 = SegmentStart + WidthOffset + TopOffset;
		const FVector WorldTopRight1 = SegmentEnd + WidthOffset + TopOffset;
		const FVector WorldTopLeft1 = SegmentEnd - WidthOffset + TopOffset;
		const FVector WorldBottomLeft0 = SegmentStart - WidthOffset + BottomOffset;
		const FVector WorldBottomRight0 = SegmentStart + WidthOffset + BottomOffset;
		const FVector WorldBottomRight1 = SegmentEnd + WidthOffset + BottomOffset;
		const FVector WorldBottomLeft1 = SegmentEnd - WidthOffset + BottomOffset;

		const int32 TopLeft0 = BeltMesh.AppendVertex(FVector3d(Settings.ComponentTransform.InverseTransformPosition(WorldTopLeft0)));
		const int32 TopRight0 = BeltMesh.AppendVertex(FVector3d(Settings.ComponentTransform.InverseTransformPosition(WorldTopRight0)));
		const int32 TopRight1 = BeltMesh.AppendVertex(FVector3d(Settings.ComponentTransform.InverseTransformPosition(WorldTopRight1)));
		const int32 TopLeft1 = BeltMesh.AppendVertex(FVector3d(Settings.ComponentTransform.InverseTransformPosition(WorldTopLeft1)));
		const int32 BottomLeft0 = BeltMesh.AppendVertex(FVector3d(Settings.ComponentTransform.InverseTransformPosition(WorldBottomLeft0)));
		const int32 BottomRight0 = BeltMesh.AppendVertex(FVector3d(Settings.ComponentTransform.InverseTransformPosition(WorldBottomRight0)));
		const int32 BottomRight1 = BeltMesh.AppendVertex(FVector3d(Settings.ComponentTransform.InverseTransformPosition(WorldBottomRight1)));
		const int32 BottomLeft1 = BeltMesh.AppendVertex(FVector3d(Settings.ComponentTransform.InverseTransformPosition(WorldBottomLeft1)));
		const FVector LocalNormal = Settings.ComponentTransform.InverseTransformVectorNoScale(Normal).GetSafeNormal();
		const float TextureWidth = FMath::Max(1.0f, HalfWidth * 2.0f);
		const float V0 = AccumulatedDistance / TextureWidth;
		const float V1 = (AccumulatedDistance + SegmentLength) / TextureWidth;
		const FVector LocalSideNormal = Settings.ComponentTransform.InverseTransformVectorNoScale(Side).GetSafeNormal();
		const FVector LocalTangentNormal = Settings.ComponentTransform.InverseTransformVectorNoScale(Tangent).GetSafeNormal();

		AppendQuad(TopLeft0, TopLeft1, TopRight1, TopRight0, LocalNormal, FVector2f(0.0f, V0), FVector2f(0.0f, V1), FVector2f(1.0f, V1), FVector2f(1.0f, V0));
		AppendQuad(BottomLeft0, BottomRight0, BottomRight1, BottomLeft1, -LocalNormal, FVector2f(0.0f, V0), FVector2f(1.0f, V0), FVector2f(1.0f, V1), FVector2f(0.0f, V1));
		AppendQuad(TopRight0, TopRight1, BottomRight1, BottomRight0, LocalSideNormal, FVector2f(0.0f, V0), FVector2f(0.0f, V1), FVector2f(1.0f, V1), FVector2f(1.0f, V0));
		AppendQuad(TopLeft0, BottomLeft0, BottomLeft1, TopLeft1, -LocalSideNormal, FVector2f(0.0f, V0), FVector2f(1.0f, V0), FVector2f(1.0f, V1), FVector2f(0.0f, V1));
		AppendQuad(TopLeft0, TopRight0, BottomRight0, BottomLeft0, -LocalTangentNormal, FVector2f(0.0f, 0.0f), FVector2f(1.0f, 0.0f), FVector2f(1.0f, 1.0f), FVector2f(0.0f, 1.0f));
		AppendQuad(TopLeft1, BottomLeft1, BottomRight1, TopRight1, LocalTangentNormal, FVector2f(0.0f, 0.0f), FVector2f(0.0f, 1.0f), FVector2f(1.0f, 1.0f), FVector2f(1.0f, 0.0f));

		AccumulatedDistance += SegmentLength;
	}

	return true;
}

bool StarRovers::Conveyor::FSRConveyorRibbonBuilder::BuildPathPoints(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRConveyorBeltPath& BeltPath,
	float HeightOffset,
	float BeltWidth,
	TArray<FVector>& OutWorldPoints,
	TArray<FVector>& OutWorldNormals)
{
	OutWorldPoints.Reset();
	OutWorldNormals.Reset();
	if (!IsValid(SurfaceGrid) || BeltPath.CellIds.IsEmpty())
	{
		return false;
	}

	const FVector PlanetCenter = SurfaceGrid->GetComponentTransform().GetLocation();
	OutWorldPoints.Reserve(BeltPath.CellIds.Num());
	OutWorldNormals.Reserve(BeltPath.CellIds.Num());

	for (const FSRPlanetSurfaceGridCellId& CellId : BeltPath.CellIds)
	{
		FSRPlanetSurfaceGridCellInfo CellInfo;
		if (!SurfaceGrid->GetCellInfoById(CellId, CellInfo))
		{
			continue;
		}

		FVector OutwardNormal = CellInfo.WorldNormal.GetSafeNormal();
		if (OutwardNormal.IsNearlyZero())
		{
			OutwardNormal = (CellInfo.WorldCenter - PlanetCenter).GetSafeNormal();
		}
		else if (FVector::DotProduct(OutwardNormal, CellInfo.WorldCenter - PlanetCenter) < 0.0f)
		{
			OutwardNormal *= -1.0f;
		}

		OutWorldPoints.Add(CellInfo.WorldCenter + OutwardNormal * HeightOffset);
		OutWorldNormals.Add(OutwardNormal);
	}

	if (OutWorldPoints.Num() == 1)
	{
		const FSRPlanetSurfaceGridCellId& CellId = BeltPath.CellIds[0];
		FSRPlanetSurfaceGridCell Cell;
		if (SurfaceGrid->GetCellById(CellId, Cell))
		{
			const FTransform SurfaceGridTransform = SurfaceGrid->GetComponentTransform();
			FVector SingleTangent = SurfaceGridTransform.TransformPosition(Cell.Corner10) - SurfaceGridTransform.TransformPosition(Cell.Corner00);
			SingleTangent = SingleTangent - OutWorldNormals[0] * FVector::DotProduct(SingleTangent, OutWorldNormals[0]);
			if (SingleTangent.Normalize())
			{
				const FVector CenterPoint = OutWorldPoints[0];
				const FVector CenterNormal = OutWorldNormals[0];
				const float CellEdgeLength = FVector::Distance(
					SurfaceGridTransform.TransformPosition(Cell.Corner00),
					SurfaceGridTransform.TransformPosition(Cell.Corner10));
				const float HalfLength = FMath::Clamp(BeltWidth * 0.5f, 1.0f, FMath::Max(1.0f, CellEdgeLength * 0.35f));
				OutWorldPoints.Reset();
				OutWorldNormals.Reset();
				OutWorldPoints.Add(CenterPoint - SingleTangent * HalfLength);
				OutWorldPoints.Add(CenterPoint + SingleTangent * HalfLength);
				OutWorldNormals.Add(CenterNormal);
				OutWorldNormals.Add(CenterNormal);
			}
		}
	}

	return OutWorldPoints.Num() >= 2 && OutWorldPoints.Num() == OutWorldNormals.Num();
}

float StarRovers::Conveyor::FSRConveyorRibbonBuilder::ResolveBeltHalfWidth(
	const TArray<FVector>& WorldPoints,
	float BeltWidth)
{
	float TotalSegmentLength = 0.0f;
	int32 SegmentCount = 0;
	for (int32 PointIndex = 1; PointIndex < WorldPoints.Num(); ++PointIndex)
	{
		const float SegmentLength = FVector::Distance(WorldPoints[PointIndex - 1], WorldPoints[PointIndex]);
		if (SegmentLength > KINDA_SMALL_NUMBER)
		{
			TotalSegmentLength += SegmentLength;
			++SegmentCount;
		}
	}

	const float DesiredHalfWidth = FMath::Max(1.0f, BeltWidth * 0.5f);
	if (SegmentCount <= 0)
	{
		return DesiredHalfWidth;
	}

	const float AverageSegmentLength = TotalSegmentLength / static_cast<float>(SegmentCount);
	return FMath::Clamp(DesiredHalfWidth, 1.0f, FMath::Max(1.0f, AverageSegmentLength * 0.35f));
}

float StarRovers::Conveyor::FSRConveyorRibbonBuilder::ResolveBeltHalfThickness(
	float HalfWidth,
	float LayerHeight,
	float BeltThickness)
{
	const float DesiredHalfThickness = FMath::Max(1.0f, BeltThickness * 0.5f);
	const float LayerLimitedHalfThickness = LayerHeight > KINDA_SMALL_NUMBER
		? FMath::Max(1.0f, LayerHeight * 0.35f)
		: DesiredHalfThickness;
	return FMath::Clamp(DesiredHalfThickness, 1.0f, FMath::Max(1.0f, FMath::Min(HalfWidth * 0.5f, LayerLimitedHalfThickness)));
}
