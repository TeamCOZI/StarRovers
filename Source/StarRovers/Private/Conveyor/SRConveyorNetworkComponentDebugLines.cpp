#include "Conveyor/SRConveyorNetworkComponent.h"

#include "Components/LineBatchComponent.h"
#include "SceneManagement.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	constexpr uint8 ConveyorPathDebugLineDepthPriority = SDPG_Foreground;
	constexpr float ConveyorDebugLineSizeScale = 0.1f;

	FVector ResolveDebugNormal(const FSRPlanetSurfaceGridCellInfo& CellInfo, const FVector& SurfaceCenter)
	{
		FVector Normal = CellInfo.WorldNormal.GetSafeNormal();
		if (Normal.IsNearlyZero())
		{
			Normal = (CellInfo.WorldCenter - SurfaceCenter).GetSafeNormal();
		}
		else if (FVector::DotProduct(Normal, CellInfo.WorldCenter - SurfaceCenter) < 0.0f)
		{
			Normal *= -1.0f;
		}

		return Normal.IsNearlyZero() ? FVector::UpVector : Normal;
	}

	FVector ResolveDebugPoint(
		const FSRPlanetSurfaceGridCellInfo& CellInfo,
		const FVector& SurfaceCenter,
		float HeightOffset)
	{
		return CellInfo.WorldCenter + ResolveDebugNormal(CellInfo, SurfaceCenter) * HeightOffset;
	}

	void DrawDebugLine(
		ULineBatchComponent* LineBatchComponent,
		const FVector& StartPoint,
		const FVector& EndPoint,
		const FColor& LineColor,
		float LineThickness)
	{
		if (!IsValid(LineBatchComponent) || FVector::DistSquared(StartPoint, EndPoint) <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		LineBatchComponent->DrawLine(
			StartPoint,
			EndPoint,
			LineColor,
			ConveyorPathDebugLineDepthPriority,
			FMath::Max(0.0f, LineThickness),
			0.0f);
	}

	void DrawDebugCross(
		ULineBatchComponent* LineBatchComponent,
		const FVector& CenterPoint,
		const FVector& Normal,
		float Size,
		const FColor& LineColor,
		float LineThickness)
	{
		if (!IsValid(LineBatchComponent))
		{
			return;
		}

		const FVector SafeNormal = Normal.GetSafeNormal().IsNearlyZero() ? FVector::UpVector : Normal.GetSafeNormal();
		FVector Tangent = FVector::CrossProduct(SafeNormal, FVector::UpVector).GetSafeNormal();
		if (Tangent.IsNearlyZero())
		{
			Tangent = FVector::CrossProduct(SafeNormal, FVector::RightVector).GetSafeNormal();
		}
		const FVector Bitangent = FVector::CrossProduct(SafeNormal, Tangent).GetSafeNormal();
		const float HalfSize = FMath::Max(0.0f, Size) * 0.5f;

		DrawDebugLine(LineBatchComponent, CenterPoint - Tangent * HalfSize, CenterPoint + Tangent * HalfSize, LineColor, LineThickness);
		DrawDebugLine(LineBatchComponent, CenterPoint - Bitangent * HalfSize, CenterPoint + Bitangent * HalfSize, LineColor, LineThickness);
	}

	void DrawDirectionArrow(
		ULineBatchComponent* LineBatchComponent,
		const FVector& StartPoint,
		const FVector& EndPoint,
		const FVector& Normal,
		const FColor& LineColor,
		float LineThickness)
	{
		const FVector Direction = (EndPoint - StartPoint).GetSafeNormal();
		if (Direction.IsNearlyZero())
		{
			return;
		}

		DrawDebugLine(LineBatchComponent, StartPoint, EndPoint, LineColor, LineThickness);

		const float SegmentLength = FVector::Distance(StartPoint, EndPoint);
		const float ArrowLength = FMath::Clamp(
			SegmentLength * 0.25f,
			45.0f * ConveyorDebugLineSizeScale,
			150.0f * ConveyorDebugLineSizeScale);
		const float ArrowWidth = ArrowLength * 0.45f;
		FVector Side = FVector::CrossProduct(Normal, Direction).GetSafeNormal();
		if (Side.IsNearlyZero())
		{
			Side = FVector::CrossProduct(FVector::UpVector, Direction).GetSafeNormal();
		}
		if (Side.IsNearlyZero())
		{
			Side = FVector::RightVector;
		}

		const FVector ArrowBase = EndPoint - Direction * ArrowLength;
		DrawDebugLine(LineBatchComponent, EndPoint, ArrowBase + Side * ArrowWidth, LineColor, LineThickness);
		DrawDebugLine(LineBatchComponent, EndPoint, ArrowBase - Side * ArrowWidth, LineColor, LineThickness);
	}
}

