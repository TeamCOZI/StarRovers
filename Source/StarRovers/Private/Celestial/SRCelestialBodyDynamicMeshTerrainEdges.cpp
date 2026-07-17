#include "Celestial/SRCelestialBodyDynamicMeshPipeline.h"

#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"

namespace StarRovers::Celestial::DynamicMesh
{
namespace
{
int32 GetDynamicMeshTerrainEdgeFlatCellIndex(int32 FaceResolution, const FSRPlanetSurfaceGridCellId& CellId)
{
	return ((static_cast<int32>(CellId.Face) * FaceResolution) + CellId.CellY) * FaceResolution + CellId.CellX;
}

bool IsFeatureToonOutlineEnabled(const FSRToonOutlineSettings& ToonOutlineSettings)
{
	return ToonOutlineSettings.bEnableToonOutline
		&& ToonOutlineSettings.bUseFeatureEdgeToonOutline
		&& ToonOutlineSettings.ToonLineThickness > KINDA_SMALL_NUMBER;
}

bool ShouldDrawNormalFeatureToonOutline(
	const FSRToonOutlineSettings& ToonOutlineSettings,
	const FVector& NormalA,
	const FVector& NormalB)
{
	if (!IsFeatureToonOutlineEnabled(ToonOutlineSettings))
	{
		return false;
	}

	const FVector SafeNormalA = NormalA.GetSafeNormal();
	const FVector SafeNormalB = NormalB.GetSafeNormal();
	if (SafeNormalA.IsNearlyZero() || SafeNormalB.IsNearlyZero())
	{
		return false;
	}

	const float AngleThresholdDegrees = FMath::Clamp(ToonOutlineSettings.FeatureEdgeAngleThresholdDegrees, 0.0f, 90.0f);
	const float CosThreshold = FMath::Cos(FMath::DegreesToRadians(AngleThresholdDegrees));
	return FVector::DotProduct(SafeNormalA, SafeNormalB) < CosThreshold;
}

uint64 BuildSideWallFeatureMaskVerticalEdgeKey(uint32 SourceHash, float HeightOffsetA, float HeightOffsetB)
{
	const float LowerHeightOffset = FMath::Min(HeightOffsetA, HeightOffsetB);
	const float UpperHeightOffset = FMath::Max(HeightOffsetA, HeightOffsetB);
	uint32 KeyHash = HashCombine(SourceHash, ::GetTypeHash(LowerHeightOffset));
	KeyHash = HashCombine(KeyHash, ::GetTypeHash(UpperHeightOffset));
	return (static_cast<uint64>(SourceHash) << 32) | KeyHash;
}

bool SetFeatureMaskUVComponent(UE::Geometry::FDynamicMeshUVOverlay* UVOverlay, int32 ElementId, int32 ComponentIndex)
{
	if (!UVOverlay || ElementId == INDEX_NONE)
	{
		return false;
	}

	FVector2f Value = UVOverlay->GetElement(ElementId);
	float& Component = ComponentIndex == 0 ? Value.X : Value.Y;
	if (Component >= 0.5f)
	{
		return false;
	}

	Component = 1.0f;
	UVOverlay->SetElement(ElementId, Value);
	return true;
}
}

FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator::FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator(
	TArray<UE::Geometry::FDynamicMesh3>& InFaceDynamicMeshes,
	TMap<FSRCelestialBodyDynamicMeshTerrainVertexKey, int32>& InWeldedVertexIds,
	TArray<FSRPlanetSurfaceGridCell>& InPreparedSurfaceGridCells,
	const TArray<int32>& InCachedCellIndexByFlatId,
	TArray<FSRCelestialBodyDynamicMeshCellColorData>& InPreparedColorDataByFlatId,
	const FSRToonOutlineSettings& InToonOutlineSettings,
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
	, ToonOutlineSettings(InToonOutlineSettings)
	, bFeatureToonOutlineEnabled(IsFeatureToonOutlineEnabled(InToonOutlineSettings))
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
	PendingSideWallFeatureMaskEdges.Reserve(FMath::Max(1, ReserveCount / 4));
}

bool FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator::ApplyFeatureEdgeMask(
	const FSRCelestialBodyDynamicMeshQuadFeatureMaskRef& FeatureMaskRef,
	int32 EdgeIndex)
{
	if (!bFeatureToonOutlineEnabled
		|| EdgeIndex < 0
		|| EdgeIndex >= 4
		|| !FaceDynamicMeshes.IsValidIndex(FeatureMaskRef.MeshComponentIndex))
	{
		return false;
	}

	const int32 RenderEdgeIndex = FeatureMaskRef.InputEdgeToFeatureMaskEdgeIndex[EdgeIndex];
	if (RenderEdgeIndex < 0 || RenderEdgeIndex >= 4)
	{
		return false;
	}

	UE::Geometry::FDynamicMesh3& TargetMesh = FaceDynamicMeshes[FeatureMaskRef.MeshComponentIndex];
	UE::Geometry::FDynamicMeshAttributeSet* Attributes = TargetMesh.Attributes();
	if (!Attributes || Attributes->NumUVLayers() <= 1)
	{
		return false;
	}

	UE::Geometry::FDynamicMeshUVOverlay* FeatureMaskUVOverlay = Attributes->GetUVLayer(1);
	if (!FeatureMaskUVOverlay)
	{
		return false;
	}

	const int32 CornerAByEdge[4] = { 0, 1, 2, 3 };
	const int32 CornerBByEdge[4] = { 1, 2, 3, 0 };
	const int32 ComponentIndexByEdge[4] = { 1, 0, 1, 0 };
	const int32 CornerA = CornerAByEdge[RenderEdgeIndex];
	const int32 CornerB = CornerBByEdge[RenderEdgeIndex];
	const int32 ComponentIndex = ComponentIndexByEdge[RenderEdgeIndex];

	const bool bChangedA = SetFeatureMaskUVComponent(
		FeatureMaskUVOverlay,
		FeatureMaskRef.FeatureMaskUVElementIds[CornerA],
		ComponentIndex);
	const bool bChangedB = SetFeatureMaskUVComponent(
		FeatureMaskUVOverlay,
		FeatureMaskRef.FeatureMaskUVElementIds[CornerB],
		ComponentIndex);
	if (bChangedA || bChangedB)
	{
		++Stats.FeatureEdgeMaskCount;
		return true;
	}
	return false;
}

void FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator::AppendSideWallFeatureMaskBoundaryEdge(
	const FSRCelestialBodyDynamicMeshQuadFeatureMaskRef& FeatureMaskRef,
	int32 EdgeIndex)
{
	if (!bFeatureToonOutlineEnabled)
	{
		return;
	}

	ApplyFeatureEdgeMask(FeatureMaskRef, EdgeIndex);
}

void FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator::RegisterSideWallFeatureMaskVerticalEdge(
	uint32 SourceHash,
	float HeightOffsetA,
	float HeightOffsetB,
	const FSRCelestialBodyDynamicMeshQuadFeatureMaskRef& FeatureMaskRef,
	int32 EdgeIndex,
	const FVector& WallNormal)
{
	if (!bFeatureToonOutlineEnabled || EdgeIndex == INDEX_NONE)
	{
		return;
	}

	const uint64 EdgeKey = BuildSideWallFeatureMaskVerticalEdgeKey(SourceHash, HeightOffsetA, HeightOffsetB);
	if (FSRCelestialBodyDynamicMeshSideWallFeatureMaskEdge* ExistingEdge = PendingSideWallFeatureMaskEdges.Find(EdgeKey))
	{
		if (ShouldDrawNormalFeatureToonOutline(ToonOutlineSettings, ExistingEdge->WallNormal, WallNormal))
		{
			ApplyFeatureEdgeMask(ExistingEdge->FeatureMaskRef, ExistingEdge->EdgeIndex);
		}
		PendingSideWallFeatureMaskEdges.Remove(EdgeKey);
		return;
	}

	FSRCelestialBodyDynamicMeshSideWallFeatureMaskEdge& NewEdge = PendingSideWallFeatureMaskEdges.Add(EdgeKey);
	NewEdge.FeatureMaskRef = FeatureMaskRef;
	NewEdge.EdgeIndex = EdgeIndex;
	NewEdge.WallNormal = WallNormal;
}

void FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator::RegisterEdge(
	uint32 EndpointHashA,
	uint32 EndpointHashB,
	const FVector& PointA,
	const FVector& PointB,
	const FVector& CellCenter,
	const FVector& CellNormal,
	float HeightOffset,
	const FLinearColor& SurfaceColor,
	const FLinearColor& TerrainBaseColor,
	const FSRCelestialBodyDynamicMeshQuadRenderData& SurfaceRenderData,
	int32 SurfaceFeatureMaskEdgeIndex,
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
			const FLinearColor WallTerrainBaseColor = FLinearColor::LerpUsingHSV(ExistingEdge->TerrainBaseColor, TerrainBaseColor, 0.5f);
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

			const FSRCelestialBodyDynamicMeshTerrainVertexKey WallVertexKeys[4] =
			{
				MakeCelestialBodyDynamicMeshTerrainVertexKey(ExistingEdge->SourceHashA, ExistingEdge->HeightOffset),
				MakeCelestialBodyDynamicMeshTerrainVertexKey(ExistingEdge->SourceHashB, ExistingEdge->HeightOffset),
				MakeCelestialBodyDynamicMeshTerrainVertexKey(OrderedHashB, HeightOffset),
				MakeCelestialBodyDynamicMeshTerrainVertexKey(OrderedHashA, HeightOffset),
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
				&WallNormalReferenceDirection,
				true,
				&WallTerrainBaseColor);
			if (bFeatureToonOutlineEnabled)
			{
				ApplyFeatureEdgeMask(ExistingEdge->SurfaceFeatureMaskRef, ExistingEdge->SurfaceFeatureMaskEdgeIndex);
				ApplyFeatureEdgeMask(SurfaceRenderData.FeatureMaskRef, SurfaceFeatureMaskEdgeIndex);
				AppendSideWallFeatureMaskBoundaryEdge(
					SideRenderData.FeatureMaskRef,
					0);
				AppendSideWallFeatureMaskBoundaryEdge(
					SideRenderData.FeatureMaskRef,
					2);
				RegisterSideWallFeatureMaskVerticalEdge(
					OrderedHashA,
					ExistingEdge->HeightOffset,
					HeightOffset,
					SideRenderData.FeatureMaskRef,
					3,
					WallNormalReferenceDirection);
				RegisterSideWallFeatureMaskVerticalEdge(
					OrderedHashB,
					ExistingEdge->HeightOffset,
					HeightOffset,
					SideRenderData.FeatureMaskRef,
					1,
					WallNormalReferenceDirection);
			}
			Stats.SideWallFailedTriangleCount += SideRenderData.FailedTriangleCount;
			Stats.SideWallFallbackTriangleCount += SideRenderData.FallbackTriangleCount;
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
		else if (ShouldDrawNormalFeatureToonOutline(ToonOutlineSettings, ExistingEdge->CellNormal, CellNormal))
		{
			ApplyFeatureEdgeMask(ExistingEdge->SurfaceFeatureMaskRef, ExistingEdge->SurfaceFeatureMaskEdgeIndex);
			ApplyFeatureEdgeMask(SurfaceRenderData.FeatureMaskRef, SurfaceFeatureMaskEdgeIndex);
		}
		PendingEdges.Remove(EdgeKey);
		return;
	}

