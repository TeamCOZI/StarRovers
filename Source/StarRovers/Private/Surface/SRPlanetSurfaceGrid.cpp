#include "Surface/SRPlanetSurfaceGrid.h"

#include "Celestial/SRCelestialBody.h"
#include "DrawDebugHelpers.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "Engine/World.h"
#include "Math/RotationMatrix.h"
#include "Materials/MaterialInterface.h"
#include "SceneManagement.h"
#include "Surface/SRPlanetSurfaceGridLibrary.h"
#include "Surface/SRPlanetTerrainGenerator.h"
#include "UDynamicMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Utility/SRTimingLog.h"
#include "Visual/SRLineThicknessUtils.h"

namespace
{
	double SRSurfaceGridNowSeconds()
	{
		return FPlatformTime::Seconds();
	}

	double SRSurfaceGridElapsedMilliseconds(double StartSeconds)
	{
		return (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
	}

	struct FSRSurfaceGridCubeFaceBasis
	{
		FVector Normal = FVector::ForwardVector;
		FVector AxisU = FVector::RightVector;
		FVector AxisV = FVector::UpVector;
	};

	FSRSurfaceGridCubeFaceBasis GetCubeFaceBasis(ESRCubeSphereFace Face)
	{
		switch (Face)
		{
		case ESRCubeSphereFace::PositiveX:
			return { FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f) };
		case ESRCubeSphereFace::NegativeX:
			return { FVector(-1.0f, 0.0f, 0.0f), FVector(0.0f, -1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f) };
		case ESRCubeSphereFace::PositiveY:
			return { FVector(0.0f, 1.0f, 0.0f), FVector(-1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f) };
		case ESRCubeSphereFace::NegativeY:
			return { FVector(0.0f, -1.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f) };
		case ESRCubeSphereFace::PositiveZ:
			return { FVector(0.0f, 0.0f, 1.0f), FVector(0.0f, 1.0f, 0.0f), FVector(-1.0f, 0.0f, 0.0f) };
		case ESRCubeSphereFace::NegativeZ:
		default:
			return { FVector(0.0f, 0.0f, -1.0f), FVector(0.0f, 1.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f) };
		}
	}

	FVector BuildCubeFacePoint(ESRCubeSphereFace Face, float FaceU, float FaceV)
	{
		const FSRSurfaceGridCubeFaceBasis Basis = GetCubeFaceBasis(Face);
		return Basis.Normal + (Basis.AxisU * FaceU) + (Basis.AxisV * FaceV);
	}

	float GetCubeFaceCellStep(int32 Resolution)
	{
		return 2.0f / static_cast<float>(FMath::Max(1, Resolution));
	}

	float GetCubeFaceCellCenter(int32 CellIndex, int32 Resolution)
	{
		return -1.0f + (GetCubeFaceCellStep(Resolution) * (static_cast<float>(CellIndex) + 0.5f));
	}

	uint32 HashGridPoint(const FVector& LocalPoint)
	{
		constexpr double QuantizeScale = 100.0;
		const int64 QuantizedX = FMath::RoundToInt64(LocalPoint.X * QuantizeScale);
		const int64 QuantizedY = FMath::RoundToInt64(LocalPoint.Y * QuantizeScale);
		const int64 QuantizedZ = FMath::RoundToInt64(LocalPoint.Z * QuantizeScale);
		auto HashInt64 = [](int64 Value)
		{
			const uint64 Bits = static_cast<uint64>(Value);
			return HashCombine(
				::GetTypeHash(static_cast<uint32>(Bits & 0xffffffff)),
				::GetTypeHash(static_cast<uint32>(Bits >> 32)));
		};
		return HashCombine(HashCombine(HashInt64(QuantizedX), HashInt64(QuantizedY)), HashInt64(QuantizedZ));
	}

	uint64 BuildGridEdgeKey(const FVector& LocalPointA, const FVector& LocalPointB)
	{
		const uint32 EndpointA = HashGridPoint(LocalPointA);
		const uint32 EndpointB = HashGridPoint(LocalPointB);
		const uint32 MinEndpoint = FMath::Min(EndpointA, EndpointB);
		const uint32 MaxEndpoint = FMath::Max(EndpointA, EndpointB);
		return (static_cast<uint64>(MinEndpoint) << 32) | static_cast<uint64>(MaxEndpoint);
	}

	bool GetGridCellEdgePoints(const FSRPlanetSurfaceGridCell& Cell, int32 EdgeIndex, FVector& OutPointA, FVector& OutPointB)
	{
		switch (EdgeIndex)
		{
		case 0:
			OutPointA = Cell.Corner00;
			OutPointB = Cell.Corner10;
			return true;
		case 1:
			OutPointA = Cell.Corner10;
			OutPointB = Cell.Corner11;
			return true;
		case 2:
			OutPointA = Cell.Corner11;
			OutPointB = Cell.Corner01;
			return true;
		case 3:
			OutPointA = Cell.Corner01;
			OutPointB = Cell.Corner00;
			return true;
		default:
			OutPointA = FVector::ZeroVector;
			OutPointB = FVector::ZeroVector;
			return false;
		}
	}

	FSRPlanetSurfaceGridCellId GetGridCellEdgeNeighborId(const FSRPlanetSurfaceGridCell& Cell, int32 EdgeIndex)
	{
		switch (EdgeIndex)
		{
		case 0:
			return Cell.Neighbors.NegativeV;
		case 1:
			return Cell.Neighbors.PositiveU;
		case 2:
			return Cell.Neighbors.PositiveV;
		case 3:
			return Cell.Neighbors.NegativeU;
		default:
			return Cell.CellId;
		}
	}

	float GetGridEdgeDirectionMatchScore(
		const FVector& PointA,
		const FVector& PointB,
		const FVector& CandidatePointA,
		const FVector& CandidatePointB)
	{
		const FVector DirectionA = PointA.GetSafeNormal();
		const FVector DirectionB = PointB.GetSafeNormal();
		const FVector CandidateDirectionA = CandidatePointA.GetSafeNormal();
		const FVector CandidateDirectionB = CandidatePointB.GetSafeNormal();
		if (DirectionA.IsNearlyZero() || DirectionB.IsNearlyZero() || CandidateDirectionA.IsNearlyZero() || CandidateDirectionB.IsNearlyZero())
		{
			return TNumericLimits<float>::Max();
		}

		return static_cast<float>(
			FVector::DistSquared(DirectionA, CandidateDirectionA)
			+ FVector::DistSquared(DirectionB, CandidateDirectionB));
	}

	bool TryGetMatchingGridCellEdgePoints(
		const FSRPlanetSurfaceGridCell& Cell,
		int32 EdgeIndex,
		const FSRPlanetSurfaceGridCell& NeighborCell,
		FVector& OutCellPointA,
		FVector& OutCellPointB,
		FVector& OutNeighborPointA,
		FVector& OutNeighborPointB)
	{
		if (!GetGridCellEdgePoints(Cell, EdgeIndex, OutCellPointA, OutCellPointB))
		{
			return false;
		}

		float BestScore = TNumericLimits<float>::Max();
		for (int32 NeighborEdgeIndex = 0; NeighborEdgeIndex < 4; ++NeighborEdgeIndex)
		{
			FVector CandidatePointA;
			FVector CandidatePointB;
			if (!GetGridCellEdgePoints(NeighborCell, NeighborEdgeIndex, CandidatePointA, CandidatePointB))
			{
				continue;
			}

			const float ForwardScore = GetGridEdgeDirectionMatchScore(OutCellPointA, OutCellPointB, CandidatePointA, CandidatePointB);
			if (ForwardScore < BestScore)
			{
				BestScore = ForwardScore;
				OutNeighborPointA = CandidatePointA;
				OutNeighborPointB = CandidatePointB;
			}

			const float ReverseScore = GetGridEdgeDirectionMatchScore(OutCellPointA, OutCellPointB, CandidatePointB, CandidatePointA);
			if (ReverseScore < BestScore)
			{
				BestScore = ReverseScore;
				OutNeighborPointA = CandidatePointB;
				OutNeighborPointB = CandidatePointA;
			}
		}

		return BestScore <= 0.0001f;
	}

	FVector OffsetGeneratedGridWirePoint(const FVector& LocalPoint, float SurfaceOffset)
	{
		if (SurfaceOffset <= KINDA_SMALL_NUMBER)
		{
			return LocalPoint;
		}

		FVector SurfaceNormal = LocalPoint.GetSafeNormal();
		if (SurfaceNormal.IsNearlyZero())
		{
			SurfaceNormal = FVector::UpVector;
		}
		return LocalPoint + (SurfaceNormal * SurfaceOffset);
	}

	void AppendGridWireVolumeSegment(
		UE::Geometry::FDynamicMesh3& GridMesh,
		const FVector& LocalPointA,
		const FVector& LocalPointB,
		const FLinearColor& LineColor,
		float LineThickness)
	{
		const FVector SegmentDirection = (LocalPointB - LocalPointA).GetSafeNormal();
		if (SegmentDirection.IsNearlyZero())
		{
			return;
		}

		FVector ReferenceDirection = ((LocalPointA + LocalPointB) * 0.5f).GetSafeNormal();
		if (ReferenceDirection.IsNearlyZero() || FMath::Abs(FVector::DotProduct(ReferenceDirection, SegmentDirection)) > 0.98f)
		{
			ReferenceDirection = FVector::UpVector;
		}
		if (FMath::Abs(FVector::DotProduct(ReferenceDirection, SegmentDirection)) > 0.98f)
		{
			ReferenceDirection = FVector::RightVector;
		}

		FVector AxisA = FVector::CrossProduct(SegmentDirection, ReferenceDirection).GetSafeNormal();
		if (AxisA.IsNearlyZero())
		{
			return;
		}

		FVector AxisB = FVector::CrossProduct(SegmentDirection, AxisA).GetSafeNormal();
		if (AxisB.IsNearlyZero())
		{
			return;
		}

		const float HalfThickness = FMath::Max(0.01f, LineThickness) * 0.5f;
		AxisA *= HalfThickness;
		AxisB *= HalfThickness;

		const FVector A0 = LocalPointA + AxisA + AxisB;
		const FVector A1 = LocalPointA - AxisA + AxisB;
		const FVector A2 = LocalPointA - AxisA - AxisB;
		const FVector A3 = LocalPointA + AxisA - AxisB;
		const FVector B0 = LocalPointB + AxisA + AxisB;
		const FVector B1 = LocalPointB - AxisA + AxisB;
		const FVector B2 = LocalPointB - AxisA - AxisB;
		const FVector B3 = LocalPointB + AxisA - AxisB;

		UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay = GridMesh.Attributes()->PrimaryNormals();
		auto* ColorOverlay = GridMesh.Attributes()->PrimaryColors();
		if (!NormalOverlay || !ColorOverlay)
		{
			return;
		}

		auto AppendColoredQuad = [&GridMesh, NormalOverlay, ColorOverlay, &LineColor](
			const FVector& Point0,
			const FVector& Point1,
			const FVector& Point2,
			const FVector& Point3)
		{
			const FVector FaceNormal = FVector::CrossProduct(Point1 - Point0, Point2 - Point0).GetSafeNormal();
			if (FaceNormal.IsNearlyZero())
			{
				return;
			}

			const int32 Vertex0 = GridMesh.AppendVertex(FVector3d(Point0));
			const int32 Vertex1 = GridMesh.AppendVertex(FVector3d(Point1));
			const int32 Vertex2 = GridMesh.AppendVertex(FVector3d(Point2));
			const int32 Vertex3 = GridMesh.AppendVertex(FVector3d(Point3));
			const int32 Triangle0 = GridMesh.AppendTriangle(Vertex0, Vertex1, Vertex2);
			const int32 Triangle1 = GridMesh.AppendTriangle(Vertex0, Vertex2, Vertex3);

			const int32 Normal0 = NormalOverlay->AppendElement(FVector3f(FaceNormal));
			const int32 Normal1 = NormalOverlay->AppendElement(FVector3f(FaceNormal));
			const int32 Normal2 = NormalOverlay->AppendElement(FVector3f(FaceNormal));
			const int32 Normal3 = NormalOverlay->AppendElement(FVector3f(FaceNormal));
			const int32 Color0 = ColorOverlay->AppendElement(FVector4f(LineColor.R, LineColor.G, LineColor.B, LineColor.A));
			const int32 Color1 = ColorOverlay->AppendElement(FVector4f(LineColor.R, LineColor.G, LineColor.B, LineColor.A));
			const int32 Color2 = ColorOverlay->AppendElement(FVector4f(LineColor.R, LineColor.G, LineColor.B, LineColor.A));
			const int32 Color3 = ColorOverlay->AppendElement(FVector4f(LineColor.R, LineColor.G, LineColor.B, LineColor.A));

			if (Triangle0 >= 0)
			{
				NormalOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(Normal0, Normal1, Normal2));
				ColorOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(Color0, Color1, Color2));
			}
			if (Triangle1 >= 0)
			{
				NormalOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(Normal0, Normal2, Normal3));
				ColorOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(Color0, Color2, Color3));
			}
		};

		AppendColoredQuad(A0, A1, B1, B0);
		AppendColoredQuad(A1, A2, B2, B1);
		AppendColoredQuad(A2, A3, B3, B2);
		AppendColoredQuad(A3, A0, B0, B3);
		AppendColoredQuad(A0, A3, A2, A1);
		AppendColoredQuad(B0, B1, B2, B3);
	}

	bool IntersectRayTriangle(
		const FVector& RayOrigin,
		const FVector& RayDirection,
		const FVector& TriangleA,
		const FVector& TriangleB,
		const FVector& TriangleC,
		float& OutHitDistance)
	{
		OutHitDistance = 0.0f;

		const FVector Edge1 = TriangleB - TriangleA;
		const FVector Edge2 = TriangleC - TriangleA;
		const FVector P = FVector::CrossProduct(RayDirection, Edge2);
		const float Determinant = FVector::DotProduct(Edge1, P);
		if (FMath::Abs(Determinant) <= UE_SMALL_NUMBER)
		{
			return false;
		}

		const float InvDeterminant = 1.0f / Determinant;
		const FVector T = RayOrigin - TriangleA;
		const float U = FVector::DotProduct(T, P) * InvDeterminant;
		if (U < 0.0f || U > 1.0f)
		{
			return false;
		}

		const FVector Q = FVector::CrossProduct(T, Edge1);
		const float V = FVector::DotProduct(RayDirection, Q) * InvDeterminant;
		if (V < 0.0f || U + V > 1.0f)
		{
			return false;
		}

		const float HitDistance = FVector::DotProduct(Edge2, Q) * InvDeterminant;
		if (HitDistance < 0.0f)
		{
			return false;
		}

		OutHitDistance = HitDistance;
		return true;
	}

	bool IntersectRayBox(const FVector& RayOrigin, const FVector& RayDirection, const FBox& Box, float& OutHitDistance)
	{
		OutHitDistance = 0.0f;
		if (!Box.IsValid)
		{
			return false;
		}

		float MinDistance = 0.0f;
		float MaxDistance = BIG_NUMBER;

		auto ClipAxis = [&MinDistance, &MaxDistance](float Origin, float Direction, float MinValue, float MaxValue)
		{
			if (FMath::Abs(Direction) <= UE_SMALL_NUMBER)
			{
				return Origin >= MinValue && Origin <= MaxValue;
			}

			float Distance0 = (MinValue - Origin) / Direction;
			float Distance1 = (MaxValue - Origin) / Direction;
			if (Distance0 > Distance1)
			{
				Swap(Distance0, Distance1);
			}

			MinDistance = FMath::Max(MinDistance, Distance0);
			MaxDistance = FMath::Min(MaxDistance, Distance1);
			return MinDistance <= MaxDistance;
		};

		if (!ClipAxis(RayOrigin.X, RayDirection.X, Box.Min.X, Box.Max.X)
			|| !ClipAxis(RayOrigin.Y, RayDirection.Y, Box.Min.Y, Box.Max.Y)
			|| !ClipAxis(RayOrigin.Z, RayDirection.Z, Box.Min.Z, Box.Max.Z)
			|| MaxDistance < 0.0f)
		{
			return false;
		}

		OutHitDistance = FMath::Max(0.0f, MinDistance);
		return true;
	}

	struct FSRSurfaceGridRaycastStats
	{
		int32 BucketTests = 0;
		int32 BucketHits = 0;
		int32 BucketSkippedByBestHit = 0;
		int32 CellTests = 0;
		int32 DuplicateCellSkips = 0;
		int32 TopTriangleTests = 0;
		int32 SideTriangleTests = 0;
		int32 TriangleHits = 0;
		int32 BestHitUpdates = 0;
	};

	constexpr int32 SurfaceGridRaycastBucketResolution = 16;

}

