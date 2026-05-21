#include "Celestial/SRCelestialBody.h"

#include "Algo/Reverse.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/LineBatchComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Gravity/SRGravityParent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "Simulation/SRTimeControlSubsystem.h"
#include "Components/SphereComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "Engine/StaticMesh.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Rendering/StaticMeshVertexBuffer.h"
#include "Surface/SRPlanetTerrainGenerator.h"
#include "UI/SRCelestialBodyCircleWidget.h"
#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

namespace
{
	FProperty* FindCelestialBodyPropertyInClassHierarchy(const UClass* InClass, const FName PropertyName)
	{
		for (const UStruct* Struct = InClass; Struct; Struct = Struct->GetSuperStruct())
		{
			if (FProperty* Property = Struct->FindPropertyByName(PropertyName))
			{
				return Property;
			}
		}

		return nullptr;
	}

	bool CopyPropertyValueByName(UObject* DestinationObject, const UObject* SourceObject, const FName PropertyName)
	{
		if (!IsValid(DestinationObject) || !IsValid(SourceObject))
		{
			return false;
		}

		FProperty* DestinationProperty = FindCelestialBodyPropertyInClassHierarchy(DestinationObject->GetClass(), PropertyName);
		FProperty* SourceProperty = FindCelestialBodyPropertyInClassHierarchy(SourceObject->GetClass(), PropertyName);
		if (!DestinationProperty || !SourceProperty || !DestinationProperty->SameType(SourceProperty))
		{
			return false;
		}

		void* DestinationValuePtr = DestinationProperty->ContainerPtrToValuePtr<void>(DestinationObject);
		const void* SourceValuePtr = SourceProperty->ContainerPtrToValuePtr<void>(SourceObject);
		DestinationProperty->CopyCompleteValue(DestinationValuePtr, SourceValuePtr);
		return true;
	}

	bool TryGetFloatPropertyValue(const UObject* SourceObject, const FName PropertyName, float& OutValue)
	{
		if (!IsValid(SourceObject))
		{
			return false;
		}

		FProperty* SourceProperty = FindCelestialBodyPropertyInClassHierarchy(SourceObject->GetClass(), PropertyName);
		if (!SourceProperty)
		{
			return false;
		}

		if (const FFloatProperty* FloatProperty = CastField<FFloatProperty>(SourceProperty))
		{
			OutValue = FloatProperty->GetPropertyValue_InContainer(SourceObject);
			return true;
		}

		if (const FDoubleProperty* DoubleProperty = CastField<FDoubleProperty>(SourceProperty))
		{
			OutValue = static_cast<float>(DoubleProperty->GetPropertyValue_InContainer(SourceObject));
			return true;
		}

		if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(SourceProperty))
		{
			const void* ValuePtr = SourceProperty->ContainerPtrToValuePtr<void>(SourceObject);
			OutValue = NumericProperty->IsFloatingPoint()
				? static_cast<float>(NumericProperty->GetFloatingPointPropertyValue(ValuePtr))
				: static_cast<float>(NumericProperty->GetSignedIntPropertyValue(ValuePtr));
			return true;
		}

