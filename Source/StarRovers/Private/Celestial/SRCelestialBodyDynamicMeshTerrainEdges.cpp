#include "Celestial/SRCelestialBodyDynamicMeshInternal.h"

namespace StarRovers::Celestial::DynamicMesh
{
namespace
{
int32 GetDynamicMeshTerrainEdgeFlatCellIndex(int32 FaceResolution, const FSRPlanetSurfaceGridCellId& CellId)
{
	return ((static_cast<int32>(CellId.Face) * FaceResolution) + CellId.CellY) * FaceResolution + CellId.CellX;
}
}

FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator::FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator(
	TArray<UE::Geometry::FDynamicMesh3>& InFaceDynamicMeshes,
	TMap<FSRTerrainVertexKey, int32>& InWeldedVertexIds,
	TArray<FSRPlanetSurfaceGridCell>& InPreparedSurfaceGridCells,
	const TArray<int32>& InCachedCellIndexByFlatId,
	TArray<FSRCelestialBodyDynamicMeshCellColorData>& InPreparedColorDataByFlatId,
	int32 InFaceResolution,
	float InBodyScale,
	float InTerrainHeightStep,
	bool bInMinecraft,
	float InMinecraftSideWallMinHeightStepRatio)
	: FaceDynamicMeshes(InFaceDynamicMeshes)
	, WeldedVertexIds(InWeldedVertexIds)
	, PreparedSurfaceGridCells(InPreparedSurfaceGridCells)
	, CachedCellIndexByFlatId(InCachedCellIndexByFlatId)
	, PreparedColorDataByFlatId(InPreparedColorDataByFlatId)
	, FaceResolution(InFaceResolution)
	, BodyScale(InBodyScale)
	, TerrainHeightStep(InTerrainHeightStep)
	, bMinecraft(bInMinecraft)
	, MinecraftSideWallMinHeightStepRatio(InMinecraftSideWallMinHeightStepRatio)
{
}

void FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator::ReservePendingEdges(int32 ReserveCount)
{
	PendingEdges.Reserve(ReserveCount);
}

void FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator::RegisterEdge(
	uint32 EndpointHashA,
	uint32 EndpointHashB,
	const FVector& PointA,
	const FVector& PointB,
	const FVector& CellCenter,
	float HeightOffset,
	const FLinearColor& SurfaceColor,
	int32 MaterialId,
	const FSRPlanetSurfaceGridCellId& CellId)
{
	++Stats.RegisterCount;
	if (EndpointHashA == EndpointHashB)
	{
		return;
	}

	FVector OrderedPointA = PointA;
	FVector OrderedPointB = PointB;
	uint32 OrderedHashA = EndpointHashA;
	uint32 OrderedHashB = EndpointHashB;
	if (EndpointHashA > EndpointHashB)
	{
		Swap(OrderedPointA, OrderedPointB);
		Swap(OrderedHashA, OrderedHashB);
	}

	const uint64 EdgeKey = BuildSourcePositionEdgeKey(OrderedHashA, OrderedHashB);
	if (FSRCelestialBodyDynamicMeshTerrainEdge* ExistingEdge = PendingEdges.Find(EdgeKey))
	{
		++Stats.MatchCount;
		const float MinMinecraftWallHeight =
			bMinecraft && TerrainHeightStep > KINDA_SMALL_NUMBER
				? TerrainHeightStep * FMath::Max(0.0f, MinecraftSideWallMinHeightStepRatio)
				: KINDA_SMALL_NUMBER;
		const bool bSameEdgePosition =
			!bMinecraft
			&& FVector::DistSquared(ExistingEdge->PointA, OrderedPointA) <= KINDA_SMALL_NUMBER
			&& FVector::DistSquared(ExistingEdge->PointB, OrderedPointB) <= KINDA_SMALL_NUMBER;
		const bool bSameMinecraftStep =
			bMinecraft
			&& FMath::Abs(HeightOffset - ExistingEdge->HeightOffset) <= MinMinecraftWallHeight;
		if (!bSameEdgePosition && !bSameMinecraftStep)
		{
			++Stats.SideWallCount;
			const FLinearColor WallColor = FLinearColor::LerpUsingHSV(ExistingEdge->SurfaceColor, SurfaceColor, 0.5f);
			const bool bExistingCellIsHigher = ExistingEdge->HeightOffset > HeightOffset + KINDA_SMALL_NUMBER;
			const bool bCurrentCellIsHigher = HeightOffset > ExistingEdge->HeightOffset + KINDA_SMALL_NUMBER;
			const FVector& HigherCellCenter = bCurrentCellIsHigher ? CellCenter : ExistingEdge->CellCenter;
			const FVector& LowerCellCenter = bCurrentCellIsHigher ? ExistingEdge->CellCenter : CellCenter;
			FVector WallNormalReferenceDirection = LowerCellCenter - HigherCellCenter;
			if (WallNormalReferenceDirection.IsNearlyZero())
			{
				const FVector ExistingEdgeCenter = (ExistingEdge->PointA + ExistingEdge->PointB) * 0.5f;
				const FVector CurrentEdgeCenter = (OrderedPointA + OrderedPointB) * 0.5f;
				WallNormalReferenceDirection = bExistingCellIsHigher
					? (CurrentEdgeCenter - ExistingEdgeCenter)
					: (ExistingEdgeCenter - CurrentEdgeCenter);
			}
			if (WallNormalReferenceDirection.IsNearlyZero())
			{
				WallNormalReferenceDirection = (ExistingEdge->CellCenter + CellCenter).GetSafeNormal();
			}

			const FSRTerrainVertexKey WallVertexKeys[4] =
			{
				MakeTerrainVertexKey(ExistingEdge->SourceHashA, ExistingEdge->HeightOffset),
				MakeTerrainVertexKey(ExistingEdge->SourceHashB, ExistingEdge->HeightOffset),
				MakeTerrainVertexKey(OrderedHashB, HeightOffset),
				MakeTerrainVertexKey(OrderedHashA, HeightOffset),
			};
			const FSRCelestialBodyDynamicMeshQuadRenderData SideRenderData = AppendFlatColoredDynamicMeshQuad(
				FaceDynamicMeshes,
				WeldedVertexIds,
				GetCubeSphereFaceComponentIndex((bCurrentCellIsHigher ? CellId : ExistingEdge->CellId).Face),
				ExistingEdge->PointA,
				ExistingEdge->PointB,
				OrderedPointB,
				OrderedPointA,
				WallColor,
				ExistingEdge->MaterialId != 0 ? ExistingEdge->MaterialId : MaterialId,
				false,
				WallVertexKeys,
				&WallNormalReferenceDirection);
			AddPreparedSurfaceGridSideWallOutline(
				PreparedSurfaceGridCells,
				CachedCellIndexByFlatId,
				FaceResolution,
				BodyScale,
				ExistingEdge->CellId,
				CellId,
				true,
				ExistingEdge->PointA,
				ExistingEdge->PointB,
				OrderedPointB,
				OrderedPointA);
			AddPreparedSurfaceGridSideWallOutline(
				PreparedSurfaceGridCells,
				CachedCellIndexByFlatId,
				FaceResolution,
				BodyScale,
				CellId,
				ExistingEdge->CellId,
				true,
				ExistingEdge->PointA,
				ExistingEdge->PointB,
				OrderedPointB,
				OrderedPointA);
			if (bExistingCellIsHigher)
			{
				AddPreparedSurfaceGridSideFace(
					PreparedSurfaceGridCells,
					CachedCellIndexByFlatId,
					FaceResolution,
					BodyScale,
					ExistingEdge->CellId,
					CellId,
					true,
					ExistingEdge->PointA,
					ExistingEdge->PointB,
					OrderedPointB,
					OrderedPointA);
				const int32 ExistingCellFlatIndex = GetDynamicMeshTerrainEdgeFlatCellIndex(FaceResolution, ExistingEdge->CellId);
				if (PreparedColorDataByFlatId.IsValidIndex(ExistingCellFlatIndex))
				{
					PreparedColorDataByFlatId[ExistingCellFlatIndex].SideColorElements.Append(SideRenderData.ColorElements);
				}
			}
			else if (bCurrentCellIsHigher)
			{
				AddPreparedSurfaceGridSideFace(
					PreparedSurfaceGridCells,
					CachedCellIndexByFlatId,
					FaceResolution,
					BodyScale,
					CellId,
					ExistingEdge->CellId,
					true,
					ExistingEdge->PointA,
					ExistingEdge->PointB,
					OrderedPointB,
					OrderedPointA);
				const int32 CellFlatIndex = GetDynamicMeshTerrainEdgeFlatCellIndex(FaceResolution, CellId);
				if (PreparedColorDataByFlatId.IsValidIndex(CellFlatIndex))
				{
					PreparedColorDataByFlatId[CellFlatIndex].SideColorElements.Append(SideRenderData.ColorElements);
				}
			}
		}
		PendingEdges.Remove(EdgeKey);
		return;
	}

	FSRCelestialBodyDynamicMeshTerrainEdge& NewEdge = PendingEdges.Add(EdgeKey);
	NewEdge.PointA = OrderedPointA;
	NewEdge.PointB = OrderedPointB;
	NewEdge.CellCenter = CellCenter;
	NewEdge.SourceHashA = OrderedHashA;
	NewEdge.SourceHashB = OrderedHashB;
	NewEdge.HeightOffset = HeightOffset;
	NewEdge.SurfaceColor = SurfaceColor;
	NewEdge.MaterialId = MaterialId;
	NewEdge.CellId = CellId;
	Stats.MaxPendingEdgeCount = FMath::Max(Stats.MaxPendingEdgeCount, PendingEdges.Num());
}

void FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator::RegisterCellEdges(
	const FSRCelestialBodyDynamicMeshSurfaceCellGeometry& CellGeometry,
	const FSRPlanetTerrainSample& TerrainSample,
	int32 MaterialId,
	const FSRPlanetSurfaceGridCellId& CellId,
	bool bProfileBuildBreakdown,
	double& TerrainEdgeRegisterMs)
{
	const double InnerStart = bProfileBuildBreakdown ? SRCelestialNowSeconds() : 0.0;
	const uint32* SourcePositionHashes = CellGeometry.SourcePositionHashes;
	const FVector* TargetPositions = CellGeometry.TargetPositions;
	const FVector& TargetCellCenter = CellGeometry.TargetCellCenter;
	RegisterEdge(
		SourcePositionHashes[0],
		SourcePositionHashes[1],
		TargetPositions[0],
		TargetPositions[1],
		TargetCellCenter,
		TerrainSample.HeightOffset,
		TerrainSample.SurfaceColor,
		MaterialId,
		CellId);
	RegisterEdge(
		SourcePositionHashes[1],
		SourcePositionHashes[2],
		TargetPositions[1],
		TargetPositions[2],
		TargetCellCenter,
		TerrainSample.HeightOffset,
		TerrainSample.SurfaceColor,
		MaterialId,
		CellId);
	RegisterEdge(
		SourcePositionHashes[2],
		SourcePositionHashes[3],
		TargetPositions[2],
		TargetPositions[3],
		TargetCellCenter,
		TerrainSample.HeightOffset,
		TerrainSample.SurfaceColor,
		MaterialId,
		CellId);
	RegisterEdge(
		SourcePositionHashes[3],
		SourcePositionHashes[0],
		TargetPositions[3],
		TargetPositions[0],
		TargetCellCenter,
		TerrainSample.HeightOffset,
		TerrainSample.SurfaceColor,
		MaterialId,
		CellId);
	if (bProfileBuildBreakdown)
	{
		TerrainEdgeRegisterMs += SRCelestialElapsedMilliseconds(InnerStart);
	}
}

int32 FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator::GetPendingEdgeCount() const
{
	return PendingEdges.Num();
}

const FSRCelestialBodyDynamicMeshTerrainEdgeStats& FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator::GetStats() const
{
	return Stats;
}
}
