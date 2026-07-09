#include "Surface/SRPlanetSurfaceGrid.h"

#include "SRPlanetSurfaceGridOwnerBody.h"
#include "SRPlanetSurfaceGridVisibilityState.h"
#include "SRPlanetSurfaceGridWireCells.h"
#include "SRPlanetSurfaceGridWirePrimitives.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"

namespace SurfaceGridOwnerBody = StarRovers::SurfaceGridOwnerBody;
namespace SurfaceGridVisibilityState = StarRovers::SurfaceGridVisibilityState;
namespace SurfaceGridWireCells = StarRovers::SurfaceGridWireCells;
namespace SurfaceGridWirePrimitives = StarRovers::SurfaceGridWirePrimitives;

void USRPlanetSurfaceGrid::AppendGeneratedGridCell(
	UE::Geometry::FDynamicMesh3& GridMesh,
	const FSRPlanetSurfaceGridCell& Cell,
	TSet<uint64>& DrawnEdges) const
{
	const FLinearColor DefaultLineColor(DebugLineColor.R, DebugLineColor.G, DebugLineColor.B, DebugLineOpacity);
	SurfaceGridWireCells::AppendGeneratedGridCell(
		GridMesh,
		Cell,
		DefaultLineColor,
		DebugLineThickness,
		GridSurfaceOffset,
		DrawnEdges);
}

void USRPlanetSurfaceGrid::RebuildGridMesh()
{
	UE::Geometry::FDynamicMesh3 GridMesh;
	GridMesh.EnableAttributes();
	GridMesh.Attributes()->EnablePrimaryColors();

	if (!Cells.IsEmpty())
	{
		const FLinearColor DefaultLineColor(DebugLineColor.R, DebugLineColor.G, DebugLineColor.B, DebugLineOpacity);

		const bool bAppendedOwnerWire = !bUsingGeneratedGridCells
			&& AppendOwnerDynamicMeshWire(GridMesh, DefaultLineColor, DebugLineThickness);
		if (!bAppendedOwnerWire)
		{
			TSet<uint64> DrawnEdges;
			DrawnEdges.Reserve(Cells.Num() * 3);
			for (const FSRPlanetSurfaceGridCell& Cell : Cells)
			{
				AppendGridWireCell(GridMesh, Cell, DefaultLineColor, DebugLineThickness, true, &DrawnEdges);
			}
		}
	}

	SetMesh(MoveTemp(GridMesh));
	SurfaceGridVisibilityState::ApplyPrimaryGridVisibility(*this, bGridVisible);
	bGridMeshDirty = false;
}

bool USRPlanetSurfaceGrid::AppendOwnerDynamicMeshWire(
	UE::Geometry::FDynamicMesh3& GridMesh,
	const FLinearColor& LineColor,
	float LineThickness) const
{
	return SurfaceGridOwnerBody::AppendDynamicMeshBoundaryWire(
		GetOwner(),
		[this, &GridMesh, &LineColor, LineThickness](const FVector& LocalPointA, const FVector& LocalPointB)
		{
			AppendGridWireSegment(GridMesh, LocalPointA, LocalPointB, LineColor, LineThickness);
		});
}

void USRPlanetSurfaceGrid::AppendGridWireCell(
	UE::Geometry::FDynamicMesh3& GridMesh,
	const FSRPlanetSurfaceGridCell& Cell,
	const FLinearColor& LineColor,
	float LineThickness,
	bool bIncludeInEdgeSet,
	TSet<uint64>* DrawnEdges) const
{
	SurfaceGridWireCells::AppendGridWireCell(
		GridMesh,
		Cell,
		LineColor,
		LineThickness,
		bIncludeInEdgeSet,
		DrawnEdges,
		bUsingGeneratedGridCells,
		GridSurfaceOffset,
		[this](const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCell& OutCell)
		{
			return GetCellById(CellId, OutCell);
		},
		[this](const FVector& LocalUnitDirection, float HeightOffset)
		{
			return ResolveLocalSurfacePoint(LocalUnitDirection, HeightOffset);
		});
}

void USRPlanetSurfaceGrid::AppendGridWireEdge(
	UE::Geometry::FDynamicMesh3& GridMesh,
	const FVector& LocalDirectionA,
	const FVector& LocalDirectionB,
	const FLinearColor& LineColor,
	float LineThickness) const
{
	SurfaceGridWirePrimitives::AppendGridWireEdge(
		GridMesh,
		LocalDirectionA,
		LocalDirectionB,
		LineColor,
		LineThickness,
		GridSurfaceOffset,
		[this](const FVector& LocalUnitDirection, float HeightOffset)
		{
			return ResolveLocalSurfacePoint(LocalUnitDirection, HeightOffset);
		});
}

void USRPlanetSurfaceGrid::AppendGridWireSegment(
	UE::Geometry::FDynamicMesh3& GridMesh,
	const FVector& LocalPointA,
	const FVector& LocalPointB,
	const FLinearColor& LineColor,
	float LineThickness) const
{
	SurfaceGridWirePrimitives::AppendGridWireSegment(GridMesh, LocalPointA, LocalPointB, LineColor, LineThickness);
}