		return false;
	}

	FVector EstimateProceduralTerrainNormal(const FVector& LocalUnitDirection, float BaseRadius, const FSRPlanetTerrainSettings& TerrainSettings)
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
		const float Height0 = FSRPlanetTerrainGenerator::SampleTerrain(Direction, TerrainSettings).HeightOffset;
		const float HeightA = FSRPlanetTerrainGenerator::SampleTerrain(DirectionA, TerrainSettings).HeightOffset;
		const float HeightB = FSRPlanetTerrainGenerator::SampleTerrain(DirectionB, TerrainSettings).HeightOffset;

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

	bool TryGetActorPropertyValue(const UObject* SourceObject, const FName PropertyName, AActor*& OutValue)
	{
		OutValue = nullptr;

		if (!IsValid(SourceObject))
		{
			return false;
		}

		FProperty* SourceProperty = FindCelestialBodyPropertyInClassHierarchy(SourceObject->GetClass(), PropertyName);
		const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(SourceProperty);
		if (!ObjectProperty)
		{
			return false;
		}

		OutValue = Cast<AActor>(ObjectProperty->GetObjectPropertyValue_InContainer(SourceObject));
		return true;
	}

	float ComputeScaledMeshRadius(const UStaticMesh* MeshAsset, const UMeshComponent* MeshComponent, const float BodyScale)
	{
		if (!IsValid(MeshAsset))
		{
			return 0.0f;
		}

		const float MeshRadius = MeshAsset->GetBounds().SphereRadius;
		const FVector AppliedScale = IsValid(MeshComponent)
			? MeshComponent->GetRelativeScale3D()
			: FVector(FMath::Max(0.0f, BodyScale));
		const float UniformScale = FMath::Max3(
			FMath::Abs(AppliedScale.X),
			FMath::Abs(AppliedScale.Y),
			FMath::Abs(AppliedScale.Z));

		return FMath::Max(0.0f, MeshRadius * UniformScale);
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

	FSRPlanetTerrainSample SampleSteppedTerrain(const FVector& LocalUnitDirection, const FSRPlanetTerrainSettings& TerrainSettings)
	{
		FSRPlanetTerrainSample Sample = FSRPlanetTerrainGenerator::SampleTerrain(LocalUnitDirection, TerrainSettings);
		const float SafeTerrainHeight = FMath::Max(0.0f, TerrainSettings.TerrainHeight);
		if (!TerrainSettings.bUseProceduralTerrain || SafeTerrainHeight <= KINDA_SMALL_NUMBER)
		{
			return Sample;
		}

		const float HeightStep = FMath::Max(1.0f, SafeTerrainHeight / 24.0f);
		Sample.HeightOffset = FMath::RoundToFloat(Sample.HeightOffset / HeightStep) * HeightStep;
		return Sample;
	}

	constexpr float CircleWidgetDefaultSizePixels = 64.0f;
	constexpr float CircleWidgetMinSizePixels = 12.0f;
	constexpr float CircleWidgetPaddingPixels = 8.0f;
}

DEFINE_LOG_CATEGORY_STATIC(LogStarRoversCelestial, Log, All);

FSRCelestialBodySpec::FSRCelestialBodySpec()
{
	DisplayName = FText::FromString(TEXT("Primary Star"));
	BodyCategory = ESRCelestialBodyCategory::Star;
	OrbitPeriodInPeriods = 0.0f;
	ConstructionHeightOffset = 15.0f;
	TerrainSettings = FSRPlanetTerrainSettings();
	TerrainSettings.bUseProceduralTerrain = false;
	TerrainSettings.TerrainHeight = 0.0f;
	bHasOcean = false;
	OceanMesh = nullptr;
	OceanMaterial = nullptr;
	OceanScaleMultiplier = 0.97f;
}

FSRCelestialBodyBiomeSpec::FSRCelestialBodyBiomeSpec()
{
	DisplayName = FText::FromString(TEXT("Biome"));
	bUseProceduralTerrain = true;
	TerrainSeed = 1337;
	TerrainHeight = 0.0f;
	TerrainFrequency = 3.0f;
	TerrainOctaves = 4;
	TerrainPersistence = 0.5f;
	TerrainSettings = FSRPlanetTerrainSettings();
	bHasOcean = false;
	OceanMesh = nullptr;
	OceanMaterial = nullptr;
	OceanScaleMultiplier = 0.97f;
	SurfaceGridSurfaceOffset = 0.0f;
	OrbitSpeed = 1.0f;
	StarPointLightIntensity = -1.0f;
	StarMaterialEmissiveStrength = -1.0f;
	StarPointLightColor = FLinearColor(1.0f, 0.956f, 0.84f, 1.0f);
}

