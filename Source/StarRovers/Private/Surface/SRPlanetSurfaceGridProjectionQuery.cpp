#include "SRPlanetSurfaceGridProjectionQuery.h"

#include "Surface/SRPlanetSurfaceGridLibrary.h"

bool StarRovers::SurfaceGridProjectionQuery::ProjectWorldLocationToCell(
	const TArray<FSRPlanetSurfaceGridCell>& Cells,
	const FVector& WorldLocation,
	const FTransform& ComponentTransform,
	int32 FaceResolution,
	FCellByIdQuery GetCellById,
	FSRPlanetSurfaceGridCell& OutCell)
{
	if (Cells.IsEmpty())
	{
		OutCell = FSRPlanetSurfaceGridCell();
		return false;
	}

	const FVector LocalDirection = ComponentTransform.InverseTransformPosition(WorldLocation).GetSafeNormal();
	if (LocalDirection.IsNearlyZero())
	{
		OutCell = FSRPlanetSurfaceGridCell();
		return false;
	}

	FSRPlanetSurfaceGridCellId CellId;
	FVector2D UnusedFaceCoordinates = FVector2D::ZeroVector;
	if (!USRPlanetSurfaceGridLibrary::ProjectDirectionToCubeSphereCellId(LocalDirection, FaceResolution, CellId, UnusedFaceCoordinates))
	{
		OutCell = FSRPlanetSurfaceGridCell();
		return false;
	}

	return GetCellById(CellId, OutCell);
}
