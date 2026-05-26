#include "Surface/SRPlanetSurfaceGrid.h"

#include "Algo/Reverse.h"
#include "Celestial/SRCelestialBody.h"
#include "DrawDebugHelpers.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Math/RotationMatrix.h"
#include "Materials/MaterialInterface.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Rendering/StaticMeshVertexBuffer.h"
#include "SceneManagement.h"
#include "Surface/SRPlanetSurfaceGridLibrary.h"
#include "Surface/SRPlanetTerrainGenerator.h"
#include "UDynamicMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Visual/SRLineThicknessUtils.h"

namespace
{
	uint32 HashGridDirection(const FVector& LocalDirection)
	{
		const FVector Direction = LocalDirection.GetSafeNormal();
		const int32 QuantizedX = FMath::RoundToInt((Direction.X + 1.0) * 100000.0);
		const int32 QuantizedY = FMath::RoundToInt((Direction.Y + 1.0) * 100000.0);
		const int32 QuantizedZ = FMath::RoundToInt((Direction.Z + 1.0) * 100000.0);
		return HashCombine(HashCombine(::GetTypeHash(QuantizedX), ::GetTypeHash(QuantizedY)), ::GetTypeHash(QuantizedZ));
	}

	uint64 BuildGridEdgeKey(const FVector& LocalDirectionA, const FVector& LocalDirectionB)
	{
		const uint32 EndpointA = HashGridDirection(LocalDirectionA);
		const uint32 EndpointB = HashGridDirection(LocalDirectionB);
		const uint32 MinEndpoint = FMath::Min(EndpointA, EndpointB);
		const uint32 MaxEndpoint = FMath::Max(EndpointA, EndpointB);
		return (static_cast<uint64>(MinEndpoint) << 32) | static_cast<uint64>(MaxEndpoint);
	}

	struct FSRSurfaceGridSourceTriangle
	{
		UE::Geometry::FIndex3i Vertices = UE::Geometry::FIndex3i(INDEX_NONE, INDEX_NONE, INDEX_NONE);
		bool bPaired = false;
	};

	struct FSRSurfaceGridSourceQuad
	{
		int32 Vertices[4] = { INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE };
	};

	uint64 BuildSurfaceGridSourceEdgeKey(int32 VertexIndexA, int32 VertexIndexB)
	{
		const uint32 MinVertex = static_cast<uint32>(FMath::Min(VertexIndexA, VertexIndexB));
		const uint32 MaxVertex = static_cast<uint32>(FMath::Max(VertexIndexA, VertexIndexB));
		return (static_cast<uint64>(MinVertex) << 32) | static_cast<uint64>(MaxVertex);
	}

	bool ContainsSurfaceGridSourceVertex(const UE::Geometry::FIndex3i& Triangle, int32 VertexIndex)
	{
		return Triangle.A == VertexIndex || Triangle.B == VertexIndex || Triangle.C == VertexIndex;
	}

	bool TryGetSurfaceGridTriangleOppositeVertex(const UE::Geometry::FIndex3i& Triangle, int32 SharedVertexA, int32 SharedVertexB, int32& OutOppositeVertex)
	{
		if (Triangle.A != SharedVertexA && Triangle.A != SharedVertexB)
		{
			OutOppositeVertex = Triangle.A;
			return true;
		}
		if (Triangle.B != SharedVertexA && Triangle.B != SharedVertexB)
		{
			OutOppositeVertex = Triangle.B;
			return true;
		}
		if (Triangle.C != SharedVertexA && Triangle.C != SharedVertexB)
		{
			OutOppositeVertex = Triangle.C;
			return true;
		}

		OutOppositeVertex = INDEX_NONE;
		return false;
	}

	bool TryOrderSurfaceGridQuadVertices(
		const FPositionVertexBuffer& PositionVertexBuffer,
		int32 VertexIndex0,
		int32 VertexIndex1,
		int32 VertexIndex2,
		int32 VertexIndex3,
		FSRSurfaceGridSourceQuad& OutQuad)
	{
		struct FSRQuadCorner
		{
			int32 VertexIndex = INDEX_NONE;
			FVector Position = FVector::ZeroVector;
			float Angle = 0.0f;
		};

		FSRQuadCorner Corners[4];
		Corners[0].VertexIndex = VertexIndex0;
		Corners[1].VertexIndex = VertexIndex1;
		Corners[2].VertexIndex = VertexIndex2;
		Corners[3].VertexIndex = VertexIndex3;

		FVector Center = FVector::ZeroVector;
		for (FSRQuadCorner& Corner : Corners)
		{
			if (Corner.VertexIndex == INDEX_NONE)
			{
				return false;
			}

			Corner.Position = FVector(PositionVertexBuffer.VertexPosition(Corner.VertexIndex));
			Center += Corner.Position;
		}
		Center /= 4.0f;

		const FVector SurfaceNormal = Center.GetSafeNormal();
		if (SurfaceNormal.IsNearlyZero())
		{
			return false;
		}

		FVector TangentA = FVector::CrossProduct(SurfaceNormal, FVector::UpVector).GetSafeNormal();
		if (TangentA.IsNearlyZero())
		{
			TangentA = FVector::CrossProduct(SurfaceNormal, FVector::RightVector).GetSafeNormal();
		}
		const FVector TangentB = FVector::CrossProduct(SurfaceNormal, TangentA).GetSafeNormal();
		if (TangentA.IsNearlyZero() || TangentB.IsNearlyZero())
		{
			return false;
		}

		TArray<FSRQuadCorner> OrderedCorners;
		OrderedCorners.Reserve(4);
		for (FSRQuadCorner& Corner : Corners)
		{
			const FVector Offset = Corner.Position - Center;
			Corner.Angle = FMath::Atan2(FVector::DotProduct(Offset, TangentB), FVector::DotProduct(Offset, TangentA));
			OrderedCorners.Add(Corner);
		}

		OrderedCorners.Sort([](const FSRQuadCorner& Left, const FSRQuadCorner& Right)
		{
			return Left.Angle < Right.Angle;
		});

		const FVector WindingNormal = FVector::CrossProduct(
			OrderedCorners[1].Position - OrderedCorners[0].Position,
			OrderedCorners[2].Position - OrderedCorners[0].Position).GetSafeNormal();
		if (FVector::DotProduct(WindingNormal, SurfaceNormal) < 0.0f)
		{
			Algo::Reverse(OrderedCorners);
		}

		for (int32 CornerIndex = 0; CornerIndex < 4; ++CornerIndex)
		{
			OutQuad.Vertices[CornerIndex] = OrderedCorners[CornerIndex].VertexIndex;
		}

		return true;
	}