void USRConveyorNetworkComponent::RefreshPathDebugLines(USRPlanetSurfaceGrid* SurfaceGrid)
{
	if (IsValid(PathDebugLineBatchComponent))
	{
		PathDebugLineBatchComponent->Flush();
	}

	if (!IsValid(SurfaceGrid) || (!bShowPathDebugLine && !bShowConnectionDebugLine))
	{
		return;
	}

	EnsurePathDebugLineBatchComponent();
	if (!IsValid(PathDebugLineBatchComponent))
	{
		return;
	}

	const FVector SurfaceCenter = SurfaceGrid->GetComponentTransform().GetLocation();
	if (bShowPathDebugLine)
	{
		const FColor LineColor = PathDebugLineColor.ToFColor(true);
		for (const FSRConveyorVisualPath& VisualPath : VisualPaths)
		{
			if (VisualPath.CellIds.IsEmpty())
			{
				continue;
			}

			FSRPlanetSurfaceGridCellInfo StartCellInfo;
			FSRPlanetSurfaceGridCellInfo EndCellInfo;
			if (!SurfaceGrid->GetCellInfoById(VisualPath.CellIds[0], StartCellInfo)
				|| !SurfaceGrid->GetCellInfoById(VisualPath.CellIds.Last(), EndCellInfo))
			{
				continue;
			}

			const float HeightOffset = static_cast<float>(FMath::Max(0, VisualPath.Layer)) * FMath::Max(0.0f, VisualPath.LayerHeight)
				+ BeltSurfaceOffset
				+ 40.0f * ConveyorDebugLineSizeScale;
			const FVector StartPoint = ResolveDebugPoint(StartCellInfo, SurfaceCenter, HeightOffset);
			const FVector EndPoint = ResolveDebugPoint(EndCellInfo, SurfaceCenter, HeightOffset);
			DrawDebugLine(PathDebugLineBatchComponent, StartPoint, EndPoint, LineColor, PathDebugLineThickness * ConveyorDebugLineSizeScale);
		}
	}

	if (!bShowConnectionDebugLine || Segments.IsEmpty())
	{
		return;
	}

	const FColor ConnectionColor = ConnectionDebugLineColor.ToFColor(true);
	const FColor BrokenConnectionColor = BrokenConnectionDebugLineColor.ToFColor(true);
	const FColor EndpointColor = EndpointDebugLineColor.ToFColor(true);
	const float LineThickness = FMath::Max(0.0f, ConnectionDebugLineThickness * ConveyorDebugLineSizeScale);
	for (const TPair<FSRConveyorLaneKey, FSRConveyorSegment>& SegmentPair : Segments)
	{
		const FSRConveyorSegment& Segment = SegmentPair.Value;

		FSRPlanetSurfaceGridCellInfo SegmentCellInfo;
		if (!SurfaceGrid->GetCellInfoById(Segment.Lane.CellId, SegmentCellInfo))
		{
			continue;
		}

		const float HeightOffset =
			static_cast<float>(FMath::Max(0, Segment.Lane.Layer)) * ResolveConveyorLayerHeight(SurfaceGrid, 0.0f)
			+ BeltSurfaceOffset
			+ ConnectionDebugLineHeightOffset * ConveyorDebugLineSizeScale;
		const FVector SegmentNormal = ResolveDebugNormal(SegmentCellInfo, SurfaceCenter);
		const FVector SegmentPoint = ResolveDebugPoint(SegmentCellInfo, SurfaceCenter, HeightOffset);

		TArray<ESRConveyorGridDirection> InputDirections;
		CollectConveyorInputDirections(Segment, InputDirections);
		if (InputDirections.IsEmpty())
		{
			DrawDebugCross(PathDebugLineBatchComponent, SegmentPoint, SegmentNormal, 80.0f * ConveyorDebugLineSizeScale, EndpointColor, LineThickness);
		}

		TArray<ESRConveyorGridDirection> OutputDirections;
		CollectConveyorOutputDirections(Segment, OutputDirections);
		if (OutputDirections.IsEmpty())
		{
			DrawDebugCross(PathDebugLineBatchComponent, SegmentPoint, SegmentNormal, 120.0f * ConveyorDebugLineSizeScale, EndpointColor, LineThickness);
			continue;
		}

		for (const ESRConveyorGridDirection OutputDirection : OutputDirections)
		{
			FSRConveyorLaneKey NextLaneKey;
			if (!TryResolveNextLaneByDirection(SurfaceGrid, Segment, OutputDirection, NextLaneKey))
			{
				DrawDebugCross(PathDebugLineBatchComponent, SegmentPoint, SegmentNormal, 120.0f * ConveyorDebugLineSizeScale, BrokenConnectionColor, LineThickness);
				continue;
			}

			FSRPlanetSurfaceGridCellInfo NextCellInfo;
			if (!SurfaceGrid->GetCellInfoById(NextLaneKey.CellId, NextCellInfo))
			{
				DrawDebugCross(PathDebugLineBatchComponent, SegmentPoint, SegmentNormal, 120.0f * ConveyorDebugLineSizeScale, BrokenConnectionColor, LineThickness);
				continue;
			}

			const FVector NextNormal = ResolveDebugNormal(NextCellInfo, SurfaceCenter);
			const FVector NextPoint = ResolveDebugPoint(NextCellInfo, SurfaceCenter, HeightOffset);
			const FColor DirectionColor = Segments.Contains(NextLaneKey) ? ConnectionColor : BrokenConnectionColor;
			DrawDirectionArrow(
				PathDebugLineBatchComponent,
				SegmentPoint,
				NextPoint,
				(SegmentNormal + NextNormal).GetSafeNormal(),
				DirectionColor,
				LineThickness);
		}
	}
}
