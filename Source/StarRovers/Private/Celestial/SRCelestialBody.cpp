#include "Celestial/SRCelestialBody.h"

#include "Algo/Reverse.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/LineBatchComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Gravity/SRGravityParent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "Components/SphereComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "Engine/StaticMesh.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Rendering/StaticMeshVertexBuffer.h"
#include "Surface/SRPlanetTerrainGenerator.h"
#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

namespace
{
	int32 GetTerrainBiomeMaterialSlotIndex(const ESRPlanetBiome Biome)
	{
		switch (Biome)
		{
		case ESRPlanetBiome::Ocean:
			return 1;
		case ESRPlanetBiome::Coast:
			return 2;
		case ESRPlanetBiome::Plains:
			return 3;
		case ESRPlanetBiome::Forest:
			return 4;
		case ESRPlanetBiome::Desert:
			return 5;
		case ESRPlanetBiome::Mountain:
			return 6;
		case ESRPlanetBiome::Snow:
			return 7;
		case ESRPlanetBiome::Ice:
			return 8;
		default:
			return 0;
		}
	}

	UMaterialInterface* GetTerrainBiomeMaterial(const FSRDynamicMeshGeneration& DynamicMeshGeneration, const ESRPlanetBiome Biome)
	{
		if (const TObjectPtr<UMaterialInterface>* Material = DynamicMeshGeneration.BiomeMaterials.Find(Biome))
		{
			return Material->Get();
		}

		return nullptr;
	}

	int32 GetTerrainBiomeMaterialId(const FSRDynamicMeshGeneration& DynamicMeshGeneration, const ESRPlanetBiome Biome)
	{
		return IsValid(GetTerrainBiomeMaterial(DynamicMeshGeneration, Biome))
			? GetTerrainBiomeMaterialSlotIndex(Biome)
			: 0;
	}

	FVector EstimateProceduralTerrainNormal(const FVector& LocalUnitDirection, float BaseRadius, const FSRDynamicMeshGeneration& DynamicMeshGeneration)
	{
		const FVector Direction = LocalUnitDirection.GetSafeNormal();
		if (Direction.IsNearlyZero())
		{
			return FVector::UpVector;
		}

		const FVector TangentA = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
		const FVector Tangent0 = TangentA.IsNearlyZero()
			? FVector::CrossProduct(Direction, FVector::RightVector).GetSafeNormal()
			: TangentA;
		const FVector Tangent1 = FVector::CrossProduct(Direction, Tangent0).GetSafeNormal();
		const float SampleStep = 0.0025f;

		const FVector DirectionA = (Direction + (Tangent0 * SampleStep)).GetSafeNormal();
		const FVector DirectionB = (Direction + (Tangent1 * SampleStep)).GetSafeNormal();
		const float Height0 = FSRPlanetTerrainGenerator::SampleTerrain(Direction, DynamicMeshGeneration).HeightOffset;
		const float HeightA = FSRPlanetTerrainGenerator::SampleTerrain(DirectionA, DynamicMeshGeneration).HeightOffset;
		const float HeightB = FSRPlanetTerrainGenerator::SampleTerrain(DirectionB, DynamicMeshGeneration).HeightOffset;

		const FVector Point0 = Direction * FMath::Max(1.0f, BaseRadius + Height0);
		const FVector PointA = DirectionA * FMath::Max(1.0f, BaseRadius + HeightA);
		const FVector PointB = DirectionB * FMath::Max(1.0f, BaseRadius + HeightB);
		FVector Normal = FVector::CrossProduct(PointB - Point0, PointA - Point0).GetSafeNormal();
		if (FVector::DotProduct(Normal, Direction) < 0.0f)
		{
			Normal *= -1.0f;
		}

		return Normal.IsNearlyZero() ? Direction : Normal;
	}

	struct FSRSourceTriangle
	{
		UE::Geometry::FIndex3i Vertices = UE::Geometry::FIndex3i(INDEX_NONE, INDEX_NONE, INDEX_NONE);
		bool bPaired = false;
	};

	struct FSRSourceQuad
	{
		int32 Vertices[4] = { INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE };
	};

	uint64 BuildSourceEdgeKey(int32 VertexIndexA, int32 VertexIndexB)
	{
		const uint32 MinVertex = static_cast<uint32>(FMath::Min(VertexIndexA, VertexIndexB));
		const uint32 MaxVertex = static_cast<uint32>(FMath::Max(VertexIndexA, VertexIndexB));
		return (static_cast<uint64>(MinVertex) << 32) | static_cast<uint64>(MaxVertex);
	}

	uint32 HashSourcePosition(const FVector& Position)
	{
		const FVector Direction = Position.GetSafeNormal();
		constexpr float DirectionQuantizationScale = 100000.0f;
		const int32 QuantizedX = FMath::RoundToInt(Direction.X * DirectionQuantizationScale);
		const int32 QuantizedY = FMath::RoundToInt(Direction.Y * DirectionQuantizationScale);
		const int32 QuantizedZ = FMath::RoundToInt(Direction.Z * DirectionQuantizationScale);
		return HashCombine(HashCombine(::GetTypeHash(QuantizedX), ::GetTypeHash(QuantizedY)), ::GetTypeHash(QuantizedZ));
	}