ASRCelestialBody::ASRCelestialBody()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CelestialBodyDynamicMesh = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("CelestialBodyDynamicMesh"));
	CelestialBodyDynamicMesh->SetupAttachment(SceneRoot);
	CelestialBodyDynamicMesh->SetMobility(EComponentMobility::Movable);

	CelestialBodyStaticMesh_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CelestialBodyStaticMesh"));
	CelestialBodyStaticMesh_->SetupAttachment(SceneRoot);

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

	CircleWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("CircleWidget"));
	CircleWidget->SetupAttachment(SceneRoot);
	CircleWidget->SetMobility(EComponentMobility::Movable);
	CircleWidget->SetWidgetSpace(EWidgetSpace::Screen);
	CircleWidget->SetWidgetClass(USRCelestialBodyCircleWidget::StaticClass());
	CircleWidget->SetDrawSize(FVector2D(CircleWidgetDefaultSizePixels, CircleWidgetDefaultSizePixels));
	CircleWidget->SetPivot(FVector2D(0.5f, 0.5f));
	CircleWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	DisplayName = FText::FromString(TEXT("Celestial Body"));
	BodyCategory = ESRCelestialBodyCategory::Unknown;
	FocusZoomMultiplier = 3.0f;
	DynamicMeshThreshold = 0.1f;
	BodyScale = 1000.0f;
	ApproximateRadius = 50000.0f;
	Mass = 2000.0f;
	GenerationSeed = 1000;
	TerrainSettings = FSRPlanetTerrainSettings();
	TerrainSettings.bUseProceduralTerrain = false;
	TerrainSettings.TerrainHeight = 0.0f;
	GravityRatio = 1.0f;
	GravityRadiusRatio = 10.0f;
	bShowGravityLine = true;
	GravityLineColor = FLinearColor(0.45f, 1.0f, 0.45f, 1.0f);
	GravityLineOpacity = 0.85f;
	GravityLineSegments = 96;
	GravityLineThickness = 20.0f;
}

void ASRCelestialBody::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	ApplyConfiguredBodyState();
}

void ASRCelestialBody::BeginPlay()
{
	Super::BeginPlay();

	if (!bHasAppliedSpec)
	{
		LogMissingSpecErrorOnce(TEXT("BeginPlay"));
		SetActorTickEnabled(false);
		return;
	}

	SetActorTickEnabled(true);

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

void ASRCelestialBody::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateCircleWidgetDrawSize();
}

#if WITH_EDITOR
void ASRCelestialBody::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	ApplyConfiguredBodyState();
}
#endif

void ASRCelestialBody::ApplySpec(const FSRCelestialBodySpec& NewSpec)
{
	bHasAppliedSpec = true;
	bHasLoggedMissingSpecError = false;
	DisplayName = NewSpec.DisplayName;
	BodyCategory = NewSpec.BodyCategory;
	FocusZoomMultiplier = NewSpec.FocusZoomMultiplier;
	GenerationSeed = NewSpec.TerrainSeed;
	TerrainSettings = NewSpec.TerrainSettings;
	BodyScale = NewSpec.BodyScale;
	Mass = NewSpec.Mass;
	GravityRatio = NewSpec.GravityRatio;
	GravityRadiusRatio = NewSpec.GravityRadiusRatio;
	bShowGravityLine = NewSpec.bShowGravityLine;
	GravityLineColor = NewSpec.GravityLineColor;
	GravityLineOpacity = NewSpec.GravityLineOpacity;
	GravityLineSegments = NewSpec.GravityLineSegments;
	GravityLineThickness = NewSpec.GravityLineThickness;

	if (HasActorBegunPlay() && GetWorld() && GetWorld()->IsGameWorld() && IsValid(CelestialBodyStaticMesh) && IsValid(CelestialBodyMaterial))
	{
		ApplyConfiguredBodyState();
	}
}

void ASRCelestialBody::ApplyBiomeSpec(const FSRCelestialBodyBiomeSpec& NewBiomeSpec)
{
	bHasAppliedSpec = true;
	bHasLoggedMissingSpecError = false;

	DisplayName = NewBiomeSpec.DisplayName;
	GenerationSeed = NewBiomeSpec.TerrainSeed;

	ApplyConfiguredBodyState();
}

