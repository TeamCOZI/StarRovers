#include "Conveyor/SRConveyorNetworkComponent.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "GameFramework/Actor.h"
#include "Surface/SRPlanetSurfaceGrid.h"

bool USRConveyorNetworkComponent::BuildConveyorSegmentRibbon(
	UE::Geometry::FDynamicMesh3& BeltMesh,
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRConveyorSegment& Segment,
	float LayerHeight) const
{
	if (!IsValid(SurfaceGrid))
	{
		return false;
	}

	FTransform CurrentTransform;
	if (!SurfaceGrid->GetCellWorldTransform(Segment.Lane.CellId, 0.0f, CurrentTransform))
	{
		return false;
	}

	auto ResolveNeighborTransform = [SurfaceGrid, &Segment](ESRConveyorGridDirection Direction, FTransform& OutTransform)
	{
		if (Direction == ESRConveyorGridDirection::None)
		{
			return false;
		}

		FSRPlanetSurfaceGridCellNeighbors Neighbors;
		FSRPlanetSurfaceGridCellId NeighborCellId;
		if (!SurfaceGrid->GetCellNeighbors(Segment.Lane.CellId, Neighbors)
			|| !GetNeighborCellIdByDirection(Neighbors, Direction, NeighborCellId))
		{
			return false;
		}

		return SurfaceGrid->GetCellWorldTransform(NeighborCellId, 0.0f, OutTransform);
	};

	const float HeightOffset = static_cast<float>(FMath::Max(0, Segment.Lane.Layer)) * FMath::Max(0.0f, LayerHeight) + BeltSurfaceOffset;
	const FVector CurrentNormal = CurrentTransform.GetRotation().GetAxisZ().GetSafeNormal();
	const FVector CurrentWorldPosition = CurrentTransform.GetLocation() + (CurrentNormal * HeightOffset);

	FTransform InputTransform;
	FTransform OutputTransform;
	const bool bHasInput = ResolveNeighborTransform(Segment.InputDirection, InputTransform);
	const bool bHasOutput = ResolveNeighborTransform(Segment.OutputDirection, OutputTransform);
	const FVector InputWorldPosition = bHasInput
		? InputTransform.GetLocation() + (InputTransform.GetRotation().GetAxisZ().GetSafeNormal() * HeightOffset)
		: FVector::ZeroVector;
	const FVector OutputWorldPosition = bHasOutput
		? OutputTransform.GetLocation() + (OutputTransform.GetRotation().GetAxisZ().GetSafeNormal() * HeightOffset)
		: FVector::ZeroVector;

	FVector Forward = FVector::ZeroVector;
	if (bHasOutput)
	{
		Forward = OutputWorldPosition - CurrentWorldPosition;
	}
	else if (bHasInput)
	{
		Forward = CurrentWorldPosition - InputWorldPosition;
	}
	else
	{
		Forward = CurrentTransform.GetRotation().GetAxisX();
	}
	Forward = Forward - CurrentNormal * FVector::DotProduct(Forward, CurrentNormal);
	if (!Forward.Normalize())
	{
		Forward = CurrentTransform.GetRotation().GetAxisX();
	}

	const float HalfSegmentLength = bHasOutput
		? FVector::Distance(CurrentWorldPosition, OutputWorldPosition) * 0.5f
		: (bHasInput ? FVector::Distance(CurrentWorldPosition, InputWorldPosition) * 0.5f : BeltWidth);
	const FVector StartWorldPosition = bHasInput
		? (InputWorldPosition + CurrentWorldPosition) * 0.5f
		: CurrentWorldPosition - Forward * HalfSegmentLength;
	const FVector EndWorldPosition = bHasOutput
		? (CurrentWorldPosition + OutputWorldPosition) * 0.5f
		: CurrentWorldPosition + Forward * HalfSegmentLength;

	auto AppendRibbonQuad = [this, &BeltMesh](const FVector& WorldPointA, const FVector& WorldPointB, const FVector& WorldNormal)
	{
		FVector SegmentTangent = WorldPointB - WorldPointA;
		SegmentTangent = SegmentTangent - WorldNormal * FVector::DotProduct(SegmentTangent, WorldNormal);
		if (!SegmentTangent.Normalize())
		{
			return;
		}

		FVector Side = FVector::CrossProduct(WorldNormal, SegmentTangent).GetSafeNormal();
		if (Side.IsNearlyZero())
		{
			return;
		}

		const float HalfWidth = FMath::Max(1.0f, BeltWidth * 0.5f);
		const FTransform ComponentTransform = GetComponentTransform();
		const FVector LocalPoint0 = ComponentTransform.InverseTransformPosition(WorldPointA - Side * HalfWidth);
		const FVector LocalPoint1 = ComponentTransform.InverseTransformPosition(WorldPointA + Side * HalfWidth);
		const FVector LocalPoint2 = ComponentTransform.InverseTransformPosition(WorldPointB + Side * HalfWidth);
		const FVector LocalPoint3 = ComponentTransform.InverseTransformPosition(WorldPointB - Side * HalfWidth);
		const FVector LocalNormal = ComponentTransform.InverseTransformVectorNoScale(WorldNormal).GetSafeNormal();

		const int32 Vertex0 = BeltMesh.AppendVertex(FVector3d(LocalPoint0));
		const int32 Vertex1 = BeltMesh.AppendVertex(FVector3d(LocalPoint1));
		const int32 Vertex2 = BeltMesh.AppendVertex(FVector3d(LocalPoint2));
		const int32 Vertex3 = BeltMesh.AppendVertex(FVector3d(LocalPoint3));
		const int32 Triangle0 = BeltMesh.AppendTriangle(Vertex0, Vertex2, Vertex1);
		const int32 Triangle1 = BeltMesh.AppendTriangle(Vertex0, Vertex3, Vertex2);
		const int32 BackTriangle0 = BeltMesh.AppendTriangle(Vertex0, Vertex1, Vertex2);
		const int32 BackTriangle1 = BeltMesh.AppendTriangle(Vertex0, Vertex2, Vertex3);

		UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay = BeltMesh.Attributes()->PrimaryNormals();
		UE::Geometry::FDynamicMeshUVOverlay* UVOverlay = BeltMesh.Attributes()->PrimaryUV();
		auto* ColorOverlay = BeltMesh.Attributes()->PrimaryColors();
		if (!NormalOverlay || !UVOverlay || !ColorOverlay)
		{
			return;
		}

		const int32 Normal0 = NormalOverlay->AppendElement(FVector3f(LocalNormal));
		const int32 Normal1 = NormalOverlay->AppendElement(FVector3f(LocalNormal));
		const int32 Normal2 = NormalOverlay->AppendElement(FVector3f(LocalNormal));
		const int32 Normal3 = NormalOverlay->AppendElement(FVector3f(LocalNormal));
		const FLinearColor BeltColor = FLinearColor::White;
		const int32 Color0 = ColorOverlay->AppendElement(FVector4f(BeltColor.R, BeltColor.G, BeltColor.B, BeltColor.A));
		const int32 Color1 = ColorOverlay->AppendElement(FVector4f(BeltColor.R, BeltColor.G, BeltColor.B, BeltColor.A));
		const int32 Color2 = ColorOverlay->AppendElement(FVector4f(BeltColor.R, BeltColor.G, BeltColor.B, BeltColor.A));
		const int32 Color3 = ColorOverlay->AppendElement(FVector4f(BeltColor.R, BeltColor.G, BeltColor.B, BeltColor.A));
		const float VLength = FVector::Distance(WorldPointA, WorldPointB) / FMath::Max(1.0f, BeltWidth);
		const int32 UV0 = UVOverlay->AppendElement(FVector2f(0.0f, 0.0f));
		const int32 UV1 = UVOverlay->AppendElement(FVector2f(1.0f, 0.0f));
		const int32 UV2 = UVOverlay->AppendElement(FVector2f(1.0f, VLength));
		const int32 UV3 = UVOverlay->AppendElement(FVector2f(0.0f, VLength));

		if (Triangle0 >= 0)
		{
			NormalOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(Normal0, Normal2, Normal1));
			UVOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(UV0, UV2, UV1));
			ColorOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(Color0, Color2, Color1));
		}
		if (Triangle1 >= 0)
		{
			NormalOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(Normal0, Normal3, Normal2));
			UVOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(UV0, UV3, UV2));
			ColorOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(Color0, Color3, Color2));
		}
		if (BackTriangle0 >= 0)
		{
			const int32 BackNormal0 = NormalOverlay->AppendElement(FVector3f(-LocalNormal));
			const int32 BackNormal1 = NormalOverlay->AppendElement(FVector3f(-LocalNormal));
			const int32 BackNormal2 = NormalOverlay->AppendElement(FVector3f(-LocalNormal));
			NormalOverlay->SetTriangle(BackTriangle0, UE::Geometry::FIndex3i(BackNormal0, BackNormal1, BackNormal2));
			UVOverlay->SetTriangle(BackTriangle0, UE::Geometry::FIndex3i(UV0, UV1, UV2));
			ColorOverlay->SetTriangle(BackTriangle0, UE::Geometry::FIndex3i(Color0, Color1, Color2));
		}
		if (BackTriangle1 >= 0)
		{
			const int32 BackNormal0 = NormalOverlay->AppendElement(FVector3f(-LocalNormal));
			const int32 BackNormal2 = NormalOverlay->AppendElement(FVector3f(-LocalNormal));
			const int32 BackNormal3 = NormalOverlay->AppendElement(FVector3f(-LocalNormal));
			NormalOverlay->SetTriangle(BackTriangle1, UE::Geometry::FIndex3i(BackNormal0, BackNormal2, BackNormal3));
			UVOverlay->SetTriangle(BackTriangle1, UE::Geometry::FIndex3i(UV0, UV2, UV3));
			ColorOverlay->SetTriangle(BackTriangle1, UE::Geometry::FIndex3i(Color0, Color2, Color3));
		}
	};

	if (bHasInput && bHasOutput && Segment.Shape == ESRConveyorSegmentShape::Corner)
	{
		AppendRibbonQuad(StartWorldPosition, CurrentWorldPosition, CurrentNormal);
		AppendRibbonQuad(CurrentWorldPosition, EndWorldPosition, CurrentNormal);
	}
	else
	{
		AppendRibbonQuad(StartWorldPosition, EndWorldPosition, CurrentNormal);
	}

	auto AppendAdditionalDirectionRibbon = [&](ESRConveyorGridDirection Direction, bool bIncoming)
	{
		if (Direction == ESRConveyorGridDirection::None
			|| Direction == Segment.InputDirection
			|| Direction == Segment.OutputDirection)
		{
			return;
		}

		FTransform DirectionTransform;
		if (!ResolveNeighborTransform(Direction, DirectionTransform))
		{
			return;
		}

		const FVector DirectionNormal = DirectionTransform.GetRotation().GetAxisZ().GetSafeNormal();
		const FVector DirectionWorldPosition = DirectionTransform.GetLocation() + (DirectionNormal * HeightOffset);
		const FVector DirectionMidPoint = (DirectionWorldPosition + CurrentWorldPosition) * 0.5f;
		if (bIncoming)
		{
			AppendRibbonQuad(DirectionMidPoint, CurrentWorldPosition, CurrentNormal);
		}
		else
		{
			AppendRibbonQuad(CurrentWorldPosition, DirectionMidPoint, CurrentNormal);
		}
	};

	AppendAdditionalDirectionRibbon(Segment.MergeInputDirection, true);
	AppendAdditionalDirectionRibbon(Segment.SecondMergeInputDirection, true);
	AppendAdditionalDirectionRibbon(Segment.BranchOutputDirection, false);
	AppendAdditionalDirectionRibbon(Segment.SecondBranchOutputDirection, false);

	return true;
}

