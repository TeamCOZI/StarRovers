#include "Surface/SRPlanetSurfaceGrid.h"

#include "SRPlanetSurfaceGridCellIndex.h"
#include "SRPlanetSurfaceGridCellPose.h"
#include "SRPlanetSurfaceGridCellQuery.h"
#include "SRPlanetSurfaceGridFootprint.h"
#include "SRPlanetSurfaceGridOwnerBody.h"
#include "SRPlanetSurfaceGridProjectionQuery.h"
#include "SRPlanetSurfaceGridTemperatureState.h"

namespace SurfaceGridCellIndex = StarRovers::SurfaceGridCellIndex;
namespace SurfaceGridCellPose = StarRovers::SurfaceGridCellPose;
namespace SurfaceGridCellQuery = StarRovers::SurfaceGridCellQuery;
namespace SurfaceGridFootprint = StarRovers::SurfaceGridFootprint;
namespace SurfaceGridOwnerBody = StarRovers::SurfaceGridOwnerBody;
namespace SurfaceGridProjectionQuery = StarRovers::SurfaceGridProjectionQuery;
namespace SurfaceGridTemperatureState = StarRovers::SurfaceGridTemperatureState;

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
	return SurfaceGridCellQuery::GetCellById(
		Cells,
		CellId,
		[this](const FSRPlanetSurfaceGridCellId& CandidateCellId, int32& OutIndex)
		{
			return GetCellIndex(CandidateCellId, OutIndex);
		},
		OutCell);
}

bool USRPlanetSurfaceGrid::GetCellInfoById(const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCellInfo& OutCellInfo) const
{
	return SurfaceGridCellQuery::GetCellInfoById(
		CellId,
		[this](const FSRPlanetSurfaceGridCellId& CandidateCellId, FSRPlanetSurfaceGridCellInfo& FoundCellInfo)
		{
			return GetStoredCellInfoById(CandidateCellId, FoundCellInfo);
		},
		[this](const FSRPlanetSurfaceGridCellInfo& CellInfo)
		{
			return ResolveRuntimeCellInfo(CellInfo);
		},
		OutCellInfo);
}

bool USRPlanetSurfaceGrid::GetCellNeighbors(const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCellNeighbors& OutNeighbors) const
{
	return SurfaceGridCellQuery::GetCellNeighbors(
		CellId,
		FaceResolution,
		[this](const FSRPlanetSurfaceGridCellId& CandidateCellId, int32& OutIndex)
		{
			return GetCellIndex(CandidateCellId, OutIndex);
		},
		OutNeighbors);
}

ESRFacilityTemperatureState USRPlanetSurfaceGrid::ResolveTemperatureStateFromSurfaceTemperature(float SurfaceTemperature)
{
	return SurfaceGridTemperatureState::ResolveTemperatureStateFromSurfaceTemperature(SurfaceTemperature);
}

bool USRPlanetSurfaceGrid::GetCellTemperatureState(const FSRPlanetSurfaceGridCellId& CellId, ESRFacilityTemperatureState& OutTemperatureState) const
{
	return SurfaceGridTemperatureState::GetCellTemperatureState(
		Cells,
		CellId,
		OutTemperatureState,
		[this](const FSRPlanetSurfaceGridCellId& CandidateCellId, int32& OutIndex)
		{
			return GetCellIndex(CandidateCellId, OutIndex);
		});
}

bool USRPlanetSurfaceGrid::SetCellTemperatureState(const FSRPlanetSurfaceGridCellId& CellId, ESRFacilityTemperatureState TemperatureState)
{
	const bool bUpdated = SurfaceGridTemperatureState::SetCellTemperatureState(
		Cells,
		CellId,
		TemperatureState,
		[this](const FSRPlanetSurfaceGridCellId& CandidateCellId, int32& OutIndex)
		{
			return GetCellIndex(CandidateCellId, OutIndex);
		},
		[this](const FSRPlanetSurfaceGridCell& Cell)
		{
			return BuildCellInfo(Cell);
		},
		[this](const FSRPlanetSurfaceGridCellId& CandidateCellId, FSRPlanetSurfaceGridCellInfo& OutCellInfo)
		{
			return GetStoredCellInfoById(CandidateCellId, OutCellInfo);
		},
		[this](const FSRPlanetSurfaceGridCellInfo& CellInfo)
		{
			StoreCellInfo(CellInfo);
		});
	if (bUpdated)
	{
		SurfaceGridOwnerBody::ApplySurfaceTemperatureStateColor(GetOwner(), CellId, TemperatureState);
		RequestInteractionHighlightRefresh();
	}
	return bUpdated;
}

bool USRPlanetSurfaceGrid::SetCellSurfaceTemperature(const FSRPlanetSurfaceGridCellId& CellId, float SurfaceTemperature)
{
	const bool bUpdated = SurfaceGridTemperatureState::SetCellSurfaceTemperature(
		Cells,
		CellId,
		SurfaceTemperature,
		[this](const FSRPlanetSurfaceGridCellId& CandidateCellId, int32& OutIndex)
		{
			return GetCellIndex(CandidateCellId, OutIndex);
		},
		[this](const FSRPlanetSurfaceGridCell& Cell)
		{
			return BuildCellInfo(Cell);
		},
		[this](const FSRPlanetSurfaceGridCellId& CandidateCellId, FSRPlanetSurfaceGridCellInfo& OutCellInfo)
		{
			return GetStoredCellInfoById(CandidateCellId, OutCellInfo);
		},
		[this](const FSRPlanetSurfaceGridCellInfo& CellInfo)
		{
			StoreCellInfo(CellInfo);
		});
	if (bUpdated)
	{
		const ESRFacilityTemperatureState TemperatureState = SurfaceGridTemperatureState::ResolveTemperatureStateFromSurfaceTemperature(SurfaceTemperature);
		SurfaceGridOwnerBody::ApplySurfaceTemperatureStateColor(GetOwner(), CellId, TemperatureState);
		RequestInteractionHighlightRefresh();
	}
	return bUpdated;
}