	uint64 BuildSourcePositionEdgeKey(const FVector& PositionA, const FVector& PositionB)
	{
		const uint32 EndpointA = HashSourcePosition(PositionA);
		const uint32 EndpointB = HashSourcePosition(PositionB);
		const uint32 MinEndpoint = FMath::Min(EndpointA, EndpointB);
		const uint32 MaxEndpoint = FMath::Max(EndpointA, EndpointB);
		return (static_cast<uint64>(MinEndpoint) << 32) | static_cast<uint64>(MaxEndpoint);
	}

	bool ContainsSourceVertex(const UE::Geometry::FIndex3i& Triangle, int32 VertexIndex)
	{
		return Triangle.A == VertexIndex || Triangle.B == VertexIndex || Triangle.C == VertexIndex;
	}

	bool TryGetTriangleOppositeVertex(const UE::Geometry::FIndex3i& Triangle, int32 SharedVertexA, int32 SharedVertexB, int32& OutOppositeVertex)
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

	bool TryOrderQuadVertices(
		const FPositionVertexBuffer& PositionVertexBuffer,
		int32 VertexIndex0,
		int32 VertexIndex1,
		int32 VertexIndex2,
		int32 VertexIndex3,
		FSRSourceQuad& OutQuad)
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

	TArray<FSRSourceQuad> RecoverSourceQuads(const FPositionVertexBuffer& PositionVertexBuffer, const FRawStaticIndexBuffer& IndexBuffer, int32 IndexCount)
	{
		TArray<FSRSourceTriangle> SourceTriangles;
		SourceTriangles.Reserve(IndexCount / 3);

		TMap<uint64, TArray<int32>> TriangleIndicesByEdge;
		for (int32 Index = 0; Index + 2 < IndexCount; Index += 3)
		{
			const int32 TriangleIndex = SourceTriangles.Num();
			FSRSourceTriangle SourceTriangle;
			SourceTriangle.Vertices = UE::Geometry::FIndex3i(
				static_cast<int32>(IndexBuffer.GetIndex(Index)),
				static_cast<int32>(IndexBuffer.GetIndex(Index + 1)),
				static_cast<int32>(IndexBuffer.GetIndex(Index + 2)));
			SourceTriangles.Add(SourceTriangle);

			TriangleIndicesByEdge.FindOrAdd(BuildSourceEdgeKey(SourceTriangle.Vertices.A, SourceTriangle.Vertices.B)).Add(TriangleIndex);
			TriangleIndicesByEdge.FindOrAdd(BuildSourceEdgeKey(SourceTriangle.Vertices.B, SourceTriangle.Vertices.C)).Add(TriangleIndex);
			TriangleIndicesByEdge.FindOrAdd(BuildSourceEdgeKey(SourceTriangle.Vertices.C, SourceTriangle.Vertices.A)).Add(TriangleIndex);
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

		TArray<FSRSourceQuad> SourceQuads;
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
			if (!ContainsSourceVertex(TriangleA, SharedVertexA)
				|| !ContainsSourceVertex(TriangleA, SharedVertexB)
				|| !ContainsSourceVertex(TriangleB, SharedVertexA)
				|| !ContainsSourceVertex(TriangleB, SharedVertexB))
			{
				continue;
			}

			int32 OppositeVertexA = INDEX_NONE;
			int32 OppositeVertexB = INDEX_NONE;
			if (!TryGetTriangleOppositeVertex(TriangleA, SharedVertexA, SharedVertexB, OppositeVertexA)
				|| !TryGetTriangleOppositeVertex(TriangleB, SharedVertexA, SharedVertexB, OppositeVertexB))
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

			FSRSourceQuad SourceQuad;
			if (!TryOrderQuadVertices(PositionVertexBuffer, OppositeVertexA, SharedVertexA, OppositeVertexB, SharedVertexB, SourceQuad))
			{
				continue;
			}

			SourceTriangles[TriangleIndexA].bPaired = true;
			SourceTriangles[TriangleIndexB].bPaired = true;
			SourceQuads.Add(SourceQuad);
		}

		return SourceQuads;
	}

	FSRPlanetTerrainSample SampleTerrainForDynamicMesh(
		const FVector& LocalUnitDirection,
		const FSRDynamicMeshGeneration& DynamicMeshGeneration)
	{
		FSRPlanetTerrainSample Sample = FSRPlanetTerrainGenerator::SampleTerrain(LocalUnitDirection, DynamicMeshGeneration);
		const float SafeDynamicMeshHeight = FMath::Max(0.0f, DynamicMeshGeneration.DynamicMeshHeight);
		if (!DynamicMeshGeneration.bMinecraft || !DynamicMeshGeneration.bDynamicMeshGeneration || SafeDynamicMeshHeight <= KINDA_SMALL_NUMBER)
		{
			return Sample;
		}

		const float HeightStep = FMath::Max(1.0f, SafeDynamicMeshHeight / 24.0f);
		Sample.HeightOffset = FMath::RoundToFloat(Sample.HeightOffset / HeightStep) * HeightStep;
		return Sample;
	}

}

DEFINE_LOG_CATEGORY_STATIC(LogStarRoversCelestial, Log, All);

FSRCelestialBodyData::FSRCelestialBodyData()
{
	VariableName = FText::FromString(TEXT("Primary Star"));
	BodyCategory = ESRCelestialBodyCategory::Star;
	OrbitPeriod = 0.0f;
	ConstructionHeightOffset = 15.0f;
	DynamicMeshGeneration = FSRDynamicMeshGeneration();
	DynamicMeshGeneration.bDynamicMeshGeneration = false;
	DynamicMeshGeneration.DynamicMeshHeight = 0.0f;
	bHasOcean = false;
	OceanMesh = nullptr;
	OceanMaterial = nullptr;
	OceanScaleMultiplier = 1.0f;
}

