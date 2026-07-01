#include "Surface/SRPlanetSurfaceGrid.h"

#include "Celestial/SRCelestialBody.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "Materials/MaterialInterface.h"
#include "Surface/SRPlanetSurfaceGridVisualHelpers.h"

using namespace StarRovers::SurfaceGridVisual;

namespace
{
	FString FormatSurfacePatchCellId(const FSRPlanetSurfaceGridCellId& CellId)
	{
		return FString::Printf(
			TEXT("Face=%d X=%d Y=%d"),
			static_cast<int32>(CellId.Face),
			CellId.CellX,
			CellId.CellY);
	}

	bool IsContiguousRectangularCellRegion(const TArray<FSRPlanetSurfaceGridCellId>& CellIds)
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
}

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
	if (InteractionBatch.IsActive())
	{
		InteractionBatch.MarkHighlightRefreshPending();
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

	for (const FSRPlanetSurfaceGridCellId& OccupiedPreviewCellId : OccupiedPreviewCellIds)
	{
		FSRPlanetSurfaceGridCell OccupiedPreviewCell;
		if (GetCellById(OccupiedPreviewCellId, OccupiedPreviewCell))
		{
			AppendInteractionCell(OverlayMesh, OccupiedPreviewCell, OccupiedCellColor, DebugLineThickness * 2.5f);
		}
	}

	AppendInteractionCellRegion(OverlayMesh, AreaSelectionCellIds, AreaSelectionCellColor, DebugLineThickness * 2.0f, true);

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
			if (bIncludeCellHighlightOverlay && (!bHasSelectedCell || !(HoveredCellId == SelectedCellId)))
			{
				AppendInteractionCell(OverlayMesh, HoveredCell, HoveredCellColor, DebugLineThickness * 2.0f);
			}
		}

		FSRPlanetSurfaceGridCell HoveredCellInfo;
		if (GetCellById(HoveredCellId, HoveredCellInfo) && HoveredCellInfo.bOccupied)
		{
			AppendInteractionCell(OverlayMesh, HoveredCellInfo, OccupiedCellColor, DebugLineThickness * 2.5f);
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

	AppendInteractionCellRegion(OverlayMesh, DeletionPreviewCellIds, DeletionPreviewCellColor, DebugLineThickness * 3.5f, true);

	for (const FSRPlanetSurfaceGridCellId& InvalidCellId : InvalidPreviewCellIds)
	{
		FSRPlanetSurfaceGridCell InvalidCell;
		if (GetCellById(InvalidCellId, InvalidCell))
		{
			AppendInteractionCell(OverlayMesh, InvalidCell, InvalidPreviewCellColor, DebugLineThickness * 3.5f);
		}
	}

	if (bHasHoveredCell && bHoveredInteractionGridPatchVisible)
	{
		AppendInteractionGridPatch(OverlayMesh, HoveredCellId, HoveredCellColor, DebugLineThickness * 1.5f, PatchDrawnEdges);
	}

	InteractionOverlayMesh->SetMesh(MoveTemp(OverlayMesh));
	SetInteractionOverlayVisible(bGridVisible && (bHasHoveredCell || bHasSelectedCell || !AreaSelectionCellIds.IsEmpty() || !OccupiedPreviewCellIds.IsEmpty() || !InputPortPreviewCellIds.IsEmpty() || !OutputPortPreviewCellIds.IsEmpty() || !DeletionPreviewCellIds.IsEmpty() || !InvalidPreviewCellIds.IsEmpty()));
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
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[SR SurfacePatch] Result=PatchBuildFailed Center={%s}"),
			*FormatSurfacePatchCellId(CenterCellId));
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
	if (!PatchCellIds.Contains(CenterCellId))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[SR SurfacePatch] Result=CenterMissing Center={%s} PatchIds=%d PatchCells=%d"),
			*FormatSurfacePatchCellId(CenterCellId),
			PatchCellIds.Num(),
			PatchCells.Num());
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

