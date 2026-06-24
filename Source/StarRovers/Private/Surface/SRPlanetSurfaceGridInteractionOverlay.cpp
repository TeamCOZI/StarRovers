#include "Surface/SRPlanetSurfaceGrid.h"

#include "Celestial/SRCelestialBody.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "Materials/MaterialInterface.h"
#include "Surface/SRPlanetSurfaceGridVisualHelpers.h"

using namespace StarRovers::SurfaceGridVisual;

void USRPlanetSurfaceGrid::EnsureInteractionOverlay()
{
	if (InteractionOverlayMesh || IsTemplate())
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	InteractionOverlayMesh = NewObject<UDynamicMeshComponent>(OwnerActor, TEXT("SurfaceGridInteractionOverlay"));
	if (!InteractionOverlayMesh)
	{
		return;
	}

	InteractionOverlayMesh->SetupAttachment(this);
	InteractionOverlayMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InteractionOverlayMesh->SetGenerateOverlapEvents(false);
	InteractionOverlayMesh->SetCastShadow(false);
	InteractionOverlayMesh->SetVisibility(false);
	InteractionOverlayMesh->SetHiddenInGame(true);
	InteractionOverlayMesh->RegisterComponent();

	UMaterialInterface* GridMaterial = GridOverlayMaterial ? GridOverlayMaterial.Get() : GetMaterial(0);
	if (GridMaterial)
	{
		InteractionOverlayMesh->SetMaterial(0, GridMaterial);
	}
}

void USRPlanetSurfaceGrid::RequestInteractionHighlightRefresh()
{
	if (InteractionHighlightBatchDepth > 0)
	{
		bHasBatchedInteractionHighlightRefresh = true;
		return;
	}

	RefreshInteractionHighlight();
}

void USRPlanetSurfaceGrid::RefreshInteractionHighlight()
{
	ASRCelestialBody* OwnerBody = Cast<ASRCelestialBody>(GetOwner());
	const bool bUseDynamicMeshHighlight = !bGridVisible;
	const bool bAppliedDynamicMeshHighlight = bUseDynamicMeshHighlight && IsValid(OwnerBody)
		&& OwnerBody->ApplySurfaceCellHighlights(
			HoveredCellId,
			bHasHoveredCell,
			SelectedCellId,
			bHasSelectedCell,
			HoveredCellColor,
			SelectedCellColor);

	if (IsValid(OwnerBody) && (!bUseDynamicMeshHighlight || (!bHasHoveredCell && !bHasSelectedCell)))
	{
		OwnerBody->ClearSurfaceCellHighlights();
	}

	RebuildInteractionOverlayMesh(!bAppliedDynamicMeshHighlight);
}