void ASRCelestialBody::ApplyConfiguredBodyState()
{
	if (!bHasAppliedSpec && GetWorld() && GetWorld()->IsGameWorld())
	{
		LogMissingSpecErrorOnce(TEXT("ApplyConfiguredBodyState"));
		return;
	}

	if (PrimaryActorTick.bCanEverTick)
	{
		SetActorTickEnabled(true);
	}

	BodyScale = FMath::Max(0.0f, BodyScale);
	Mass = FMath::Max(0.0f, Mass);
	GravityRatio = FMath::Max(0.0f, GravityRatio);
	GravityRadiusRatio = FMath::Max(0.0f, GravityRadiusRatio);
	GravityLineOpacity = FMath::Clamp(GravityLineOpacity, 0.0f, 1.0f);
	GravityLineSegments = FMath::Max(3, GravityLineSegments);
	GravityLineThickness = FMath::Max(0.0f, GravityLineThickness);
	FocusZoomMultiplier = FMath::Max(0.0f, FocusZoomMultiplier);
	DynamicMeshThreshold = FMath::Clamp(DynamicMeshThreshold, 0.0f, 1.0f);

	SetActorScale3D(FVector::OneVector);
	if (IsValid(CelestialBodyDynamicMesh.Get()))
	{
		CelestialBodyDynamicMesh->SetRelativeLocation(FVector::ZeroVector);
		CelestialBodyDynamicMesh->SetRelativeRotation(FRotator::ZeroRotator);
		CelestialBodyDynamicMesh->SetRelativeScale3D(FVector(BodyScale));
	}
	if (IsValid(CelestialBodyStaticMesh_.Get()))
	{
		CelestialBodyStaticMesh_->SetRelativeLocation(FVector::ZeroVector);
		CelestialBodyStaticMesh_->SetRelativeRotation(FRotator::ZeroRotator);
		CelestialBodyStaticMesh_->SetRelativeScale3D(FVector(BodyScale));
	}

	EnsureCelestialBodyDynamicMeshVisuals();

	if (IsValid(ClickSphereCollision))
	{
		ClickSphereCollision->SetRelativeLocation(FVector::ZeroVector);
		ClickSphereCollision->SetRelativeRotation(FRotator::ZeroRotator);
		ClickSphereCollision->SetRelativeScale3D(FVector::OneVector);
		ClickSphereCollision->SetSphereRadius(FMath::Max(ComputeCelestialBodyDynamicMeshRadius(), 1.0f));
	}
	ApplyGravityLineSettings();
	SyncApproximateRadiusFromCelestialBodyDynamicMesh();
	UpdateCircleWidgetDrawSize();
}

void ASRCelestialBody::SyncApproximateRadiusFromCelestialBodyDynamicMesh()
{
	ApproximateRadius = ComputeCelestialBodyDynamicMeshRadius();
}

void ASRCelestialBody::UpdateCircleWidgetDrawSize()
{
	if (!IsValid(CircleWidget))
	{
		return;
	}

	UWorld* World = GetWorld();
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	if (!IsValid(PlayerController) || !IsValid(PlayerController->PlayerCameraManager))
	{
		CircleWidget->SetDrawSize(FVector2D(CircleWidgetDefaultSizePixels, CircleWidgetDefaultSizePixels));
		return;
	}

	const FVector BodyLocation = GetActorLocation();
	const float BodyRadius = FMath::Max(0.0f, ApproximateRadius);
	if (BodyRadius <= KINDA_SMALL_NUMBER)
	{
		CircleWidget->SetVisibility(false);
		return;
	}

	const FVector CameraLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
	const FVector CameraForward = PlayerController->PlayerCameraManager->GetCameraRotation().Vector().GetSafeNormal();
	if (FVector::DotProduct(BodyLocation - CameraLocation, CameraForward) <= KINDA_SMALL_NUMBER)
	{
		CircleWidget->SetVisibility(false);
		return;
	}

	const FRotator CameraRotation = PlayerController->PlayerCameraManager->GetCameraRotation();
	const FVector CameraRight = CameraRotation.RotateVector(FVector::RightVector).GetSafeNormal();
	const FVector CameraUp = CameraRotation.RotateVector(FVector::UpVector).GetSafeNormal();

	FVector2D CenterScreenPosition = FVector2D::ZeroVector;
	FVector2D RadiusRightScreenPosition = FVector2D::ZeroVector;
	FVector2D RadiusUpScreenPosition = FVector2D::ZeroVector;
	const bool bProjectedCenter = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PlayerController, BodyLocation, CenterScreenPosition, true);
	const bool bProjectedRight = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PlayerController, BodyLocation + CameraRight * BodyRadius, RadiusRightScreenPosition, true);
	const bool bProjectedUp = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PlayerController, BodyLocation + CameraUp * BodyRadius, RadiusUpScreenPosition, true);
	if (!bProjectedCenter || !bProjectedRight || !bProjectedUp)
	{
		CircleWidget->SetVisibility(false);
		return;
	}

	const float ProjectedRadius = FMath::Max(
		FVector2D::Distance(CenterScreenPosition, RadiusRightScreenPosition),
		FVector2D::Distance(CenterScreenPosition, RadiusUpScreenPosition));
	const float DrawSizePixels = FMath::Max(CircleWidgetMinSizePixels, (ProjectedRadius * 2.0f) + CircleWidgetPaddingPixels);
	CircleWidget->SetDrawSize(FVector2D(DrawSizePixels, DrawSizePixels));
	CircleWidget->SetVisibility(true);
}

