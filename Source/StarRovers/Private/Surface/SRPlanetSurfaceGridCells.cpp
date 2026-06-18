#include "Surface/SRPlanetSurfaceGrid.h"

#include "Math/RotationMatrix.h"
#include "Surface/SRPlanetSurfaceGridLibrary.h"

int32 USRPlanetSurfaceGrid::GetCellCount() const
{
	return Cells.Num();
}

TArray<FSRPlanetSurfaceGridCell> USRPlanetSurfaceGrid::GetCells() const
{
	return Cells;
}

const TArray<FSRPlanetSurfaceGridCell>& USRPlanetSurfaceGrid::GetCellsRef() const
{
	return Cells;
}

bool USRPlanetSurfaceGrid::GetCellById(const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCell& OutCell) const
{
	int32 CellIndex = INDEX_NONE;
	if (!GetCellIndex(CellId, CellIndex))
	{
		OutCell = FSRPlanetSurfaceGridCell();
		return false;
	}

	OutCell = Cells[CellIndex];
	return true;
}

bool USRPlanetSurfaceGrid::GetCellInfoById(const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCellInfo& OutCellInfo) const
{
	FSRPlanetSurfaceGridCellInfo FoundCellInfo;
	if (!GetStoredCellInfoById(CellId, FoundCellInfo))
	{
		OutCellInfo = FSRPlanetSurfaceGridCellInfo();
		return false;
	}

	OutCellInfo = ResolveRuntimeCellInfo(FoundCellInfo);
	return true;
}

bool USRPlanetSurfaceGrid::GetCellNeighbors(const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCellNeighbors& OutNeighbors) const
{
	FSRPlanetSurfaceGridCell Cell;
	if (!GetCellById(CellId, Cell))
	{
		OutNeighbors = FSRPlanetSurfaceGridCellNeighbors();
		return false;
	}

	OutNeighbors = Cell.Neighbors;
	return true;
}

bool USRPlanetSurfaceGrid::GetFootprintCellIds(
	const FSRPlanetSurfaceGridCellId& OriginCellId,
	int32 FootprintCellsX,
	int32 FootprintCellsY,
	TArray<FSRPlanetSurfaceGridCellId>& OutCellIds) const
{
	OutCellIds.Reset();

	const int32 SafeFootprintCellsX = FMath::Max(1, FootprintCellsX);
	const int32 SafeFootprintCellsY = FMath::Max(1, FootprintCellsY);
	const int32 SafeFaceResolution = FMath::Max(1, FaceResolution);
	if (!OriginCellId.IsValid(SafeFaceResolution))
	{
		return false;
	}

	const int32 StartCellX = OriginCellId.CellX - (SafeFootprintCellsX / 2);
	const int32 StartCellY = OriginCellId.CellY - (SafeFootprintCellsY / 2);
	const int32 EndCellX = StartCellX + SafeFootprintCellsX - 1;
	const int32 EndCellY = StartCellY + SafeFootprintCellsY - 1;
	if (StartCellX < 0 || StartCellY < 0 || EndCellX >= SafeFaceResolution || EndCellY >= SafeFaceResolution)
	{
		return false;
	}

	OutCellIds.Reserve(SafeFootprintCellsX * SafeFootprintCellsY);
	for (int32 CellY = StartCellY; CellY <= EndCellY; ++CellY)
	{
		for (int32 CellX = StartCellX; CellX <= EndCellX; ++CellX)
		{
			FSRPlanetSurfaceGridCellId FootprintCellId;
			FootprintCellId.Face = OriginCellId.Face;
			FootprintCellId.CellX = CellX;
			FootprintCellId.CellY = CellY;
			int32 FootprintCellIndex = INDEX_NONE;
			if (!GetCellIndex(FootprintCellId, FootprintCellIndex))
			{
				OutCellIds.Reset();
				return false;
			}

			OutCellIds.Add(FootprintCellId);
		}
	}

	return !OutCellIds.IsEmpty();
}