USRPlanetSurfaceGrid::USRPlanetSurfaceGrid()
{
	PrimaryComponentTick.bCanEverTick = true;

	FaceResolution = 8;
	PlanetRadius = 1000.0f;
	bRebuildGridOnRegister = false;
	bGridVisible = false;
	DebugLineColor = FLinearColor(0.15f, 0.85f, 1.0f, 1.0f);
	DebugLineOpacity = 1.0f;
	HoveredCellColor = FLinearColor(1.0f, 0.85f, 0.2f, 1.0f);
	SelectedCellColor = FLinearColor(0.25f, 1.0f, 0.35f, 1.0f);
	OccupiedCellColor = FLinearColor(1.0f, 0.35f, 0.35f, 1.0f);
	DebugLineThickness = 1.0f;
	GridSurfaceOffset = 0.0f;
	DynamicMeshGeneration = FSRDynamicMeshGeneration();
	DynamicMeshGeneration.bDynamicMeshGeneration = false;
	DynamicMeshGeneration.DynamicMeshHeight = 0.0f;
	bHasHoveredCell = false;
	bHasSelectedCell = false;
	bUsingGeneratedGridCells = false;
	bGridMeshDirty = true;
	bCellsDirty = true;
	InteractionHighlightBatchDepth = 0;
	bHasBatchedInteractionHighlightRefresh = false;

	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCastShadow(false);
	SetVisibility(false);
	SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> VertexColorMaterialFinder(
		TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	if (VertexColorMaterialFinder.Succeeded())
	{
		GridOverlayMaterial = VertexColorMaterialFinder.Object;
		SetMaterial(0, GridOverlayMaterial);
	}
}

void USRPlanetSurfaceGrid::OnRegister()
{
	Super::OnRegister();
	if (GridOverlayMaterial)
	{
		SetMaterial(0, GridOverlayMaterial);
	}
	UpdateDebugTickState();

	if (bRebuildGridOnRegister && !IsTemplate())
	{
		RebuildGrid();
	}
}

void USRPlanetSurfaceGrid::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void USRPlanetSurfaceGrid::RebuildGrid()
{
	if (!RebuildCellsFromOwnerGeneratedGrid())
	{
		bUsingGeneratedGridCells = false;
		Cells = USRPlanetSurfaceGridLibrary::GenerateCubeSphereCells(FMath::Max(1, FaceResolution), FMath::Max(1.0f, PlanetRadius));
		for (FSRPlanetSurfaceGridCell& Cell : Cells)
		{
			const FSRPlanetTerrainSample TerrainSample = GetTerrainSampleAtDirection(Cell.LocalCenter.GetSafeNormal());
			Cell.Biome = TerrainSample.Biome;
			Cell.BiomeId = TerrainSample.BiomeId;
			Cell.WaterRole = TerrainSample.WaterRole;
		}
	}
	if (ASRCelestialBody* OwnerBody = Cast<ASRCelestialBody>(GetOwner()))
	{
		OwnerBody->ClearSurfaceCellHighlights();
	}
	bHasHoveredCell = false;
	HoveredCellId = FSRPlanetSurfaceGridCellId();
	bHasSelectedCell = false;
	SelectedCellId = FSRPlanetSurfaceGridCellId();
	SetInteractionOverlayVisible(false);
	RebuildCellIndex();
	RebuildCellInfoIndex();
	RebuildRaycastIndex();
	bCellsDirty = false;
	bGridMeshDirty = true;
	if (bGridVisible)
	{
		RequestInteractionHighlightRefresh();
	}
}

void USRPlanetSurfaceGrid::SetPlanetRadius(float NewPlanetRadius)
{
	PlanetRadius = FMath::Max(1.0f, NewPlanetRadius);
	bCellsDirty = true;
	bGridMeshDirty = true;
	if (bGridVisible)
	{
		RebuildGrid();
	}
}

void USRPlanetSurfaceGrid::SetFaceResolution(int32 NewFaceResolution)
{
	FaceResolution = FMath::Max(1, NewFaceResolution);
	bCellsDirty = true;
	bGridMeshDirty = true;
	if (bGridVisible)
	{
		RebuildGrid();
	}
}

void USRPlanetSurfaceGrid::ClearOccupancy()
{
	for (FSRPlanetSurfaceGridCell& Cell : Cells)
	{
		Cell.bOccupied = false;
		Cell.OccupantId = NAME_None;
		FSRPlanetSurfaceGridCellInfo UpdatedCellInfo = BuildCellInfo(Cell);
		FSRPlanetSurfaceGridCellInfo ExistingCellInfo;
		if (GetStoredCellInfoById(Cell.CellId, ExistingCellInfo))
		{
			UpdatedCellInfo.FaceCellIndex = ExistingCellInfo.FaceCellIndex;
		}
		StoreCellInfo(UpdatedCellInfo);
	}
	bGridMeshDirty = true;
	if (bGridVisible)
	{
		RequestInteractionHighlightRefresh();
	}
}

int32 USRPlanetSurfaceGrid::GetFaceResolution() const
{
	return FaceResolution;
}

float USRPlanetSurfaceGrid::GetPlanetRadius() const
{
	return PlanetRadius;
}

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

bool USRPlanetSurfaceGrid::RaycastCell(const FVector& RayOrigin, const FVector& RayDirection, FSRPlanetSurfaceGridCell& OutCell, FVector& OutHitLocation) const
{
	OutHitLocation = FVector::ZeroVector;
	const double RaycastStartSeconds = SRSurfaceGridNowSeconds();
	FSRSurfaceGridRaycastStats RaycastStats;
	auto LogRaycastStats = [this, RaycastStartSeconds, &RaycastStats](bool bHit, bool bGeneratedGridPath, float BestHitDistance)
	{
		static uint64 RaycastSampleCounter = 0;
		++RaycastSampleCounter;

		const double ElapsedMs = SRSurfaceGridElapsedMilliseconds(RaycastStartSeconds);
		constexpr uint64 SampleInterval = 60;
		constexpr double SlowRaycastThresholdMs = 0.25;
		if (ElapsedMs < SlowRaycastThresholdMs && (RaycastSampleCounter % SampleInterval) != 0)
		{
			return;
		}

		FSRTimingLog::AddLine(FString::Printf(
			TEXT("SurfaceGrid.RaycastCell Body=%s Total=%.3fms Hit=%s GeneratedGrid=%s Buckets=%d/%d SkipBuckets=%d Cells=%d DupSkips=%d TopTris=%d SideTris=%d TriHits=%d BestUpdates=%d BestDist=%.2f"),
			*GetNameSafe(GetOwner()),
			ElapsedMs,
			bHit ? TEXT("true") : TEXT("false"),
			bGeneratedGridPath ? TEXT("true") : TEXT("false"),
			RaycastStats.BucketHits,
			RaycastStats.BucketTests,
			RaycastStats.BucketSkippedByBestHit,
			RaycastStats.CellTests,
			RaycastStats.DuplicateCellSkips,
			RaycastStats.TopTriangleTests,
			RaycastStats.SideTriangleTests,
			RaycastStats.TriangleHits,
			RaycastStats.BestHitUpdates,
			BestHitDistance));
	};

	if (bUsingGeneratedGridCells && !Cells.IsEmpty())
	{
		const FTransform ComponentTransform = GetComponentTransform();
		const FVector LocalRayOrigin = ComponentTransform.InverseTransformPosition(RayOrigin);
		const FVector LocalRayDirection = ComponentTransform.InverseTransformVectorNoScale(RayDirection).GetSafeNormal();
		if (!LocalRayDirection.IsNearlyZero())
		{
			float BestHitDistance = BIG_NUMBER;
			int32 BestCellIndex = INDEX_NONE;

			auto ConsiderCell = [this, &LocalRayOrigin, &LocalRayDirection, &BestHitDistance, &BestCellIndex, &RaycastStats](int32 CellIndex)
			{
				if (!Cells.IsValidIndex(CellIndex))
				{
					return;
				}

				++RaycastStats.CellTests;

				auto ConsiderTriangleHit = [&LocalRayOrigin, &LocalRayDirection, &BestHitDistance, &BestCellIndex, &RaycastStats, CellIndex](
					const FVector& Point0,
					const FVector& Point1,
					const FVector& Point2,
					bool bSideTriangle)
				{
					if (bSideTriangle)
					{
						++RaycastStats.SideTriangleTests;
					}
					else
					{
						++RaycastStats.TopTriangleTests;
					}

					float HitDistance = 0.0f;
					if (!IntersectRayTriangle(LocalRayOrigin, LocalRayDirection, Point0, Point1, Point2, HitDistance))
					{
						return;
					}

					++RaycastStats.TriangleHits;
					if (HitDistance < BestHitDistance)
					{
						BestHitDistance = HitDistance;
						BestCellIndex = CellIndex;
						++RaycastStats.BestHitUpdates;
					}
				};

				const FSRPlanetSurfaceGridCell& Cell = Cells[CellIndex];
				ConsiderTriangleHit(Cell.Corner00, Cell.Corner10, Cell.Corner11, false);
				ConsiderTriangleHit(Cell.Corner00, Cell.Corner11, Cell.Corner01, false);
				for (const FSRPlanetSurfaceGridSideFace& SideFace : Cell.SideFaces)
				{
					ConsiderTriangleHit(SideFace.LocalPoint0, SideFace.LocalPoint1, SideFace.LocalPoint2, true);
					ConsiderTriangleHit(SideFace.LocalPoint0, SideFace.LocalPoint2, SideFace.LocalPoint3, true);
				}
			};

			for (const FSRSurfaceGridRaycastBucket& Bucket : RaycastBuckets)
			{
				++RaycastStats.BucketTests;
				float BucketHitDistance = 0.0f;
				if (!IntersectRayBox(LocalRayOrigin, LocalRayDirection, Bucket.LocalBounds, BucketHitDistance))
				{
					continue;
				}
				++RaycastStats.BucketHits;
				if (BucketHitDistance > BestHitDistance)
				{
					++RaycastStats.BucketSkippedByBestHit;
					continue;
				}

				for (const int32 CellIndex : Bucket.CellIndices)
				{
					ConsiderCell(CellIndex);
				}
			}

			if (Cells.IsValidIndex(BestCellIndex))
			{
				OutCell = Cells[BestCellIndex];
				OutHitLocation = ComponentTransform.TransformPosition(LocalRayOrigin + (LocalRayDirection * BestHitDistance));
				LogRaycastStats(true, true, BestHitDistance);
				return true;
			}

			LogRaycastStats(false, true, BestHitDistance);
			return false;
		}
	}

	if (!IntersectRayWithSurfaceSphere(RayOrigin, RayDirection, OutHitLocation))
	{
		OutCell = FSRPlanetSurfaceGridCell();
		LogRaycastStats(false, false, BIG_NUMBER);
		return false;
	}

	if (!ProjectWorldLocationToCell(OutHitLocation, OutCell))
	{
		LogRaycastStats(false, false, BIG_NUMBER);
		return false;
	}

	OutHitLocation = bUsingGeneratedGridCells
		? GetComponentTransform().TransformPosition(OutCell.LocalCenter)
		: ResolveWorldSurfacePoint(OutCell.LocalNormal, 0.0f);
	LogRaycastStats(true, false, 0.0f);
	return true;
}

bool USRPlanetSurfaceGrid::SetCellOccupied(const FSRPlanetSurfaceGridCellId& CellId, bool bOccupied, FName OccupantId)
{
	int32 CellIndex = INDEX_NONE;
	if (!GetCellIndex(CellId, CellIndex))
	{
		return false;
	}

	FSRPlanetSurfaceGridCell& Cell = Cells[CellIndex];
	Cell.bOccupied = bOccupied;
	Cell.OccupantId = bOccupied ? OccupantId : NAME_None;
	FSRPlanetSurfaceGridCellInfo UpdatedCellInfo = BuildCellInfo(Cell);
	FSRPlanetSurfaceGridCellInfo ExistingCellInfo;
	if (GetStoredCellInfoById(CellId, ExistingCellInfo))
	{
		UpdatedCellInfo.FaceCellIndex = ExistingCellInfo.FaceCellIndex;
	}
	StoreCellInfo(UpdatedCellInfo);
	bGridMeshDirty = true;
	if (bGridVisible)
	{
		RequestInteractionHighlightRefresh();
	}
	return true;
}

bool USRPlanetSurfaceGrid::CanOccupyCells(const TArray<FSRPlanetSurfaceGridCellId>& CellIds) const
{
	if (CellIds.IsEmpty())
	{
		return false;
	}

	for (const FSRPlanetSurfaceGridCellId& CellId : CellIds)
	{
		FSRPlanetSurfaceGridCellInfo CellInfo;
		if (!GetCellInfoById(CellId, CellInfo) || !CellInfo.bCanConstruct || CellInfo.bOccupied)
		{
			return false;
		}
	}

	return true;
}

bool USRPlanetSurfaceGrid::SetCellsOccupied(const TArray<FSRPlanetSurfaceGridCellId>& CellIds, bool bOccupied, FName OccupantId)
{
	if (CellIds.IsEmpty())
	{
		return false;
	}

	TArray<int32> CellIndices;
	CellIndices.Reserve(CellIds.Num());
	for (const FSRPlanetSurfaceGridCellId& CellId : CellIds)
	{
		int32 CellIndex = INDEX_NONE;
		if (!GetCellIndex(CellId, CellIndex))
		{
			return false;
		}

		CellIndices.Add(CellIndex);
	}

	for (int32 CellIndex : CellIndices)
	{
		FSRPlanetSurfaceGridCell& Cell = Cells[CellIndex];
		Cell.bOccupied = bOccupied;
		Cell.OccupantId = bOccupied ? OccupantId : NAME_None;
		FSRPlanetSurfaceGridCellInfo UpdatedCellInfo = BuildCellInfo(Cell);
		FSRPlanetSurfaceGridCellInfo ExistingCellInfo;
		if (GetStoredCellInfoById(Cell.CellId, ExistingCellInfo))
		{
			UpdatedCellInfo.FaceCellIndex = ExistingCellInfo.FaceCellIndex;
		}
		StoreCellInfo(UpdatedCellInfo);
	}

	bGridMeshDirty = true;
	if (bGridVisible)
	{
		RequestInteractionHighlightRefresh();
	}
	return true;
}

void USRPlanetSurfaceGrid::BeginInteractionHighlightBatch()
{
	++InteractionHighlightBatchDepth;
}

void USRPlanetSurfaceGrid::EndInteractionHighlightBatch()
{
	InteractionHighlightBatchDepth = FMath::Max(0, InteractionHighlightBatchDepth - 1);
	if (InteractionHighlightBatchDepth == 0 && bHasBatchedInteractionHighlightRefresh)
	{
		bHasBatchedInteractionHighlightRefresh = false;
		RefreshInteractionHighlight();
	}
}

bool USRPlanetSurfaceGrid::SetHoveredCell(const FSRPlanetSurfaceGridCellId& CellId)
{
	int32 CellIndex = INDEX_NONE;
	if (!GetCellIndex(CellId, CellIndex))
	{
		return false;
	}

	if (bHasHoveredCell && HoveredCellId == CellId)
	{
		return true;
	}

	bHasHoveredCell = true;
	HoveredCellId = CellId;
	RequestInteractionHighlightRefresh();
	UpdateDebugTickState();
	return true;
}

void USRPlanetSurfaceGrid::ClearHoveredCell()
{
	if (!bHasHoveredCell)
	{
		return;
	}

	bHasHoveredCell = false;
	HoveredCellId = FSRPlanetSurfaceGridCellId();
	RequestInteractionHighlightRefresh();
	UpdateDebugTickState();
}

bool USRPlanetSurfaceGrid::HasHoveredCell() const
{
	return bHasHoveredCell;
}

bool USRPlanetSurfaceGrid::GetHoveredCell(FSRPlanetSurfaceGridCell& OutCell) const
{
	return bHasHoveredCell && GetCellById(HoveredCellId, OutCell);
}

bool USRPlanetSurfaceGrid::GetHoveredCellInfo(FSRPlanetSurfaceGridCellInfo& OutCellInfo) const
{
	return bHasHoveredCell && GetCellInfoById(HoveredCellId, OutCellInfo);
}

bool USRPlanetSurfaceGrid::GetInteractionGridPatchCellIds(
	const FSRPlanetSurfaceGridCellId& CenterCellId,
	TArray<FSRPlanetSurfaceGridCellId>& OutCellIds) const
{
	OutCellIds.Reset();

	FSRPlanetSurfaceGridCell CenterCell;
	if (!GetCellById(CenterCellId, CenterCell))
	{
		return false;
	}

	constexpr int32 PatchRadius = 2;
	const int32 SafeFaceResolution = FMath::Max(1, FaceResolution);

	enum class ESRGridDisplayEdge : uint8
	{
		NegativeX,
		PositiveX,
		NegativeY,
		PositiveY
	};

	struct FSRDisplayGridCoord
	{
		ESRCubeSphereFace Face = ESRCubeSphereFace::PositiveX;
		int32 X = 0;
		int32 Y = 0;
	};

	struct FSRDisplayFaceBasis
	{
		FVector Normal = FVector::ForwardVector;
		FVector AxisX = FVector::RightVector;
		FVector AxisY = FVector::UpVector;
	};

	struct FSRDisplayWalkBasis
	{
		FIntPoint AxisX = FIntPoint(1, 0);
		FIntPoint AxisY = FIntPoint(0, 1);
	};

	auto CanonicalToDisplay = [SafeFaceResolution](const FSRPlanetSurfaceGridCellId& CellId)
	{
		FSRDisplayGridCoord Coord;
		Coord.Face = CellId.Face;
		Coord.X = CellId.CellX;
		Coord.Y = CellId.CellY;
		if (CellId.Face == ESRCubeSphereFace::PositiveZ || CellId.Face == ESRCubeSphereFace::NegativeZ)
		{
			Coord.X = SafeFaceResolution - 1 - CellId.CellX;
			Coord.Y = SafeFaceResolution - 1 - CellId.CellY;
		}

		return Coord;
	};

	auto DisplayToCanonical = [SafeFaceResolution](const FSRDisplayGridCoord& Coord)
	{
		FSRPlanetSurfaceGridCellId CellId;
		CellId.Face = Coord.Face;
		CellId.CellX = Coord.X;
		CellId.CellY = Coord.Y;
		if (Coord.Face == ESRCubeSphereFace::PositiveZ || Coord.Face == ESRCubeSphereFace::NegativeZ)
		{
			CellId.CellX = SafeFaceResolution - 1 - Coord.X;
			CellId.CellY = SafeFaceResolution - 1 - Coord.Y;
		}

		return CellId;
	};

	auto GetDisplayFaceBasis = [](ESRCubeSphereFace Face)
	{
		const FSRSurfaceGridCubeFaceBasis CubeBasis = GetCubeFaceBasis(Face);
		FSRDisplayFaceBasis Basis;
		Basis.Normal = CubeBasis.Normal;
		Basis.AxisX = CubeBasis.AxisU;
		Basis.AxisY = CubeBasis.AxisV;
		if (Face == ESRCubeSphereFace::PositiveZ || Face == ESRCubeSphereFace::NegativeZ)
		{
			Basis.AxisX *= -1.0f;
			Basis.AxisY *= -1.0f;
		}
		return Basis;
	};

	auto ResolveDisplayDirection = [&GetDisplayFaceBasis](ESRCubeSphereFace Face, const FIntPoint& LocalDirection)
	{
		const FSRDisplayFaceBasis Basis = GetDisplayFaceBasis(Face);
		return (Basis.AxisX * static_cast<float>(LocalDirection.X))
			+ (Basis.AxisY * static_cast<float>(LocalDirection.Y));
	};

	auto TryResolveLocalDisplayDirection = [&GetDisplayFaceBasis](ESRCubeSphereFace Face, const FVector& WorldDirection, FIntPoint& OutLocalDirection)
	{
		const FSRDisplayFaceBasis Basis = GetDisplayFaceBasis(Face);
		const float AxisXDot = FVector::DotProduct(WorldDirection, Basis.AxisX);
		const float AxisYDot = FVector::DotProduct(WorldDirection, Basis.AxisY);
		if (FMath::Abs(AxisXDot) > 0.999f)
		{
			OutLocalDirection = FIntPoint(AxisXDot > 0.0f ? 1 : -1, 0);
			return true;
		}
		if (FMath::Abs(AxisYDot) > 0.999f)
		{
			OutLocalDirection = FIntPoint(0, AxisYDot > 0.0f ? 1 : -1);
			return true;
		}

		return false;
	};

	auto RotateDisplayDirectionAcrossEdge = [&GetDisplayFaceBasis](
		const FVector& SourceWorldDirection,
		ESRCubeSphereFace SourceFace,
		ESRCubeSphereFace TargetFace)
	{
		const FSRDisplayFaceBasis SourceBasis = GetDisplayFaceBasis(SourceFace);
		const FSRDisplayFaceBasis TargetBasis = GetDisplayFaceBasis(TargetFace);
		const float TargetNormalDot = FVector::DotProduct(SourceWorldDirection, TargetBasis.Normal);
		const FVector SharedEdgeDirection = SourceWorldDirection - (TargetBasis.Normal * TargetNormalDot);
		return SharedEdgeDirection + ((-SourceBasis.Normal) * TargetNormalDot);
	};

	auto TryRotateWalkBasisAcrossEdge = [
		&ResolveDisplayDirection,
		&TryResolveLocalDisplayDirection,
		&RotateDisplayDirectionAcrossEdge](
		ESRCubeSphereFace SourceFace,
		ESRCubeSphereFace TargetFace,
		FSRDisplayWalkBasis& WalkBasis)
	{
		const FVector SourceAxisX = ResolveDisplayDirection(SourceFace, WalkBasis.AxisX);
		const FVector SourceAxisY = ResolveDisplayDirection(SourceFace, WalkBasis.AxisY);
		const FVector TargetAxisX = RotateDisplayDirectionAcrossEdge(SourceAxisX, SourceFace, TargetFace);
		const FVector TargetAxisY = RotateDisplayDirectionAcrossEdge(SourceAxisY, SourceFace, TargetFace);
		return TryResolveLocalDisplayDirection(TargetFace, TargetAxisX, WalkBasis.AxisX)
			&& TryResolveLocalDisplayDirection(TargetFace, TargetAxisY, WalkBasis.AxisY);
	};

	auto TryTransitionDisplayEdge = [SafeFaceResolution](
		const FSRDisplayGridCoord& From,
		ESRGridDisplayEdge Edge,
		FSRDisplayGridCoord& Out)
	{
		const int32 MaxCell = SafeFaceResolution - 1;
		switch (From.Face)
		{
		case ESRCubeSphereFace::PositiveX:
			switch (Edge)
			{
			case ESRGridDisplayEdge::NegativeX:
				Out = { ESRCubeSphereFace::NegativeY, MaxCell, From.Y };
				return true;
			case ESRGridDisplayEdge::PositiveX:
				Out = { ESRCubeSphereFace::PositiveY, 0, From.Y };
				return true;
			case ESRGridDisplayEdge::NegativeY:
				Out = { ESRCubeSphereFace::NegativeZ, MaxCell - From.X, 0 };
				return true;
			case ESRGridDisplayEdge::PositiveY:
				Out = { ESRCubeSphereFace::PositiveZ, MaxCell - From.X, MaxCell };
				return true;
			}
			break;
		case ESRCubeSphereFace::NegativeX:
			switch (Edge)
			{
			case ESRGridDisplayEdge::NegativeX:
				Out = { ESRCubeSphereFace::PositiveY, MaxCell, From.Y };
				return true;
			case ESRGridDisplayEdge::PositiveX:
				Out = { ESRCubeSphereFace::NegativeY, 0, From.Y };
				return true;
			case ESRGridDisplayEdge::NegativeY:
				Out = { ESRCubeSphereFace::NegativeZ, From.X, MaxCell };
				return true;
			case ESRGridDisplayEdge::PositiveY:
				Out = { ESRCubeSphereFace::PositiveZ, From.X, 0 };
				return true;
			}
			break;
		case ESRCubeSphereFace::PositiveY:
			switch (Edge)
			{
			case ESRGridDisplayEdge::NegativeX:
				Out = { ESRCubeSphereFace::PositiveX, MaxCell, From.Y };
				return true;
			case ESRGridDisplayEdge::PositiveX:
				Out = { ESRCubeSphereFace::NegativeX, 0, From.Y };
				return true;
			case ESRGridDisplayEdge::NegativeY:
				Out = { ESRCubeSphereFace::NegativeZ, 0, From.X };
				return true;
			case ESRGridDisplayEdge::PositiveY:
				Out = { ESRCubeSphereFace::PositiveZ, 0, MaxCell - From.X };
				return true;
			}
			break;
		case ESRCubeSphereFace::NegativeY:
			switch (Edge)
			{
			case ESRGridDisplayEdge::NegativeX:
				Out = { ESRCubeSphereFace::NegativeX, MaxCell, From.Y };
				return true;
			case ESRGridDisplayEdge::PositiveX:
				Out = { ESRCubeSphereFace::PositiveX, 0, From.Y };
				return true;
			case ESRGridDisplayEdge::NegativeY:
				Out = { ESRCubeSphereFace::NegativeZ, MaxCell, MaxCell - From.X };
				return true;
			case ESRGridDisplayEdge::PositiveY:
				Out = { ESRCubeSphereFace::PositiveZ, MaxCell, From.X };
				return true;
			}
			break;
		case ESRCubeSphereFace::PositiveZ:
			switch (Edge)
			{
			case ESRGridDisplayEdge::NegativeX:
				Out = { ESRCubeSphereFace::PositiveY, MaxCell - From.Y, MaxCell };
				return true;
			case ESRGridDisplayEdge::PositiveX:
				Out = { ESRCubeSphereFace::NegativeY, From.Y, MaxCell };
				return true;
			case ESRGridDisplayEdge::NegativeY:
				Out = { ESRCubeSphereFace::NegativeX, From.X, MaxCell };
				return true;
			case ESRGridDisplayEdge::PositiveY:
				Out = { ESRCubeSphereFace::PositiveX, MaxCell - From.X, MaxCell };
				return true;
			}
			break;
		case ESRCubeSphereFace::NegativeZ:
			switch (Edge)
			{
			case ESRGridDisplayEdge::NegativeX:
				Out = { ESRCubeSphereFace::PositiveY, From.Y, 0 };
				return true;
			case ESRGridDisplayEdge::PositiveX:
				Out = { ESRCubeSphereFace::NegativeY, MaxCell - From.Y, 0 };
				return true;
			case ESRGridDisplayEdge::NegativeY:
				Out = { ESRCubeSphereFace::PositiveX, MaxCell - From.X, 0 };
				return true;
			case ESRGridDisplayEdge::PositiveY:
				Out = { ESRCubeSphereFace::NegativeX, From.X, 0 };
				return true;
			}
			break;
		default:
			break;
		}

		return false;
	};

	auto TryStepDisplayCoord = [SafeFaceResolution, &TryTransitionDisplayEdge](
		const FSRDisplayGridCoord& FromCoord,
		const FIntPoint& LocalStep,
		FSRDisplayGridCoord& OutCoord,
		bool& bOutCrossedEdge)
	{
		OutCoord = FromCoord;
		bOutCrossedEdge = false;
		if (LocalStep.X == 0 && LocalStep.Y == 0)
		{
			return true;
		}

		if (LocalStep.X != 0)
		{
			const int32 TargetX = FromCoord.X + (LocalStep.X < 0 ? -1 : 1);
			if (TargetX >= 0 && TargetX < SafeFaceResolution)
			{
				OutCoord.X = TargetX;
				return true;
			}

			bOutCrossedEdge = true;
			return TryTransitionDisplayEdge(
				FromCoord,
				LocalStep.X < 0 ? ESRGridDisplayEdge::NegativeX : ESRGridDisplayEdge::PositiveX,
				OutCoord);
		}

		if (LocalStep.Y != 0)
		{
			const int32 TargetY = FromCoord.Y + (LocalStep.Y < 0 ? -1 : 1);
			if (TargetY >= 0 && TargetY < SafeFaceResolution)
			{
				OutCoord.Y = TargetY;
				return true;
			}

			bOutCrossedEdge = true;
			return TryTransitionDisplayEdge(
				FromCoord,
				LocalStep.Y < 0 ? ESRGridDisplayEdge::NegativeY : ESRGridDisplayEdge::PositiveY,
				OutCoord);
		}

		return false;
	};

	auto TryStepWalker = [
		this,
		&DisplayToCanonical,
		&TryStepDisplayCoord,
		&TryRotateWalkBasisAcrossEdge](
		FSRDisplayGridCoord& CurrentCoord,
		FSRDisplayWalkBasis& WalkBasis,
		bool bIsGlobalX,
		int32 StepSign)
	{
		const FIntPoint LocalAxis = bIsGlobalX ? WalkBasis.AxisX : WalkBasis.AxisY;
		const FIntPoint LocalStep(LocalAxis.X * StepSign, LocalAxis.Y * StepSign);
		FSRDisplayGridCoord NextCoord;
		bool bCrossedEdge = false;
		if (!TryStepDisplayCoord(CurrentCoord, LocalStep, NextCoord, bCrossedEdge))
		{
			return false;
		}

		if (bCrossedEdge && !TryRotateWalkBasisAcrossEdge(CurrentCoord.Face, NextCoord.Face, WalkBasis))
		{
			return false;
		}

		FSRPlanetSurfaceGridCell ResolvedCell;
		const FSRPlanetSurfaceGridCellId NextCellId = DisplayToCanonical(NextCoord);
		if (!GetCellById(NextCellId, ResolvedCell))
		{
			return false;
		}

		CurrentCoord = NextCoord;
		return true;
	};

	auto TryWalkPatchCellId = [&CanonicalToDisplay, &DisplayToCanonical, &TryStepWalker, CenterCellId](
		int32 OffsetX,
		int32 OffsetY,
		bool bWalkXFirst,
		FSRPlanetSurfaceGridCellId& OutCellId)
	{
		FSRDisplayGridCoord CurrentCoord = CanonicalToDisplay(CenterCellId);
		FSRDisplayWalkBasis WalkBasis;
		auto WalkAxis = [&TryStepWalker, &CurrentCoord, &WalkBasis](int32 Offset, bool bIsX)
		{
			const int32 Step = Offset < 0 ? -1 : 1;
			for (int32 StepIndex = 0; StepIndex < FMath::Abs(Offset); ++StepIndex)
			{
				if (!TryStepWalker(CurrentCoord, WalkBasis, bIsX, Step))
				{
					return false;
				}
			}
			return true;
		};

		if (bWalkXFirst)
		{
			if (!WalkAxis(OffsetX, true) || !WalkAxis(OffsetY, false))
			{
				return false;
			}
		}
		else if (!WalkAxis(OffsetY, false) || !WalkAxis(OffsetX, true))
		{
			return false;
		}

		OutCellId = DisplayToCanonical(CurrentCoord);
		return true;
	};
	TSet<FSRPlanetSurfaceGridCellId> PatchCellIds;
	PatchCellIds.Reserve((PatchRadius * 2 + 1) * (PatchRadius * 2 + 1));
	OutCellIds.Reserve((PatchRadius * 2 + 1) * (PatchRadius * 2 + 1));

	auto AddPatchCellId = [this, &PatchCellIds, &OutCellIds](const FSRPlanetSurfaceGridCellId& PatchCellId)
	{
		FSRPlanetSurfaceGridCell PatchCell;
		if (!GetCellById(PatchCellId, PatchCell) || PatchCellIds.Contains(PatchCellId))
		{
			return;
		}

		PatchCellIds.Add(PatchCellId);
		OutCellIds.Add(PatchCellId);
	};
	auto AddWalkedPatchCellId = [&TryWalkPatchCellId, &AddPatchCellId](int32 OffsetX, int32 OffsetY, bool bWalkXFirst)
	{
		FSRPlanetSurfaceGridCellId PatchCellId;
		if (TryWalkPatchCellId(OffsetX, OffsetY, bWalkXFirst, PatchCellId))
		{
			AddPatchCellId(PatchCellId);
		}
	};
	auto GetOverflowOffset = [PatchRadius](int32 DirectionSign, int32 OverflowIndex, int32 OverflowCount)
	{
		const int32 DistanceFromCenter = PatchRadius - OverflowCount + 1 + OverflowIndex;
		return DirectionSign > 0 ? DistanceFromCenter : -DistanceFromCenter;
	};
	auto GetCornerOverflowSpan = [](int32 XOverflow, int32 YOverflow)
	{
		if (XOverflow <= 0 || YOverflow <= 0)
		{
			return 0;
		}

		return FMath::Min(XOverflow, YOverflow);
	};

	const FSRDisplayGridCoord CenterDisplayCoord = CanonicalToDisplay(CenterCellId);
	const int32 DesiredMinX = CenterDisplayCoord.X - PatchRadius;
	const int32 DesiredMaxX = CenterDisplayCoord.X + PatchRadius;
	const int32 DesiredMinY = CenterDisplayCoord.Y - PatchRadius;
	const int32 DesiredMaxY = CenterDisplayCoord.Y + PatchRadius;

	const int32 ClippedMinX = FMath::Clamp(DesiredMinX, 0, SafeFaceResolution - 1);
	const int32 ClippedMaxX = FMath::Clamp(DesiredMaxX, 0, SafeFaceResolution - 1);
	const int32 ClippedMinY = FMath::Clamp(DesiredMinY, 0, SafeFaceResolution - 1);
	const int32 ClippedMaxY = FMath::Clamp(DesiredMaxY, 0, SafeFaceResolution - 1);

	const int32 NegativeXOverflow = FMath::Max(0, -DesiredMinX);
	const int32 PositiveXOverflow = FMath::Max(0, DesiredMaxX - (SafeFaceResolution - 1));
	const int32 NegativeYOverflow = FMath::Max(0, -DesiredMinY);
	const int32 PositiveYOverflow = FMath::Max(0, DesiredMaxY - (SafeFaceResolution - 1));

	for (int32 CellY = ClippedMinY; CellY <= ClippedMaxY; ++CellY)
	{
		for (int32 CellX = ClippedMinX; CellX <= ClippedMaxX; ++CellX)
		{
			FSRDisplayGridCoord PatchCoord = CenterDisplayCoord;
			PatchCoord.X = CellX;
			PatchCoord.Y = CellY;
			const FSRPlanetSurfaceGridCellId PatchCellId = DisplayToCanonical(PatchCoord);
			AddPatchCellId(PatchCellId);
		}
	}

	for (int32 OverflowIndex = 0; OverflowIndex < NegativeXOverflow; ++OverflowIndex)
	{
		const int32 OffsetX = GetOverflowOffset(-1, OverflowIndex, NegativeXOverflow);
		for (int32 CellY = ClippedMinY; CellY <= ClippedMaxY; ++CellY)
		{
			AddWalkedPatchCellId(OffsetX, CellY - CenterDisplayCoord.Y, true);
		}
	}
	for (int32 OverflowIndex = 0; OverflowIndex < PositiveXOverflow; ++OverflowIndex)
	{
		const int32 OffsetX = GetOverflowOffset(1, OverflowIndex, PositiveXOverflow);
		for (int32 CellY = ClippedMinY; CellY <= ClippedMaxY; ++CellY)
		{
			AddWalkedPatchCellId(OffsetX, CellY - CenterDisplayCoord.Y, true);
		}
	}
	for (int32 OverflowIndex = 0; OverflowIndex < NegativeYOverflow; ++OverflowIndex)
	{
		const int32 OffsetY = GetOverflowOffset(-1, OverflowIndex, NegativeYOverflow);
		for (int32 CellX = ClippedMinX; CellX <= ClippedMaxX; ++CellX)
		{
			AddWalkedPatchCellId(CellX - CenterDisplayCoord.X, OffsetY, false);
		}
	}
	for (int32 OverflowIndex = 0; OverflowIndex < PositiveYOverflow; ++OverflowIndex)
	{
		const int32 OffsetY = GetOverflowOffset(1, OverflowIndex, PositiveYOverflow);
		for (int32 CellX = ClippedMinX; CellX <= ClippedMaxX; ++CellX)
		{
			AddWalkedPatchCellId(CellX - CenterDisplayCoord.X, OffsetY, false);
		}
	}

	auto AddCornerOverflow = [
		&AddWalkedPatchCellId,
		&GetOverflowOffset,
		&GetCornerOverflowSpan](
		int32 DirectionX,
		int32 XOverflow,
		int32 DirectionY,
		int32 YOverflow)
	{
		const int32 CornerSpan = GetCornerOverflowSpan(XOverflow, YOverflow);
		if (CornerSpan <= 0)
		{
			return;
		}

		const bool bWalkXFirst = XOverflow <= YOverflow;
		for (int32 OverflowYIndex = 0; OverflowYIndex < CornerSpan; ++OverflowYIndex)
		{
			const int32 OffsetY = GetOverflowOffset(DirectionY, OverflowYIndex, YOverflow);
			for (int32 OverflowXIndex = 0; OverflowXIndex < CornerSpan; ++OverflowXIndex)
			{
				const int32 OffsetX = GetOverflowOffset(DirectionX, OverflowXIndex, XOverflow);
				AddWalkedPatchCellId(OffsetX, OffsetY, bWalkXFirst);
			}
		}
	};

	AddCornerOverflow(-1, NegativeXOverflow, -1, NegativeYOverflow);
	AddCornerOverflow(1, PositiveXOverflow, -1, NegativeYOverflow);
	AddCornerOverflow(-1, NegativeXOverflow, 1, PositiveYOverflow);
	AddCornerOverflow(1, PositiveXOverflow, 1, PositiveYOverflow);

	return !OutCellIds.IsEmpty();
}

bool USRPlanetSurfaceGrid::SetSelectedCell(const FSRPlanetSurfaceGridCellId& CellId)
{
	int32 CellIndex = INDEX_NONE;
	if (!GetCellIndex(CellId, CellIndex))
	{
		return false;
	}

	if (bHasSelectedCell && SelectedCellId == CellId)
	{
		return true;
	}

	bHasSelectedCell = true;
	SelectedCellId = CellId;
	RequestInteractionHighlightRefresh();
	UpdateDebugTickState();
	return true;
}

void USRPlanetSurfaceGrid::ClearSelectedCell()
{
	if (!bHasSelectedCell)
	{
		return;
	}

	bHasSelectedCell = false;
	SelectedCellId = FSRPlanetSurfaceGridCellId();
	RequestInteractionHighlightRefresh();
	UpdateDebugTickState();
}

bool USRPlanetSurfaceGrid::HasSelectedCell() const
{
	return bHasSelectedCell;
}

bool USRPlanetSurfaceGrid::GetSelectedCell(FSRPlanetSurfaceGridCell& OutCell) const
{
	return bHasSelectedCell && GetCellById(SelectedCellId, OutCell);
}

bool USRPlanetSurfaceGrid::GetSelectedCellInfo(FSRPlanetSurfaceGridCellInfo& OutCellInfo) const
{
	return bHasSelectedCell && GetCellInfoById(SelectedCellId, OutCellInfo);
}

void USRPlanetSurfaceGrid::DrawDebugGrid(float Duration) const
{
	if (!GetWorld() || Cells.IsEmpty() || !bGridVisible)
	{
		return;
	}

	const FLinearColor DefaultDebugLineColor(DebugLineColor.R, DebugLineColor.G, DebugLineColor.B, DebugLineOpacity);
	const FColor DefaultLineColor = DefaultDebugLineColor.ToFColor(true);
	const FColor HoverLineColor = HoveredCellColor.ToFColor(true);
	const FColor SelectedLineColor = SelectedCellColor.ToFColor(true);

	FSRCameraInfo CameraInfo;
	FSRLineThicknessUtils::TryBuildPrimaryCameraInfo(GetWorld(), CameraInfo);

	float ReferenceViewDepth = FSRLineThicknessUtils::DefaultReferenceViewDepth;
	float ReferenceFieldOfViewDegrees = FSRLineThicknessUtils::DefaultReferenceFieldOfViewDegrees;
	FSRLineThicknessUtils::ResolveReferenceView(GetWorld(), ReferenceViewDepth, ReferenceFieldOfViewDegrees);

	TSet<uint64> DrawnEdges;
	DrawnEdges.Reserve(Cells.Num() * 2);
	auto DrawUniqueDefaultEdge = [this, &DrawnEdges, &DefaultLineColor, Duration, &CameraInfo, ReferenceViewDepth, ReferenceFieldOfViewDegrees](
		const FVector& CornerA,
		const FVector& CornerB)
	{
		const uint64 EdgeKey = BuildGridEdgeKey(CornerA, CornerB);
		if (DrawnEdges.Contains(EdgeKey))
		{
			return;
		}

		DrawnEdges.Add(EdgeKey);
		DrawDebugSurfaceLine(CornerA, CornerB, DefaultLineColor, Duration, DebugLineThickness, CameraInfo, ReferenceViewDepth, ReferenceFieldOfViewDegrees);
	};

	for (const FSRPlanetSurfaceGridCell& Cell : Cells)
	{
		for (int32 EdgeIndex = 0; EdgeIndex < 4; ++EdgeIndex)
		{
			FVector EdgePointA;
			FVector EdgePointB;
			if (GetGridCellEdgePoints(Cell, EdgeIndex, EdgePointA, EdgePointB))
			{
				DrawUniqueDefaultEdge(EdgePointA, EdgePointB);
			}
		}
	}

	for (const FSRPlanetSurfaceGridCell& Cell : Cells)
	{
		const bool bIsHovered = bHasHoveredCell && (Cell.CellId == HoveredCellId);
		const bool bIsSelected = bHasSelectedCell && (Cell.CellId == SelectedCellId);
		const bool bShouldHighlightCell = bIsHovered || bIsSelected;
		if (!bShouldHighlightCell)
		{
			continue;
		}

		const FColor LineColor = bIsSelected ? SelectedLineColor : HoverLineColor;
		const float LineThickness = bIsSelected ? DebugLineThickness * 2.5f : DebugLineThickness * 2.0f;
		for (int32 EdgeIndex = 0; EdgeIndex < 4; ++EdgeIndex)
		{
			FVector EdgePointA;
			FVector EdgePointB;
			if (GetGridCellEdgePoints(Cell, EdgeIndex, EdgePointA, EdgePointB))
			{
				DrawDebugSurfaceLine(EdgePointA, EdgePointB, LineColor, Duration, LineThickness, CameraInfo, ReferenceViewDepth, ReferenceFieldOfViewDegrees);
			}
		}
	}
}

void USRPlanetSurfaceGrid::SetGridVisible(bool bNewGridVisible)
{
	bGridVisible = bNewGridVisible;
	SetVisibility(false);
	SetHiddenInGame(true);
	SetInteractionOverlayVisible(bGridVisible && (bHasHoveredCell || bHasSelectedCell));
	if (!bGridVisible)
	{
		ClearHoveredCell();
		ClearSelectedCell();
	}
	else
	{
		if (ASRCelestialBody* OwnerBody = Cast<ASRCelestialBody>(GetOwner()))
		{
			OwnerBody->PrepareCelestialBodyDynamicMesh();
		}

		if (bCellsDirty)
		{
			RebuildGrid();
		}

		RequestInteractionHighlightRefresh();
	}
	UpdateDebugTickState();
}

bool USRPlanetSurfaceGrid::IsGridVisible() const
{
	return bGridVisible;
}

void USRPlanetSurfaceGrid::PrepareGridForAssembly()
{
	if (ASRCelestialBody* OwnerBody = Cast<ASRCelestialBody>(GetOwner()))
	{
		OwnerBody->PrepareCelestialBodyDynamicMesh();
	}

	if (Cells.IsEmpty() || bCellsDirty)
	{
		RebuildGrid();
	}

	if (!bCellsDirty && bGridMeshDirty)
	{
		UE::Geometry::FDynamicMesh3 EmptyGridMesh;
		EmptyGridMesh.EnableAttributes();
		EmptyGridMesh.Attributes()->EnablePrimaryColors();
		SetMesh(MoveTemp(EmptyGridMesh));
		bGridMeshDirty = false;
	}
}

void USRPlanetSurfaceGrid::ConfigureDebugGrid(
	FLinearColor NewGridLineColor,
	float NewGridLineOpacity,
	float NewLineThickness,
	FLinearColor NewHoveredCellColor,
	FLinearColor NewSelectedCellColor,
	FLinearColor NewOccupiedCellColor,
	float NewSurfaceOffset)
{
	DebugLineColor = NewGridLineColor;
	DebugLineOpacity = FMath::Clamp(NewGridLineOpacity, 0.0f, 1.0f);
	DebugLineThickness = FMath::Clamp(NewLineThickness, 0.0f, 2.0f);
	HoveredCellColor = NewHoveredCellColor;
	SelectedCellColor = NewSelectedCellColor;
	OccupiedCellColor = NewOccupiedCellColor;
	GridSurfaceOffset = FMath::Clamp(NewSurfaceOffset, 0.0f, 1.0f);
	bGridMeshDirty = true;
	if (bGridVisible)
	{
		RequestInteractionHighlightRefresh();
	}
}

void USRPlanetSurfaceGrid::SetGridOverlayMaterial(UMaterialInterface* NewGridOverlayMaterial)
{
	GridOverlayMaterial = NewGridOverlayMaterial;
	UMaterialInterface* EffectiveGridMaterial = GridOverlayMaterial ? GridOverlayMaterial.Get() : GetMaterial(0);
	if (EffectiveGridMaterial)
	{
		SetMaterial(0, EffectiveGridMaterial);
		if (InteractionOverlayMesh)
		{
			InteractionOverlayMesh->SetMaterial(0, EffectiveGridMaterial);
		}
	}
}

float USRPlanetSurfaceGrid::GetSurfaceHeightOffsetAtDirection_Implementation(FVector LocalUnitDirection) const
{
	return ComputeProceduralDynamicMeshHeight(LocalUnitDirection);
}

void USRPlanetSurfaceGrid::ConfigureProceduralTerrain(
	bool bNewDynamicMeshGeneration,
	int32 NewGenerationSeed,
	float NewDynamicMeshHeight,
	float NewDetailFrequency,
	int32 NewNoiseOctaves,
	float NewNoisePersistence)
{
	FSRDynamicMeshGeneration NewDynamicMeshGeneration = DynamicMeshGeneration;
	NewDynamicMeshGeneration.bDynamicMeshGeneration = bNewDynamicMeshGeneration;
	NewDynamicMeshGeneration.GenerationSeed = NewGenerationSeed;
	NewDynamicMeshGeneration.DynamicMeshHeight = FMath::Max(0.0f, NewDynamicMeshHeight);
	NewDynamicMeshGeneration.DetailFrequency = FMath::Max(0.01f, NewDetailFrequency);
	NewDynamicMeshGeneration.NoiseOctaves = FMath::Max(1, NewNoiseOctaves);
	NewDynamicMeshGeneration.NoisePersistence = FMath::Clamp(NewNoisePersistence, 0.0f, 1.0f);
	ConfigureTerrain(NewDynamicMeshGeneration);
}

void USRPlanetSurfaceGrid::ConfigureTerrain(const FSRDynamicMeshGeneration& NewDynamicMeshGeneration)
{
	DynamicMeshGeneration = NewDynamicMeshGeneration;
	DynamicMeshGeneration.DynamicMeshHeight = FMath::Max(0.0f, DynamicMeshGeneration.DynamicMeshHeight);
	DynamicMeshGeneration.ContinentFrequency = FMath::Max(0.01f, DynamicMeshGeneration.ContinentFrequency);
	DynamicMeshGeneration.MountainFrequency = FMath::Max(0.01f, DynamicMeshGeneration.MountainFrequency);
	DynamicMeshGeneration.DetailFrequency = FMath::Max(0.01f, DynamicMeshGeneration.DetailFrequency);
	DynamicMeshGeneration.ValleyStrength = FMath::Clamp(DynamicMeshGeneration.ValleyStrength, 0.0f, 1.0f);
	DynamicMeshGeneration.MountainStrength = FMath::Clamp(DynamicMeshGeneration.MountainStrength, 0.5f, 4.0f);
	DynamicMeshGeneration.NoiseStrength = FMath::Clamp(DynamicMeshGeneration.NoiseStrength, 0.0f, 1.0f);
	DynamicMeshGeneration.RiverStrength = FMath::Clamp(DynamicMeshGeneration.RiverStrength, 0.0f, 1.0f);
	DynamicMeshGeneration.LakeStrength = FMath::Clamp(DynamicMeshGeneration.LakeStrength, 0.0f, 1.0f);
	DynamicMeshGeneration.DetailStrength = FMath::Clamp(DynamicMeshGeneration.DetailStrength, 0.0f, 1.0f);
	DynamicMeshGeneration.MoistureFrequency = FMath::Max(0.01f, DynamicMeshGeneration.MoistureFrequency);
	DynamicMeshGeneration.TemperatureFrequency = FMath::Max(0.01f, DynamicMeshGeneration.TemperatureFrequency);
	DynamicMeshGeneration.NoiseOctaves = FMath::Max(1, DynamicMeshGeneration.NoiseOctaves);
	DynamicMeshGeneration.NoisePersistence = FMath::Clamp(DynamicMeshGeneration.NoisePersistence, 0.0f, 1.0f);
	DynamicMeshGeneration.OceanThreshold = FMath::Clamp(DynamicMeshGeneration.OceanThreshold, -1.0f, 1.0f);
	DynamicMeshGeneration.AtmosphereThreshold = FMath::Max(0.01f, DynamicMeshGeneration.AtmosphereThreshold);
	if (DynamicMeshGeneration.BiomeDataAssets.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Planet surface grid terrain requires Profile BiomeDataAssets."));
	}
	else
	{
		DynamicMeshGeneration.NormalizeBiomeMaterials(DynamicMeshGeneration.BiomeDataAssets);
	}
	bCellsDirty = true;
	bGridMeshDirty = true;
	if (bGridVisible)
	{
		RebuildGrid();
	}
}

FSRPlanetTerrainSample USRPlanetSurfaceGrid::GetTerrainSampleAtDirection(FVector LocalUnitDirection) const
{
	FSRBiomeSampleContext SampleContext;
	SampleContext.LocalUnitDirection = LocalUnitDirection.GetSafeNormal();
	if (SampleContext.LocalUnitDirection.IsNearlyZero())
	{
		SampleContext.LocalUnitDirection = FVector::UpVector;
	}

	FSRPlanetSurfaceGridCellId CellId;
	FVector2D FaceCoordinates = FVector2D::ZeroVector;
	if (USRPlanetSurfaceGridLibrary::ProjectDirectionToCubeSphereCellId(
		SampleContext.LocalUnitDirection,
		FMath::Max(1, FaceResolution),
		CellId,
		FaceCoordinates))
	{
		SampleContext.Face = CellId.Face;
		SampleContext.CellX = CellId.CellX;
		SampleContext.CellY = CellId.CellY;
		SampleContext.FaceResolution = FMath::Max(1, FaceResolution);
		SampleContext.FaceUV = FaceCoordinates;
	}

	FSRPlanetTerrainSample Sample = FSRPlanetTerrainGenerator::SampleTerrain(SampleContext, DynamicMeshGeneration);
	const float HeightStep = GetTerrainHeightStep();
	if (HeightStep > KINDA_SMALL_NUMBER)
	{
		Sample.HeightOffset = FMath::RoundToFloat(Sample.HeightOffset / HeightStep) * HeightStep;
	}
	return Sample;
}

float USRPlanetSurfaceGrid::GetTerrainHeightStep() const
{
	const float SafeDynamicMeshHeight = FMath::Max(0.0f, DynamicMeshGeneration.DynamicMeshHeight);
	return DynamicMeshGeneration.bMinecraft && DynamicMeshGeneration.bDynamicMeshGeneration && SafeDynamicMeshHeight > KINDA_SMALL_NUMBER
		? (2.0f * FMath::Max(1.0f, PlanetRadius)) / static_cast<float>(FMath::Max(1, FaceResolution))
		: 0.0f;
}

void USRPlanetSurfaceGrid::AppendGeneratedGridCell(
	UE::Geometry::FDynamicMesh3& GridMesh,
	const FSRPlanetSurfaceGridCell& Cell,
	TSet<uint64>& DrawnEdges) const
{
	const FLinearColor DefaultLineColor(DebugLineColor.R, DebugLineColor.G, DebugLineColor.B, DebugLineOpacity);
	auto AppendDedupedSegment = [this, &GridMesh, &DefaultLineColor, &DrawnEdges](const FVector& PointA, const FVector& PointB)
	{
		const uint64 EdgeKey = BuildGridEdgeKey(PointA, PointB);
		if (DrawnEdges.Contains(EdgeKey))
		{
			return;
		}

		DrawnEdges.Add(EdgeKey);
		AppendGridWireSegment(GridMesh, PointA, PointB, DefaultLineColor, DebugLineThickness);
	};

	for (int32 EdgeIndex = 0; EdgeIndex < 4; ++EdgeIndex)
	{
		FVector EdgePointA;
		FVector EdgePointB;
		if (GetGridCellEdgePoints(Cell, EdgeIndex, EdgePointA, EdgePointB))
		{
			AppendDedupedSegment(
				OffsetGeneratedGridWirePoint(EdgePointA, GridSurfaceOffset),
				OffsetGeneratedGridWirePoint(EdgePointB, GridSurfaceOffset));
		}
	}

	for (const FSRPlanetSurfaceGridLineSegment& SideLineSegment : Cell.SideLineSegments)
	{
		AppendDedupedSegment(
			OffsetGeneratedGridWirePoint(SideLineSegment.LocalPointA, GridSurfaceOffset),
			OffsetGeneratedGridWirePoint(SideLineSegment.LocalPointB, GridSurfaceOffset));
	}
}

void USRPlanetSurfaceGrid::ApplyGeneratedGridBuild(
	TArray<FSRPlanetSurfaceGridCell>&& NewCells,
	UE::Geometry::FDynamicMesh3&& NewGridMesh)
{
	TArray<int32> EmptyCellIndexByFlatId;
	ApplyGeneratedGridBuild(MoveTemp(NewCells), MoveTemp(NewGridMesh), MoveTemp(EmptyCellIndexByFlatId));
}

void USRPlanetSurfaceGrid::ApplyGeneratedGridBuild(
	TArray<FSRPlanetSurfaceGridCell>&& NewCells,
	UE::Geometry::FDynamicMesh3&& NewGridMesh,
	TMap<FSRPlanetSurfaceGridCellId, int32>&& NewCellIndexById)
{
	TArray<int32> NewCellIndexByFlatId;
	const int32 ExpectedFlatCellCount = 6 * FMath::Max(1, FaceResolution) * FMath::Max(1, FaceResolution);
	NewCellIndexByFlatId.Init(INDEX_NONE, ExpectedFlatCellCount);
	for (const TPair<FSRPlanetSurfaceGridCellId, int32>& CellIndexPair : NewCellIndexById)
	{
		const int32 FlatIndex = CellIndexPair.Key.IsValid(FaceResolution)
			? ((static_cast<int32>(CellIndexPair.Key.Face) * FaceResolution + CellIndexPair.Key.CellY) * FaceResolution + CellIndexPair.Key.CellX)
			: INDEX_NONE;
		if (NewCellIndexByFlatId.IsValidIndex(FlatIndex))
		{
			NewCellIndexByFlatId[FlatIndex] = CellIndexPair.Value;
		}
	}
	ApplyGeneratedGridBuild(MoveTemp(NewCells), MoveTemp(NewGridMesh), MoveTemp(NewCellIndexByFlatId));
}

void USRPlanetSurfaceGrid::ApplyGeneratedGridBuild(
	TArray<FSRPlanetSurfaceGridCell>&& NewCells,
	UE::Geometry::FDynamicMesh3&& NewGridMesh,
	TArray<int32>&& NewCellIndexByFlatId)
{
	FSRTimingLogSession TimingLogSession(FString::Printf(TEXT("SurfaceGrid.ApplyGeneratedGridBuild Body=%s"), *GetNameSafe(GetOwner())));
	const double TotalStart = SRSurfaceGridNowSeconds();
	const int32 IncomingCellCount = NewCells.Num();
	double StageStart = SRSurfaceGridNowSeconds();
	if (ASRCelestialBody* OwnerBody = Cast<ASRCelestialBody>(GetOwner()))
	{
		OwnerBody->ClearSurfaceCellHighlights();
	}
	const double ClearHighlightMs = SRSurfaceGridElapsedMilliseconds(StageStart);

	StageStart = SRSurfaceGridNowSeconds();
	Cells = MoveTemp(NewCells);
	bUsingGeneratedGridCells = true;
	TMap<ESRCubeSphereFace, int32> CellCountByFace;
	for (const FSRPlanetSurfaceGridCell& Cell : Cells)
	{
		CellCountByFace.FindOrAdd(Cell.CellId.Face)++;
	}
	for (const TPair<ESRCubeSphereFace, int32>& CellCountPair : CellCountByFace)
	{
		FaceResolution = FMath::Max(1, FMath::RoundToInt(FMath::Sqrt(static_cast<float>(CellCountPair.Value))));
		break;
	}
	bHasHoveredCell = false;
	HoveredCellId = FSRPlanetSurfaceGridCellId();
	bHasSelectedCell = false;
	SelectedCellId = FSRPlanetSurfaceGridCellId();
	SetInteractionOverlayVisible(false);
	const double AssignCellsMs = SRSurfaceGridElapsedMilliseconds(StageStart);

	StageStart = SRSurfaceGridNowSeconds();
	const int32 ExpectedFlatCellCount = 6 * FaceResolution * FaceResolution;
	if (NewCellIndexByFlatId.Num() == ExpectedFlatCellCount)
	{
		CellIndexById.Reset();
		CellIndexByFlatId = MoveTemp(NewCellIndexByFlatId);
	}
	else
	{
		RebuildCellIndex();
	}
	const double RebuildCellIndexMs = SRSurfaceGridElapsedMilliseconds(StageStart);

	StageStart = SRSurfaceGridNowSeconds();
	RebuildCellInfoIndex();
	const double RebuildCellInfoIndexMs = SRSurfaceGridElapsedMilliseconds(StageStart);

	StageStart = SRSurfaceGridNowSeconds();
	RebuildRaycastIndex();
	const double RebuildRaycastIndexMs = SRSurfaceGridElapsedMilliseconds(StageStart);

	StageStart = SRSurfaceGridNowSeconds();
	UE::Geometry::FDynamicMesh3 EmptyGridMesh;
	EmptyGridMesh.EnableAttributes();
	EmptyGridMesh.Attributes()->EnablePrimaryColors();
	SetMesh(MoveTemp(EmptyGridMesh));
	SetVisibility(false);
	SetHiddenInGame(true);
	bCellsDirty = false;
	bGridMeshDirty = false;
	UpdateDebugTickState();
	const double FinalizeMs = SRSurfaceGridElapsedMilliseconds(StageStart);

	FSRTimingLog::AddLine(FString::Printf(TEXT("SurfaceGrid.ApplyGeneratedGridBuild Body=%s Total=%.2fms Cells=%d FaceResolution=%d ClearHighlights=%.2fms AssignCells=%.2fms CellIndex=%.2fms CellInfoIndex=%.2fms RaycastIndex=%.2fms Finalize=%.2fms"),
		*GetNameSafe(GetOwner()),
		SRSurfaceGridElapsedMilliseconds(TotalStart),
		IncomingCellCount,
		FaceResolution,
		ClearHighlightMs,
		AssignCellsMs,
		RebuildCellIndexMs,
		RebuildCellInfoIndexMs,
		RebuildRaycastIndexMs,
		FinalizeMs));
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

void USRPlanetSurfaceGrid::RebuildRaycastIndex()
{
	RaycastBuckets.Reset();
	if (!bUsingGeneratedGridCells || Cells.IsEmpty())
	{
		return;
	}

	constexpr int32 FaceCount = 6;
	constexpr int32 BucketResolution = SurfaceGridRaycastBucketResolution;
	const ESRCubeSphereFace Faces[FaceCount] =
	{
		ESRCubeSphereFace::PositiveX,
		ESRCubeSphereFace::NegativeX,
		ESRCubeSphereFace::PositiveY,
		ESRCubeSphereFace::NegativeY,
		ESRCubeSphereFace::PositiveZ,
		ESRCubeSphereFace::NegativeZ,
	};

	auto GetFaceBucketOffset = [&Faces](ESRCubeSphereFace Face) -> int32
	{
		for (int32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
		{
			if (Faces[FaceIndex] == Face)
			{
				return FaceIndex * BucketResolution * BucketResolution;
			}
		}
		return INDEX_NONE;
	};

	RaycastBuckets.SetNum(FaceCount * BucketResolution * BucketResolution);
	for (int32 FaceIndex = 0; FaceIndex < FaceCount; ++FaceIndex)
	{
		for (int32 BucketY = 0; BucketY < BucketResolution; ++BucketY)
		{
			for (int32 BucketX = 0; BucketX < BucketResolution; ++BucketX)
			{
				const int32 BucketIndex = FaceIndex * BucketResolution * BucketResolution + BucketY * BucketResolution + BucketX;
				FSRSurfaceGridRaycastBucket& Bucket = RaycastBuckets[BucketIndex];
				Bucket.Face = Faces[FaceIndex];
				Bucket.BucketX = BucketX;
				Bucket.BucketY = BucketY;
				Bucket.LocalBounds = FBox(ForceInit);
				Bucket.CellIndices.Reset();
			}
		}
	}

	const int32 BucketCellSize = FMath::Max(1, FMath::DivideAndRoundUp(FMath::Max(1, FaceResolution), BucketResolution));
	for (int32 CellIndex = 0; CellIndex < Cells.Num(); ++CellIndex)
	{
		const FSRPlanetSurfaceGridCell& Cell = Cells[CellIndex];
		const int32 FaceBucketOffset = GetFaceBucketOffset(Cell.CellId.Face);
		if (FaceBucketOffset == INDEX_NONE)
		{
			continue;
		}

		const int32 BucketX = FMath::Clamp(Cell.CellId.CellX / BucketCellSize, 0, BucketResolution - 1);
		const int32 BucketY = FMath::Clamp(Cell.CellId.CellY / BucketCellSize, 0, BucketResolution - 1);
		const int32 BucketIndex = FaceBucketOffset + BucketY * BucketResolution + BucketX;
		if (!RaycastBuckets.IsValidIndex(BucketIndex))
		{
			continue;
		}

		FSRSurfaceGridRaycastBucket& Bucket = RaycastBuckets[BucketIndex];
		Bucket.CellIndices.Add(CellIndex);
		Bucket.LocalBounds += Cell.Corner00;
		Bucket.LocalBounds += Cell.Corner10;
		Bucket.LocalBounds += Cell.Corner11;
		Bucket.LocalBounds += Cell.Corner01;
		for (const FSRPlanetSurfaceGridSideFace& SideFace : Cell.SideFaces)
		{
			Bucket.LocalBounds += SideFace.LocalPoint0;
			Bucket.LocalBounds += SideFace.LocalPoint1;
			Bucket.LocalBounds += SideFace.LocalPoint2;
			Bucket.LocalBounds += SideFace.LocalPoint3;
		}
	}

	const float BoundsPadding = FMath::Max(1.0f, GridSurfaceOffset + 1.0f);
	for (FSRSurfaceGridRaycastBucket& Bucket : RaycastBuckets)
	{
		if (Bucket.LocalBounds.IsValid)
		{
			Bucket.LocalBounds = Bucket.LocalBounds.ExpandBy(BoundsPadding);
		}
	}
}

bool USRPlanetSurfaceGrid::RebuildCellsFromOwnerGeneratedGrid()
{
	const ASRCelestialBody* OwnerBody = Cast<ASRCelestialBody>(GetOwner());
	if (!IsValid(OwnerBody))
	{
		return false;
	}

	if (!OwnerBody->GetCachedSurfaceGridCells(Cells))
	{
		return false;
	}

	bUsingGeneratedGridCells = true;
	return true;
}

void USRPlanetSurfaceGrid::UpdateDebugTickState()
{
	SetComponentTickEnabled(false);
}

void USRPlanetSurfaceGrid::EnsureInteractionOverlay()
{
	if (InteractionOverlayMesh || IsTemplate())
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	InteractionOverlayMesh = NewObject<UDynamicMeshComponent>(OwnerActor, TEXT("SurfaceGridInteractionOverlay"));
	if (!InteractionOverlayMesh)
	{
		return;
	}

	InteractionOverlayMesh->SetupAttachment(this);
	InteractionOverlayMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InteractionOverlayMesh->SetGenerateOverlapEvents(false);
	InteractionOverlayMesh->SetCastShadow(false);
	InteractionOverlayMesh->SetVisibility(false);
	InteractionOverlayMesh->SetHiddenInGame(true);
	InteractionOverlayMesh->RegisterComponent();

	UMaterialInterface* GridMaterial = GridOverlayMaterial ? GridOverlayMaterial.Get() : GetMaterial(0);
	if (GridMaterial)
	{
		InteractionOverlayMesh->SetMaterial(0, GridMaterial);
	}
}

void USRPlanetSurfaceGrid::RequestInteractionHighlightRefresh()
{
	if (InteractionHighlightBatchDepth > 0)
	{
		bHasBatchedInteractionHighlightRefresh = true;
		return;
	}

	RefreshInteractionHighlight();
}

void USRPlanetSurfaceGrid::RefreshInteractionHighlight()
{
	const double TotalStart = SRSurfaceGridNowSeconds();
	ASRCelestialBody* OwnerBody = Cast<ASRCelestialBody>(GetOwner());
	double StageStart = SRSurfaceGridNowSeconds();
	const bool bUseDynamicMeshHighlight = !bGridVisible;
	const bool bAppliedDynamicMeshHighlight = bUseDynamicMeshHighlight && IsValid(OwnerBody)
		&& OwnerBody->ApplySurfaceCellHighlights(
			HoveredCellId,
			bHasHoveredCell,
			SelectedCellId,
			bHasSelectedCell,
			HoveredCellColor,
			SelectedCellColor);
	const double ApplyHighlightMs = SRSurfaceGridElapsedMilliseconds(StageStart);

	StageStart = SRSurfaceGridNowSeconds();
	if (IsValid(OwnerBody) && (!bUseDynamicMeshHighlight || (!bHasHoveredCell && !bHasSelectedCell)))
	{
		OwnerBody->ClearSurfaceCellHighlights();
	}
	const double ClearHighlightMs = SRSurfaceGridElapsedMilliseconds(StageStart);

	StageStart = SRSurfaceGridNowSeconds();
	RebuildInteractionOverlayMesh(!bAppliedDynamicMeshHighlight);
	const double OverlayMs = SRSurfaceGridElapsedMilliseconds(StageStart);
	const double TotalMs = SRSurfaceGridElapsedMilliseconds(TotalStart);

	static uint64 RefreshSampleCounter = 0;
	++RefreshSampleCounter;
	constexpr uint64 SampleInterval = 60;
	constexpr double SlowThresholdMs = 0.25;
	if (TotalMs >= SlowThresholdMs || (RefreshSampleCounter % SampleInterval) == 0)
	{
		FSRTimingLog::AddLine(FString::Printf(
			TEXT("SurfaceGrid.RefreshInteractionHighlight Body=%s Total=%.3fms UseDynamicMesh=%s AppliedDynamicMesh=%s HasHover=%s HasSelected=%s ApplyHighlight=%.3fms ClearHighlight=%.3fms Overlay=%.3fms"),
			*GetNameSafe(GetOwner()),
			TotalMs,
			bUseDynamicMeshHighlight ? TEXT("true") : TEXT("false"),
			bAppliedDynamicMeshHighlight ? TEXT("true") : TEXT("false"),
			bHasHoveredCell ? TEXT("true") : TEXT("false"),
			bHasSelectedCell ? TEXT("true") : TEXT("false"),
			ApplyHighlightMs,
			ClearHighlightMs,
			OverlayMs));
	}
}

void USRPlanetSurfaceGrid::RebuildInteractionOverlayMesh(bool bIncludeCellHighlightOverlay)
{
	const double TotalStart = SRSurfaceGridNowSeconds();
	double StageStart = SRSurfaceGridNowSeconds();
	EnsureInteractionOverlay();
	const double EnsureOverlayMs = SRSurfaceGridElapsedMilliseconds(StageStart);
	if (!InteractionOverlayMesh)
	{
		return;
	}

	StageStart = SRSurfaceGridNowSeconds();
	UE::Geometry::FDynamicMesh3 OverlayMesh;
	OverlayMesh.EnableAttributes();
	OverlayMesh.Attributes()->EnablePrimaryColors();

	TSet<uint64> PatchDrawnEdges;
	PatchDrawnEdges.Reserve(320);
	const double SetupMeshMs = SRSurfaceGridElapsedMilliseconds(StageStart);
	double SelectedMs = 0.0;
	double HoverPatchMs = 0.0;
	double HoverHighlightMs = 0.0;
	double OccupiedPatchMs = 0.0;
	int32 PatchCellCount = 0;
	int32 OccupiedOverlayCellCount = 0;

	FSRPlanetSurfaceGridCell SelectedCell;
	if (bHasSelectedCell && GetCellById(SelectedCellId, SelectedCell))
	{
		StageStart = SRSurfaceGridNowSeconds();
		if (bIncludeCellHighlightOverlay)
		{
			AppendInteractionCell(OverlayMesh, SelectedCell, SelectedCellColor, DebugLineThickness * 2.5f);
		}
		SelectedMs = SRSurfaceGridElapsedMilliseconds(StageStart);
	}

	if (bHasHoveredCell)
	{
		FSRPlanetSurfaceGridCell HoveredCell;
		if (GetCellById(HoveredCellId, HoveredCell))
		{
			StageStart = SRSurfaceGridNowSeconds();
			AppendInteractionGridPatch(OverlayMesh, HoveredCellId, HoveredCellColor, DebugLineThickness, PatchDrawnEdges);
			HoverPatchMs = SRSurfaceGridElapsedMilliseconds(StageStart);
			if (bIncludeCellHighlightOverlay && (!bHasSelectedCell || !(HoveredCellId == SelectedCellId)))
			{
				StageStart = SRSurfaceGridNowSeconds();
				AppendInteractionCell(OverlayMesh, HoveredCell, HoveredCellColor, DebugLineThickness * 2.0f);
				HoverHighlightMs = SRSurfaceGridElapsedMilliseconds(StageStart);
			}
		}

		StageStart = SRSurfaceGridNowSeconds();
		TArray<FSRPlanetSurfaceGridCellId> PatchCellIds;
		if (GetInteractionGridPatchCellIds(HoveredCellId, PatchCellIds))
		{
			PatchCellCount = PatchCellIds.Num();
			for (const FSRPlanetSurfaceGridCellId& PatchCellId : PatchCellIds)
			{
				FSRPlanetSurfaceGridCell PatchCell;
				if (GetCellById(PatchCellId, PatchCell) && PatchCell.bOccupied)
				{
					AppendInteractionCell(OverlayMesh, PatchCell, OccupiedCellColor, DebugLineThickness * 2.5f);
					++OccupiedOverlayCellCount;
				}
			}
		}
		OccupiedPatchMs = SRSurfaceGridElapsedMilliseconds(StageStart);
	}

	const int32 OverlayVertexCount = OverlayMesh.VertexCount();
	const int32 OverlayTriangleCount = OverlayMesh.TriangleCount();
	const int32 DrawnEdgeCount = PatchDrawnEdges.Num();
	StageStart = SRSurfaceGridNowSeconds();
	InteractionOverlayMesh->SetMesh(MoveTemp(OverlayMesh));
	const double SetMeshMs = SRSurfaceGridElapsedMilliseconds(StageStart);
	StageStart = SRSurfaceGridNowSeconds();
	SetInteractionOverlayVisible(bGridVisible && (bHasHoveredCell || bHasSelectedCell));
	const double VisibilityMs = SRSurfaceGridElapsedMilliseconds(StageStart);
	const double TotalMs = SRSurfaceGridElapsedMilliseconds(TotalStart);

	static uint64 OverlaySampleCounter = 0;
	++OverlaySampleCounter;
	constexpr uint64 SampleInterval = 60;
	constexpr double SlowThresholdMs = 0.25;
	if (TotalMs >= SlowThresholdMs || (OverlaySampleCounter % SampleInterval) == 0)
	{
		FSRTimingLog::AddLine(FString::Printf(
			TEXT("SurfaceGrid.RebuildInteractionOverlayMesh Body=%s Total=%.3fms IncludeCell=%s HasHover=%s HasSelected=%s Ensure=%.3fms Setup=%.3fms Selected=%.3fms HoverPatch=%.3fms HoverCell=%.3fms OccupiedPatch=%.3fms SetMesh=%.3fms Visibility=%.3fms PatchCells=%d OccupiedCells=%d Edges=%d Verts=%d Tris=%d"),
			*GetNameSafe(GetOwner()),
			TotalMs,
			bIncludeCellHighlightOverlay ? TEXT("true") : TEXT("false"),
			bHasHoveredCell ? TEXT("true") : TEXT("false"),
			bHasSelectedCell ? TEXT("true") : TEXT("false"),
			EnsureOverlayMs,
			SetupMeshMs,
			SelectedMs,
			HoverPatchMs,
			HoverHighlightMs,
			OccupiedPatchMs,
			SetMeshMs,
			VisibilityMs,
			PatchCellCount,
			OccupiedOverlayCellCount,
			DrawnEdgeCount,
			OverlayVertexCount,
			OverlayTriangleCount));
	}
}

void USRPlanetSurfaceGrid::AppendInteractionGridPatch(
	UE::Geometry::FDynamicMesh3& OverlayMesh,
	const FSRPlanetSurfaceGridCellId& CenterCellId,
	const FLinearColor& BaseLineColor,
	float LineThickness,
	TSet<uint64>& DrawnEdges) const
{
	TArray<FSRPlanetSurfaceGridCell> PatchCells;
	TArray<FSRPlanetSurfaceGridCellId> PatchCellIds;
	if (!GetInteractionGridPatchCellIds(CenterCellId, PatchCellIds))
	{
		return;
	}
	PatchCells.Reserve(PatchCellIds.Num());

	FLinearColor PatchLineColor = BaseLineColor;
	PatchLineColor.A = FMath::Clamp(BaseLineColor.A * DebugLineOpacity, 0.0f, 1.0f);
	if (PatchLineColor.A <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	for (const FSRPlanetSurfaceGridCellId& PatchCellId : PatchCellIds)
	{
		FSRPlanetSurfaceGridCell PatchCell;
		if (GetCellById(PatchCellId, PatchCell))
		{
			PatchCells.Add(PatchCell);
		}
	}

	auto AppendDedupedSegment = [this, &OverlayMesh, &PatchLineColor, LineThickness, &DrawnEdges](
		const FVector& PointA,
		const FVector& PointB)
	{
		if (FVector::DistSquared(PointA, PointB) <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		const uint64 EdgeKey = BuildGridEdgeKey(PointA, PointB);
		if (DrawnEdges.Contains(EdgeKey))
		{
			return;
		}

		DrawnEdges.Add(EdgeKey);
		if (bUsingGeneratedGridCells)
		{
			AppendGridWireVolumeSegment(
				OverlayMesh,
				OffsetGeneratedGridWirePoint(PointA, GridSurfaceOffset),
				OffsetGeneratedGridWirePoint(PointB, GridSurfaceOffset),
				PatchLineColor,
				LineThickness);
			return;
		}

		AppendGridWireEdge(OverlayMesh, PointA, PointB, PatchLineColor, LineThickness);
	};

	for (const FSRPlanetSurfaceGridCell& PatchCell : PatchCells)
	{
		for (int32 EdgeIndex = 0; EdgeIndex < 4; ++EdgeIndex)
		{
			FVector EdgePointA;
			FVector EdgePointB;
			if (GetGridCellEdgePoints(PatchCell, EdgeIndex, EdgePointA, EdgePointB))
			{
				AppendDedupedSegment(EdgePointA, EdgePointB);
			}
		}
	}

	for (const FSRPlanetSurfaceGridCell& PatchCell : PatchCells)
	{
		for (const FSRPlanetSurfaceGridLineSegment& SideLineSegment : PatchCell.SideLineSegments)
		{
			if (!SideLineSegment.bHasAdjacentCell || !PatchCellIds.Contains(SideLineSegment.AdjacentCellId))
			{
				continue;
			}

			AppendDedupedSegment(SideLineSegment.LocalPointA, SideLineSegment.LocalPointB);
		}
	}
}

void USRPlanetSurfaceGrid::SetInteractionOverlayVisible(bool bNewVisible)
{
	if (InteractionOverlayMesh)
	{
		InteractionOverlayMesh->SetVisibility(bNewVisible);
		InteractionOverlayMesh->SetHiddenInGame(!bNewVisible);
	}
}

void USRPlanetSurfaceGrid::AppendInteractionCell(
	UE::Geometry::FDynamicMesh3& OverlayMesh,
	const FSRPlanetSurfaceGridCell& Cell,
	const FLinearColor& LineColor,
	float LineThickness) const
{
	const float HighlightOffset = FMath::Max(0.5f, LineThickness * 0.25f);
	auto AppendFilledQuad = [&OverlayMesh](const FVector& Point0, const FVector& Point1, const FVector& Point2, const FVector& Point3, FLinearColor FillColor)
	{
		FVector QuadPoints[4] = { Point0, Point1, Point2, Point3 };
		FVector QuadNormal = FVector::CrossProduct(QuadPoints[1] - QuadPoints[0], QuadPoints[2] - QuadPoints[0]).GetSafeNormal();
		const FVector OutwardDirection = ((Point0 + Point1 + Point2 + Point3) * 0.25f).GetSafeNormal();
		if (!OutwardDirection.IsNearlyZero() && FVector::DotProduct(QuadNormal, OutwardDirection) < 0.0f)
		{
			Swap(QuadPoints[1], QuadPoints[3]);
			QuadNormal *= -1.0f;
		}
		if (QuadNormal.IsNearlyZero())
		{
			QuadNormal = OutwardDirection.IsNearlyZero() ? FVector::UpVector : OutwardDirection;
		}

		FillColor.A = FMath::Clamp(FillColor.A * 0.55f, 0.0f, 1.0f);
		const int32 Vertex0 = OverlayMesh.AppendVertex(FVector3d(QuadPoints[0]));
		const int32 Vertex1 = OverlayMesh.AppendVertex(FVector3d(QuadPoints[1]));
		const int32 Vertex2 = OverlayMesh.AppendVertex(FVector3d(QuadPoints[2]));
		const int32 Vertex3 = OverlayMesh.AppendVertex(FVector3d(QuadPoints[3]));

		const int32 Triangle0 = OverlayMesh.AppendTriangle(Vertex0, Vertex2, Vertex1);
		const int32 Triangle1 = OverlayMesh.AppendTriangle(Vertex0, Vertex3, Vertex2);

		UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay = OverlayMesh.Attributes()->PrimaryNormals();
		auto* ColorOverlay = OverlayMesh.Attributes()->PrimaryColors();
		if (!NormalOverlay || !ColorOverlay)
		{
			return;
		}

		const int32 Normal0 = NormalOverlay->AppendElement(FVector3f(QuadNormal));
		const int32 Normal1 = NormalOverlay->AppendElement(FVector3f(QuadNormal));
		const int32 Normal2 = NormalOverlay->AppendElement(FVector3f(QuadNormal));
		const int32 Normal3 = NormalOverlay->AppendElement(FVector3f(QuadNormal));
		const int32 Color0 = ColorOverlay->AppendElement(FVector4f(FillColor.R, FillColor.G, FillColor.B, FillColor.A));
		const int32 Color1 = ColorOverlay->AppendElement(FVector4f(FillColor.R, FillColor.G, FillColor.B, FillColor.A));
		const int32 Color2 = ColorOverlay->AppendElement(FVector4f(FillColor.R, FillColor.G, FillColor.B, FillColor.A));
		const int32 Color3 = ColorOverlay->AppendElement(FVector4f(FillColor.R, FillColor.G, FillColor.B, FillColor.A));

		if (Triangle0 >= 0)
		{
			NormalOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(Normal0, Normal2, Normal1));
			ColorOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(Color0, Color2, Color1));
		}
		if (Triangle1 >= 0)
		{
			NormalOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(Normal0, Normal3, Normal2));
			ColorOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(Color0, Color3, Color2));
		}
	};

	if (bUsingGeneratedGridCells)
	{
		const FVector Offset = Cell.LocalNormal.GetSafeNormal() * HighlightOffset;
		AppendFilledQuad(Cell.Corner00 + Offset, Cell.Corner10 + Offset, Cell.Corner11 + Offset, Cell.Corner01 + Offset, LineColor);
		AppendGridWireSegment(OverlayMesh, Cell.Corner00 + Offset, Cell.Corner10 + Offset, LineColor, LineThickness);
		AppendGridWireSegment(OverlayMesh, Cell.Corner10 + Offset, Cell.Corner11 + Offset, LineColor, LineThickness);
		AppendGridWireSegment(OverlayMesh, Cell.Corner11 + Offset, Cell.Corner01 + Offset, LineColor, LineThickness);
		AppendGridWireSegment(OverlayMesh, Cell.Corner01 + Offset, Cell.Corner00 + Offset, LineColor, LineThickness);
		return;
	}

	const FVector FillPoint00 = ResolveLocalSurfacePoint(Cell.Corner00.GetSafeNormal(), GridSurfaceOffset + HighlightOffset);
	const FVector FillPoint10 = ResolveLocalSurfacePoint(Cell.Corner10.GetSafeNormal(), GridSurfaceOffset + HighlightOffset);
	const FVector FillPoint11 = ResolveLocalSurfacePoint(Cell.Corner11.GetSafeNormal(), GridSurfaceOffset + HighlightOffset);
	const FVector FillPoint01 = ResolveLocalSurfacePoint(Cell.Corner01.GetSafeNormal(), GridSurfaceOffset + HighlightOffset);
	AppendFilledQuad(FillPoint00, FillPoint10, FillPoint11, FillPoint01, LineColor);
	AppendGridWireCell(OverlayMesh, Cell, LineColor, LineThickness, false, nullptr);
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
	SetVisibility(bGridVisible);
	SetHiddenInGame(!bGridVisible);
	bGridMeshDirty = false;
}

bool USRPlanetSurfaceGrid::AppendOwnerDynamicMeshWire(
	UE::Geometry::FDynamicMesh3& GridMesh,
	const FLinearColor& LineColor,
	float LineThickness) const
{
	const ASRCelestialBody* OwnerBody = Cast<ASRCelestialBody>(GetOwner());
	if (!IsValid(OwnerBody))
	{
		return false;
	}

	UDynamicMeshComponent* OwnerDynamicMeshComponent = OwnerBody->GetCelestialBodyDynamicMesh();
	UDynamicMesh* OwnerDynamicMeshObject = IsValid(OwnerDynamicMeshComponent)
		? OwnerDynamicMeshComponent->GetDynamicMesh()
		: nullptr;
	if (!IsValid(OwnerDynamicMeshObject))
	{
		return false;
	}

	bool bAppendedAnyEdge = false;
	const FTransform OwnerDynamicMeshRelativeTransform = OwnerDynamicMeshComponent->GetRelativeTransform();
	OwnerDynamicMeshObject->ProcessMesh([this, &GridMesh, &LineColor, LineThickness, &bAppendedAnyEdge, &OwnerDynamicMeshRelativeTransform](const UE::Geometry::FDynamicMesh3& OwnerMesh)
	{
		for (const int32 EdgeId : OwnerMesh.EdgeIndicesItr())
		{
			const auto EdgeTriangles = OwnerMesh.GetEdgeT(EdgeId);
			if (EdgeTriangles.A >= 0 && EdgeTriangles.B >= 0)
			{
				continue;
			}

			const auto EdgeVertices = OwnerMesh.GetEdgeV(EdgeId);
			const FVector LocalPointA = OwnerDynamicMeshRelativeTransform.TransformPosition(FVector(OwnerMesh.GetVertex(EdgeVertices.A)));
			const FVector LocalPointB = OwnerDynamicMeshRelativeTransform.TransformPosition(FVector(OwnerMesh.GetVertex(EdgeVertices.B)));
			AppendGridWireSegment(GridMesh, LocalPointA, LocalPointB, LineColor, LineThickness);
			bAppendedAnyEdge = true;
		}
	});

	return bAppendedAnyEdge;
}

void USRPlanetSurfaceGrid::AppendGridWireCell(
	UE::Geometry::FDynamicMesh3& GridMesh,
	const FSRPlanetSurfaceGridCell& Cell,
	const FLinearColor& LineColor,
	float LineThickness,
	bool bIncludeInEdgeSet,
	TSet<uint64>* DrawnEdges) const
{
	auto AppendDedupedSegment = [&GridMesh, &LineColor, LineThickness, bIncludeInEdgeSet, DrawnEdges, this](
		const FVector& PointA,
		const FVector& PointB)
	{
		if (bIncludeInEdgeSet && DrawnEdges)
		{
			const uint64 EdgeKey = BuildGridEdgeKey(PointA, PointB);
			if (DrawnEdges->Contains(EdgeKey))
			{
				return;
			}
			DrawnEdges->Add(EdgeKey);
		}

		AppendGridWireSegment(GridMesh, PointA, PointB, LineColor, LineThickness);
	};

	if (bUsingGeneratedGridCells)
	{
		for (int32 EdgeIndex = 0; EdgeIndex < 4; ++EdgeIndex)
		{
			FVector EdgePointA;
			FVector EdgePointB;
			if (GetGridCellEdgePoints(Cell, EdgeIndex, EdgePointA, EdgePointB))
			{
				AppendDedupedSegment(
					OffsetGeneratedGridWirePoint(EdgePointA, GridSurfaceOffset),
					OffsetGeneratedGridWirePoint(EdgePointB, GridSurfaceOffset));
			}
		}

		for (const FSRPlanetSurfaceGridLineSegment& SideLineSegment : Cell.SideLineSegments)
		{
			AppendDedupedSegment(
				OffsetGeneratedGridWirePoint(SideLineSegment.LocalPointA, GridSurfaceOffset),
				OffsetGeneratedGridWirePoint(SideLineSegment.LocalPointB, GridSurfaceOffset));
		}

		for (int32 EdgeIndex = 0; EdgeIndex < 4; ++EdgeIndex)
		{
			const FSRPlanetSurfaceGridCellId NeighborId = GetGridCellEdgeNeighborId(Cell, EdgeIndex);
			if (NeighborId == Cell.CellId)
			{
				continue;
			}

			FSRPlanetSurfaceGridCell NeighborCell;
			if (!GetCellById(NeighborId, NeighborCell))
			{
				continue;
			}

			FVector CellPointA;
			FVector CellPointB;
			FVector NeighborPointA;
			FVector NeighborPointB;
			if (!TryGetMatchingGridCellEdgePoints(Cell, EdgeIndex, NeighborCell, CellPointA, CellPointB, NeighborPointA, NeighborPointB))
			{
				continue;
			}

			if (FVector::DistSquared(CellPointA, NeighborPointA) > KINDA_SMALL_NUMBER)
			{
				AppendDedupedSegment(
					OffsetGeneratedGridWirePoint(CellPointA, GridSurfaceOffset),
					OffsetGeneratedGridWirePoint(NeighborPointA, GridSurfaceOffset));
			}
			if (FVector::DistSquared(CellPointB, NeighborPointB) > KINDA_SMALL_NUMBER)
			{
				AppendDedupedSegment(
					OffsetGeneratedGridWirePoint(CellPointB, GridSurfaceOffset),
					OffsetGeneratedGridWirePoint(NeighborPointB, GridSurfaceOffset));
			}
		}
		return;
	}

	auto AppendEdge = [this, &GridMesh, &LineColor, LineThickness, bIncludeInEdgeSet, DrawnEdges](
		const FVector& CornerA,
		const FVector& CornerB)
	{
		if (bIncludeInEdgeSet && DrawnEdges)
		{
			const uint64 EdgeKey = BuildGridEdgeKey(CornerA, CornerB);
			if (DrawnEdges->Contains(EdgeKey))
			{
				return;
			}
			DrawnEdges->Add(EdgeKey);
		}

		AppendGridWireEdge(GridMesh, CornerA, CornerB, LineColor, LineThickness);
	};

	for (int32 EdgeIndex = 0; EdgeIndex < 4; ++EdgeIndex)
	{
		FVector EdgePointA;
		FVector EdgePointB;
		if (GetGridCellEdgePoints(Cell, EdgeIndex, EdgePointA, EdgePointB))
		{
			AppendEdge(EdgePointA, EdgePointB);
		}
	}
}

void USRPlanetSurfaceGrid::AppendGridWireEdge(
	UE::Geometry::FDynamicMesh3& GridMesh,
	const FVector& LocalDirectionA,
	const FVector& LocalDirectionB,
	const FLinearColor& LineColor,
	float LineThickness) const
{
	const FVector DirectionA = LocalDirectionA.GetSafeNormal();
	const FVector DirectionB = LocalDirectionB.GetSafeNormal();
	if (DirectionA.IsNearlyZero() || DirectionB.IsNearlyZero())
	{
		return;
	}

	constexpr int32 SegmentCount = 8;
	FVector PreviousPoint = ResolveLocalSurfacePoint(DirectionA, GridSurfaceOffset);
	for (int32 SegmentIndex = 1; SegmentIndex <= SegmentCount; ++SegmentIndex)
	{
		const float Alpha = static_cast<float>(SegmentIndex) / static_cast<float>(SegmentCount);
		const FVector SampleDirection = FMath::Lerp(DirectionA, DirectionB, Alpha).GetSafeNormal();
		if (SampleDirection.IsNearlyZero())
		{
			continue;
		}

		const FVector CurrentPoint = ResolveLocalSurfacePoint(SampleDirection, GridSurfaceOffset);
		AppendGridWireSegment(GridMesh, PreviousPoint, CurrentPoint, LineColor, LineThickness);
		PreviousPoint = CurrentPoint;
	}
}

void USRPlanetSurfaceGrid::AppendGridWireSegment(
	UE::Geometry::FDynamicMesh3& GridMesh,
	const FVector& LocalPointA,
	const FVector& LocalPointB,
	const FLinearColor& LineColor,
	float LineThickness) const
{
	const FVector SegmentDirection = (LocalPointB - LocalPointA).GetSafeNormal();
	if (SegmentDirection.IsNearlyZero())
	{
		return;
	}

	const FVector Midpoint = (LocalPointA + LocalPointB) * 0.5f;
	FVector SurfaceNormal = Midpoint.GetSafeNormal();
	if (SurfaceNormal.IsNearlyZero())
	{
		SurfaceNormal = FVector::UpVector;
	}

	FVector SideDirection = FVector::CrossProduct(SurfaceNormal, SegmentDirection).GetSafeNormal();
	if (SideDirection.IsNearlyZero())
	{
		SideDirection = FVector::CrossProduct(FVector::UpVector, SegmentDirection).GetSafeNormal();
	}
	if (SideDirection.IsNearlyZero())
	{
		return;
	}

	const float HalfThickness = FMath::Max(0.01f, LineThickness) * 0.5f;
	const FVector Offset = SideDirection * HalfThickness;
	const int32 Vertex0 = GridMesh.AppendVertex(FVector3d(LocalPointA - Offset));
	const int32 Vertex1 = GridMesh.AppendVertex(FVector3d(LocalPointA + Offset));
	const int32 Vertex2 = GridMesh.AppendVertex(FVector3d(LocalPointB + Offset));
	const int32 Vertex3 = GridMesh.AppendVertex(FVector3d(LocalPointB - Offset));

	const int32 Triangle0 = GridMesh.AppendTriangle(Vertex0, Vertex1, Vertex2);
	const int32 Triangle1 = GridMesh.AppendTriangle(Vertex0, Vertex2, Vertex3);

	UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay = GridMesh.Attributes()->PrimaryNormals();
	auto* ColorOverlay = GridMesh.Attributes()->PrimaryColors();
	if (!NormalOverlay || !ColorOverlay)
	{
		return;
	}

	const int32 Normal0 = NormalOverlay->AppendElement(FVector3f(SurfaceNormal));
	const int32 Normal1 = NormalOverlay->AppendElement(FVector3f(SurfaceNormal));
	const int32 Normal2 = NormalOverlay->AppendElement(FVector3f(SurfaceNormal));
	const int32 Normal3 = NormalOverlay->AppendElement(FVector3f(SurfaceNormal));
	const int32 Color0 = ColorOverlay->AppendElement(FVector4f(LineColor.R, LineColor.G, LineColor.B, LineColor.A));
	const int32 Color1 = ColorOverlay->AppendElement(FVector4f(LineColor.R, LineColor.G, LineColor.B, LineColor.A));
	const int32 Color2 = ColorOverlay->AppendElement(FVector4f(LineColor.R, LineColor.G, LineColor.B, LineColor.A));
	const int32 Color3 = ColorOverlay->AppendElement(FVector4f(LineColor.R, LineColor.G, LineColor.B, LineColor.A));

	if (Triangle0 >= 0)
	{
		NormalOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(Normal0, Normal1, Normal2));
		ColorOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(Color0, Color1, Color2));
	}
	if (Triangle1 >= 0)
	{
		NormalOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(Normal0, Normal2, Normal3));
		ColorOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(Color0, Color2, Color3));
	}
}