	TArray<FSRSurfaceGridSourceQuad> RecoverSurfaceGridSourceQuads(const FPositionVertexBuffer& PositionVertexBuffer, const FRawStaticIndexBuffer& IndexBuffer, int32 IndexCount)
	{
		TArray<FSRSurfaceGridSourceTriangle> SourceTriangles;
		SourceTriangles.Reserve(IndexCount / 3);

		TMap<uint64, TArray<int32>> TriangleIndicesByEdge;
		for (int32 Index = 0; Index + 2 < IndexCount; Index += 3)
		{
			const int32 TriangleIndex = SourceTriangles.Num();
			FSRSurfaceGridSourceTriangle SourceTriangle;
			SourceTriangle.Vertices = UE::Geometry::FIndex3i(
				static_cast<int32>(IndexBuffer.GetIndex(Index)),
				static_cast<int32>(IndexBuffer.GetIndex(Index + 1)),
				static_cast<int32>(IndexBuffer.GetIndex(Index + 2)));
			SourceTriangles.Add(SourceTriangle);

			TriangleIndicesByEdge.FindOrAdd(BuildSurfaceGridSourceEdgeKey(SourceTriangle.Vertices.A, SourceTriangle.Vertices.B)).Add(TriangleIndex);
			TriangleIndicesByEdge.FindOrAdd(BuildSurfaceGridSourceEdgeKey(SourceTriangle.Vertices.B, SourceTriangle.Vertices.C)).Add(TriangleIndex);
			TriangleIndicesByEdge.FindOrAdd(BuildSurfaceGridSourceEdgeKey(SourceTriangle.Vertices.C, SourceTriangle.Vertices.A)).Add(TriangleIndex);
		}

		TArray<uint64> CandidateEdgeKeys;
		TriangleIndicesByEdge.GetKeys(CandidateEdgeKeys);
		CandidateEdgeKeys.Sort([&TriangleIndicesByEdge, &SourceTriangles, &PositionVertexBuffer](uint64 LeftKey, uint64 RightKey)
		{
			auto ResolveSharedLength = [&TriangleIndicesByEdge, &SourceTriangles, &PositionVertexBuffer](uint64 EdgeKey)
			{
				const TArray<int32>* TriangleIndices = TriangleIndicesByEdge.Find(EdgeKey);
				if (!TriangleIndices || TriangleIndices->Num() != 2)
				{
					return 0.0;
				}

				const int32 SharedVertexA = static_cast<int32>(EdgeKey >> 32);
				const int32 SharedVertexB = static_cast<int32>(EdgeKey & 0xffffffff);
				const FVector PositionA(PositionVertexBuffer.VertexPosition(SharedVertexA));
				const FVector PositionB(PositionVertexBuffer.VertexPosition(SharedVertexB));
				return FVector::DistSquared(PositionA, PositionB);
			};

			return ResolveSharedLength(LeftKey) > ResolveSharedLength(RightKey);
		});

		TArray<FSRSurfaceGridSourceQuad> SourceQuads;
		SourceQuads.Reserve(SourceTriangles.Num() / 2);
		for (const uint64 EdgeKey : CandidateEdgeKeys)
		{
			const TArray<int32>* TriangleIndices = TriangleIndicesByEdge.Find(EdgeKey);
			if (!TriangleIndices || TriangleIndices->Num() != 2)
			{
				continue;
			}

			const int32 TriangleIndexA = (*TriangleIndices)[0];
			const int32 TriangleIndexB = (*TriangleIndices)[1];
			if (!SourceTriangles.IsValidIndex(TriangleIndexA)
				|| !SourceTriangles.IsValidIndex(TriangleIndexB)
				|| SourceTriangles[TriangleIndexA].bPaired
				|| SourceTriangles[TriangleIndexB].bPaired)
			{
				continue;
			}

			const int32 SharedVertexA = static_cast<int32>(EdgeKey >> 32);
			const int32 SharedVertexB = static_cast<int32>(EdgeKey & 0xffffffff);
			const UE::Geometry::FIndex3i TriangleA = SourceTriangles[TriangleIndexA].Vertices;
			const UE::Geometry::FIndex3i TriangleB = SourceTriangles[TriangleIndexB].Vertices;
			if (!ContainsSurfaceGridSourceVertex(TriangleA, SharedVertexA)
				|| !ContainsSurfaceGridSourceVertex(TriangleA, SharedVertexB)
				|| !ContainsSurfaceGridSourceVertex(TriangleB, SharedVertexA)
				|| !ContainsSurfaceGridSourceVertex(TriangleB, SharedVertexB))
			{
				continue;
			}

			int32 OppositeVertexA = INDEX_NONE;
			int32 OppositeVertexB = INDEX_NONE;
			if (!TryGetSurfaceGridTriangleOppositeVertex(TriangleA, SharedVertexA, SharedVertexB, OppositeVertexA)
				|| !TryGetSurfaceGridTriangleOppositeVertex(TriangleB, SharedVertexA, SharedVertexB, OppositeVertexB))
			{
				continue;
			}

			const FVector SharedPositionA(PositionVertexBuffer.VertexPosition(SharedVertexA));
			const FVector SharedPositionB(PositionVertexBuffer.VertexPosition(SharedVertexB));
			const FVector OppositePositionA(PositionVertexBuffer.VertexPosition(OppositeVertexA));
			const FVector OppositePositionB(PositionVertexBuffer.VertexPosition(OppositeVertexB));
			const float SharedLength = FVector::Distance(SharedPositionA, SharedPositionB);
			const float PerimeterAverageLength = (
				FVector::Distance(SharedPositionA, OppositePositionA)
				+ FVector::Distance(OppositePositionA, SharedPositionB)
				+ FVector::Distance(SharedPositionB, OppositePositionB)
				+ FVector::Distance(OppositePositionB, SharedPositionA)) * 0.25f;

			if (PerimeterAverageLength <= KINDA_SMALL_NUMBER || SharedLength < PerimeterAverageLength * 1.12f)
			{
				continue;
			}

			FSRSurfaceGridSourceQuad SourceQuad;
			if (!TryOrderSurfaceGridQuadVertices(PositionVertexBuffer, OppositeVertexA, SharedVertexA, OppositeVertexB, SharedVertexB, SourceQuad))
			{
				continue;
			}

			SourceTriangles[TriangleIndexA].bPaired = true;
			SourceTriangles[TriangleIndexB].bPaired = true;
			SourceQuads.Add(SourceQuad);
		}

		return SourceQuads;
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
}

USRPlanetSurfaceGrid::USRPlanetSurfaceGrid()
{
	PrimaryComponentTick.bCanEverTick = true;

	FaceResolution = 8;
	PlanetRadius = 1000.0f;
	bRebuildGridOnRegister = false;
	ConstructionHeightOffset = 15.0f;
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
	bUsingRecoveredQuadCells = false;
	bGridMeshDirty = true;
	bCellsDirty = true;

	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCastShadow(false);
	SetVisibility(false);
	SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> VertexColorMaterialFinder(
		TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	if (VertexColorMaterialFinder.Succeeded())
	{
		SetMaterial(0, VertexColorMaterialFinder.Object);
	}
}

void USRPlanetSurfaceGrid::OnRegister()
{
	Super::OnRegister();
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
	if (!RebuildCellsFromOwnerStaticMeshQuads())
	{
		bUsingRecoveredQuadCells = false;
		Cells = USRPlanetSurfaceGridLibrary::GenerateCubeSphereCells(FMath::Max(1, FaceResolution), FMath::Max(1.0f, PlanetRadius));
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
	RebuildRaycastIndex();
	bCellsDirty = false;
	bGridMeshDirty = true;
	if (bGridVisible)
	{
		RefreshInteractionHighlight();
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
	}
	bGridMeshDirty = true;
	if (bGridVisible)
	{
		RefreshInteractionHighlight();
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

	const FVector LocalPosition = bUsingRecoveredQuadCells
		? Cell.LocalCenter + (Cell.LocalNormal.GetSafeNormal() * (ConstructionHeightOffset + HeightOffset))
		: ResolveLocalSurfacePoint(Cell.LocalNormal, ConstructionHeightOffset + HeightOffset);
	const FVector WorldPosition = GetComponentTransform().TransformPosition(LocalPosition);
	const FVector WorldCorner00 = bUsingRecoveredQuadCells
		? GetComponentTransform().TransformPosition(Cell.Corner00 + (Cell.LocalNormal.GetSafeNormal() * GridSurfaceOffset))
		: ResolveWorldSurfacePoint(Cell.Corner00.GetSafeNormal(), GridSurfaceOffset);
	const FVector WorldCorner10 = bUsingRecoveredQuadCells
		? GetComponentTransform().TransformPosition(Cell.Corner10 + (Cell.LocalNormal.GetSafeNormal() * GridSurfaceOffset))
		: ResolveWorldSurfacePoint(Cell.Corner10.GetSafeNormal(), GridSurfaceOffset);
	const FVector WorldCorner01 = bUsingRecoveredQuadCells
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

	if (bUsingRecoveredQuadCells)
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

	if (bUsingRecoveredQuadCells)
	{
		float BestDot = -BIG_NUMBER;
		int32 BestCellIndex = INDEX_NONE;
		for (int32 CellIndex = 0; CellIndex < Cells.Num(); ++CellIndex)
		{
			const float CandidateDot = FVector::DotProduct(Cells[CellIndex].LocalNormal.GetSafeNormal(), LocalDirection);
			if (CandidateDot > BestDot)
			{
				BestDot = CandidateDot;
				BestCellIndex = CellIndex;
			}
		}

		if (!Cells.IsValidIndex(BestCellIndex))
		{
			OutCell = FSRPlanetSurfaceGridCell();
			return false;
		}

		OutCell = Cells[BestCellIndex];
		return true;
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
	if (bUsingRecoveredQuadCells && !Cells.IsEmpty())
	{
		const FTransform ComponentTransform = GetComponentTransform();
		const FVector LocalRayOrigin = ComponentTransform.InverseTransformPosition(RayOrigin);
		const FVector LocalRayDirection = ComponentTransform.InverseTransformVectorNoScale(RayDirection).GetSafeNormal();
		if (!LocalRayDirection.IsNearlyZero())
		{
			TArray<int32> CandidateCellIndices;
			FVector BroadHitLocation = FVector::ZeroVector;
			if (IntersectRayWithSurfaceSphere(RayOrigin, RayDirection, BroadHitLocation))
			{
				const FVector LocalBroadHitDirection = ComponentTransform.InverseTransformPosition(BroadHitLocation).GetSafeNormal();
				GatherRaycastCandidateCells(LocalBroadHitDirection, CandidateCellIndices);
			}

			auto TryRaycastCandidates = [this, &ComponentTransform, &LocalRayOrigin, &LocalRayDirection, &OutCell, &OutHitLocation](const TArray<int32>& CandidateIndices)
			{
				float BestHitDistance = BIG_NUMBER;
				int32 BestCellIndex = INDEX_NONE;
				for (const int32 CellIndex : CandidateIndices)
				{
					if (!Cells.IsValidIndex(CellIndex))
					{
						continue;
					}

					const FSRPlanetSurfaceGridCell& Cell = Cells[CellIndex];
					float HitDistance = 0.0f;
					if ((IntersectRayTriangle(LocalRayOrigin, LocalRayDirection, Cell.Corner00, Cell.Corner10, Cell.Corner11, HitDistance)
							|| IntersectRayTriangle(LocalRayOrigin, LocalRayDirection, Cell.Corner00, Cell.Corner11, Cell.Corner01, HitDistance))
						&& HitDistance < BestHitDistance)
					{
						BestHitDistance = HitDistance;
						BestCellIndex = CellIndex;
					}
				}

				if (!Cells.IsValidIndex(BestCellIndex))
				{
					return false;
				}

				OutCell = Cells[BestCellIndex];
				OutHitLocation = ComponentTransform.TransformPosition(LocalRayOrigin + (LocalRayDirection * BestHitDistance));
				return true;
			};

			if (!CandidateCellIndices.IsEmpty() && TryRaycastCandidates(CandidateCellIndices))
			{
				return true;
			}

			TArray<int32> AllCellIndices;
			AllCellIndices.Reserve(Cells.Num());
			for (int32 CellIndex = 0; CellIndex < Cells.Num(); ++CellIndex)
			{
				AllCellIndices.Add(CellIndex);
			}
			if (TryRaycastCandidates(AllCellIndices))
			{
				return true;
			}
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

	OutHitLocation = ResolveWorldSurfacePoint(OutCell.LocalNormal, 0.0f);
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
	bGridMeshDirty = true;
	if (bGridVisible)
	{
		RefreshInteractionHighlight();
	}
	return true;
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
	RefreshInteractionHighlight();
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
	RefreshInteractionHighlight();
}

bool USRPlanetSurfaceGrid::HasHoveredCell() const
{
	return bHasHoveredCell;
}

bool USRPlanetSurfaceGrid::GetHoveredCell(FSRPlanetSurfaceGridCell& OutCell) const
{
	return bHasHoveredCell && GetCellById(HoveredCellId, OutCell);
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
	RefreshInteractionHighlight();
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
	RefreshInteractionHighlight();
}

bool USRPlanetSurfaceGrid::HasSelectedCell() const
{
	return bHasSelectedCell;
}

bool USRPlanetSurfaceGrid::GetSelectedCell(FSRPlanetSurfaceGridCell& OutCell) const
{
	return bHasSelectedCell && GetCellById(SelectedCellId, OutCell);
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
	const FColor OccupiedLineColor = OccupiedCellColor.ToFColor(true);

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
		DrawUniqueDefaultEdge(Cell.Corner00, Cell.Corner10);
		DrawUniqueDefaultEdge(Cell.Corner10, Cell.Corner11);
		DrawUniqueDefaultEdge(Cell.Corner11, Cell.Corner01);
		DrawUniqueDefaultEdge(Cell.Corner01, Cell.Corner00);
		DrawUniqueDefaultEdge(Cell.Corner00, Cell.Corner11);
	}

	for (const FSRPlanetSurfaceGridCell& Cell : Cells)
	{
		const bool bIsHovered = bHasHoveredCell && (Cell.CellId == HoveredCellId);
		const bool bIsSelected = bHasSelectedCell && (Cell.CellId == SelectedCellId);
		const bool bShouldHighlightCell = bIsHovered || bIsSelected || Cell.bOccupied;
		if (!bShouldHighlightCell)
		{
			continue;
		}

		const FColor LineColor = bIsSelected ? SelectedLineColor : (bIsHovered ? HoverLineColor : OccupiedLineColor);
		const float LineThickness = bIsSelected ? DebugLineThickness * 2.5f : (bIsHovered ? DebugLineThickness * 2.0f : DebugLineThickness);
		DrawDebugSurfaceLine(Cell.Corner00, Cell.Corner10, LineColor, Duration, LineThickness, CameraInfo, ReferenceViewDepth, ReferenceFieldOfViewDegrees);
		DrawDebugSurfaceLine(Cell.Corner10, Cell.Corner11, LineColor, Duration, LineThickness, CameraInfo, ReferenceViewDepth, ReferenceFieldOfViewDegrees);
		DrawDebugSurfaceLine(Cell.Corner11, Cell.Corner01, LineColor, Duration, LineThickness, CameraInfo, ReferenceViewDepth, ReferenceFieldOfViewDegrees);
		DrawDebugSurfaceLine(Cell.Corner01, Cell.Corner00, LineColor, Duration, LineThickness, CameraInfo, ReferenceViewDepth, ReferenceFieldOfViewDegrees);
		DrawDebugSurfaceLine(Cell.Corner00, Cell.Corner11, LineColor, Duration, LineThickness, CameraInfo, ReferenceViewDepth, ReferenceFieldOfViewDegrees);
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
		RefreshInteractionHighlight();
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
		RefreshInteractionHighlight();
	}
	else
	{
		RefreshInteractionHighlight();
	}
}

void USRPlanetSurfaceGrid::ConfigureConstructionHeightOffset(float NewConstructionHeightOffset)
{
	ConstructionHeightOffset = NewConstructionHeightOffset;
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
	NewDynamicMeshGeneration.BiomeProfile = ESRPlanetBiomeProfile::EarthLike;
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
	bCellsDirty = true;
	bGridMeshDirty = true;
	if (bGridVisible)
	{
		RebuildGrid();
	}
}

FSRPlanetTerrainSample USRPlanetSurfaceGrid::GetTerrainSampleAtDirection(FVector LocalUnitDirection) const
{
	FSRPlanetTerrainSample Sample = FSRPlanetTerrainGenerator::SampleTerrain(LocalUnitDirection, DynamicMeshGeneration);
	const float SafeDynamicMeshHeight = FMath::Max(0.0f, DynamicMeshGeneration.DynamicMeshHeight);
	if (DynamicMeshGeneration.bMinecraft && DynamicMeshGeneration.bDynamicMeshGeneration && SafeDynamicMeshHeight > KINDA_SMALL_NUMBER)
	{
		const float HeightStep = FMath::Max(1.0f, SafeDynamicMeshHeight / 24.0f);
		Sample.HeightOffset = FMath::RoundToFloat(Sample.HeightOffset / HeightStep) * HeightStep;
	}
	return Sample;
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

	AppendDedupedSegment(Cell.Corner00, Cell.Corner10);
	AppendDedupedSegment(Cell.Corner10, Cell.Corner11);
	AppendDedupedSegment(Cell.Corner11, Cell.Corner01);
	AppendDedupedSegment(Cell.Corner01, Cell.Corner00);
}

void USRPlanetSurfaceGrid::ApplyGeneratedGridBuild(
	TArray<FSRPlanetSurfaceGridCell>&& NewCells,
	UE::Geometry::FDynamicMesh3&& NewGridMesh)
{
	if (ASRCelestialBody* OwnerBody = Cast<ASRCelestialBody>(GetOwner()))
	{
		OwnerBody->ClearSurfaceCellHighlights();
	}

	Cells = MoveTemp(NewCells);
	bUsingRecoveredQuadCells = true;
	bHasHoveredCell = false;
	HoveredCellId = FSRPlanetSurfaceGridCellId();
	bHasSelectedCell = false;
	SelectedCellId = FSRPlanetSurfaceGridCellId();
	SetInteractionOverlayVisible(false);
	RebuildCellIndex();
	RebuildRaycastIndex();
	UE::Geometry::FDynamicMesh3 EmptyGridMesh;
	EmptyGridMesh.EnableAttributes();
	EmptyGridMesh.Attributes()->EnablePrimaryColors();
	SetMesh(MoveTemp(EmptyGridMesh));
	SetVisibility(false);
	SetHiddenInGame(true);
	bCellsDirty = false;
	bGridMeshDirty = false;
	UpdateDebugTickState();
}

bool USRPlanetSurfaceGrid::GetCellIndex(const FSRPlanetSurfaceGridCellId& CellId, int32& OutIndex) const
{
	if (const int32* FoundIndex = CellIndexById.Find(CellId))
	{
		OutIndex = *FoundIndex;
		return Cells.IsValidIndex(OutIndex);
	}

	OutIndex = INDEX_NONE;
	return false;
}

void USRPlanetSurfaceGrid::RebuildCellIndex()
{
	CellIndexById.Reset();
	CellIndexById.Reserve(Cells.Num());

	for (int32 CellIndex = 0; CellIndex < Cells.Num(); ++CellIndex)
	{
		CellIndexById.Add(Cells[CellIndex].CellId, CellIndex);
	}
}

uint64 USRPlanetSurfaceGrid::BuildRaycastBinKey(const FSRPlanetSurfaceGridCellId& BinId) const
{
	return (static_cast<uint64>(static_cast<uint8>(BinId.Face)) << 32)
		| (static_cast<uint64>(static_cast<uint16>(BinId.CellX)) << 16)
		| static_cast<uint64>(static_cast<uint16>(BinId.CellY));
}

void USRPlanetSurfaceGrid::AddCellToRaycastBin(const FSRPlanetSurfaceGridCellId& BinId, int32 CellIndex)
{
	if (!Cells.IsValidIndex(CellIndex)
		|| BinId.CellX < 0
		|| BinId.CellY < 0
		|| BinId.CellX >= RaycastBinResolution
		|| BinId.CellY >= RaycastBinResolution)
	{
		return;
	}

	RaycastCellIndicesByBin.FindOrAdd(BuildRaycastBinKey(BinId)).AddUnique(CellIndex);
}

bool USRPlanetSurfaceGrid::GetRaycastBinForDirection(const FVector& LocalDirection, FSRPlanetSurfaceGridCellId& OutBinId) const
{
	FVector2D UnusedFaceCoordinates = FVector2D::ZeroVector;
	return USRPlanetSurfaceGridLibrary::ProjectDirectionToCubeSphereCellId(
		LocalDirection.GetSafeNormal(),
		RaycastBinResolution,
		OutBinId,
		UnusedFaceCoordinates);
}

void USRPlanetSurfaceGrid::RebuildRaycastIndex()
{
	RaycastCellIndicesByBin.Reset();
	if (!bUsingRecoveredQuadCells || Cells.IsEmpty())
	{
		return;
	}

	RaycastCellIndicesByBin.Reserve(Cells.Num());
	for (int32 CellIndex = 0; CellIndex < Cells.Num(); ++CellIndex)
	{
		const FSRPlanetSurfaceGridCell& Cell = Cells[CellIndex];
		const FVector SampleDirections[5] =
		{
			Cell.LocalCenter,
			Cell.Corner00,
			Cell.Corner10,
			Cell.Corner11,
			Cell.Corner01,
		};

		for (const FVector& SampleDirection : SampleDirections)
		{
			FSRPlanetSurfaceGridCellId BinId;
			if (!GetRaycastBinForDirection(SampleDirection, BinId))
			{
				continue;
			}

			for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
			{
				for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
				{
					FSRPlanetSurfaceGridCellId ExpandedBinId = BinId;
					ExpandedBinId.CellX += OffsetX;
					ExpandedBinId.CellY += OffsetY;
					AddCellToRaycastBin(ExpandedBinId, CellIndex);
				}
			}
		}
	}
}

void USRPlanetSurfaceGrid::GatherRaycastCandidateCells(const FVector& LocalDirection, TArray<int32>& OutCandidateCellIndices) const
{
	OutCandidateCellIndices.Reset();

	FSRPlanetSurfaceGridCellId BinId;
	if (!GetRaycastBinForDirection(LocalDirection, BinId))
	{
		return;
	}

	for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
	{
		for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
		{
			FSRPlanetSurfaceGridCellId CandidateBinId = BinId;
			CandidateBinId.CellX += OffsetX;
			CandidateBinId.CellY += OffsetY;
			if (CandidateBinId.CellX < 0
				|| CandidateBinId.CellY < 0
				|| CandidateBinId.CellX >= RaycastBinResolution
				|| CandidateBinId.CellY >= RaycastBinResolution)
			{
				continue;
			}

			const TArray<int32>* BinCellIndices = RaycastCellIndicesByBin.Find(BuildRaycastBinKey(CandidateBinId));
			if (!BinCellIndices)
			{
				continue;
			}

			for (const int32 CellIndex : *BinCellIndices)
			{
				OutCandidateCellIndices.AddUnique(CellIndex);
			}
		}
	}
}

bool USRPlanetSurfaceGrid::RebuildCellsFromOwnerStaticMeshQuads()
{
	const ASRCelestialBody* OwnerBody = Cast<ASRCelestialBody>(GetOwner());
	if (!IsValid(OwnerBody))
	{
		return false;
	}

	if (OwnerBody->GetCachedSurfaceGridCells(Cells))
	{
		bUsingRecoveredQuadCells = true;
		return true;
	}

	const FSRCelestialBodyData OwnerData = OwnerBody->GetData();
	UStaticMesh* OwnerStaticMesh = OwnerData.StaticMesh.Get();
	if (!IsValid(OwnerStaticMesh))
	{
		return false;
	}

	const FStaticMeshRenderData* RenderData = OwnerStaticMesh->GetRenderData();
	if (!RenderData || RenderData->LODResources.IsEmpty())
	{
		return false;
	}

	const FStaticMeshLODResources& LODResource = RenderData->LODResources[0];
	const FPositionVertexBuffer& PositionVertexBuffer = LODResource.VertexBuffers.PositionVertexBuffer;
	const FRawStaticIndexBuffer& IndexBuffer = LODResource.IndexBuffer;
	const int32 VertexCount = static_cast<int32>(PositionVertexBuffer.GetNumVertices());
	const int32 IndexCount = static_cast<int32>(IndexBuffer.GetNumIndices());
	if (VertexCount <= 0 || IndexCount < 3)
	{
		return false;
	}

	const TArray<FSRSurfaceGridSourceQuad> SourceQuads = RecoverSurfaceGridSourceQuads(PositionVertexBuffer, IndexBuffer, IndexCount);
	if (SourceQuads.IsEmpty())
	{
		return false;
	}

	Cells.Reset(SourceQuads.Num());
	Cells.Reserve(SourceQuads.Num());
	const float OwnerScale = FMath::Max(0.0f, OwnerData.Scale);
	if (OwnerScale <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	TMap<uint64, FIntPoint> CellEdgeBySourceEdge;
	CellEdgeBySourceEdge.Reserve(SourceQuads.Num() * 4);

	auto AssignNeighbor = [this](int32 CellIndex, int32 EdgeIndex, const FSRPlanetSurfaceGridCellId& NeighborId)
	{
		if (!Cells.IsValidIndex(CellIndex))
		{
			return;
		}

		switch (EdgeIndex)
		{
		case 0:
			Cells[CellIndex].Neighbors.NegativeV = NeighborId;
			break;
		case 1:
			Cells[CellIndex].Neighbors.PositiveU = NeighborId;
			break;
		case 2:
			Cells[CellIndex].Neighbors.PositiveV = NeighborId;
			break;
		case 3:
			Cells[CellIndex].Neighbors.NegativeU = NeighborId;
			break;
		default:
			break;
		}
	};

	for (int32 QuadIndex = 0; QuadIndex < SourceQuads.Num(); ++QuadIndex)
	{
		const FSRSurfaceGridSourceQuad& SourceQuad = SourceQuads[QuadIndex];

		FVector SourcePositions[4];
		FVector SourceCenter = FVector::ZeroVector;
		bool bHasValidVertices = true;
		for (int32 CornerIndex = 0; CornerIndex < 4; ++CornerIndex)
		{
			const int32 SourceVertexIndex = SourceQuad.Vertices[CornerIndex];
			if (SourceVertexIndex < 0 || SourceVertexIndex >= VertexCount)
			{
				bHasValidVertices = false;
				break;
			}

			SourcePositions[CornerIndex] = FVector(PositionVertexBuffer.VertexPosition(SourceVertexIndex));
			SourceCenter += SourcePositions[CornerIndex];
		}

		if (!bHasValidVertices)
		{
			continue;
		}

		SourceCenter /= 4.0f;
		const FVector CellDirection = SourceCenter.GetSafeNormal();
		if (CellDirection.IsNearlyZero())
		{
			continue;
		}

		float CellScale = 1.0f;
		if (DynamicMeshGeneration.bDynamicMeshGeneration && DynamicMeshGeneration.DynamicMeshHeight > KINDA_SMALL_NUMBER)
		{
			const FSRPlanetTerrainSample TerrainSample = GetTerrainSampleAtDirection(CellDirection);
			const float SourceCellRadius = FMath::Max(SourceCenter.Length(), 1.0f);
			CellScale = FMath::Max(0.01f, (SourceCellRadius + TerrainSample.HeightOffset) / SourceCellRadius);
		}

		FVector TargetPositions[4];
		for (int32 CornerIndex = 0; CornerIndex < 4; ++CornerIndex)
		{
			TargetPositions[CornerIndex] = SourcePositions[CornerIndex] * CellScale * OwnerScale;
		}

		FVector CellNormal = FVector::CrossProduct(
			TargetPositions[1] - TargetPositions[0],
			TargetPositions[2] - TargetPositions[0]).GetSafeNormal();
		if (FVector::DotProduct(CellNormal, CellDirection) < 0.0f)
		{
			Swap(TargetPositions[1], TargetPositions[3]);
			Swap(SourcePositions[1], SourcePositions[3]);
			CellNormal *= -1.0f;
		}
		if (CellNormal.IsNearlyZero())
		{
			CellNormal = CellDirection;
		}

		FSRPlanetSurfaceGridCell Cell;
		Cell.CellId.CellX = QuadIndex;
		Cell.CellId.CellY = 0;
		FVector2D UnusedFaceCoordinates = FVector2D::ZeroVector;
		USRPlanetSurfaceGridLibrary::ProjectDirectionToCubeSphereCellId(CellDirection, 1, Cell.CellId, UnusedFaceCoordinates);
		Cell.CellId.CellX = QuadIndex;
		Cell.CellId.CellY = 0;
		Cell.LocalCenter = (TargetPositions[0] + TargetPositions[1] + TargetPositions[2] + TargetPositions[3]) * 0.25f;
		Cell.LocalNormal = CellNormal;
		Cell.Corner00 = TargetPositions[0];
		Cell.Corner10 = TargetPositions[1];
		Cell.Corner11 = TargetPositions[2];
		Cell.Corner01 = TargetPositions[3];
		Cell.FaceUVMin = FVector2D::ZeroVector;
		Cell.FaceUVMax = FVector2D::UnitVector;
		Cell.ApproxSurfaceArea =
			(FVector::CrossProduct(Cell.Corner10 - Cell.Corner00, Cell.Corner11 - Cell.Corner00).Size() * 0.5f)
			+ (FVector::CrossProduct(Cell.Corner11 - Cell.Corner00, Cell.Corner01 - Cell.Corner00).Size() * 0.5f);

		const int32 CellIndex = Cells.Num();
		Cells.Add(Cell);

		const uint64 EdgeKeys[4] =
		{
			BuildSurfaceGridSourceEdgeKey(SourceQuad.Vertices[0], SourceQuad.Vertices[1]),
			BuildSurfaceGridSourceEdgeKey(SourceQuad.Vertices[1], SourceQuad.Vertices[2]),
			BuildSurfaceGridSourceEdgeKey(SourceQuad.Vertices[2], SourceQuad.Vertices[3]),
			BuildSurfaceGridSourceEdgeKey(SourceQuad.Vertices[3], SourceQuad.Vertices[0]),
		};

		for (int32 EdgeIndex = 0; EdgeIndex < 4; ++EdgeIndex)
		{
			if (const FIntPoint* ExistingCellEdge = CellEdgeBySourceEdge.Find(EdgeKeys[EdgeIndex]))
			{
				if (Cells.IsValidIndex(ExistingCellEdge->X))
				{
					AssignNeighbor(CellIndex, EdgeIndex, Cells[ExistingCellEdge->X].CellId);
					AssignNeighbor(ExistingCellEdge->X, ExistingCellEdge->Y, Cells[CellIndex].CellId);
				}
				continue;
			}

			CellEdgeBySourceEdge.Add(EdgeKeys[EdgeIndex], FIntPoint(CellIndex, EdgeIndex));
		}
	}

	bUsingRecoveredQuadCells = !Cells.IsEmpty();
	return bUsingRecoveredQuadCells;
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

	if (UMaterialInterface* GridMaterial = GetMaterial(0))
	{
		InteractionOverlayMesh->SetMaterial(0, GridMaterial);
	}
}

void USRPlanetSurfaceGrid::RefreshInteractionHighlight()
{
	ASRCelestialBody* OwnerBody = Cast<ASRCelestialBody>(GetOwner());
	const bool bAppliedDynamicMeshHighlight = IsValid(OwnerBody)
		&& OwnerBody->ApplySurfaceCellHighlights(
			HoveredCellId,
			bHasHoveredCell,
			SelectedCellId,
			bHasSelectedCell,
			HoveredCellColor,
			SelectedCellColor);

	if (IsValid(OwnerBody) && !bHasHoveredCell && !bHasSelectedCell)
	{
		OwnerBody->ClearSurfaceCellHighlights();
	}

	RebuildInteractionOverlayMesh(!bAppliedDynamicMeshHighlight);
}

void USRPlanetSurfaceGrid::RebuildInteractionOverlayMesh(bool bIncludeCellHighlightOverlay)
{
	EnsureInteractionOverlay();
	if (!InteractionOverlayMesh)
	{
		return;
	}

	UE::Geometry::FDynamicMesh3 OverlayMesh;
	OverlayMesh.EnableAttributes();
	OverlayMesh.Attributes()->EnablePrimaryColors();

	TSet<uint64> PatchDrawnEdges;
	PatchDrawnEdges.Reserve(96);
	FSRPlanetSurfaceGridCell SelectedCell;
	if (bHasSelectedCell && GetCellById(SelectedCellId, SelectedCell))
	{
		AppendInteractionGridPatch(OverlayMesh, SelectedCellId, SelectedCellColor, DebugLineThickness * 1.75f, PatchDrawnEdges);
		if (bIncludeCellHighlightOverlay)
		{
			AppendInteractionCell(OverlayMesh, SelectedCell, SelectedCellColor, DebugLineThickness * 2.5f);
		}
	}

	if (bHasHoveredCell && (!bHasSelectedCell || !(HoveredCellId == SelectedCellId)))
	{
		FSRPlanetSurfaceGridCell HoveredCell;
		if (GetCellById(HoveredCellId, HoveredCell))
		{
			AppendInteractionGridPatch(OverlayMesh, HoveredCellId, HoveredCellColor, DebugLineThickness * 1.5f, PatchDrawnEdges);
			if (bIncludeCellHighlightOverlay)
			{
				AppendInteractionCell(OverlayMesh, HoveredCell, HoveredCellColor, DebugLineThickness * 2.0f);
			}
		}
	}

	InteractionOverlayMesh->SetMesh(MoveTemp(OverlayMesh));
	SetInteractionOverlayVisible(bGridVisible && (bHasHoveredCell || bHasSelectedCell));
}

void USRPlanetSurfaceGrid::AppendInteractionGridPatch(
	UE::Geometry::FDynamicMesh3& OverlayMesh,
	const FSRPlanetSurfaceGridCellId& CenterCellId,
	const FLinearColor& BaseLineColor,
	float LineThickness,
	TSet<uint64>& DrawnEdges) const
{
	constexpr int32 PatchRadius = 2;
	for (int32 OffsetY = -PatchRadius; OffsetY <= PatchRadius; ++OffsetY)
	{
		for (int32 OffsetX = -PatchRadius; OffsetX <= PatchRadius; ++OffsetX)
		{
			FSRPlanetSurfaceGridCellId PatchCellId = CenterCellId;
			PatchCellId.CellX += OffsetX;
			PatchCellId.CellY += OffsetY;

			FSRPlanetSurfaceGridCell PatchCell;
			if (!GetCellById(PatchCellId, PatchCell))
			{
				continue;
			}

			const int32 Distance = FMath::Max(FMath::Abs(OffsetX), FMath::Abs(OffsetY));
			const float FadeAlpha = 1.0f - (static_cast<float>(Distance) / static_cast<float>(PatchRadius + 1));
			FLinearColor PatchLineColor = BaseLineColor;
			PatchLineColor.A = FMath::Clamp(BaseLineColor.A * DebugLineOpacity * FadeAlpha, 0.0f, 1.0f);
			if (PatchLineColor.A <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			const float PatchLineThickness = FMath::Max(0.1f, LineThickness * FMath::Lerp(0.65f, 1.0f, FadeAlpha));
			AppendGridWireCell(OverlayMesh, PatchCell, PatchLineColor, PatchLineThickness, true, &DrawnEdges);
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

	if (bUsingRecoveredQuadCells)
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
		const FLinearColor OccupiedLineColor(OccupiedCellColor.R, OccupiedCellColor.G, OccupiedCellColor.B, OccupiedCellColor.A);

		const bool bAppendedOwnerWire = !bUsingRecoveredQuadCells
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

		for (const FSRPlanetSurfaceGridCell& Cell : Cells)
		{
			if (!Cell.bOccupied)
			{
				continue;
			}

			AppendGridWireCell(GridMesh, Cell, OccupiedLineColor, DebugLineThickness, false, nullptr);
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

	if (bUsingRecoveredQuadCells)
	{
		AppendDedupedSegment(Cell.Corner00, Cell.Corner10);
		AppendDedupedSegment(Cell.Corner10, Cell.Corner11);
		AppendDedupedSegment(Cell.Corner11, Cell.Corner01);
		AppendDedupedSegment(Cell.Corner01, Cell.Corner00);
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

	AppendEdge(Cell.Corner00, Cell.Corner10);
	AppendEdge(Cell.Corner10, Cell.Corner11);
	AppendEdge(Cell.Corner11, Cell.Corner01);
	AppendEdge(Cell.Corner01, Cell.Corner00);
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
	FVector PreviousPoint = ResolveWorldSurfacePoint(DirectionA, GridSurfaceOffset);

	for (int32 SegmentIndex = 1; SegmentIndex <= SegmentCount; ++SegmentIndex)
	{
		const float Alpha = static_cast<float>(SegmentIndex) / static_cast<float>(SegmentCount);
		const FVector SampleDirection = FMath::Lerp(DirectionA, DirectionB, Alpha).GetSafeNormal();
		if (SampleDirection.IsNearlyZero())
		{
			continue;
		}

		const FVector CurrentPoint = ResolveWorldSurfacePoint(SampleDirection, GridSurfaceOffset);
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