bool USRPlanetSurfaceGrid::GetFootprintCellIds(
	const FSRPlanetSurfaceGridCellId& OriginCellId,
	int32 FootprintCellsX,
	int32 FootprintCellsY,
	TArray<FSRPlanetSurfaceGridCellId>& OutCellIds) const
{
	return SurfaceGridFootprint::BuildFootprintCellIds(
		OriginCellId,
		FootprintCellsX,
		FootprintCellsY,
		FaceResolution,
		[this](const FSRPlanetSurfaceGridCellId& CellId, int32& OutIndex)
		{
			return GetCellIndex(CellId, OutIndex);
		},
		OutCellIds);
}

bool USRPlanetSurfaceGrid::GetCellWorldTransform(const FSRPlanetSurfaceGridCellId& CellId, float HeightOffset, FTransform& OutTransform) const
{
	FSRPlanetSurfaceGridCell Cell;
	if (!GetCellById(CellId, Cell))
	{
		OutTransform = FTransform::Identity;
		return false;
	}

	SurfaceGridCellPose::BuildCellWorldTransform(
		Cell,
		GetComponentTransform(),
		bUsingGeneratedGridCells,
		GridSurfaceOffset,
		HeightOffset,
		[this](const FVector& LocalUnitDirection, float SurfaceHeightOffset)
		{
			return ResolveLocalSurfacePoint(LocalUnitDirection, SurfaceHeightOffset);
		},
		[this](const FVector& LocalUnitDirection, float SurfaceHeightOffset)
		{
			return ResolveWorldSurfacePoint(LocalUnitDirection, SurfaceHeightOffset);
		},
		OutTransform);
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

	SurfaceGridCellPose::GetCellWorldCorners(
		Cell,
		GetComponentTransform(),
		bUsingGeneratedGridCells,
		GridSurfaceOffset,
		[this](const FVector& LocalUnitDirection, float SurfaceHeightOffset)
		{
			return ResolveWorldSurfacePoint(LocalUnitDirection, SurfaceHeightOffset);
		},
		OutCorner00,
		OutCorner10,
		OutCorner11,
		OutCorner01);
	return true;
}

bool USRPlanetSurfaceGrid::ProjectWorldLocationToCell(const FVector& WorldLocation, FSRPlanetSurfaceGridCell& OutCell) const
{
	return SurfaceGridProjectionQuery::ProjectWorldLocationToCell(
		Cells,
		WorldLocation,
		GetComponentTransform(),
		FaceResolution,
		[this](const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCell& FoundCell)
		{
			return GetCellById(CellId, FoundCell);
		},
		OutCell);
}

bool USRPlanetSurfaceGrid::GetCellIndex(const FSRPlanetSurfaceGridCellId& CellId, int32& OutIndex) const
{
	return SurfaceGridCellIndex::GetCellIndex(CellIndexState, Cells, FaceResolution, CellId, OutIndex);
}

int32 USRPlanetSurfaceGrid::GetFlatCellIndex(const FSRPlanetSurfaceGridCellId& CellId) const
{
	return SurfaceGridCellIndex::GetFlatCellIndex(CellId, FaceResolution);
}

void USRPlanetSurfaceGrid::RebuildCellIndex()
{
	SurfaceGridCellIndex::RebuildCellIndex(CellIndexState, Cells, FaceResolution);
}

FSRPlanetSurfaceGridCellInfo USRPlanetSurfaceGrid::BuildCellInfo(const FSRPlanetSurfaceGridCell& Cell) const
{
	return SurfaceGridCellIndex::BuildCellInfo(Cell, FaceResolution);
}

FSRPlanetSurfaceGridCellInfo USRPlanetSurfaceGrid::ResolveRuntimeCellInfo(const FSRPlanetSurfaceGridCellInfo& CellInfo) const
{
	return SurfaceGridCellIndex::ResolveRuntimeCellInfo(CellInfo, GetComponentTransform());
}

void USRPlanetSurfaceGrid::RebuildCellInfoIndex()
{
	SurfaceGridCellIndex::RebuildCellInfoIndex(CellIndexState, Cells, FaceResolution);
}

bool USRPlanetSurfaceGrid::GetStoredCellInfoById(const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCellInfo& OutCellInfo) const
{
	return SurfaceGridCellIndex::GetStoredCellInfoById(CellIndexState, Cells, FaceResolution, CellId, OutCellInfo);
}

void USRPlanetSurfaceGrid::StoreCellInfo(const FSRPlanetSurfaceGridCellInfo& CellInfo)
{
	SurfaceGridCellIndex::StoreCellInfo(CellIndexState, FaceResolution, CellInfo);
}