void USRPlanetSurfaceGrid::RebuildInteractionOverlayMesh(bool bIncludeCellHighlightOverlay)
{
	EnsureInteractionOverlay();
	if (!InteractionOverlayMesh)
	{
		return;
	}

	UE::Geometry::FDynamicMesh3 OverlayMesh;
	OverlayMesh.EnableAttributes();
	OverlayMesh.Attributes()->EnablePrimaryColors();

	TSet<uint64> PatchDrawnEdges;
	PatchDrawnEdges.Reserve(320);

	FSRPlanetSurfaceGridCell SelectedCell;
	if (bHasSelectedCell && GetCellById(SelectedCellId, SelectedCell))
	{
		if (bIncludeCellHighlightOverlay)
		{
			AppendInteractionCell(OverlayMesh, SelectedCell, SelectedCellColor, DebugLineThickness * 2.5f);
		}
	}

	if (bHasHoveredCell)
	{
		FSRPlanetSurfaceGridCell HoveredCell;
		if (GetCellById(HoveredCellId, HoveredCell))
		{
			AppendInteractionGridPatch(OverlayMesh, HoveredCellId, HoveredCellColor, DebugLineThickness, PatchDrawnEdges);
			if (bIncludeCellHighlightOverlay && (!bHasSelectedCell || !(HoveredCellId == SelectedCellId)))
			{
				AppendInteractionCell(OverlayMesh, HoveredCell, HoveredCellColor, DebugLineThickness * 2.0f);
			}
		}

		TArray<FSRPlanetSurfaceGridCellId> PatchCellIds;
		if (GetInteractionGridPatchCellIds(HoveredCellId, PatchCellIds))
		{
			for (const FSRPlanetSurfaceGridCellId& PatchCellId : PatchCellIds)
			{
				FSRPlanetSurfaceGridCell PatchCell;
				if (GetCellById(PatchCellId, PatchCell) && PatchCell.bOccupied)
				{
					AppendInteractionCell(OverlayMesh, PatchCell, OccupiedCellColor, DebugLineThickness * 2.5f);
				}
			}
		}
	}

	for (const FSRPlanetSurfaceGridCellId& InputPortCellId : InputPortPreviewCellIds)
	{
		FSRPlanetSurfaceGridCell InputPortCell;
		if (GetCellById(InputPortCellId, InputPortCell))
		{
			AppendInteractionCell(OverlayMesh, InputPortCell, InputPortPreviewCellColor, DebugLineThickness * 3.0f);
		}
	}

	for (const FSRPlanetSurfaceGridCellId& OutputPortCellId : OutputPortPreviewCellIds)
	{
		FSRPlanetSurfaceGridCell OutputPortCell;
		if (GetCellById(OutputPortCellId, OutputPortCell))
		{
			AppendInteractionCell(OverlayMesh, OutputPortCell, OutputPortPreviewCellColor, DebugLineThickness * 3.0f);
		}
	}

	for (const FSRPlanetSurfaceGridCellId& DeletionCellId : DeletionPreviewCellIds)
	{
		FSRPlanetSurfaceGridCell DeletionCell;
		if (GetCellById(DeletionCellId, DeletionCell))
		{
			AppendInteractionCell(OverlayMesh, DeletionCell, DeletionPreviewCellColor, DebugLineThickness * 3.5f);
		}
	}

	InteractionOverlayMesh->SetMesh(MoveTemp(OverlayMesh));
	SetInteractionOverlayVisible(bGridVisible && (bHasHoveredCell || bHasSelectedCell || !InputPortPreviewCellIds.IsEmpty() || !OutputPortPreviewCellIds.IsEmpty() || !DeletionPreviewCellIds.IsEmpty()));
}