ASRCelestialBody::ASRCelestialBody()
{
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CelestialBodyDynamicMesh = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("CelestialBodyDynamicMesh"));
	CelestialBodyDynamicMesh->SetupAttachment(SceneRoot);
	CelestialBodyDynamicMesh->SetMobility(EComponentMobility::Movable);

	CelestialBodyStaticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CelestialBodyStaticMesh"));
	CelestialBodyStaticMesh->SetupAttachment(SceneRoot);

	ClickSphereCollision = CreateDefaultSubobject<USphereComponent>(TEXT("ClickSphereCollision"));
	ClickSphereCollision->SetupAttachment(SceneRoot);
	ClickSphereCollision->SetMobility(EComponentMobility::Movable);

	GravityParent = CreateDefaultSubobject<USRGravityParent>(TEXT("GravityParent"));

	GravityLineBatch = CreateDefaultSubobject<ULineBatchComponent>(TEXT("GravityLineBatch"));
	GravityLineBatch->SetupAttachment(SceneRoot);
	GravityLineBatch->SetMobility(EComponentMobility::Movable);
	GravityLineBatch->SetUsingAbsoluteLocation(true);
	GravityLineBatch->SetUsingAbsoluteRotation(true);
	GravityLineBatch->SetUsingAbsoluteScale(true);
	GravityLineBatch->ComponentTags.AddUnique(TEXT("StarRovers.GravityLine"));
	GravityLineBatch->ComponentTags.AddUnique(TEXT("StarRovers.GravityLineRoot"));

	VariableName = FText::FromString(TEXT("Celestial Body"));
	BodyCategory = ESRCelestialBodyCategory::Unknown;
	FocusZoomMultiplier = 3.0f;
	Scale = 1000.0f;
	Mass = 2000.0f;
	GenerationSeed = 1000;
	DynamicMeshGeneration = FSRDynamicMeshGeneration();
	DynamicMeshGeneration.bDynamicMeshGeneration = false;
	DynamicMeshGeneration.DynamicMeshHeight = 0.0f;
	GravityRatio = 1.0f;
	GravityRadiusRatio = 10.0f;
	ShowGravityLine = true;
	GravityLineColor = FLinearColor(0.45f, 1.0f, 0.45f, 1.0f);
	GravityLineOpacity = 0.85f;
	GravityLineSegments = 96;
	GravityLineThickness = 20.0f;
}

void ASRCelestialBody::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	ApplyData();
}

void ASRCelestialBody::BeginPlay()
{
	Super::BeginPlay();

	if (!bHasAppliedData)
	{
		LogMissingDataErrorOnce(TEXT("BeginPlay"));
		return;
	}

	if (USRCelestialBodyRegistrySubsystem* CelestialRegistry = FindCelestialRegistry())
	{
		CelestialRegistry->RegisterCelestialBody(this);
	}

}

void ASRCelestialBody::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USRCelestialBodyRegistrySubsystem* CelestialRegistry = FindCelestialRegistry())
	{
		CelestialRegistry->UnregisterCelestialBody(this);
	}

	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void ASRCelestialBody::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	ApplyData();
}
#endif

void ASRCelestialBody::SetData(const FSRCelestialBodyData& NewData)
{
	bHasAppliedData = true;
	bHasLoggedMissingDataError = false;
	VariableName = NewData.VariableName;
	BodyCategory = NewData.BodyCategory;
	FocusZoomMultiplier = NewData.FocusZoomMultiplier;
	GenerationSeed = NewData.GenerationSeed;
	DynamicMeshGeneration = NewData.DynamicMeshGeneration;
	Scale = NewData.Scale;
	StaticMesh = NewData.StaticMesh;
	Material = NewData.Material;
	Mass = NewData.Mass;
	GravityRatio = NewData.GravityRatio;
	GravityRadiusRatio = NewData.GravityRadiusRatio;
	ShowGravityLine = NewData.ShowGravityLine;
	GravityLineColor = NewData.GravityLineColor;
	GravityLineOpacity = NewData.GravityLineOpacity;
	GravityLineSegments = NewData.GravityLineSegments;
	GravityLineThickness = NewData.GravityLineThickness;

	if (HasActorBegunPlay() && GetWorld() && GetWorld()->IsGameWorld() && IsValid(StaticMesh) && IsValid(Material))
	{
		ApplyData();
	}
}