float USRPlanetSurfaceGrid::GetEffectiveWorldRadius() const
{
	const FVector Scale3D = GetComponentTransform().GetScale3D().GetAbs();
	return PlanetRadius * Scale3D.GetMax();
}

void USRPlanetSurfaceGrid::DrawDebugSurfaceLine(
	const FVector& LocalDirectionA,
	const FVector& LocalDirectionB,
	const FColor& LineColor,
	float Duration,
	float LineThickness,
	const FSRCameraInfo& CameraInfo,
	float ReferenceViewDepth,
	float ReferenceFieldOfViewDegrees) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector DirectionA = LocalDirectionA.GetSafeNormal();
	const FVector DirectionB = LocalDirectionB.GetSafeNormal();
	if (DirectionA.IsNearlyZero() || DirectionB.IsNearlyZero())
	{
		return;
	}

	constexpr int32 SegmentCount = 8;
	const float EffectiveSurfaceOffset = FMath::Max(GridSurfaceOffset, FMath::Max(1.0f, LineThickness * 1.5f));
	FVector PreviousPoint = ResolveWorldSurfacePoint(DirectionA, EffectiveSurfaceOffset);

	for (int32 SegmentIndex = 1; SegmentIndex <= SegmentCount; ++SegmentIndex)
	{
		const float Alpha = static_cast<float>(SegmentIndex) / static_cast<float>(SegmentCount);
		const FVector SampleDirection = FMath::Lerp(DirectionA, DirectionB, Alpha).GetSafeNormal();
		if (SampleDirection.IsNearlyZero())
		{
			continue;
		}

		const FVector CurrentPoint = ResolveWorldSurfacePoint(SampleDirection, EffectiveSurfaceOffset);
		const FVector SegmentMidpoint = (PreviousPoint + CurrentPoint) * 0.5f;
		const float ScreenSpaceThickness = FSRLineThicknessUtils::ComputeWorldThicknessAtLocation(
			CameraInfo,
			SegmentMidpoint,
			LineThickness,
			ReferenceViewDepth,
			ReferenceFieldOfViewDegrees);
		DrawDebugLine(World, PreviousPoint, CurrentPoint, LineColor, false, Duration, SDPG_Foreground, FMath::Max(0.0f, ScreenSpaceThickness));
		PreviousPoint = CurrentPoint;
	}
}

