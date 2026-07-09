#include "SRPlanetSurfaceGridInteractionCellBuilder.h"

#include "SRPlanetSurfaceGridInteractionOverlayGeometry.h"
#include "SRPlanetSurfaceGridWireCells.h"
#include "SRPlanetSurfaceGridWirePrimitives.h"

#include "DynamicMesh/DynamicMesh3.h"

using namespace StarRovers::SurfaceGridInteractionOverlayGeometry;

void StarRovers::SurfaceGridInteractionCellBuilder::AppendInteractionCell(
	UE::Geometry::FDynamicMesh3& OverlayMesh,
	const FSRPlanetSurfaceGridCell& Cell,
	const FLinearColor& LineColor,
	float LineThickness,
	bool bUsingGeneratedGridCells,
	float GridSurfaceOffset,
	FCellLookup GetCellById,
	FSurfacePointResolver ResolveLocalSurfacePoint)
{
	const float HighlightOffset = FMath::Max(0.5f, LineThickness * 0.25f);

	if (bUsingGeneratedGridCells)
	{
		const FVector Offset = Cell.LocalNormal.GetSafeNormal() * HighlightOffset;
		AppendInteractionFilledQuad(
			OverlayMesh,
			Cell.Corner00 + Offset,
			Cell.Corner10 + Offset,
			Cell.Corner11 + Offset,
			Cell.Corner01 + Offset,
			LineColor);
		SurfaceGridWirePrimitives::AppendGridWireSegment(OverlayMesh, Cell.Corner00 + Offset, Cell.Corner10 + Offset, LineColor, LineThickness);
		SurfaceGridWirePrimitives::AppendGridWireSegment(OverlayMesh, Cell.Corner10 + Offset, Cell.Corner11 + Offset, LineColor, LineThickness);
		SurfaceGridWirePrimitives::AppendGridWireSegment(OverlayMesh, Cell.Corner11 + Offset, Cell.Corner01 + Offset, LineColor, LineThickness);
		SurfaceGridWirePrimitives::AppendGridWireSegment(OverlayMesh, Cell.Corner01 + Offset, Cell.Corner00 + Offset, LineColor, LineThickness);
		return;
	}

	const FVector FillPoint00 = ResolveLocalSurfacePoint(Cell.Corner00.GetSafeNormal(), GridSurfaceOffset + HighlightOffset);
	const FVector FillPoint10 = ResolveLocalSurfacePoint(Cell.Corner10.GetSafeNormal(), GridSurfaceOffset + HighlightOffset);
	const FVector FillPoint11 = ResolveLocalSurfacePoint(Cell.Corner11.GetSafeNormal(), GridSurfaceOffset + HighlightOffset);
	const FVector FillPoint01 = ResolveLocalSurfacePoint(Cell.Corner01.GetSafeNormal(), GridSurfaceOffset + HighlightOffset);
	AppendInteractionFilledQuad(OverlayMesh, FillPoint00, FillPoint10, FillPoint11, FillPoint01, LineColor);
	SurfaceGridWireCells::AppendGridWireCell(
		OverlayMesh,
		Cell,
		LineColor,
		LineThickness,
		false,
		nullptr,
		bUsingGeneratedGridCells,
		GridSurfaceOffset,
		GetCellById,
		ResolveLocalSurfacePoint);
}