FSRCelestialBodySpec ASRCelestialBody::GetSpec() const
{
	FSRCelestialBodySpec CurrentSpec;
	CurrentSpec.DisplayName = DisplayName;
	CurrentSpec.BodyCategory = BodyCategory;
	CurrentSpec.FocusZoomMultiplier = FocusZoomMultiplier;
	CurrentSpec.TerrainSeed = GenerationSeed;
	CurrentSpec.BodyScale = BodyScale;
	CurrentSpec.ApproximateRadius = ApproximateRadius;
	CurrentSpec.Mass = Mass;
	CurrentSpec.GravityRatio = GravityRatio;
	CurrentSpec.GravityRadiusRatio = GravityRadiusRatio;
	CurrentSpec.bShowGravityLine = bShowGravityLine;
	CurrentSpec.GravityLineColor = GravityLineColor;
	CurrentSpec.GravityLineOpacity = GravityLineOpacity;
	CurrentSpec.GravityLineSegments = GravityLineSegments;
	CurrentSpec.GravityLineThickness = GravityLineThickness;
	return CurrentSpec;
}

float ASRCelestialBody::ConvertPeriodsToSeconds(float PeriodValue) const
{
	return FMath::Max(0.0f, PeriodValue) * FMath::Max(0.0f, ResolveSecondsPerPeriod());
}

UDynamicMeshComponent* ASRCelestialBody::GetCelestialBodyDynamicMesh() const
{
	return CelestialBodyDynamicMesh;
}

void ASRCelestialBody::SetDynamicMeshEnabled(bool bUseDynamicMesh)
{
	if (IsValid(CelestialBodyDynamicMesh.Get()))
	{
		CelestialBodyDynamicMesh->SetVisibility(bUseDynamicMesh);
		CelestialBodyDynamicMesh->SetHiddenInGame(!bUseDynamicMesh);
	}

	if (IsValid(CelestialBodyStaticMesh_.Get()))
	{
		CelestialBodyStaticMesh_->SetVisibility(!bUseDynamicMesh);
		CelestialBodyStaticMesh_->SetHiddenInGame(bUseDynamicMesh);
	}
}

float ASRCelestialBody::GetDynamicMeshThreshold() const
{
	return FMath::Clamp(DynamicMeshThreshold, 0.0f, 1.0f);
}

ESRCelestialBodyCategory ASRCelestialBody::GetBodyCategory() const
{
	return BodyCategory;
}

USRGravityParent* ASRCelestialBody::GetGravityParent() const
{
	return GravityParent;
}

USROrbit* ASRCelestialBody::GetOrbitComponent() const
{
	return nullptr;
}

USRPlanetSurfaceGrid* ASRCelestialBody::GetSurfaceGrid() const
{
	return nullptr;
}