bool USRPlanetSurfaceGrid::GetCellWorldTransform(const FSRPlanetSurfaceGridCellId& CellId, float HeightOffset, FTransform& OutTransform) const
{
	FSRPlanetSurfaceGridCell Cell;
	if (!GetCellById(CellId, Cell))
	{
		OutTransform = FTransform::Identity;
		return false;
	}

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
	const FVector WorldPosition = GetComponentTransform().TransformPosition(LocalPosition);
	const FVector WorldCorner00 = bUsingGeneratedGridCells
		? GetComponentTransform().TransformPosition(Cell.Corner00 + (Cell.LocalNormal.GetSafeNormal() * GridSurfaceOffset))
		: ResolveWorldSurfacePoint(Cell.Corner00.GetSafeNormal(), GridSurfaceOffset);
	const FVector WorldCorner10 = bUsingGeneratedGridCells
		? GetComponentTransform().TransformPosition(Cell.Corner10 + (Cell.LocalNormal.GetSafeNormal() * GridSurfaceOffset))
		: ResolveWorldSurfacePoint(Cell.Corner10.GetSafeNormal(), GridSurfaceOffset);
	const FVector WorldCorner01 = bUsingGeneratedGridCells
		? GetComponentTransform().TransformPosition(Cell.Corner01 + (Cell.LocalNormal.GetSafeNormal() * GridSurfaceOffset))
		: ResolveWorldSurfacePoint(Cell.Corner01.GetSafeNormal(), GridSurfaceOffset);
	const FVector DerivedWorldNormal = FVector::CrossProduct(WorldCorner10 - WorldCorner00, WorldCorner01 - WorldCorner00).GetSafeNormal();
	const FVector WorldNormal = DerivedWorldNormal.IsNearlyZero()
		? GetComponentTransform().TransformVectorNoScale(Cell.LocalNormal).GetSafeNormal()
		: DerivedWorldNormal;
	FVector WorldTangent = (WorldCorner10 - WorldCorner00).GetSafeNormal();
	if (WorldTangent.IsNearlyZero())
	{
		WorldTangent = GetComponentTransform().TransformVectorNoScale(LocalTangent).GetSafeNormal();
	}
	const FQuat WorldRotation = FRotationMatrix::MakeFromXZ(WorldTangent, WorldNormal).ToQuat();

	OutTransform = FTransform(WorldRotation, WorldPosition, FVector::OneVector);
	return true;
}

bool USRPlanetSurfaceGrid::GetCellWorldCorners(const FSRPlanetSurfaceGridCellId& CellId, FVector& OutCorner00, FVector& OutCorner10, FVector& OutCorner11, FVector& OutCorner01) const
{
	FSRPlanetSurfaceGridCell Cell;
	if (!GetCellById(CellId, Cell))
	{
		OutCorner00 = FVector::ZeroVector;
		OutCorner10 = FVector::ZeroVector;
		OutCorner11 = FVector::ZeroVector;
		OutCorner01 = FVector::ZeroVector;
		return false;
	}

	if (bUsingGeneratedGridCells)
	{
		const FVector LocalOffset = Cell.LocalNormal.GetSafeNormal() * GridSurfaceOffset;
		OutCorner00 = GetComponentTransform().TransformPosition(Cell.Corner00 + LocalOffset);
		OutCorner10 = GetComponentTransform().TransformPosition(Cell.Corner10 + LocalOffset);
		OutCorner11 = GetComponentTransform().TransformPosition(Cell.Corner11 + LocalOffset);
		OutCorner01 = GetComponentTransform().TransformPosition(Cell.Corner01 + LocalOffset);
		return true;
	}

	OutCorner00 = ResolveWorldSurfacePoint(Cell.Corner00.GetSafeNormal(), GridSurfaceOffset);
	OutCorner10 = ResolveWorldSurfacePoint(Cell.Corner10.GetSafeNormal(), GridSurfaceOffset);
	OutCorner11 = ResolveWorldSurfacePoint(Cell.Corner11.GetSafeNormal(), GridSurfaceOffset);
	OutCorner01 = ResolveWorldSurfacePoint(Cell.Corner01.GetSafeNormal(), GridSurfaceOffset);
	return true;
}

