#include "Surface/SRPlanetSurfaceGrid.h"

#include "SRPlanetSurfaceGridRaycastQuery.h"

namespace SurfaceGridRaycast = StarRovers::SurfaceGridRaycast;

bool USRPlanetSurfaceGrid::RaycastCell(const FVector& RayOrigin, const FVector& RayDirection, FSRPlanetSurfaceGridCell& OutCell, FVector& OutHitLocation) const
{
	OutHitLocation = FVector::ZeroVector;
	if (bUsingGeneratedGridCells)
	{
		bool bGeneratedRaycastAttempted = false;
		int32 GeneratedCellIndex = INDEX_NONE;
		FVector GeneratedHitLocation = FVector::ZeroVector;
		if (SurfaceGridRaycast::RaycastGeneratedGrid(
			Cells,
			RaycastState,
			GetComponentTransform(),
			RayOrigin,
			RayDirection,
			GeneratedCellIndex,
			GeneratedHitLocation,
			bGeneratedRaycastAttempted))
		{
			OutCell = Cells[GeneratedCellIndex];
			OutHitLocation = GeneratedHitLocation;
			return true;
		}

		if (bGeneratedRaycastAttempted)
		{
			return false;
		}
	}

	if (!IntersectRayWithSurfaceSphere(RayOrigin, RayDirection, OutHitLocation))
	{
		OutCell = FSRPlanetSurfaceGridCell();
		return false;
	}

	if (!ProjectWorldLocationToCell(OutHitLocation, OutCell))
	{
		return false;
	}

	OutHitLocation = bUsingGeneratedGridCells
		? GetComponentTransform().TransformPosition(OutCell.LocalCenter)
		: ResolveWorldSurfacePoint(OutCell.LocalNormal, 0.0f);
	return true;
}

void USRPlanetSurfaceGrid::RebuildRaycastIndex()
{
	SurfaceGridRaycast::RebuildRaycastIndex(RaycastState, Cells, bUsingGeneratedGridCells, FaceResolution, GridSurfaceOffset);
}

float USRPlanetSurfaceGrid::GetEffectiveWorldRadius() const
{
	const FVector Scale3D = GetComponentTransform().GetScale3D().GetAbs();
	return PlanetRadius * Scale3D.GetMax();
}

bool USRPlanetSurfaceGrid::IntersectRayWithSurfaceSphere(const FVector& RayOrigin, const FVector& RayDirection, FVector& OutHitLocation) const
{
	const FVector SphereCenter = GetComponentLocation();
	const float SphereRadius = GetEffectiveWorldRadius()
		+ (DynamicMeshGeneration.bDynamicMeshGeneration ? FMath::Max(0.0f, DynamicMeshGeneration.DynamicMeshHeight) * GetComponentTransform().GetScale3D().GetAbsMax() : 0.0f);
	return SurfaceGridRaycast::IntersectRayWithSphere(RayOrigin, RayDirection, SphereCenter, SphereRadius, OutHitLocation);
}