void ASRCelestialBody::ApplyData()
{
	if (!bHasAppliedData && GetWorld() && GetWorld()->IsGameWorld())
	{
		LogMissingDataErrorOnce(TEXT("ApplyData"));
		return;
	}

	Scale = FMath::Max(0.0f, Scale);
	Mass = FMath::Max(0.0f, Mass);
	GravityRatio = FMath::Max(0.0f, GravityRatio);
	GravityRadiusRatio = FMath::Max(0.0f, GravityRadiusRatio);
	GravityLineOpacity = FMath::Clamp(GravityLineOpacity, 0.0f, 1.0f);
	GravityLineSegments = FMath::Max(3, GravityLineSegments);
	GravityLineThickness = FMath::Max(0.0f, GravityLineThickness);
	FocusZoomMultiplier = FMath::Max(0.0f, FocusZoomMultiplier);

	SetActorScale3D(FVector::OneVector);
	if (IsValid(CelestialBodyDynamicMesh.Get()))
	{
		CelestialBodyDynamicMesh->SetRelativeLocation(FVector::ZeroVector);
		CelestialBodyDynamicMesh->SetRelativeRotation(FRotator::ZeroRotator);
		CelestialBodyDynamicMesh->SetRelativeScale3D(FVector(Scale));
	}
	if (IsValid(CelestialBodyStaticMesh.Get()))
	{
		CelestialBodyStaticMesh->SetRelativeLocation(FVector::ZeroVector);
		CelestialBodyStaticMesh->SetRelativeRotation(FRotator::ZeroRotator);
		CelestialBodyStaticMesh->SetRelativeScale3D(FVector(Scale));
	}

	EnsureCelestialBodyDynamicMeshVisuals();

	if (IsValid(ClickSphereCollision))
	{
		ClickSphereCollision->SetRelativeLocation(FVector::ZeroVector);
		ClickSphereCollision->SetRelativeRotation(FRotator::ZeroRotator);
		ClickSphereCollision->SetRelativeScale3D(FVector::OneVector);
		const float BodyRadius = IsValid(StaticMesh.Get())
			? StaticMesh->GetBounds().SphereRadius * Scale
			: 0.0f;
		ClickSphereCollision->SetSphereRadius(FMath::Max(BodyRadius, 1.0f));
	}
	ApplyGravityLineSettings();
}

FSRCelestialBodyData ASRCelestialBody::GetData() const
{
	FSRCelestialBodyData CurrentData;
	CurrentData.VariableName = VariableName;
	CurrentData.BodyCategory = BodyCategory;
	CurrentData.FocusZoomMultiplier = FocusZoomMultiplier;
	CurrentData.GenerationSeed = GenerationSeed;
	CurrentData.Scale = Scale;
	CurrentData.StaticMesh = StaticMesh;
	CurrentData.Material = Material;
	CurrentData.Mass = Mass;
	CurrentData.GravityRatio = GravityRatio;
	CurrentData.GravityRadiusRatio = GravityRadiusRatio;
	CurrentData.ShowGravityLine = ShowGravityLine;
	CurrentData.GravityLineColor = GravityLineColor;
	CurrentData.GravityLineOpacity = GravityLineOpacity;
	CurrentData.GravityLineSegments = GravityLineSegments;
	CurrentData.GravityLineThickness = GravityLineThickness;
	return CurrentData;
}

UDynamicMeshComponent* ASRCelestialBody::GetCelestialBodyDynamicMesh() const
{
	return CelestialBodyDynamicMesh;
}

void ASRCelestialBody::SetCelestialBodyMesh(bool bUseDynamicMesh)
{
	if (IsValid(CelestialBodyDynamicMesh.Get()))
	{
		CelestialBodyDynamicMesh->SetVisibility(bUseDynamicMesh);
		CelestialBodyDynamicMesh->SetHiddenInGame(!bUseDynamicMesh);
	}

	if (IsValid(CelestialBodyStaticMesh.Get()))
	{
		CelestialBodyStaticMesh->SetVisibility(!bUseDynamicMesh);
		CelestialBodyStaticMesh->SetHiddenInGame(bUseDynamicMesh);
	}
}

ESRCelestialBodyCategory ASRCelestialBody::GetBodyCategory() const
{
	return BodyCategory;
}

USRGravityParent* ASRCelestialBody::GetGravityParent() const
{
	return GravityParent;
}

USROrbit* ASRCelestialBody::GetOrbit() const
{
	return nullptr;
}

USRPlanetSurfaceGrid* ASRCelestialBody::GetSurfaceGrid() const
{
	return nullptr;
}

void ASRCelestialBody::ApplyGravityLineSettings()
{
	if (!IsValid(GravityParent))
	{
		return;
	}

	GravityParent->ConfigureGravity(
		Mass,
		GravityRatio,
		GravityRadiusRatio,
		ShowGravityLine,
		GravityLineColor,
		GravityLineOpacity,
		GravityLineSegments,
		GravityLineThickness);
}