void ASRCelestialBody::SetCelestialBodyAssets(UStaticMesh* NewCelestialBodyStaticMesh, UMaterialInterface* NewCelestialBodyMaterial)
{
	CelestialBodyStaticMesh = NewCelestialBodyStaticMesh;
	CelestialBodyMaterial = NewCelestialBodyMaterial;
	if (bHasAppliedSpec && HasActorBegunPlay())
	{
		ApplyConfiguredBodyState();
	}
}

void ASRCelestialBody::ApplyGravityLineSettings()
{
	if (!IsValid(GravityParent))
	{
		return;
	}

	static const FName GravityPropertyNames[] =
	{
		TEXT("Mass"),
		TEXT("GravityRatio"),
		TEXT("GravityRadiusRatio"),
		TEXT("bShowGravityLine"),
		TEXT("GravityLineColor"),
		TEXT("GravityLineOpacity"),
		TEXT("GravityLineSegments"),
		TEXT("GravityLineThickness")
	};

	for (const FName PropertyName : GravityPropertyNames)
	{
		CopyPropertyValueByName(GravityParent, this, PropertyName);
	}

	GravityParent->RecomputeDerivedValues();
}

void ASRCelestialBody::EnsureCelestialBodyDynamicMeshVisuals()
{
	if (!IsValid(CelestialBodyDynamicMesh.Get()))
	{
		return;
	}

	UStaticMesh* DesiredMesh = nullptr;
	if (IsValid(CelestialBodyStaticMesh))
	{
		DesiredMesh = CelestialBodyStaticMesh.Get();
	}
	if (!IsValid(DesiredMesh))
	{
		UE_LOG(LogStarRoversCelestial, Error, TEXT("Celestial body '%s' requires CelestialBodyStaticMesh."), *GetName());
		return;
	}

	if (IsValid(CelestialBodyStaticMesh_.Get()) && CelestialBodyStaticMesh_->GetStaticMesh() != DesiredMesh)
	{
		CelestialBodyStaticMesh_->SetStaticMesh(DesiredMesh);
	}

	CopySourceStaticMeshToCelestialBodyDynamicMesh();

	UMaterialInterface* DesiredBaseMaterial = CelestialBodyMaterial;
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
		UE_LOG(LogStarRoversCelestial, Error, TEXT("Celestial body '%s' requires CelestialBodyMaterial."), *GetName());
		return;
	}

	if (IsValid(CelestialBodyStaticMesh_.Get()))
	{
		CelestialBodyStaticMesh_->SetMaterial(0, DesiredBaseMaterial);
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
}

