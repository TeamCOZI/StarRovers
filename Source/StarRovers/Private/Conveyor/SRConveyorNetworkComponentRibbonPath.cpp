#include "Conveyor/SRConveyorNetworkComponent.h"

#include "Surface/SRPlanetSurfaceGrid.h"

bool USRConveyorNetworkComponent::BuildConveyorPathSplinePoints(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRConveyorVisualPath& VisualPath,
	TArray<FVector>& OutWorldPoints,
	TArray<FVector>& OutWorldNormals) const
{
	OutWorldPoints.Reset();
	OutWorldNormals.Reset();
	if (!IsValid(SurfaceGrid) || VisualPath.CellIds.IsEmpty())
	{
		return false;
	}

	const FVector PlanetCenter = SurfaceGrid->GetComponentTransform().GetLocation();
	const float LayerOffset = static_cast<float>(FMath::Max(0, VisualPath.Layer)) * FMath::Max(0.0f, VisualPath.LayerHeight);
	const float HeightOffset = LayerOffset + FMath::Max(0.0f, BeltSurfaceOffset) + PCGSplineHeightOffset;
	OutWorldPoints.Reserve(VisualPath.CellIds.Num());
	OutWorldNormals.Reserve(VisualPath.CellIds.Num());

	for (const FSRPlanetSurfaceGridCellId& CellId : VisualPath.CellIds)
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
		const FSRPlanetSurfaceGridCellId& CellId = VisualPath.CellIds[0];
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

float USRConveyorNetworkComponent::ResolveBeltHalfWidth(const TArray<FVector>& WorldPoints) const
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

float USRConveyorNetworkComponent::ResolveBeltHalfThickness(float HalfWidth, float LayerHeight) const
{
	const float DesiredHalfThickness = FMath::Max(1.0f, BeltThickness * 0.5f);
	const float LayerLimitedHalfThickness = LayerHeight > KINDA_SMALL_NUMBER
		? FMath::Max(1.0f, LayerHeight * 0.35f)
		: DesiredHalfThickness;
	return FMath::Clamp(DesiredHalfThickness, 1.0f, FMath::Max(1.0f, FMath::Min(HalfWidth * 0.5f, LayerLimitedHalfThickness)));
}