void USRPlanetSurfaceGrid::AppendInteractionCellRegion(
	UE::Geometry::FDynamicMesh3& OverlayMesh,
	const TArray<FSRPlanetSurfaceGridCellId>& CellIds,
	const FLinearColor& LineColor,
	float LineThickness,
	bool bPreferCompactRectangles) const
{
	if (CellIds.IsEmpty())
	{
		return;
	}

	constexpr int32 CompactRegionCellThreshold = 16;
	if (bPreferCompactRectangles && CellIds.Num() >= CompactRegionCellThreshold)
	{
		TArray<FSRPlanetSurfaceGridCellId> FaceCellIds[6];
		bool bCanGroupByFace = true;
		for (const FSRPlanetSurfaceGridCellId& CellId : CellIds)
		{
			const int32 FaceIndex = static_cast<int32>(CellId.Face);
			if (FaceIndex >= 0 && FaceIndex < UE_ARRAY_COUNT(FaceCellIds))
			{
				FaceCellIds[FaceIndex].Add(CellId);
				continue;
			}

			bCanGroupByFace = false;
			break;
		}

		bool bAllFaceRegionsRectangular = bCanGroupByFace;
		if (bAllFaceRegionsRectangular)
		{
			for (const TArray<FSRPlanetSurfaceGridCellId>& RegionCellIds : FaceCellIds)
			{
				if (RegionCellIds.IsEmpty())
				{
					continue;
				}

				if (!IsContiguousRectangularCellRegion(RegionCellIds))
				{
					bAllFaceRegionsRectangular = false;
					break;
				}
			}
		}

		if (bAllFaceRegionsRectangular)
		{
			for (const TArray<FSRPlanetSurfaceGridCellId>& RegionCellIds : FaceCellIds)
			{
				if (!RegionCellIds.IsEmpty())
				{
					TryAppendRectangularInteractionCellRegion(OverlayMesh, RegionCellIds, LineColor, LineThickness);
				}
			}

			return;
		}
	}

	for (const FSRPlanetSurfaceGridCellId& CellId : CellIds)
	{
		FSRPlanetSurfaceGridCell Cell;
		if (GetCellById(CellId, Cell))
		{
			AppendInteractionCell(OverlayMesh, Cell, LineColor, LineThickness);
		}
	}
}

bool USRPlanetSurfaceGrid::TryAppendRectangularInteractionCellRegion(
	UE::Geometry::FDynamicMesh3& OverlayMesh,
	const TArray<FSRPlanetSurfaceGridCellId>& CellIds,
	const FLinearColor& LineColor,
	float LineThickness) const
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

	const int32 RegionCellsX = MaxCellX - MinCellX + 1;
	const int32 RegionCellsY = MaxCellY - MinCellY + 1;
	if (RegionCellsX <= 0 || RegionCellsY <= 0 || RegionCellIdSet.Num() != RegionCellsX * RegionCellsY)
	{
		return false;
	}

	TSet<uint64> DrawnEdges;
	DrawnEdges.Reserve((RegionCellsX + RegionCellsY) * 2);
	const float HighlightOffset = FMath::Max(0.5f, LineThickness * 0.25f);

	auto AppendBoundaryEdge = [this, &OverlayMesh, &LineColor, LineThickness, HighlightOffset, &DrawnEdges](
		const FSRPlanetSurfaceGridCellId& CellId,
		int32 EdgeIndex)
	{
		FSRPlanetSurfaceGridCell Cell;
		if (!GetCellById(CellId, Cell))
		{
			return;
		}

		FVector EdgePointA;
		FVector EdgePointB;
		if (!GetGridCellEdgePoints(Cell, EdgeIndex, EdgePointA, EdgePointB))
		{
			return;
		}

		if (bUsingGeneratedGridCells)
		{
			EdgePointA = OffsetGeneratedGridWirePoint(EdgePointA, GridSurfaceOffset + HighlightOffset);
			EdgePointB = OffsetGeneratedGridWirePoint(EdgePointB, GridSurfaceOffset + HighlightOffset);
		}

		if (FVector::DistSquared(EdgePointA, EdgePointB) <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		const uint64 EdgeKey = BuildGridEdgeKey(EdgePointA, EdgePointB);
		if (DrawnEdges.Contains(EdgeKey))
		{
			return;
		}
		DrawnEdges.Add(EdgeKey);

		if (bUsingGeneratedGridCells)
		{
			AppendGridWireSegment(OverlayMesh, EdgePointA, EdgePointB, LineColor, LineThickness);
			return;
		}

		AppendGridWireEdge(OverlayMesh, EdgePointA, EdgePointB, LineColor, LineThickness);
	};

	for (int32 CellX = MinCellX; CellX <= MaxCellX; ++CellX)
	{
		FSRPlanetSurfaceGridCellId TopCellId;
		TopCellId.Face = Face;
		TopCellId.CellX = CellX;
		TopCellId.CellY = MinCellY;
		AppendBoundaryEdge(TopCellId, 0);

		FSRPlanetSurfaceGridCellId BottomCellId;
		BottomCellId.Face = Face;
		BottomCellId.CellX = CellX;
		BottomCellId.CellY = MaxCellY;
		AppendBoundaryEdge(BottomCellId, 2);
	}

	for (int32 CellY = MinCellY; CellY <= MaxCellY; ++CellY)
	{
		FSRPlanetSurfaceGridCellId LeftCellId;
		LeftCellId.Face = Face;
		LeftCellId.CellX = MinCellX;
		LeftCellId.CellY = CellY;
		AppendBoundaryEdge(LeftCellId, 3);

		FSRPlanetSurfaceGridCellId RightCellId;
		RightCellId.Face = Face;
		RightCellId.CellX = MaxCellX;
		RightCellId.CellY = CellY;
		AppendBoundaryEdge(RightCellId, 1);
	}

	return true;
}