void ASRCelestialBody::EnsureCelestialBodyDynamicMeshVisuals()
{
	if (!IsValid(CelestialBodyDynamicMesh.Get()))
	{
		return;
	}

	UStaticMesh* DesiredMesh = nullptr;
	if (IsValid(StaticMesh))
	{
		DesiredMesh = StaticMesh.Get();
	}
	if (!IsValid(DesiredMesh))
	{
		UE_LOG(LogStarRoversCelestial, Error, TEXT("Celestial body '%s' requires StaticMesh."), *GetName());
		return;
	}

	if (IsValid(CelestialBodyStaticMesh.Get()) && CelestialBodyStaticMesh->GetStaticMesh() != DesiredMesh)
	{
		CelestialBodyStaticMesh->SetStaticMesh(DesiredMesh);
	}

	CopyStaticMeshToCelestialBodyDynamicMesh();

	UMaterialInterface* DesiredBaseMaterial = Material;
	UMaterialInterface* CurrentAssignedMaterial = CelestialBodyDynamicMesh->GetMaterial(0);

	if (!IsValid(DesiredBaseMaterial))
	{
		if (IsStellarBody() && IsValid(CurrentAssignedMaterial))
		{
			DesiredBaseMaterial = CurrentAssignedMaterial;
		}
	}

	if (!IsValid(DesiredBaseMaterial))
	{
		UE_LOG(LogStarRoversCelestial, Error, TEXT("Celestial body '%s' requires Material."), *GetName());
		return;
	}

	if (IsValid(CelestialBodyStaticMesh.Get()))
	{
		CelestialBodyStaticMesh->SetMaterial(0, DesiredBaseMaterial);
	}

	UMaterialInstanceDynamic* ActiveDynamicMaterial = GetActiveBodyDynamicMaterial();
	const UMaterialInstance* ActiveMaterialInstance = ActiveDynamicMaterial;
	if (!IsValid(ActiveDynamicMaterial) || ActiveMaterialInstance->Parent != DesiredBaseMaterial)
	{
		ActiveDynamicMaterial = UMaterialInstanceDynamic::Create(DesiredBaseMaterial, this);
		if (IsValid(ActiveDynamicMaterial))
		{
			CelestialBodyDynamicMesh->SetMaterial(0, ActiveDynamicMaterial);
		}
		else
		{
			CelestialBodyDynamicMesh->SetMaterial(0, DesiredBaseMaterial);
		}
	}

	for (const TPair<ESRPlanetBiome, TObjectPtr<UMaterialInterface>>& BiomeMaterialPair : DynamicMeshGeneration.BiomeMaterials)
	{
		UMaterialInterface* BiomeMaterial = BiomeMaterialPair.Value.Get();
		const int32 MaterialSlotIndex = GetTerrainBiomeMaterialSlotIndex(BiomeMaterialPair.Key);
		if (IsValid(BiomeMaterial) && MaterialSlotIndex > 0)
		{
			CelestialBodyDynamicMesh->SetMaterial(MaterialSlotIndex, BiomeMaterial);
		}
	}
}