bool USRPlanetSurfaceGrid::ProjectWorldLocationToCell(const FVector& WorldLocation, FSRPlanetSurfaceGridCell& OutCell) const
{
	if (Cells.IsEmpty())
	{
		OutCell = FSRPlanetSurfaceGridCell();
		return false;
	}

	const FVector LocalDirection = GetComponentTransform().InverseTransformPosition(WorldLocation).GetSafeNormal();
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

bool USRPlanetSurfaceGrid::GetCellIndex(const FSRPlanetSurfaceGridCellId& CellId, int32& OutIndex) const
{
	const int32 FlatIndex = GetFlatCellIndex(CellId);
	if (CellIndexByFlatId.IsValidIndex(FlatIndex))
	{
		OutIndex = CellIndexByFlatId[FlatIndex];
		return Cells.IsValidIndex(OutIndex);
	}

	if (const int32* FoundIndex = CellIndexById.Find(CellId))
	{
		OutIndex = *FoundIndex;
		return Cells.IsValidIndex(OutIndex);
	}

	OutIndex = INDEX_NONE;
	return false;
}

int32 USRPlanetSurfaceGrid::GetFlatCellIndex(const FSRPlanetSurfaceGridCellId& CellId) const
{
	if (!CellId.IsValid(FaceResolution))
	{
		return INDEX_NONE;
	}

	return ((static_cast<int32>(CellId.Face) * FaceResolution) + CellId.CellY) * FaceResolution + CellId.CellX;
}

void USRPlanetSurfaceGrid::RebuildCellIndex()
{
	CellIndexById.Reset();
	CellIndexByFlatId.Init(INDEX_NONE, 6 * FaceResolution * FaceResolution);

	for (int32 CellIndex = 0; CellIndex < Cells.Num(); ++CellIndex)
	{
		const int32 FlatIndex = GetFlatCellIndex(Cells[CellIndex].CellId);
		if (CellIndexByFlatId.IsValidIndex(FlatIndex))
		{
			CellIndexByFlatId[FlatIndex] = CellIndex;
		}
		else
		{
			CellIndexById.Add(Cells[CellIndex].CellId, CellIndex);
		}
	}
}

FSRPlanetSurfaceGridCellInfo USRPlanetSurfaceGrid::BuildCellInfo(const FSRPlanetSurfaceGridCell& Cell) const
{
	FSRPlanetSurfaceGridCellInfo CellInfo;
	CellInfo.CellId = Cell.CellId;
	CellInfo.FaceResolution = FaceResolution;
	CellInfo.FaceCellIndex = Cell.CellId.IsValid(FaceResolution)
		? Cell.CellId.CellY * FaceResolution + Cell.CellId.CellX
		: Cell.CellId.CellX;
	CellInfo.DisplayCellX = Cell.CellId.CellX;
	CellInfo.DisplayCellY = Cell.CellId.CellY;
	if (Cell.CellId.Face == ESRCubeSphereFace::PositiveZ || Cell.CellId.Face == ESRCubeSphereFace::NegativeZ)
	{
		const int32 SafeFaceResolution = FMath::Max(1, FaceResolution);
		CellInfo.DisplayCellX = SafeFaceResolution - 1 - Cell.CellId.CellX;
		CellInfo.DisplayCellY = SafeFaceResolution - 1 - Cell.CellId.CellY;
	}
	CellInfo.DisplayCellIndex = Cell.CellId.IsValid(FaceResolution)
		? CellInfo.DisplayCellY * FaceResolution + CellInfo.DisplayCellX
		: CellInfo.DisplayCellX;
	CellInfo.FaceUVMin = Cell.FaceUVMin;
	CellInfo.FaceUVMax = Cell.FaceUVMax;
	CellInfo.FaceUVCenter = (Cell.FaceUVMin + Cell.FaceUVMax) * 0.5f;
	CellInfo.LocalCenter = Cell.LocalCenter;
	CellInfo.LocalNormal = Cell.LocalNormal;
	const FVector LatitudeDirection = Cell.LocalCenter.GetSafeNormal();
	const FVector FallbackLatitudeDirection = LatitudeDirection.IsNearlyZero()
		? Cell.LocalNormal.GetSafeNormal()
		: LatitudeDirection;
	const float LatitudeSin = static_cast<float>(FMath::Clamp(FallbackLatitudeDirection.Z, -1.0, 1.0));
	CellInfo.LatitudeDegrees = FMath::RadiansToDegrees(FMath::Asin(LatitudeSin));
	CellInfo.ApproxSurfaceArea = Cell.ApproxSurfaceArea;
	CellInfo.Biome = Cell.Biome;
	CellInfo.BiomeId = Cell.BiomeId;
	CellInfo.WaterRole = Cell.WaterRole;
	CellInfo.Neighbors = Cell.Neighbors;
	CellInfo.bOccupied = Cell.bOccupied;
	CellInfo.OccupantId = Cell.OccupantId;
	CellInfo.bCanConstruct = !Cell.bOccupied;
	return CellInfo;
}

FSRPlanetSurfaceGridCellInfo USRPlanetSurfaceGrid::ResolveRuntimeCellInfo(const FSRPlanetSurfaceGridCellInfo& CellInfo) const
{
	FSRPlanetSurfaceGridCellInfo RuntimeCellInfo = CellInfo;
	const FTransform& ComponentTransform = GetComponentTransform();
	RuntimeCellInfo.WorldCenter = ComponentTransform.TransformPosition(CellInfo.LocalCenter);
	RuntimeCellInfo.WorldNormal = ComponentTransform.TransformVectorNoScale(CellInfo.LocalNormal).GetSafeNormal();
	if (RuntimeCellInfo.WorldNormal.IsNearlyZero())
	{
		RuntimeCellInfo.WorldNormal = FVector::UpVector;
	}
	return RuntimeCellInfo;
}

void USRPlanetSurfaceGrid::RebuildCellInfoIndex()
{
	CellInfoById.Reset();
	CellInfoByFlatId.Reset();
	CellInfoByFlatId.SetNum(6 * FaceResolution * FaceResolution);

	int32 FaceCellCounts[6] = {};
	for (const FSRPlanetSurfaceGridCell& Cell : Cells)
	{
		FSRPlanetSurfaceGridCellInfo CellInfo = BuildCellInfo(Cell);
		int32& FaceCellCount = FaceCellCounts[static_cast<int32>(Cell.CellId.Face)];
		CellInfo.FaceCellIndex = FaceCellCount;
		++FaceCellCount;
		StoreCellInfo(CellInfo);
	}
}

bool USRPlanetSurfaceGrid::GetStoredCellInfoById(const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCellInfo& OutCellInfo) const
{
	const int32 FlatIndex = GetFlatCellIndex(CellId);
	if (CellInfoByFlatId.IsValidIndex(FlatIndex)
		&& CellIndexByFlatId.IsValidIndex(FlatIndex)
		&& Cells.IsValidIndex(CellIndexByFlatId[FlatIndex])
		&& CellInfoByFlatId[FlatIndex].CellId == CellId)
	{
		OutCellInfo = CellInfoByFlatId[FlatIndex];
		return true;
	}

	if (const FSRPlanetSurfaceGridCellInfo* FoundCellInfo = CellInfoById.Find(CellId))
	{
		OutCellInfo = *FoundCellInfo;
		return true;
	}

	return false;
}

void USRPlanetSurfaceGrid::StoreCellInfo(const FSRPlanetSurfaceGridCellInfo& CellInfo)
{
	const int32 FlatIndex = GetFlatCellIndex(CellInfo.CellId);
	if (CellInfoByFlatId.IsValidIndex(FlatIndex))
	{
		CellInfoByFlatId[FlatIndex] = CellInfo;
	}
	else
	{
		CellInfoById.Add(CellInfo.CellId, CellInfo);
	}
}