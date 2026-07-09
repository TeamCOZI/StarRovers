#include "SRPlanetSurfaceGridCellPose.h"

#include "Math/RotationMatrix.h"

void StarRovers::SurfaceGridCellPose::BuildCellWorldTransform(
	const FSRPlanetSurfaceGridCell& Cell,
	const FTransform& ComponentTransform,
	bool bUsingGeneratedGridCells,
	float GridSurfaceOffset,
	float HeightOffset,
	FSurfacePointResolver ResolveLocalSurfacePoint,
	FSurfacePointResolver ResolveWorldSurfacePoint,
	FTransform& OutTransform)
{
	FVector LocalTangent = ((Cell.Corner10 + Cell.Corner11) - (Cell.Corner00 + Cell.Corner01)) * 0.5f;
	if (LocalTangent.IsNearlyZero())
	{
		LocalTangent = FVector::CrossProduct(FVector::UpVector, Cell.LocalNormal);
		if (LocalTangent.IsNearlyZero())
		{
			LocalTangent = FVector::ForwardVector;
		}
	}

	const FVector LocalPosition = bUsingGeneratedGridCells
		? Cell.LocalCenter + (Cell.LocalNormal.GetSafeNormal() * HeightOffset)
		: ResolveLocalSurfacePoint(Cell.LocalNormal, HeightOffset);
	const FVector WorldPosition = ComponentTransform.TransformPosition(LocalPosition);
	const FVector WorldCorner00 = bUsingGeneratedGridCells
		? ComponentTransform.TransformPosition(Cell.Corner00 + (Cell.LocalNormal.GetSafeNormal() * GridSurfaceOffset))
		: ResolveWorldSurfacePoint(Cell.Corner00.GetSafeNormal(), GridSurfaceOffset);
	const FVector WorldCorner10 = bUsingGeneratedGridCells
		? ComponentTransform.TransformPosition(Cell.Corner10 + (Cell.LocalNormal.GetSafeNormal() * GridSurfaceOffset))
		: ResolveWorldSurfacePoint(Cell.Corner10.GetSafeNormal(), GridSurfaceOffset);
	const FVector WorldCorner01 = bUsingGeneratedGridCells
		? ComponentTransform.TransformPosition(Cell.Corner01 + (Cell.LocalNormal.GetSafeNormal() * GridSurfaceOffset))
		: ResolveWorldSurfacePoint(Cell.Corner01.GetSafeNormal(), GridSurfaceOffset);
	const FVector DerivedWorldNormal = FVector::CrossProduct(WorldCorner10 - WorldCorner00, WorldCorner01 - WorldCorner00).GetSafeNormal();
	const FVector WorldNormal = DerivedWorldNormal.IsNearlyZero()
		? ComponentTransform.TransformVectorNoScale(Cell.LocalNormal).GetSafeNormal()
		: DerivedWorldNormal;
	FVector WorldTangent = (WorldCorner10 - WorldCorner00).GetSafeNormal();
	if (WorldTangent.IsNearlyZero())
	{
		WorldTangent = ComponentTransform.TransformVectorNoScale(LocalTangent).GetSafeNormal();
	}
	const FQuat WorldRotation = FRotationMatrix::MakeFromXZ(WorldTangent, WorldNormal).ToQuat();

	OutTransform = FTransform(WorldRotation, WorldPosition, FVector::OneVector);
}

void StarRovers::SurfaceGridCellPose::GetCellWorldCorners(
	const FSRPlanetSurfaceGridCell& Cell,
	const FTransform& ComponentTransform,
	bool bUsingGeneratedGridCells,
	float GridSurfaceOffset,
	FSurfacePointResolver ResolveWorldSurfacePoint,
	FVector& OutCorner00,
	FVector& OutCorner10,
	FVector& OutCorner11,
	FVector& OutCorner01)
{
	if (bUsingGeneratedGridCells)
	{
		const FVector LocalOffset = Cell.LocalNormal.GetSafeNormal() * GridSurfaceOffset;
		OutCorner00 = ComponentTransform.TransformPosition(Cell.Corner00 + LocalOffset);
		OutCorner10 = ComponentTransform.TransformPosition(Cell.Corner10 + LocalOffset);
		OutCorner11 = ComponentTransform.TransformPosition(Cell.Corner11 + LocalOffset);
		OutCorner01 = ComponentTransform.TransformPosition(Cell.Corner01 + LocalOffset);
		return;
	}

	OutCorner00 = ResolveWorldSurfacePoint(Cell.Corner00.GetSafeNormal(), GridSurfaceOffset);
	OutCorner10 = ResolveWorldSurfacePoint(Cell.Corner10.GetSafeNormal(), GridSurfaceOffset);
	OutCorner11 = ResolveWorldSurfacePoint(Cell.Corner11.GetSafeNormal(), GridSurfaceOffset);
	OutCorner01 = ResolveWorldSurfacePoint(Cell.Corner01.GetSafeNormal(), GridSurfaceOffset);
}
