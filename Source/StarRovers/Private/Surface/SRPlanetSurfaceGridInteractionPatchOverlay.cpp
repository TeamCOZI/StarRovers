#include "SRPlanetSurfaceGridInteractionPatchOverlay.h"

#include "SRPlanetSurfaceGridInteractionOverlayGeometry.h"

#include "DynamicMesh/DynamicMesh3.h"

using namespace StarRovers::SurfaceGridInteractionOverlayGeometry;

void StarRovers::SurfaceGridInteractionPatchOverlay::AppendInteractionGridPatch(
	UE::Geometry::FDynamicMesh3& OverlayMesh,
	const FSRPlanetSurfaceGridCellId& CenterCellId,
	const FLinearColor& BaseLineColor,
	float LineThickness,
	float DebugLineOpacity,
	TSet<uint64>& DrawnEdges,
	FPatchCellIdQuery GetPatchCellIds,
	FAppendPatchRegion AppendPatchRegion)
{
	TArray<FSRPlanetSurfaceGridCellId> PatchCellIds;
	if (!GetPatchCellIds(CenterCellId, PatchCellIds))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[SR SurfacePatch] Result=PatchBuildFailed Center={%s}"),
			*FormatSurfacePatchCellId(CenterCellId));
		return;
	}

	FLinearColor PatchLineColor = BaseLineColor;
	PatchLineColor.A = FMath::Clamp(BaseLineColor.A * DebugLineOpacity, 0.0f, 1.0f);
	if (PatchLineColor.A <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	if (!PatchCellIds.Contains(CenterCellId))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[SR SurfacePatch] Result=CenterMissing Center={%s} PatchIds=%d"),
			*FormatSurfacePatchCellId(CenterCellId),
			PatchCellIds.Num());
	}

	AppendPatchRegion(OverlayMesh, PatchCellIds, PatchLineColor, LineThickness, DrawnEdges);
}