void USRPlanetSurfaceGrid::AppendInteractionGridPatch(
	UE::Geometry::FDynamicMesh3& OverlayMesh,
	const FSRPlanetSurfaceGridCellId& CenterCellId,
	const FLinearColor& BaseLineColor,
	float LineThickness,
	TSet<uint64>& DrawnEdges) const
{
	TArray<FSRPlanetSurfaceGridCell> PatchCells;
	TArray<FSRPlanetSurfaceGridCellId> PatchCellIds;
	if (!GetInteractionGridPatchCellIds(CenterCellId, PatchCellIds))
	{
		return;
	}
	PatchCells.Reserve(PatchCellIds.Num());

	FLinearColor PatchLineColor = BaseLineColor;
	PatchLineColor.A = FMath::Clamp(BaseLineColor.A * DebugLineOpacity, 0.0f, 1.0f);
	if (PatchLineColor.A <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	for (const FSRPlanetSurfaceGridCellId& PatchCellId : PatchCellIds)
	{
		FSRPlanetSurfaceGridCell PatchCell;
		if (GetCellById(PatchCellId, PatchCell))
		{
			PatchCells.Add(PatchCell);
		}
	}

	auto AppendDedupedSegment = [this, &OverlayMesh, &PatchLineColor, LineThickness, &DrawnEdges](
		const FVector& PointA,
		const FVector& PointB)
	{
		if (FVector::DistSquared(PointA, PointB) <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		const uint64 EdgeKey = BuildGridEdgeKey(PointA, PointB);
		if (DrawnEdges.Contains(EdgeKey))
		{
			return;
		}

		DrawnEdges.Add(EdgeKey);
		if (bUsingGeneratedGridCells)
		{
			AppendGridWireVolumeSegment(
				OverlayMesh,
				OffsetGeneratedGridWirePoint(PointA, GridSurfaceOffset),
				OffsetGeneratedGridWirePoint(PointB, GridSurfaceOffset),
				PatchLineColor,
				LineThickness);
			return;
		}

		AppendGridWireEdge(OverlayMesh, PointA, PointB, PatchLineColor, LineThickness);
	};

	for (const FSRPlanetSurfaceGridCell& PatchCell : PatchCells)
	{
		for (int32 EdgeIndex = 0; EdgeIndex < 4; ++EdgeIndex)
		{
			FVector EdgePointA;
			FVector EdgePointB;
			if (GetGridCellEdgePoints(PatchCell, EdgeIndex, EdgePointA, EdgePointB))
			{
				AppendDedupedSegment(EdgePointA, EdgePointB);
			}
		}
	}

	for (const FSRPlanetSurfaceGridCell& PatchCell : PatchCells)
	{
		for (const FSRPlanetSurfaceGridLineSegment& SideLineSegment : PatchCell.SideLineSegments)
		{
			if (!SideLineSegment.bHasAdjacentCell || !PatchCellIds.Contains(SideLineSegment.AdjacentCellId))
			{
				continue;
			}

			AppendDedupedSegment(SideLineSegment.LocalPointA, SideLineSegment.LocalPointB);
		}
	}
}

void USRPlanetSurfaceGrid::SetInteractionOverlayVisible(bool bNewVisible)
{
	if (InteractionOverlayMesh)
	{
		InteractionOverlayMesh->SetVisibility(bNewVisible);
		InteractionOverlayMesh->SetHiddenInGame(!bNewVisible);
	}
}

void USRPlanetSurfaceGrid::AppendInteractionCell(
	UE::Geometry::FDynamicMesh3& OverlayMesh,
	const FSRPlanetSurfaceGridCell& Cell,
	const FLinearColor& LineColor,
	float LineThickness) const
{
	const float HighlightOffset = FMath::Max(0.5f, LineThickness * 0.25f);
	auto AppendFilledQuad = [&OverlayMesh](const FVector& Point0, const FVector& Point1, const FVector& Point2, const FVector& Point3, FLinearColor FillColor)
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
	};

	if (bUsingGeneratedGridCells)
	{
		const FVector Offset = Cell.LocalNormal.GetSafeNormal() * HighlightOffset;
		AppendFilledQuad(Cell.Corner00 + Offset, Cell.Corner10 + Offset, Cell.Corner11 + Offset, Cell.Corner01 + Offset, LineColor);
		AppendGridWireSegment(OverlayMesh, Cell.Corner00 + Offset, Cell.Corner10 + Offset, LineColor, LineThickness);
		AppendGridWireSegment(OverlayMesh, Cell.Corner10 + Offset, Cell.Corner11 + Offset, LineColor, LineThickness);
		AppendGridWireSegment(OverlayMesh, Cell.Corner11 + Offset, Cell.Corner01 + Offset, LineColor, LineThickness);
		AppendGridWireSegment(OverlayMesh, Cell.Corner01 + Offset, Cell.Corner00 + Offset, LineColor, LineThickness);
		return;
	}

	const FVector FillPoint00 = ResolveLocalSurfacePoint(Cell.Corner00.GetSafeNormal(), GridSurfaceOffset + HighlightOffset);
	const FVector FillPoint10 = ResolveLocalSurfacePoint(Cell.Corner10.GetSafeNormal(), GridSurfaceOffset + HighlightOffset);
	const FVector FillPoint11 = ResolveLocalSurfacePoint(Cell.Corner11.GetSafeNormal(), GridSurfaceOffset + HighlightOffset);
	const FVector FillPoint01 = ResolveLocalSurfacePoint(Cell.Corner01.GetSafeNormal(), GridSurfaceOffset + HighlightOffset);
	AppendFilledQuad(FillPoint00, FillPoint10, FillPoint11, FillPoint01, LineColor);
	AppendGridWireCell(OverlayMesh, Cell, LineColor, LineThickness, false, nullptr);
}