FVector USRPlanetSurfaceGrid::ResolveLocalSurfacePoint(const FVector& LocalUnitDirection, float HeightOffset) const
{
	const FVector LocalDirection = LocalUnitDirection.GetSafeNormal();
	if (LocalDirection.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	const float SurfaceHeightOffset = GetSurfaceHeightOffsetAtDirection(LocalDirection);
	const FVector LocalBasePoint = LocalDirection * FMath::Max(1.0f, PlanetRadius + SurfaceHeightOffset);
	const FVector LocalSurfaceNormal = DynamicMeshGeneration.bDynamicMeshGeneration
		? ComputeProceduralSurfaceNormal(LocalDirection)
		: LocalDirection;
	return LocalBasePoint + (LocalSurfaceNormal.GetSafeNormal() * HeightOffset);
}

FVector USRPlanetSurfaceGrid::ResolveWorldSurfacePoint(const FVector& LocalUnitDirection, float HeightOffset) const
{
	return GetComponentTransform().TransformPosition(ResolveLocalSurfacePoint(LocalUnitDirection, HeightOffset));
}

float USRPlanetSurfaceGrid::ComputeProceduralDynamicMeshHeight(FVector LocalUnitDirection) const
{
	return GetTerrainSampleAtDirection(LocalUnitDirection).HeightOffset;
}

FVector USRPlanetSurfaceGrid::ComputeProceduralSurfaceNormal(FVector LocalUnitDirection) const
{
	const FVector Direction = LocalUnitDirection.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return FVector::UpVector;
	}

	FVector TangentA = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
	if (TangentA.IsNearlyZero())
	{
		TangentA = FVector::CrossProduct(Direction, FVector::RightVector).GetSafeNormal();
	}
	const FVector TangentB = FVector::CrossProduct(Direction, TangentA).GetSafeNormal();
	if (TangentA.IsNearlyZero() || TangentB.IsNearlyZero())
	{
		return Direction;
	}

	auto ResolveBasePoint = [this](const FVector& SampleDirection)
	{
		const FVector SafeDirection = SampleDirection.GetSafeNormal();
		const float SurfaceHeightOffset = GetSurfaceHeightOffsetAtDirection(SafeDirection);
		return SafeDirection * FMath::Max(1.0f, PlanetRadius + SurfaceHeightOffset);
	};

	constexpr float NormalSampleStep = 0.003f;
	const FVector PointA0 = ResolveBasePoint(Direction - TangentA * NormalSampleStep);
	const FVector PointA1 = ResolveBasePoint(Direction + TangentA * NormalSampleStep);
	const FVector PointB0 = ResolveBasePoint(Direction - TangentB * NormalSampleStep);
	const FVector PointB1 = ResolveBasePoint(Direction + TangentB * NormalSampleStep);

	FVector Normal = FVector::CrossProduct(PointA1 - PointA0, PointB1 - PointB0).GetSafeNormal();
	if (Normal.IsNearlyZero())
	{
		return Direction;
	}

	if (FVector::DotProduct(Normal, Direction) < 0.0f)
	{
		Normal *= -1.0f;
	}
	return Normal;
}