bool ASRCelestialBody::CopyStaticMeshToCelestialBodyDynamicMesh()
{
	if (!IsValid(CelestialBodyDynamicMesh.Get()) || !IsValid(StaticMesh.Get()))
	{
		return false;
	}

	const FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData();
	if (!RenderData || RenderData->LODResources.IsEmpty())
	{
		UE_LOG(LogStarRoversCelestial, Error, TEXT("Celestial body '%s' requires render data on StaticMesh."), *GetName());
		return false;
	}

	const FStaticMeshLODResources& LODResource = RenderData->LODResources[0];
	const FPositionVertexBuffer& PositionVertexBuffer = LODResource.VertexBuffers.PositionVertexBuffer;
	const FStaticMeshVertexBuffer& StaticMeshVertexBuffer = LODResource.VertexBuffers.StaticMeshVertexBuffer;
	const FRawStaticIndexBuffer& IndexBuffer = LODResource.IndexBuffer;
	const int32 VertexCount = static_cast<int32>(PositionVertexBuffer.GetNumVertices());
	const int32 IndexCount = static_cast<int32>(IndexBuffer.GetNumIndices());
	if (VertexCount <= 0 || IndexCount < 3)
	{
		UE_LOG(LogStarRoversCelestial, Error, TEXT("Celestial body '%s' requires valid vertices and triangles on StaticMesh."), *GetName());
		return false;
	}

	UE::Geometry::FDynamicMesh3 DynamicMesh;
	DynamicMesh.EnableAttributes();
	DynamicMesh.Attributes()->EnablePrimaryColors();
	DynamicMesh.Attributes()->EnableMaterialID();
	UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay = DynamicMesh.Attributes()->PrimaryNormals();
	auto* ColorOverlay = DynamicMesh.Attributes()->PrimaryColors();
	auto* MaterialIdAttribute = DynamicMesh.Attributes()->GetMaterialID();

	const bool bShouldGenerateSteppedQuadTerrain =
		(BodyCategory == ESRCelestialBodyCategory::Planet || BodyCategory == ESRCelestialBodyCategory::Moon)
		&& DynamicMeshGeneration.bDynamicMeshGeneration
		&& DynamicMeshGeneration.DynamicMeshHeight > KINDA_SMALL_NUMBER;
	if (bShouldGenerateSteppedQuadTerrain)
	{
		const TArray<FSRSourceQuad> SourceQuads = RecoverSourceQuads(PositionVertexBuffer, IndexBuffer, IndexCount);
		if (!SourceQuads.IsEmpty())
		{
			struct FSRGeneratedTerrainEdge
			{
				FVector SourcePointA = FVector::ZeroVector;
				FVector SourcePointB = FVector::ZeroVector;
				FVector PointA = FVector::ZeroVector;
				FVector PointB = FVector::ZeroVector;
				FLinearColor SurfaceColor = FLinearColor::White;
				int32 MaterialId = 0;
			};

			auto AppendFlatColoredQuad = [&DynamicMesh, NormalOverlay, ColorOverlay, MaterialIdAttribute](
				const FVector& Point0,
				const FVector& Point1,
				const FVector& Point2,
				const FVector& Point3,
				const FLinearColor& SurfaceColor,
				const int32 MaterialId,
				bool bDoubleSided = false)
			{
				FVector QuadPoints[4] = { Point0, Point1, Point2, Point3 };
				const FVector QuadCenter = (Point0 + Point1 + Point2 + Point3) * 0.25f;
				const FVector OutwardDirection = QuadCenter.GetSafeNormal();
				FVector QuadNormal = FVector::CrossProduct(QuadPoints[1] - QuadPoints[0], QuadPoints[2] - QuadPoints[0]).GetSafeNormal();
				if (!OutwardDirection.IsNearlyZero() && FVector::DotProduct(QuadNormal, OutwardDirection) < 0.0f)
				{
					Swap(QuadPoints[1], QuadPoints[3]);
					QuadNormal *= -1.0f;
				}
				if (QuadNormal.IsNearlyZero())
				{
					QuadNormal = OutwardDirection.IsNearlyZero() ? FVector::UpVector : OutwardDirection;
				}

				const int32 Vertex0 = DynamicMesh.AppendVertex(FVector3d(QuadPoints[0]));
				const int32 Vertex1 = DynamicMesh.AppendVertex(FVector3d(QuadPoints[1]));
				const int32 Vertex2 = DynamicMesh.AppendVertex(FVector3d(QuadPoints[2]));
				const int32 Vertex3 = DynamicMesh.AppendVertex(FVector3d(QuadPoints[3]));

				const int32 Normal0 = NormalOverlay->AppendElement(FVector3f(QuadNormal));
				const int32 Normal1 = NormalOverlay->AppendElement(FVector3f(QuadNormal));
				const int32 Normal2 = NormalOverlay->AppendElement(FVector3f(QuadNormal));
				const int32 Normal3 = NormalOverlay->AppendElement(FVector3f(QuadNormal));
				const int32 Color0 = ColorOverlay->AppendElement(FVector4f(SurfaceColor.R, SurfaceColor.G, SurfaceColor.B, SurfaceColor.A));
				const int32 Color1 = ColorOverlay->AppendElement(FVector4f(SurfaceColor.R, SurfaceColor.G, SurfaceColor.B, SurfaceColor.A));
				const int32 Color2 = ColorOverlay->AppendElement(FVector4f(SurfaceColor.R, SurfaceColor.G, SurfaceColor.B, SurfaceColor.A));
				const int32 Color3 = ColorOverlay->AppendElement(FVector4f(SurfaceColor.R, SurfaceColor.G, SurfaceColor.B, SurfaceColor.A));

				const int32 Triangle0 = DynamicMesh.AppendTriangle(Vertex0, Vertex2, Vertex1);
				const int32 Triangle1 = DynamicMesh.AppendTriangle(Vertex0, Vertex3, Vertex2);
				if (Triangle0 >= 0)
				{
					NormalOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(Normal0, Normal2, Normal1));
					ColorOverlay->SetTriangle(Triangle0, UE::Geometry::FIndex3i(Color0, Color2, Color1));
					if (MaterialIdAttribute)
					{
						MaterialIdAttribute->SetValue(Triangle0, MaterialId);
					}
				}
				if (Triangle1 >= 0)
				{
					NormalOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(Normal0, Normal3, Normal2));
					ColorOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(Color0, Color3, Color2));
					if (MaterialIdAttribute)
					{
						MaterialIdAttribute->SetValue(Triangle1, MaterialId);
					}
				}

				if (bDoubleSided)
				{
					const int32 BackVertex0 = DynamicMesh.AppendVertex(FVector3d(QuadPoints[0]));
					const int32 BackVertex1 = DynamicMesh.AppendVertex(FVector3d(QuadPoints[1]));
					const int32 BackVertex2 = DynamicMesh.AppendVertex(FVector3d(QuadPoints[2]));
					const int32 BackVertex3 = DynamicMesh.AppendVertex(FVector3d(QuadPoints[3]));

					const FVector BackNormal = -QuadNormal;
					const int32 BackNormal0 = NormalOverlay->AppendElement(FVector3f(BackNormal));
					const int32 BackNormal1 = NormalOverlay->AppendElement(FVector3f(BackNormal));
					const int32 BackNormal2 = NormalOverlay->AppendElement(FVector3f(BackNormal));
					const int32 BackNormal3 = NormalOverlay->AppendElement(FVector3f(BackNormal));
					const int32 BackColor0 = ColorOverlay->AppendElement(FVector4f(SurfaceColor.R, SurfaceColor.G, SurfaceColor.B, SurfaceColor.A));
					const int32 BackColor1 = ColorOverlay->AppendElement(FVector4f(SurfaceColor.R, SurfaceColor.G, SurfaceColor.B, SurfaceColor.A));
					const int32 BackColor2 = ColorOverlay->AppendElement(FVector4f(SurfaceColor.R, SurfaceColor.G, SurfaceColor.B, SurfaceColor.A));
					const int32 BackColor3 = ColorOverlay->AppendElement(FVector4f(SurfaceColor.R, SurfaceColor.G, SurfaceColor.B, SurfaceColor.A));

					const int32 BackTriangle0 = DynamicMesh.AppendTriangle(BackVertex0, BackVertex1, BackVertex2);
					const int32 BackTriangle1 = DynamicMesh.AppendTriangle(BackVertex0, BackVertex2, BackVertex3);
					if (BackTriangle0 >= 0)
					{
						NormalOverlay->SetTriangle(BackTriangle0, UE::Geometry::FIndex3i(BackNormal0, BackNormal1, BackNormal2));
						ColorOverlay->SetTriangle(BackTriangle0, UE::Geometry::FIndex3i(BackColor0, BackColor1, BackColor2));
						if (MaterialIdAttribute)
						{
							MaterialIdAttribute->SetValue(BackTriangle0, MaterialId);
						}
					}
					if (BackTriangle1 >= 0)
					{
						NormalOverlay->SetTriangle(BackTriangle1, UE::Geometry::FIndex3i(BackNormal0, BackNormal2, BackNormal3));
						ColorOverlay->SetTriangle(BackTriangle1, UE::Geometry::FIndex3i(BackColor0, BackColor2, BackColor3));
						if (MaterialIdAttribute)
						{
							MaterialIdAttribute->SetValue(BackTriangle1, MaterialId);
						}
					}
				}
			};

			TMap<uint64, FSRGeneratedTerrainEdge> PendingTerrainEdges;
			PendingTerrainEdges.Reserve(SourceQuads.Num() * 2);
			auto RegisterTerrainEdge = [&PendingTerrainEdges, &AppendFlatColoredQuad](
				const FVector& SourcePointA,
				const FVector& SourcePointB,
				const FVector& PointA,
				const FVector& PointB,
				const FLinearColor& SurfaceColor,
				const int32 MaterialId)
			{
				const uint32 EndpointHashA = HashSourcePosition(SourcePointA);
				const uint32 EndpointHashB = HashSourcePosition(SourcePointB);
				if (EndpointHashA == EndpointHashB)
				{
					return;
				}

				FVector OrderedPointA = PointA;
				FVector OrderedPointB = PointB;
				FVector OrderedSourcePointA = SourcePointA;
				FVector OrderedSourcePointB = SourcePointB;
				if (EndpointHashA > EndpointHashB)
				{
					Swap(OrderedPointA, OrderedPointB);
					Swap(OrderedSourcePointA, OrderedSourcePointB);
				}

				const uint64 EdgeKey = BuildSourcePositionEdgeKey(OrderedSourcePointA, OrderedSourcePointB);
				if (FSRGeneratedTerrainEdge* ExistingEdge = PendingTerrainEdges.Find(EdgeKey))
				{
					const bool bSameEdgePosition =
						FVector::DistSquared(ExistingEdge->PointA, OrderedPointA) <= KINDA_SMALL_NUMBER
						&& FVector::DistSquared(ExistingEdge->PointB, OrderedPointB) <= KINDA_SMALL_NUMBER;
					if (!bSameEdgePosition)
					{
						const FLinearColor WallColor = FLinearColor::LerpUsingHSV(ExistingEdge->SurfaceColor, SurfaceColor, 0.5f);
						AppendFlatColoredQuad(
							ExistingEdge->PointA,
							ExistingEdge->PointB,
							OrderedPointB,
							OrderedPointA,
							WallColor,
							ExistingEdge->MaterialId != 0 ? ExistingEdge->MaterialId : MaterialId,
							true);
					}
					PendingTerrainEdges.Remove(EdgeKey);
					return;
				}

				FSRGeneratedTerrainEdge NewEdge;
				NewEdge.SourcePointA = OrderedSourcePointA;
				NewEdge.SourcePointB = OrderedSourcePointB;
				NewEdge.PointA = OrderedPointA;
				NewEdge.PointB = OrderedPointB;
				NewEdge.SurfaceColor = SurfaceColor;
				NewEdge.MaterialId = MaterialId;
				PendingTerrainEdges.Add(EdgeKey, NewEdge);
			};

			for (const FSRSourceQuad& SourceQuad : SourceQuads)
			{
				FVector SourcePositions[4];
				FVector CellCenter = FVector::ZeroVector;
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
					CellCenter += SourcePositions[CornerIndex];
				}
				if (!bHasValidVertices)
				{
					continue;
				}

				CellCenter /= 4.0f;
				const FVector CellDirection = CellCenter.GetSafeNormal();
				if (CellDirection.IsNearlyZero())
				{
					continue;
				}

				const FSRPlanetTerrainSample TerrainSample = SampleTerrainForDynamicMesh(CellDirection, DynamicMeshGeneration);
				FVector TargetPositions[4];
				const float SourceCellRadius = FMath::Max(CellCenter.Length(), 1.0f);
				const float CellScale = FMath::Max(0.01f, (SourceCellRadius + TerrainSample.HeightOffset) / SourceCellRadius);
				for (int32 CornerIndex = 0; CornerIndex < 4; ++CornerIndex)
				{
					TargetPositions[CornerIndex] = SourcePositions[CornerIndex] * CellScale;
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

				const int32 MaterialId = GetTerrainBiomeMaterialId(DynamicMeshGeneration, TerrainSample.Biome);
				AppendFlatColoredQuad(TargetPositions[0], TargetPositions[1], TargetPositions[2], TargetPositions[3], TerrainSample.SurfaceColor, MaterialId);
				RegisterTerrainEdge(SourcePositions[0], SourcePositions[1], TargetPositions[0], TargetPositions[1], TerrainSample.SurfaceColor, MaterialId);
				RegisterTerrainEdge(SourcePositions[1], SourcePositions[2], TargetPositions[1], TargetPositions[2], TerrainSample.SurfaceColor, MaterialId);
				RegisterTerrainEdge(SourcePositions[2], SourcePositions[3], TargetPositions[2], TargetPositions[3], TerrainSample.SurfaceColor, MaterialId);
				RegisterTerrainEdge(SourcePositions[3], SourcePositions[0], TargetPositions[3], TargetPositions[0], TerrainSample.SurfaceColor, MaterialId);
			}

			for (const TPair<uint64, FSRGeneratedTerrainEdge>& PendingEdgePair : PendingTerrainEdges)
			{
				const FSRGeneratedTerrainEdge& PendingEdge = PendingEdgePair.Value;
				const bool bNeedsBoundaryWall =
					FVector::DistSquared(PendingEdge.SourcePointA, PendingEdge.PointA) > KINDA_SMALL_NUMBER
					|| FVector::DistSquared(PendingEdge.SourcePointB, PendingEdge.PointB) > KINDA_SMALL_NUMBER;
				if (!bNeedsBoundaryWall)
				{
					continue;
				}

				const FLinearColor WallColor = PendingEdge.SurfaceColor * 0.78f;
				AppendFlatColoredQuad(
					PendingEdge.PointA,
					PendingEdge.PointB,
					PendingEdge.SourcePointB,
					PendingEdge.SourcePointA,
					WallColor,
					PendingEdge.MaterialId,
					true);
			}

			CelestialBodyDynamicMesh->SetMesh(MoveTemp(DynamicMesh));
			return true;
		}

		UE_LOG(
			LogStarRoversCelestial,
			Warning,
			TEXT("Celestial body '%s' could not recover quad terrain cells from StaticMesh; falling back to triangle mesh copy."),
			*GetName());
	}

	TArray<int32> DynamicVertexIds;
	DynamicVertexIds.Reserve(VertexCount);
	TArray<int32> DynamicNormalIds;
	DynamicNormalIds.Reserve(VertexCount);
	TArray<int32> DynamicColorIds;
	DynamicColorIds.Reserve(VertexCount);

	for (int32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
	{
		const FVector SourcePosition(PositionVertexBuffer.VertexPosition(VertexIndex));
		FVector TargetPosition = SourcePosition;
		FLinearColor TargetColor = FLinearColor::White;

		DynamicVertexIds.Add(DynamicMesh.AppendVertex(FVector3d(TargetPosition)));

		FVector TargetNormal = TargetPosition.GetSafeNormal();
		if (TargetNormal.IsNearlyZero())
		{
			TargetNormal = FVector(StaticMeshVertexBuffer.VertexTangentZ(VertexIndex)).GetSafeNormal();
		}
		if (TargetNormal.IsNearlyZero())
		{
			TargetNormal = FVector::UpVector;
		}

		DynamicNormalIds.Add(NormalOverlay->AppendElement(FVector3f(TargetNormal)));
		DynamicColorIds.Add(ColorOverlay->AppendElement(FVector4f(TargetColor.R, TargetColor.G, TargetColor.B, TargetColor.A)));
	}

	for (int32 Index = 0; Index + 2 < IndexCount; Index += 3)
	{
		const int32 SourceVertexIndex0 = static_cast<int32>(IndexBuffer.GetIndex(Index));
		const int32 SourceVertexIndex1 = static_cast<int32>(IndexBuffer.GetIndex(Index + 1));
		const int32 SourceVertexIndex2 = static_cast<int32>(IndexBuffer.GetIndex(Index + 2));
		if (!DynamicVertexIds.IsValidIndex(SourceVertexIndex0)
			|| !DynamicVertexIds.IsValidIndex(SourceVertexIndex1)
			|| !DynamicVertexIds.IsValidIndex(SourceVertexIndex2))
		{
			continue;
		}

		const int32 TriangleId = DynamicMesh.AppendTriangle(
			DynamicVertexIds[SourceVertexIndex0],
			DynamicVertexIds[SourceVertexIndex1],
			DynamicVertexIds[SourceVertexIndex2]);
		if (TriangleId >= 0)
		{
			NormalOverlay->SetTriangle(
				TriangleId,
				UE::Geometry::FIndex3i(
					DynamicNormalIds[SourceVertexIndex0],
					DynamicNormalIds[SourceVertexIndex1],
					DynamicNormalIds[SourceVertexIndex2]));
			ColorOverlay->SetTriangle(
				TriangleId,
				UE::Geometry::FIndex3i(
					DynamicColorIds[SourceVertexIndex0],
					DynamicColorIds[SourceVertexIndex1],
					DynamicColorIds[SourceVertexIndex2]));
			if (MaterialIdAttribute)
			{
				MaterialIdAttribute->SetValue(TriangleId, 0);
			}
		}
	}

	CelestialBodyDynamicMesh->SetMesh(MoveTemp(DynamicMesh));
	return true;
}

UMaterialInstanceDynamic* ASRCelestialBody::GetActiveBodyDynamicMaterial() const
{
	return IsValid(CelestialBodyDynamicMesh.Get())
		? Cast<UMaterialInstanceDynamic>(CelestialBodyDynamicMesh->GetMaterial(0))
		: nullptr;
}

void ASRCelestialBody::LogMissingDataErrorOnce(const TCHAR* Context) const
{
	if (bHasLoggedMissingDataError)
	{
		return;
	}

	bHasLoggedMissingDataError = true;
	UE_LOG(
		LogStarRoversCelestial,
		Error,
		TEXT("%s '%s' requires body data before runtime use. SetData() was never called. Configure it through a data asset-driven spawn path instead of Blueprint defaults."),
		Context ? Context : TEXT("ASRCelestialBody"),
		*GetName());
}

USRCelestialBodyRegistrySubsystem* ASRCelestialBody::FindCelestialRegistry() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<USRCelestialBodyRegistrySubsystem>() : nullptr;
}

bool ASRCelestialBody::IsStellarBody() const
{
	return BodyCategory == ESRCelestialBodyCategory::Star;
}