bool ASRCelestialBody::CopySourceStaticMeshToCelestialBodyDynamicMesh()
{
	if (!IsValid(CelestialBodyDynamicMesh.Get()) || !IsValid(CelestialBodyStaticMesh.Get()))
	{
		return false;
	}

	const FStaticMeshRenderData* RenderData = CelestialBodyStaticMesh->GetRenderData();
	if (!RenderData || RenderData->LODResources.IsEmpty())
	{
		UE_LOG(LogStarRoversCelestial, Error, TEXT("Celestial body '%s' requires render data on CelestialBodyStaticMesh."), *GetName());
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
		UE_LOG(LogStarRoversCelestial, Error, TEXT("Celestial body '%s' requires valid vertices and triangles on CelestialBodyStaticMesh."), *GetName());
		return false;
	}

	UE::Geometry::FDynamicMesh3 DynamicMesh;
	DynamicMesh.EnableAttributes();
	DynamicMesh.Attributes()->EnablePrimaryColors();
	UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay = DynamicMesh.Attributes()->PrimaryNormals();
	auto* ColorOverlay = DynamicMesh.Attributes()->PrimaryColors();

	const bool bShouldGenerateSteppedQuadTerrain =
		(BodyCategory == ESRCelestialBodyCategory::Planet || BodyCategory == ESRCelestialBodyCategory::Moon)
		&& TerrainSettings.bUseProceduralTerrain
		&& TerrainSettings.TerrainHeight > KINDA_SMALL_NUMBER;
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
			};

			auto AppendFlatColoredQuad = [&DynamicMesh, NormalOverlay, ColorOverlay](
				const FVector& Point0,
				const FVector& Point1,
				const FVector& Point2,
				const FVector& Point3,
				const FLinearColor& SurfaceColor,
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
				}
				if (Triangle1 >= 0)
				{
					NormalOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(Normal0, Normal3, Normal2));
					ColorOverlay->SetTriangle(Triangle1, UE::Geometry::FIndex3i(Color0, Color3, Color2));
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
					}
					if (BackTriangle1 >= 0)
					{
						NormalOverlay->SetTriangle(BackTriangle1, UE::Geometry::FIndex3i(BackNormal0, BackNormal2, BackNormal3));
						ColorOverlay->SetTriangle(BackTriangle1, UE::Geometry::FIndex3i(BackColor0, BackColor2, BackColor3));
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
				const FLinearColor& SurfaceColor)
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

				const FSRPlanetTerrainSample TerrainSample = SampleSteppedTerrain(CellDirection, TerrainSettings);
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

				AppendFlatColoredQuad(TargetPositions[0], TargetPositions[1], TargetPositions[2], TargetPositions[3], TerrainSample.SurfaceColor);
				RegisterTerrainEdge(SourcePositions[0], SourcePositions[1], TargetPositions[0], TargetPositions[1], TerrainSample.SurfaceColor);
				RegisterTerrainEdge(SourcePositions[1], SourcePositions[2], TargetPositions[1], TargetPositions[2], TerrainSample.SurfaceColor);
				RegisterTerrainEdge(SourcePositions[2], SourcePositions[3], TargetPositions[2], TargetPositions[3], TerrainSample.SurfaceColor);
				RegisterTerrainEdge(SourcePositions[3], SourcePositions[0], TargetPositions[3], TargetPositions[0], TerrainSample.SurfaceColor);
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
					true);
			}

			CelestialBodyDynamicMesh->SetMesh(MoveTemp(DynamicMesh));
			return true;
		}

		UE_LOG(
			LogStarRoversCelestial,
			Warning,
			TEXT("Celestial body '%s' could not recover quad terrain cells from CelestialBodyStaticMesh; falling back to triangle mesh copy."),
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

float ASRCelestialBody::ComputeCelestialBodyDynamicMeshRadius() const
{
	const float CelestialBodyDynamicMeshRadius = ComputeScaledMeshRadius(CelestialBodyStaticMesh.Get(), CelestialBodyDynamicMesh.Get(), BodyScale);
	if (CelestialBodyDynamicMeshRadius <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogStarRoversCelestial, Error, TEXT("Celestial body '%s' requires a valid CelestialBodyDynamicMesh to compute approximate radius."), *GetName());
	}

	return CelestialBodyDynamicMeshRadius;
}

void ASRCelestialBody::LogMissingSpecErrorOnce(const TCHAR* Context) const
{
	if (bHasLoggedMissingSpecError)
	{
		return;
	}

	bHasLoggedMissingSpecError = true;
	UE_LOG(
		LogStarRoversCelestial,
		Error,
		TEXT("%s '%s' requires a data-driven body spec before runtime use. ApplySpec() was never called. Configure it through a data asset-driven spawn path instead of Blueprint defaults."),
		Context ? Context : TEXT("ASRCelestialBody"),
		*GetName());
}

USRCelestialBodyRegistrySubsystem* ASRCelestialBody::FindCelestialRegistry() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<USRCelestialBodyRegistrySubsystem>() : nullptr;
}

USRTimeControlSubsystem* ASRCelestialBody::FindTimeControlSubsystem() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<USRTimeControlSubsystem>() : nullptr;
}

bool ASRCelestialBody::IsStellarBody() const
{
	return BodyCategory == ESRCelestialBodyCategory::Star;
}

float ASRCelestialBody::ResolveSecondsPerPeriod() const
{
	if (const USRTimeControlSubsystem* TimeControlSubsystem = FindTimeControlSubsystem())
	{
		return FMath::Max(0.0f, TimeControlSubsystem->GetSecondsPerPeriod());
	}

	return 20.0f;
}