bool USRConveyorNetworkComponent::BuildConveyorPathRibbon(
	UE::Geometry::FDynamicMesh3& BeltMesh,
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRConveyorVisualPath& VisualPath) const
{
	if (!IsValid(SurfaceGrid) || VisualPath.CellIds.IsEmpty())
	{
		return false;
	}

	const AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return false;
	}

	const FVector PlanetCenter = SurfaceGrid->GetComponentTransform().GetLocation();
	const float LayerOffset = static_cast<float>(FMath::Max(0, VisualPath.Layer)) * FMath::Max(0.0f, VisualPath.LayerHeight);
	TArray<FVector> WorldPoints;
	TArray<FVector> WorldNormals;
	WorldPoints.Reserve(VisualPath.CellIds.Num());
	WorldNormals.Reserve(VisualPath.CellIds.Num());

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

		WorldPoints.Add(CellInfo.WorldCenter + OutwardNormal * LayerOffset);
		WorldNormals.Add(OutwardNormal);
	}

	if (WorldPoints.IsEmpty())
	{
		return false;
	}

	if (WorldPoints.Num() == 1)
	{
		const FSRPlanetSurfaceGridCellId& CellId = VisualPath.CellIds[0];
		FSRPlanetSurfaceGridCell Cell;
		if (SurfaceGrid->GetCellById(CellId, Cell))
		{
			const FTransform SurfaceGridTransform = SurfaceGrid->GetComponentTransform();
			FVector SingleTangent = SurfaceGridTransform.TransformPosition(Cell.Corner10) - SurfaceGridTransform.TransformPosition(Cell.Corner00);
			SingleTangent = SingleTangent - WorldNormals[0] * FVector::DotProduct(SingleTangent, WorldNormals[0]);
			if (SingleTangent.Normalize())
			{
				const FVector CenterPoint = WorldPoints[0];
				const FVector CenterNormal = WorldNormals[0];
				const float CellEdgeLength = FVector::Distance(
					SurfaceGridTransform.TransformPosition(Cell.Corner00),
					SurfaceGridTransform.TransformPosition(Cell.Corner10));
				const float HalfLength = FMath::Clamp(BeltWidth * 0.5f, 1.0f, FMath::Max(1.0f, CellEdgeLength * 0.35f));
				WorldPoints.Reset();
				WorldNormals.Reset();
				WorldPoints.Add(CenterPoint - SingleTangent * HalfLength);
				WorldPoints.Add(CenterPoint + SingleTangent * HalfLength);
				WorldNormals.Add(CenterNormal);
				WorldNormals.Add(CenterNormal);
			}
		}
	}

	if (WorldPoints.Num() < 2)
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

	const FTransform ComponentTransform = GetComponentTransform();
	const float HalfWidth = ResolveBeltHalfWidth(WorldPoints);
	const float HalfThickness = ResolveBeltHalfThickness(HalfWidth, VisualPath.LayerHeight);
	const float CenterSurfaceOffset = FMath::Max(0.0f, BeltSurfaceOffset) + HalfThickness;
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

		const int32 TopLeft0 = BeltMesh.AppendVertex(FVector3d(ComponentTransform.InverseTransformPosition(WorldTopLeft0)));
		const int32 TopRight0 = BeltMesh.AppendVertex(FVector3d(ComponentTransform.InverseTransformPosition(WorldTopRight0)));
		const int32 TopRight1 = BeltMesh.AppendVertex(FVector3d(ComponentTransform.InverseTransformPosition(WorldTopRight1)));
		const int32 TopLeft1 = BeltMesh.AppendVertex(FVector3d(ComponentTransform.InverseTransformPosition(WorldTopLeft1)));
		const int32 BottomLeft0 = BeltMesh.AppendVertex(FVector3d(ComponentTransform.InverseTransformPosition(WorldBottomLeft0)));
		const int32 BottomRight0 = BeltMesh.AppendVertex(FVector3d(ComponentTransform.InverseTransformPosition(WorldBottomRight0)));
		const int32 BottomRight1 = BeltMesh.AppendVertex(FVector3d(ComponentTransform.InverseTransformPosition(WorldBottomRight1)));
		const int32 BottomLeft1 = BeltMesh.AppendVertex(FVector3d(ComponentTransform.InverseTransformPosition(WorldBottomLeft1)));
		const FVector LocalNormal = ComponentTransform.InverseTransformVectorNoScale(Normal).GetSafeNormal();
		const float TextureWidth = FMath::Max(1.0f, HalfWidth * 2.0f);
		const float V0 = AccumulatedDistance / TextureWidth;
		const float V1 = (AccumulatedDistance + SegmentLength) / TextureWidth;
		const FVector LocalSideNormal = ComponentTransform.InverseTransformVectorNoScale(Side).GetSafeNormal();
		const FVector LocalTangentNormal = ComponentTransform.InverseTransformVectorNoScale(Tangent).GetSafeNormal();

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