	FSRCelestialBodyDynamicMeshTerrainEdge& NewEdge = PendingEdges.Add(EdgeKey);
	NewEdge.PointA = OrderedPointA;
	NewEdge.PointB = OrderedPointB;
	NewEdge.CellCenter = CellCenter;
	NewEdge.CellNormal = CellNormal;
	NewEdge.SourceHashA = OrderedHashA;
	NewEdge.SourceHashB = OrderedHashB;
	NewEdge.HeightOffset = HeightOffset;
	NewEdge.SurfaceColor = SurfaceColor;
	NewEdge.TerrainBaseColor = TerrainBaseColor;
	NewEdge.MaterialId = MaterialId;
	NewEdge.CellId = CellId;
	NewEdge.SurfaceFeatureMaskRef = SurfaceRenderData.FeatureMaskRef;
	NewEdge.SurfaceFeatureMaskEdgeIndex = SurfaceFeatureMaskEdgeIndex;
	Stats.MaxPendingEdgeCount = FMath::Max(Stats.MaxPendingEdgeCount, PendingEdges.Num());
}

void FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator::RegisterCellEdges(
	const FSRCelestialBodyDynamicMeshSurfaceCellGeometry& CellGeometry,
	const FSRPlanetTerrainSample& TerrainSample,
	const FLinearColor& TerrainBaseColor,
	const FSRCelestialBodyDynamicMeshQuadRenderData& SurfaceRenderData,
	int32 MaterialId,
	const FSRPlanetSurfaceGridCellId& CellId,
	bool bProfileBuildBreakdown,
	double& TerrainEdgeRegisterMs)
{
	const double InnerStart = bProfileBuildBreakdown ? GetDynamicMeshTimingSeconds() : 0.0;
	const uint32* SourcePositionHashes = CellGeometry.SourcePositionHashes;
	const FVector* TargetPositions = CellGeometry.TargetPositions;
	const FVector& TargetCellCenter = CellGeometry.TargetCellCenter;
	RegisterEdge(
		SourcePositionHashes[0],
		SourcePositionHashes[1],
		TargetPositions[0],
		TargetPositions[1],
		TargetCellCenter,
		CellGeometry.CellNormal,
		TerrainSample.HeightOffset,
		TerrainSample.SurfaceColor,
		TerrainBaseColor,
		SurfaceRenderData,
		0,
		MaterialId,
		CellId);
	RegisterEdge(
		SourcePositionHashes[1],
		SourcePositionHashes[2],
		TargetPositions[1],
		TargetPositions[2],
		TargetCellCenter,
		CellGeometry.CellNormal,
		TerrainSample.HeightOffset,
		TerrainSample.SurfaceColor,
		TerrainBaseColor,
		SurfaceRenderData,
		1,
		MaterialId,
		CellId);
	RegisterEdge(
		SourcePositionHashes[2],
		SourcePositionHashes[3],
		TargetPositions[2],
		TargetPositions[3],
		TargetCellCenter,
		CellGeometry.CellNormal,
		TerrainSample.HeightOffset,
		TerrainSample.SurfaceColor,
		TerrainBaseColor,
		SurfaceRenderData,
		2,
		MaterialId,
		CellId);
	RegisterEdge(
		SourcePositionHashes[3],
		SourcePositionHashes[0],
		TargetPositions[3],
		TargetPositions[0],
		TargetCellCenter,
		CellGeometry.CellNormal,
		TerrainSample.HeightOffset,
		TerrainSample.SurfaceColor,
		TerrainBaseColor,
		SurfaceRenderData,
		3,
		MaterialId,
		CellId);
	if (bProfileBuildBreakdown)
	{
		TerrainEdgeRegisterMs += GetDynamicMeshTimingElapsedMilliseconds(InnerStart);
	}
}

void FSRCelestialBodyDynamicMeshTerrainEdgeAccumulator::FlushPendingSideWallFeatureMaskEdges()
{
	if (!bFeatureToonOutlineEnabled)
	{
		PendingSideWallFeatureMaskEdges.Reset();
		return;
	}

	for (const TPair<uint64, FSRCelestialBodyDynamicMeshSideWallFeatureMaskEdge>& PendingPair : PendingSideWallFeatureMaskEdges)
	{
		const FSRCelestialBodyDynamicMeshSideWallFeatureMaskEdge& PendingEdge = PendingPair.Value;
		ApplyFeatureEdgeMask(PendingEdge.FeatureMaskRef, PendingEdge.EdgeIndex);
	}
	PendingSideWallFeatureMaskEdges.Reset();
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