bool USRPlanetSurfaceGrid::IntersectRayWithSurfaceSphere(const FVector& RayOrigin, const FVector& RayDirection, FVector& OutHitLocation) const
{
	OutHitLocation = FVector::ZeroVector;

	const FVector Direction = RayDirection.GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return false;
	}

	const FVector SphereCenter = GetComponentLocation();
	const float SphereRadius = GetEffectiveWorldRadius()
		+ (DynamicMeshGeneration.bDynamicMeshGeneration ? FMath::Max(0.0f, DynamicMeshGeneration.DynamicMeshHeight) * GetComponentTransform().GetScale3D().GetAbsMax() : 0.0f);
	if (SphereRadius <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector OriginToCenter = RayOrigin - SphereCenter;
	const double HalfB = FVector::DotProduct(OriginToCenter, Direction);
	const double C = OriginToCenter.SizeSquared() - FMath::Square(static_cast<double>(SphereRadius));
	const double Discriminant = (HalfB * HalfB) - C;
	if (Discriminant < 0.0)
	{
		return false;
	}

	const double SqrtDiscriminant = FMath::Sqrt(Discriminant);
	double HitDistance = -HalfB - SqrtDiscriminant;
	if (HitDistance < 0.0)
	{
		HitDistance = -HalfB + SqrtDiscriminant;
	}

	if (HitDistance < 0.0)
	{
		return false;
	}

	OutHitLocation = RayOrigin + (Direction * static_cast<float>(HitDistance));
	return true;
}
