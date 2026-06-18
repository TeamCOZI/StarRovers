#include "Surface/SRPlanetSurfaceGrid.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "SceneManagement.h"
#include "Surface/SRPlanetSurfaceGridVisualHelpers.h"
#include "Visual/SRLineThicknessUtils.h"

using namespace StarRovers::SurfaceGridVisual;

void USRPlanetSurfaceGrid::DrawDebugGrid(float Duration) const
{
	if (!GetWorld() || Cells.IsEmpty() || !bGridVisible)
	{
		return;
	}

	const FLinearColor DefaultDebugLineColor(DebugLineColor.R, DebugLineColor.G, DebugLineColor.B, DebugLineOpacity);
	const FColor DefaultLineColor = DefaultDebugLineColor.ToFColor(true);
	const FColor HoverLineColor = HoveredCellColor.ToFColor(true);
	const FColor SelectedLineColor = SelectedCellColor.ToFColor(true);

	FSRCameraInfo CameraInfo;
	FSRLineThicknessUtils::TryBuildPrimaryCameraInfo(GetWorld(), CameraInfo);

	float ReferenceViewDepth = FSRLineThicknessUtils::DefaultReferenceViewDepth;
	float ReferenceFieldOfViewDegrees = FSRLineThicknessUtils::DefaultReferenceFieldOfViewDegrees;
	FSRLineThicknessUtils::ResolveReferenceView(GetWorld(), ReferenceViewDepth, ReferenceFieldOfViewDegrees);

	TSet<uint64> DrawnEdges;
	DrawnEdges.Reserve(Cells.Num() * 2);
	auto DrawUniqueDefaultEdge = [this, &DrawnEdges, &DefaultLineColor, Duration, &CameraInfo, ReferenceViewDepth, ReferenceFieldOfViewDegrees](
		const FVector& CornerA,
		const FVector& CornerB)
	{
		const uint64 EdgeKey = BuildGridEdgeKey(CornerA, CornerB);
		if (DrawnEdges.Contains(EdgeKey))
		{
			return;
		}

		DrawnEdges.Add(EdgeKey);
		DrawDebugSurfaceLine(CornerA, CornerB, DefaultLineColor, Duration, DebugLineThickness, CameraInfo, ReferenceViewDepth, ReferenceFieldOfViewDegrees);
	};

	for (const FSRPlanetSurfaceGridCell& Cell : Cells)
	{
		for (int32 EdgeIndex = 0; EdgeIndex < 4; ++EdgeIndex)
		{
			FVector EdgePointA;
			FVector EdgePointB;
			if (GetGridCellEdgePoints(Cell, EdgeIndex, EdgePointA, EdgePointB))
			{
				DrawUniqueDefaultEdge(EdgePointA, EdgePointB);
			}
		}
	}

	for (const FSRPlanetSurfaceGridCell& Cell : Cells)
	{
		const bool bIsHovered = bHasHoveredCell && (Cell.CellId == HoveredCellId);
		const bool bIsSelected = bHasSelectedCell && (Cell.CellId == SelectedCellId);
		const bool bShouldHighlightCell = bIsHovered || bIsSelected;
		if (!bShouldHighlightCell)
		{
			continue;
		}

		const FColor LineColor = bIsSelected ? SelectedLineColor : HoverLineColor;
		const float LineThickness = bIsSelected ? DebugLineThickness * 2.5f : DebugLineThickness * 2.0f;
		for (int32 EdgeIndex = 0; EdgeIndex < 4; ++EdgeIndex)
		{
			FVector EdgePointA;
			FVector EdgePointB;
			if (GetGridCellEdgePoints(Cell, EdgeIndex, EdgePointA, EdgePointB))
			{
				DrawDebugSurfaceLine(EdgePointA, EdgePointB, LineColor, Duration, LineThickness, CameraInfo, ReferenceViewDepth, ReferenceFieldOfViewDegrees);
			}
		}
	}
}

void USRPlanetSurfaceGrid::DrawDebugSurfaceLine(
	const FVector& LocalDirectionA,
	const FVector& LocalDirectionB,
	const FColor& LineColor,
	float Duration,
	float LineThickness,
	const FSRCameraInfo& CameraInfo,
	float ReferenceViewDepth,
	float ReferenceFieldOfViewDegrees) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector DirectionA = LocalDirectionA.GetSafeNormal();
	const FVector DirectionB = LocalDirectionB.GetSafeNormal();
	if (DirectionA.IsNearlyZero() || DirectionB.IsNearlyZero())
	{
		return;
	}

	constexpr int32 SegmentCount = 8;
	const float EffectiveSurfaceOffset = FMath::Max(GridSurfaceOffset, FMath::Max(1.0f, LineThickness * 1.5f));
	FVector PreviousPoint = ResolveWorldSurfacePoint(DirectionA, EffectiveSurfaceOffset);

	for (int32 SegmentIndex = 1; SegmentIndex <= SegmentCount; ++SegmentIndex)
	{
		const float Alpha = static_cast<float>(SegmentIndex) / static_cast<float>(SegmentCount);
		const FVector SampleDirection = FMath::Lerp(DirectionA, DirectionB, Alpha).GetSafeNormal();
		if (SampleDirection.IsNearlyZero())
		{
			continue;
		}

		const FVector CurrentPoint = ResolveWorldSurfacePoint(SampleDirection, EffectiveSurfaceOffset);
		const FVector SegmentMidpoint = (PreviousPoint + CurrentPoint) * 0.5f;
		const float ScreenSpaceThickness = FSRLineThicknessUtils::ComputeWorldThicknessAtLocation(
			CameraInfo,
			SegmentMidpoint,
			LineThickness,
			ReferenceViewDepth,
			ReferenceFieldOfViewDegrees);
		DrawDebugLine(World, PreviousPoint, CurrentPoint, LineColor, false, Duration, SDPG_Foreground, FMath::Max(0.0f, ScreenSpaceThickness));
		PreviousPoint = CurrentPoint;
	}
}
