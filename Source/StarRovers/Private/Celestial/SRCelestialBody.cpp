#include "Celestial/SRCelestialBody.h"

#include "Async/ParallelFor.h"
#include "Celestial/SRDynamicMeshBaseDataAsset.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/LineBatchComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Gravity/SRGravityParent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Crc.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "Components/SphereComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicMeshOverlay.h"
#include "Engine/StaticMesh.h"
#include "HAL/CriticalSection.h"
#include "HAL/IConsoleManager.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Rendering/StaticMeshVertexBuffer.h"
#include "Misc/ScopeLock.h"
#include "Surface/SRPlanetSurfaceGrid.h"
#include "Surface/SRPlanetSurfaceGridLibrary.h"
#include "Surface/SRPlanetBiomeDataAsset.h"
#include "Surface/SRPlanetTerrainGenerator.h"
#include "Surface/SRPlanetTerrainProfileDataAsset.h"
#include "UDynamicMesh.h"
#include "Utility/SRTimingLog.h"
#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

namespace
{
	constexpr int32 CubeSphereFaceComponentCount = 6;
	constexpr bool bEnableGlobalDynamicMeshRuntimeCache = false;
	constexpr int32 MaxRuntimeDynamicMeshCacheEntries = 16;
	const FName PlanetCenterMaterialParameterName(TEXT("PlanetCenterWS"));

	TAutoConsoleVariable<float> CVarSRDynamicMeshMinecraftSideWallMinHeightStepRatio(
		TEXT("sr.DynamicMesh.MinecraftSideWallMinHeightStepRatio"),
		0.25f,
		TEXT("Minimum Minecraft height-step ratio required to generate an internal terrain side wall. Set 0 to only skip exact same-height edges."));

	TAutoConsoleVariable<int32> CVarSRDynamicMeshBuildBreakdownTimings(
		TEXT("sr.DynamicMesh.BuildBreakdownTimings"),
		1,
		TEXT("Log detailed dynamic mesh build stage timings. Set 0 to disable per-cell timing probes."));

	double SRCelestialNowSeconds()
	{
		return FPlatformTime::Seconds();
	}

	double SRCelestialElapsedMilliseconds(double StartSeconds)
	{
		return (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
	}

	uint64 BuildDynamicMeshColorElementKey(int32 MeshComponentIndex, int32 ElementId)
	{
		return (static_cast<uint64>(static_cast<uint32>(MeshComponentIndex)) << 32)
			| static_cast<uint64>(static_cast<uint32>(ElementId));
	}

	int32 GetCubeSphereFaceComponentIndex(ESRCubeSphereFace Face)
	{
		const int32 FaceIndex = static_cast<int32>(Face);
		return FaceIndex >= 0 && FaceIndex < CubeSphereFaceComponentCount ? FaceIndex : 0;
	}

	UMaterialInterface* GetTerrainBiomeMaterial(const FSRDynamicMeshGeneration& DynamicMeshGeneration, FName BiomeId)
	{
		return DynamicMeshGeneration.GetBiomeMaterial(BiomeId);
	}

	int32 GetTerrainBiomeMaterialId(const FSRDynamicMeshGeneration& DynamicMeshGeneration, FName BiomeId)
	{
		return IsValid(GetTerrainBiomeMaterial(DynamicMeshGeneration, BiomeId))
			? DynamicMeshGeneration.GetBiomeMaterialSlotIndex(BiomeId)
			: 0;
	}

	uint32 HashBiomeDataAssetSettings(uint32 Hash, const USRPlanetBiomeDataAsset* BiomeDataAsset)
	{
		Hash = HashCombine(Hash, PointerHash(BiomeDataAsset));
		if (!IsValid(BiomeDataAsset))
		{
			return Hash;
		}

		Hash = HashCombine(Hash, FCrc::StrCrc32(*BiomeDataAsset->BiomeId.ToString()));
		Hash = HashCombine(Hash, ::GetTypeHash(static_cast<uint8>(BiomeDataAsset->WaterRole)));
		Hash = HashCombine(Hash, ::GetTypeHash(BiomeDataAsset->SpawnWeight));
		Hash = HashCombine(Hash, ::GetTypeHash(BiomeDataAsset->RegionSize));
		Hash = HashCombine(Hash, ::GetTypeHash(BiomeDataAsset->Priority));
		Hash = HashCombine(Hash, ::GetTypeHash(BiomeDataAsset->bCanOverrideLowerPriorityBiomes ? 1 : 0));
		Hash = HashCombine(Hash, ::GetTypeHash(BiomeDataAsset->OverrideMinScore));

		for (const FSRBiomePlacementRule& Rule : BiomeDataAsset->PlacementRules)
		{
			Hash = HashCombine(Hash, ::GetTypeHash(static_cast<uint8>(Rule.Metric)));
			Hash = HashCombine(Hash, ::GetTypeHash(static_cast<uint8>(Rule.Comparison)));
			Hash = HashCombine(Hash, ::GetTypeHash(Rule.Threshold));
			Hash = HashCombine(Hash, ::GetTypeHash(Rule.MaxThreshold));
		}

		return Hash;
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

	struct FSRCelestialBodyDynamicMeshRuntimeCacheEntry
	{
		TArray<UE::Geometry::FDynamicMesh3> FaceDynamicMeshes;
		TArray<FSRPlanetSurfaceGridCell> SurfaceGridCells;
		TArray<FSRCelestialBodyDynamicMeshCellColorData> ColorDataByFlatId;
	};

	struct FSRCelestialBodyBaseSourceMetadataCacheEntry
	{
		int32 FaceResolution = 0;
		TArray<FSRDynamicMeshBaseSourceMetadataCell> Cells;
	};

	TMap<uint32, FSRCelestialBodyDynamicMeshRuntimeCacheEntry> GCelestialBodyDynamicMeshRuntimeCache;
	TWeakObjectPtr<UWorld> GCelestialBodyRuntimeCacheWorld;

	void ClearCelestialBodyRuntimeCaches(const TCHAR* Reason)
	{
		const int32 DynamicMeshCacheEntries = GCelestialBodyDynamicMeshRuntimeCache.Num();
		if (DynamicMeshCacheEntries <= 0)
		{
			return;
		}

		GCelestialBodyDynamicMeshRuntimeCache.Empty();
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Celestial body runtime caches cleared. Reason=%s DynamicMeshEntries=%d"),
			Reason ? Reason : TEXT("Unknown"),
			DynamicMeshCacheEntries);
	}

	FSRCelestialBodyDynamicMeshRuntimeCacheEntry& StoreCelestialBodyDynamicMeshRuntimeCache(
		uint32 BuildHash,
		FSRCelestialBodyDynamicMeshRuntimeCacheEntry&& Entry)
	{
		if (!GCelestialBodyDynamicMeshRuntimeCache.Contains(BuildHash)
			&& GCelestialBodyDynamicMeshRuntimeCache.Num() >= MaxRuntimeDynamicMeshCacheEntries)
		{
			GCelestialBodyDynamicMeshRuntimeCache.Reset();
		}

		FSRCelestialBodyDynamicMeshRuntimeCacheEntry& StoredEntry = GCelestialBodyDynamicMeshRuntimeCache.FindOrAdd(BuildHash);
		StoredEntry = MoveTemp(Entry);
		return StoredEntry;
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

	uint64 BuildSourcePositionEdgeKey(uint32 EndpointA, uint32 EndpointB)
	{
		const uint32 MinEndpoint = FMath::Min(EndpointA, EndpointB);
		const uint32 MaxEndpoint = FMath::Max(EndpointA, EndpointB);
		return (static_cast<uint64>(MinEndpoint) << 32) | static_cast<uint64>(MaxEndpoint);
	}

	TSharedRef<const FSRCelestialBodyBaseSourceMetadataCacheEntry> BuildBaseSourceMetadataCacheEntry(
		int32 FaceResolution,
		const TArray<FSRPlanetSurfaceGridCell>& BaseCells)
	{
		TSharedRef<FSRCelestialBodyBaseSourceMetadataCacheEntry> CacheEntry = MakeShared<FSRCelestialBodyBaseSourceMetadataCacheEntry>();
		CacheEntry->FaceResolution = FaceResolution;
		CacheEntry->Cells.Reserve(BaseCells.Num());

		for (const FSRPlanetSurfaceGridCell& BaseCell : BaseCells)
		{
			FSRDynamicMeshBaseSourceMetadataCell& CellMetadata = CacheEntry->Cells.AddDefaulted_GetRef();
			CellMetadata.CornerHash00 = HashSourcePosition(BaseCell.Corner00);
			CellMetadata.CornerHash10 = HashSourcePosition(BaseCell.Corner10);
			CellMetadata.CornerHash11 = HashSourcePosition(BaseCell.Corner11);
			CellMetadata.CornerHash01 = HashSourcePosition(BaseCell.Corner01);
		}

		return CacheEntry;
	}

	const FSRCelestialBodyBaseSourceMetadataCacheEntry& GetBaseSourceMetadataCacheEntry(
		int32 FaceResolution,
		const TArray<FSRPlanetSurfaceGridCell>& BaseCells)
	{
		static TMap<int32, TSharedPtr<const FSRCelestialBodyBaseSourceMetadataCacheEntry>> CacheByResolution;
		static FCriticalSection CacheCriticalSection;

		const double LockStart = SRCelestialNowSeconds();
		CacheCriticalSection.Lock();
		const double LockWaitMs = SRCelestialElapsedMilliseconds(LockStart);
		if (const TSharedPtr<const FSRCelestialBodyBaseSourceMetadataCacheEntry>* CachedEntry = CacheByResolution.Find(FaceResolution))
		{
			if (CachedEntry->IsValid())
			{
				const FSRCelestialBodyBaseSourceMetadataCacheEntry* Result = CachedEntry->Get();
				CacheCriticalSection.Unlock();
				if (LockWaitMs > 1.0)
				{
					FSRTimingLog::AddLine(FString::Printf(
						TEXT("BaseSourceMetadata CacheHit Resolution=%d LockWait=%.2f ms"),
						FaceResolution,
						LockWaitMs));
				}
				return *Result;
			}
		}

		const double BuildStart = SRCelestialNowSeconds();
		const TSharedRef<const FSRCelestialBodyBaseSourceMetadataCacheEntry> NewEntry =
			BuildBaseSourceMetadataCacheEntry(FaceResolution, BaseCells);
		const double BuildMs = SRCelestialElapsedMilliseconds(BuildStart);
		CacheByResolution.Add(FaceResolution, NewEntry);
		CacheCriticalSection.Unlock();
		FSRTimingLog::AddLine(FString::Printf(
			TEXT("BaseSourceMetadata CacheMissBuild Resolution=%d LockWait=%.2f ms Build=%.2f ms Cells=%d"),
			FaceResolution,
			LockWaitMs,
			BuildMs,
			NewEntry->Cells.Num()));
		return NewEntry.Get();
	}

	struct FSRTerrainVertexKey
	{
		int32 A = 0;
		int32 B = 0;
		int32 C = 0;
		int32 D = 0;

		bool operator==(const FSRTerrainVertexKey& Other) const
		{
			return A == Other.A && B == Other.B && C == Other.C && D == Other.D;
		}
	};

	uint32 GetTypeHash(const FSRTerrainVertexKey& Key)
	{
		uint32 Hash = ::GetTypeHash(Key.A);
		Hash = HashCombine(Hash, ::GetTypeHash(Key.B));
		Hash = HashCombine(Hash, ::GetTypeHash(Key.C));
		Hash = HashCombine(Hash, ::GetTypeHash(Key.D));
		return Hash;
	}

	FSRTerrainVertexKey MakeTerrainVertexKey(const FVector& Position)
	{
		constexpr double PositionQuantizationScale = 100000.0;
		FSRTerrainVertexKey Key;
		Key.A = FMath::RoundToInt(Position.X * PositionQuantizationScale);
		Key.B = FMath::RoundToInt(Position.Y * PositionQuantizationScale);
		Key.C = FMath::RoundToInt(Position.Z * PositionQuantizationScale);
		Key.D = 0;
		return Key;
	}

	FSRTerrainVertexKey MakeTerrainVertexKey(uint32 SourcePositionHash, float HeightOffset)
	{
		constexpr float HeightQuantizationScale = 10000.0f;
		FSRTerrainVertexKey Key;
		Key.A = static_cast<int32>(SourcePositionHash);
		Key.B = FMath::RoundToInt(HeightOffset * HeightQuantizationScale);
		Key.C = 0;
		Key.D = 1;
		return Key;
	}

	int32 CountDynamicMeshBoundaryEdges(const UE::Geometry::FDynamicMesh3& Mesh)
	{
		int32 BoundaryEdgeCount = 0;
		for (const int32 EdgeId : Mesh.EdgeIndicesItr())
		{
			if (Mesh.IsBoundaryEdge(EdgeId))
			{
				++BoundaryEdgeCount;
			}
		}
		return BoundaryEdgeCount;
	}

	float ComputeRegularCubeFaceCellEdgeLength(float SourceRadius, int32 FaceResolution)
	{
		return (2.0f * FMath::Max(1.0f, SourceRadius)) / static_cast<float>(FMath::Max(1, FaceResolution));
	}

	FSRPlanetTerrainSample SampleTerrainForDynamicMesh(
		const FSRBiomeSampleContext& Context,
		const FSRDynamicMeshGeneration& DynamicMeshGeneration,
		float HeightStep)
	{
		FSRPlanetTerrainSample Sample = FSRPlanetTerrainGenerator::SampleTerrain(Context, DynamicMeshGeneration);
		const float SafeDynamicMeshHeight = FMath::Max(0.0f, DynamicMeshGeneration.DynamicMeshHeight);
		if (!DynamicMeshGeneration.bMinecraft
			|| !DynamicMeshGeneration.bDynamicMeshGeneration
			|| SafeDynamicMeshHeight <= KINDA_SMALL_NUMBER
			|| HeightStep <= KINDA_SMALL_NUMBER)
		{
			return Sample;
		}

		Sample.HeightOffset = FMath::RoundToFloat(Sample.HeightOffset / HeightStep) * HeightStep;
		return Sample;
	}

	FSRPlanetTerrainSample SampleTerrainForDynamicMesh(
		const FSRBiomeSampleContext& Context,
		const FSRDynamicMeshGenerationSnapshot& DynamicMeshGeneration,
		float HeightStep)
	{
		FSRPlanetTerrainSample Sample = FSRPlanetTerrainGenerator::SampleTerrain(Context, DynamicMeshGeneration);
		const float SafeDynamicMeshHeight = FMath::Max(0.0f, DynamicMeshGeneration.DynamicMeshHeight);
		if (!DynamicMeshGeneration.bMinecraft
			|| !DynamicMeshGeneration.bDynamicMeshGeneration
			|| SafeDynamicMeshHeight <= KINDA_SMALL_NUMBER
			|| HeightStep <= KINDA_SMALL_NUMBER)
		{
			return Sample;
		}

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
	DynamicMeshGeneration = FSRDynamicMeshGeneration();
	DynamicMeshGeneration.bDynamicMeshGeneration = false;
	DynamicMeshGeneration.DynamicMeshHeight = 0.0f;
	bHasOcean = false;
	OceanMesh = nullptr;
	OceanMaterial = nullptr;
	OceanScaleMultiplier = 1.0f;
	bHasAtmosphere = false;
	AtmosphereMesh = nullptr;
	AtmosphereMaterial = nullptr;
	AtmosphereScaleMultiplier = 1.0f;
}

ASRCelestialBody::ASRCelestialBody()
{
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CelestialBodyDynamicMesh = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("CelestialBodyDynamicMesh"));
	CelestialBodyDynamicMesh->SetupAttachment(SceneRoot);
	CelestialBodyDynamicMesh->SetMobility(EComponentMobility::Movable);
	CelestialBodyDynamicMesh->SetVisibility(false);
	CelestialBodyDynamicMesh->SetHiddenInGame(true);
	CelestialBodyDynamicMeshFaces.Add(CelestialBodyDynamicMesh);
	for (int32 FaceIndex = 1; FaceIndex < CubeSphereFaceComponentCount; ++FaceIndex)
	{
		UDynamicMeshComponent* FaceDynamicMesh = CreateDefaultSubobject<UDynamicMeshComponent>(
			*FString::Printf(TEXT("CelestialBodyDynamicMeshFace%d"), FaceIndex));
		FaceDynamicMesh->SetupAttachment(SceneRoot);
		FaceDynamicMesh->SetMobility(EComponentMobility::Movable);
		FaceDynamicMesh->SetVisibility(false);
		FaceDynamicMesh->SetHiddenInGame(true);
		CelestialBodyDynamicMeshFaces.Add(FaceDynamicMesh);
	}

	CelestialBodyStaticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CelestialBodyStaticMesh"));
	CelestialBodyStaticMesh->SetupAttachment(SceneRoot);

	ClickSphereCollision = CreateDefaultSubobject<USphereComponent>(TEXT("ClickSphereCollision"));
	ClickSphereCollision->SetupAttachment(SceneRoot);
	ClickSphereCollision->SetMobility(EComponentMobility::Movable);
	ClickSphereCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ClickSphereCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	ClickSphereCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ClickSphereCollision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

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
	bRandomizeGenerationSeedEachRun = false;
	TerrainProfileDataAsset = nullptr;
	DynamicMeshGeneration = FSRDynamicMeshGeneration();
	DynamicMeshGeneration.bDynamicMeshGeneration = false;
	DynamicMeshGeneration.DynamicMeshHeight = 0.0f;
	DynamicMeshBaseDataAsset = nullptr;
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

	if (UWorld* World = GetWorld(); World && World->IsGameWorld() && GCelestialBodyRuntimeCacheWorld.Get() != World)
	{
		ClearCelestialBodyRuntimeCaches(TEXT("BeginPlay.NewGameWorld"));
		GCelestialBodyRuntimeCacheWorld = World;
	}

	if (!bHasAppliedData)
	{
		LogMissingDataErrorOnce(TEXT("BeginPlay"));
		return;
	}

	ApplyData();

	if (USRCelestialBodyRegistrySubsystem* CelestialRegistry = FindCelestialRegistry())
	{
		CelestialRegistry->RegisterCelestialBody(this);
	}

}

void ASRCelestialBody::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld(); World && World->IsGameWorld() && GCelestialBodyRuntimeCacheWorld.Get() == World)
	{
		ClearCelestialBodyRuntimeCaches(TEXT("EndPlay.GameWorld"));
		GCelestialBodyRuntimeCacheWorld.Reset();
	}

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
	bRandomizeGenerationSeedEachRun = NewData.bRandomizeGenerationSeedEachRun;
	TerrainProfileDataAsset = NewData.TerrainProfileDataAsset;
	ProfileNaturalStructureSpawnRuleOverrides = NewData.ProfileNaturalStructureSpawnRuleOverrides;
	DynamicMeshGeneration = NewData.DynamicMeshGeneration;
	if (IsValid(TerrainProfileDataAsset.Get()))
	{
		TerrainProfileDataAsset->ApplyToDynamicMeshGeneration(DynamicMeshGeneration);
	}
	else if (BodyCategory == ESRCelestialBodyCategory::Planet || BodyCategory == ESRCelestialBodyCategory::Moon)
	{
		UE_LOG(LogTemp, Error, TEXT("Celestial body '%s' requires TerrainProfileDataAsset for procedural terrain."), *GetName());
	}
	Scale = NewData.Scale;
	StaticMesh = NewData.StaticMesh;
	DynamicMeshBaseDataAsset = NewData.DynamicMeshBaseDataAsset;
	Material = NewData.Material;
	Mass = NewData.Mass;
	GravityRatio = NewData.GravityRatio;
	GravityRadiusRatio = NewData.GravityRadiusRatio;
	ShowGravityLine = NewData.ShowGravityLine;
	GravityLineColor = NewData.GravityLineColor;
	GravityLineOpacity = NewData.GravityLineOpacity;
	GravityLineSegments = NewData.GravityLineSegments;
	GravityLineThickness = NewData.GravityLineThickness;
	if (HasActorBegunPlay()
		&& GetWorld()
		&& GetWorld()->IsGameWorld()
		&& (IsValid(StaticMesh) || IsValid(DynamicMeshBaseDataAsset))
		&& IsValid(Material))
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
		for (UDynamicMeshComponent* DynamicMeshComponent : CelestialBodyDynamicMeshFaces)
		{
			if (!IsValid(DynamicMeshComponent))
			{
				continue;
			}

			DynamicMeshComponent->SetRelativeLocation(FVector::ZeroVector);
			DynamicMeshComponent->SetRelativeRotation(FRotator::ZeroRotator);
			DynamicMeshComponent->SetRelativeScale3D(FVector(Scale));
		}
	}
	if (IsValid(CelestialBodyStaticMesh.Get()))
	{
		CelestialBodyStaticMesh->SetRelativeLocation(FVector::ZeroVector);
		CelestialBodyStaticMesh->SetRelativeRotation(FRotator::ZeroRotator);
		CelestialBodyStaticMesh->SetRelativeScale3D(FVector(Scale));
	}

	if (bHasCachedDynamicMeshBuildHash && CachedDynamicMeshBuildHash != ComputeDynamicMeshBuildHash())
	{
		ResetDynamicMeshCellColorData();
	}

	const bool bIsGameWorld = GetWorld() && GetWorld()->IsGameWorld();
	const bool bShouldBuildDynamicMesh = !bIsGameWorld || (IsValid(CelestialBodyDynamicMesh.Get()) && CelestialBodyDynamicMesh->IsVisible());
	EnsureCelestialBodyDynamicMeshVisuals(bShouldBuildDynamicMesh);

	if (IsValid(ClickSphereCollision))
	{
		ClickSphereCollision->SetRelativeLocation(FVector::ZeroVector);
		ClickSphereCollision->SetRelativeRotation(FRotator::ZeroRotator);
		ClickSphereCollision->SetRelativeScale3D(FVector::OneVector);
		const float BodyRadius = IsValid(StaticMesh.Get())
			? StaticMesh->GetBounds().SphereRadius * Scale
			: IsValid(DynamicMeshBaseDataAsset.Get())
				? DynamicMeshBaseDataAsset->GetSafeBaseRadius() * Scale
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
	CurrentData.bRandomizeGenerationSeedEachRun = bRandomizeGenerationSeedEachRun;
	CurrentData.TerrainProfileDataAsset = TerrainProfileDataAsset;
	CurrentData.ProfileNaturalStructureSpawnRuleOverrides = ProfileNaturalStructureSpawnRuleOverrides;
	CurrentData.DynamicMeshGeneration = DynamicMeshGeneration;
	CurrentData.Scale = Scale;
	CurrentData.StaticMesh = StaticMesh;
	CurrentData.DynamicMeshBaseDataAsset = DynamicMeshBaseDataAsset;
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

void ASRCelestialBody::RefreshMaterialParameters()
{
	const FVector PlanetCenterWS = GetActorLocation();
	if (UMaterialInstanceDynamic* ActiveDynamicMaterial = GetActiveBodyDynamicMaterial())
	{
		ActiveDynamicMaterial->SetVectorParameterValue(PlanetCenterMaterialParameterName, FLinearColor(PlanetCenterWS));
	}
	if (IsValid(CelestialBodyStaticMesh.Get()))
	{
		if (UMaterialInstanceDynamic* StaticDynamicMaterial = Cast<UMaterialInstanceDynamic>(CelestialBodyStaticMesh->GetMaterial(0)))
		{
			StaticDynamicMaterial->SetVectorParameterValue(PlanetCenterMaterialParameterName, FLinearColor(PlanetCenterWS));
		}
	}
}

void ASRCelestialBody::RefreshRotationAxisLineVisual()
{
}

void ASRCelestialBody::SetCelestialBodyMesh(bool bUseDynamicMesh)
{
	if (bUseDynamicMesh && !HasCelestialBodyDynamicMeshBuild())
	{
		bUseDynamicMesh = false;
	}

	bool bDynamicMeshAlreadyVisible = true;
	for (UDynamicMeshComponent* DynamicMeshComponent : CelestialBodyDynamicMeshFaces)
	{
		if (IsValid(DynamicMeshComponent) && DynamicMeshComponent->IsVisible() != bUseDynamicMesh)
		{
			bDynamicMeshAlreadyVisible = false;
			break;
		}
	}
	const bool bStaticMeshAlreadyVisible = IsValid(CelestialBodyStaticMesh.Get())
		&& CelestialBodyStaticMesh->IsVisible() != bUseDynamicMesh;
	if (bDynamicMeshAlreadyVisible && bStaticMeshAlreadyVisible)
	{
		return;
	}

	for (UDynamicMeshComponent* DynamicMeshComponent : CelestialBodyDynamicMeshFaces)
	{
		if (!IsValid(DynamicMeshComponent))
		{
			continue;
		}

		if (DynamicMeshComponent->IsVisible() != bUseDynamicMesh)
		{
			DynamicMeshComponent->SetVisibility(bUseDynamicMesh);
		}
		DynamicMeshComponent->SetHiddenInGame(!bUseDynamicMesh);
	}

	if (IsValid(CelestialBodyStaticMesh.Get()))
	{
		if (CelestialBodyStaticMesh->IsVisible() == bUseDynamicMesh)
		{
			CelestialBodyStaticMesh->SetVisibility(!bUseDynamicMesh);
		}
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

bool ASRCelestialBody::HasSurfaceCellRenderData(const FSRPlanetSurfaceGridCellId& CellId) const
{
	return FindDynamicMeshCellColorData(CellId) != nullptr;
}

const FSRCelestialBodyDynamicMeshCellColorData* ASRCelestialBody::FindDynamicMeshCellColorData(const FSRPlanetSurfaceGridCellId& CellId) const
{
	if (!IsValid(DynamicMeshBaseDataAsset.Get()))
	{
		return nullptr;
	}

	const int32 FaceResolution = DynamicMeshBaseDataAsset->GetClampedFaceResolution();
	if (!CellId.IsValid(FaceResolution))
	{
		return nullptr;
	}

	const int32 FlatIndex = ((static_cast<int32>(CellId.Face) * FaceResolution) + CellId.CellY) * FaceResolution + CellId.CellX;
	return DynamicMeshColorDataByFlatId.IsValidIndex(FlatIndex) ? &DynamicMeshColorDataByFlatId[FlatIndex] : nullptr;
}

bool ASRCelestialBody::GetCachedSurfaceGridCells(TArray<FSRPlanetSurfaceGridCell>& OutCells) const
{
	if (!bHasCachedDynamicMeshBuildHash || CachedSurfaceGridCells.IsEmpty())
	{
		OutCells.Reset();
		return false;
	}

	OutCells = CachedSurfaceGridCells;
	return true;
}

bool ASRCelestialBody::PrepareCelestialBodyDynamicMesh()
{
	if (HasCelestialBodyDynamicMeshBuild())
	{
		return true;
	}

	EnsureCelestialBodyDynamicMeshVisuals(true);
	return HasCelestialBodyDynamicMeshBuild();
}

bool ASRCelestialBody::HasCelestialBodyDynamicMeshBuild() const
{
	return bHasCachedDynamicMeshBuildHash;
}

void ASRCelestialBody::AppendRuntimeMemoryDiagnostics(TArray<FString>& OutLines)
{
	OutLines.Add(FString::Printf(
		TEXT("CelestialRuntimeCache DynamicMeshEntries=%d CacheWorld=%s"),
		GCelestialBodyDynamicMeshRuntimeCache.Num(),
		*GetNameSafe(GCelestialBodyRuntimeCacheWorld.Get())));
}

bool ASRCelestialBody::ApplySurfaceCellHighlights(
	const FSRPlanetSurfaceGridCellId& HoveredCellId,
	bool bHasHoveredCell,
	const FSRPlanetSurfaceGridCellId& SelectedCellId,
	bool bHasSelectedCell,
	const FLinearColor& HoveredCellColor,
	const FLinearColor& SelectedCellColor)
{
	if (!IsValid(CelestialBodyDynamicMesh.Get()) || DynamicMeshColorDataByFlatId.IsEmpty())
	{
		return false;
	}

	TMap<uint64, FLinearColor> TargetColorsByElement;
	TMap<uint64, FLinearColor> NextHighlightedBaseColorByElement;
	auto BlendHighlightColor = [](const FLinearColor& BaseColor, const FLinearColor& HighlightColor)
	{
		constexpr float HighlightIntensity = 0.45f;
		return FLinearColor(
			FMath::Clamp(BaseColor.R + (HighlightColor.R * HighlightIntensity), 0.0f, 1.0f),
			FMath::Clamp(BaseColor.G + (HighlightColor.G * HighlightIntensity), 0.0f, 1.0f),
			FMath::Clamp(BaseColor.B + (HighlightColor.B * HighlightIntensity), 0.0f, 1.0f),
			BaseColor.A);
	};
	auto AddCellHighlight = [this, &TargetColorsByElement, &NextHighlightedBaseColorByElement, &BlendHighlightColor](const FSRPlanetSurfaceGridCellId& CellId, const FLinearColor& HighlightColor)
	{
		const FSRCelestialBodyDynamicMeshCellColorData* CellColorData = FindDynamicMeshCellColorData(CellId);
		if (!CellColorData)
		{
			return;
		}

		auto AddElements = [&TargetColorsByElement, &NextHighlightedBaseColorByElement, &HighlightColor, &BlendHighlightColor](const auto& Elements)
		{
			for (const FSRCelestialBodyDynamicMeshColorElement& Element : Elements)
			{
				if (Element.MeshComponentIndex != INDEX_NONE && Element.ElementId != INDEX_NONE)
				{
					const uint64 ElementKey = BuildDynamicMeshColorElementKey(Element.MeshComponentIndex, Element.ElementId);
					TargetColorsByElement.Add(
						ElementKey,
						BlendHighlightColor(Element.BaseColor, HighlightColor));
					NextHighlightedBaseColorByElement.Add(ElementKey, Element.BaseColor);
				}
			}
		};

		AddElements(CellColorData->SurfaceColorElements);
		AddElements(CellColorData->SideColorElements);
	};

	if (bHasHoveredCell)
	{
		AddCellHighlight(HoveredCellId, HoveredCellColor);
	}
	if (bHasSelectedCell)
	{
		AddCellHighlight(SelectedCellId, SelectedCellColor);
	}

	TSet<uint64> NextHighlightedElements;
	for (const TPair<uint64, FLinearColor>& TargetColorPair : TargetColorsByElement)
	{
		NextHighlightedElements.Add(TargetColorPair.Key);
	}
	bool bHasAnyColorChange = false;
	for (const uint64 PreviousElementKey : HighlightedDynamicMeshColorElements)
	{
		if (!NextHighlightedElements.Contains(PreviousElementKey))
		{
			bHasAnyColorChange = true;
			break;
		}
	}
	if (!bHasAnyColorChange)
	{
		for (const uint64 NextElementKey : NextHighlightedElements)
		{
			if (!HighlightedDynamicMeshColorElements.Contains(NextElementKey))
			{
				bHasAnyColorChange = true;
				break;
			}
		}
	}
	if (!bHasAnyColorChange && NextHighlightedElements.IsEmpty())
	{
		return true;
	}

	TMap<int32, TMap<int32, FLinearColor>> TargetColorsByMesh;
	for (const TPair<uint64, FLinearColor>& TargetColorPair : TargetColorsByElement)
	{
		const int32 MeshComponentIndex = static_cast<int32>(TargetColorPair.Key >> 32);
		const int32 ElementId = static_cast<int32>(TargetColorPair.Key & 0xffffffff);
		TargetColorsByMesh.FindOrAdd(MeshComponentIndex).Add(ElementId, TargetColorPair.Value);
	}

	TMap<int32, TSet<int32>> NextHighlightedElementsByMesh;
	for (const uint64 NextElementKey : NextHighlightedElements)
	{
		const int32 MeshComponentIndex = static_cast<int32>(NextElementKey >> 32);
		const int32 ElementId = static_cast<int32>(NextElementKey & 0xffffffff);
		NextHighlightedElementsByMesh.FindOrAdd(MeshComponentIndex).Add(ElementId);
	}

	TSet<int32> MeshIndicesToEdit;
	for (const uint64 PreviousElementKey : HighlightedDynamicMeshColorElements)
	{
		MeshIndicesToEdit.Add(static_cast<int32>(PreviousElementKey >> 32));
	}
	for (const uint64 NextElementKey : NextHighlightedElements)
	{
		MeshIndicesToEdit.Add(static_cast<int32>(NextElementKey >> 32));
	}

	for (const int32 MeshComponentIndex : MeshIndicesToEdit)
	{
		UDynamicMeshComponent* DynamicMeshComponent = GetDynamicMeshFaceComponent(MeshComponentIndex);
		if (!IsValid(DynamicMeshComponent))
		{
			continue;
		}

		UDynamicMesh* DynamicMeshObject = DynamicMeshComponent->GetDynamicMesh();
		if (!IsValid(DynamicMeshObject))
		{
			continue;
		}

		const TMap<int32, FLinearColor>* MeshTargetColors = TargetColorsByMesh.Find(MeshComponentIndex);
		const TSet<int32>* MeshNextHighlightedElements = NextHighlightedElementsByMesh.Find(MeshComponentIndex);
		DynamicMeshObject->EditMesh(
			[this, MeshComponentIndex, MeshTargetColors, MeshNextHighlightedElements](UE::Geometry::FDynamicMesh3& Mesh)
			{
				if (!Mesh.HasAttributes())
				{
					return;
				}

				auto* ColorOverlay = Mesh.Attributes()->PrimaryColors();
				auto ToVectorColor = [](const FLinearColor& Color)
				{
					return FVector4f(Color.R, Color.G, Color.B, Color.A);
				};

				if (ColorOverlay)
				{
					for (const uint64 PreviousElementKey : HighlightedDynamicMeshColorElements)
					{
						if (static_cast<int32>(PreviousElementKey >> 32) != MeshComponentIndex)
						{
							continue;
						}

						const int32 PreviousElementId = static_cast<int32>(PreviousElementKey & 0xffffffff);
						if (MeshNextHighlightedElements && MeshNextHighlightedElements->Contains(PreviousElementId))
						{
							continue;
						}

						if (const FLinearColor* BaseColor = HighlightedDynamicMeshBaseColorByElement.Find(PreviousElementKey))
						{
							ColorOverlay->SetElement(PreviousElementId, ToVectorColor(*BaseColor));
						}
					}

					if (MeshTargetColors)
					{
						for (const TPair<int32, FLinearColor>& TargetColorPair : *MeshTargetColors)
						{
							ColorOverlay->SetElement(TargetColorPair.Key, ToVectorColor(TargetColorPair.Value));
						}
					}
				}
			},
			EDynamicMeshChangeType::DeformationEdit,
			EDynamicMeshAttributeChangeFlags::VertexColors,
			false);
	}

	HighlightedDynamicMeshColorElements = MoveTemp(NextHighlightedElements);
	HighlightedDynamicMeshBaseColorByElement = MoveTemp(NextHighlightedBaseColorByElement);
	return !TargetColorsByElement.IsEmpty() || bHasAnyColorChange;
}

void ASRCelestialBody::ClearSurfaceCellHighlights()
{
	if (HighlightedDynamicMeshColorElements.IsEmpty())
	{
		HighlightedDynamicMeshColorElements.Reset();
		HighlightedDynamicMeshBaseColorByElement.Reset();
		return;
	}

	TMap<int32, TArray<uint64>> HighlightedElementsByMesh;
	for (const uint64 ElementKey : HighlightedDynamicMeshColorElements)
	{
		HighlightedElementsByMesh.FindOrAdd(static_cast<int32>(ElementKey >> 32)).Add(ElementKey);
	}

	for (const TPair<int32, TArray<uint64>>& HighlightedMeshPair : HighlightedElementsByMesh)
	{
		UDynamicMeshComponent* DynamicMeshComponent = GetDynamicMeshFaceComponent(HighlightedMeshPair.Key);
		if (!IsValid(DynamicMeshComponent))
		{
			continue;
		}

		UDynamicMesh* DynamicMeshObject = DynamicMeshComponent->GetDynamicMesh();
		if (!IsValid(DynamicMeshObject))
		{
			continue;
		}

		const TArray<uint64>& MeshHighlightedElements = HighlightedMeshPair.Value;
		DynamicMeshObject->EditMesh(
			[this, &MeshHighlightedElements](UE::Geometry::FDynamicMesh3& Mesh)
			{
				if (!Mesh.HasAttributes())
				{
					return;
				}

				auto* ColorOverlay = Mesh.Attributes()->PrimaryColors();
				if (ColorOverlay)
				{
					for (const uint64 ElementKey : MeshHighlightedElements)
					{
						const int32 ElementId = static_cast<int32>(ElementKey & 0xffffffff);
						if (const FLinearColor* BaseColor = HighlightedDynamicMeshBaseColorByElement.Find(ElementKey))
						{
							ColorOverlay->SetElement(ElementId, FVector4f(BaseColor->R, BaseColor->G, BaseColor->B, BaseColor->A));
						}
					}
				}
			},
			EDynamicMeshChangeType::DeformationEdit,
			EDynamicMeshAttributeChangeFlags::VertexColors,
			false);
	}

	HighlightedDynamicMeshColorElements.Reset();
	HighlightedDynamicMeshBaseColorByElement.Reset();
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

void ASRCelestialBody::EnsureCelestialBodyDynamicMeshVisuals(bool bBuildDynamicMesh)
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
	if (!IsValid(DesiredMesh) && !IsValid(DynamicMeshBaseDataAsset.Get()))
	{
		UE_LOG(LogStarRoversCelestial, Error, TEXT("Celestial body '%s' requires StaticMesh or DynamicMeshBaseDataAsset."), *GetName());
		return;
	}

	if (IsValid(CelestialBodyStaticMesh.Get()) && CelestialBodyStaticMesh->GetStaticMesh() != DesiredMesh)
	{
		CelestialBodyStaticMesh->SetStaticMesh(DesiredMesh);
	}

	if (bBuildDynamicMesh)
	{
		BuildCelestialBodyDynamicMesh();
	}

	SyncDynamicMeshFaceComponentSettings();

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

	UMaterialInstanceDynamic* ActiveDynamicMaterial = GetActiveBodyDynamicMaterial();
	const UMaterialInstance* ActiveMaterialInstance = ActiveDynamicMaterial;
	if (!IsValid(ActiveDynamicMaterial) || ActiveMaterialInstance->Parent != DesiredBaseMaterial)
	{
		ActiveDynamicMaterial = UMaterialInstanceDynamic::Create(DesiredBaseMaterial, this);
	}

	if (IsValid(CelestialBodyStaticMesh.Get()))
	{
		CelestialBodyStaticMesh->SetMaterial(0, IsValid(ActiveDynamicMaterial) ? ActiveDynamicMaterial : DesiredBaseMaterial);
	}

	for (UDynamicMeshComponent* DynamicMeshComponent : CelestialBodyDynamicMeshFaces)
	{
		if (!IsValid(DynamicMeshComponent))
		{
			continue;
		}

		DynamicMeshComponent->SetMaterial(0, IsValid(ActiveDynamicMaterial) ? ActiveDynamicMaterial : DesiredBaseMaterial);
	}

	for (const FSRBiomeMaterialEntry& BiomeMaterialEntry : DynamicMeshGeneration.BiomeMaterials)
	{
		UMaterialInterface* BiomeMaterial = BiomeMaterialEntry.Material.Get();
		const int32 MaterialSlotIndex = DynamicMeshGeneration.GetBiomeMaterialSlotIndex(BiomeMaterialEntry.BiomeId);
		if (IsValid(BiomeMaterial) && MaterialSlotIndex > 0)
		{
			for (UDynamicMeshComponent* DynamicMeshComponent : CelestialBodyDynamicMeshFaces)
			{
				if (IsValid(DynamicMeshComponent))
				{
					DynamicMeshComponent->SetMaterial(MaterialSlotIndex, BiomeMaterial);
				}
			}
		}
	}

	RefreshMaterialParameters();
}

bool ASRCelestialBody::BuildPreparedCelestialBodyDynamicMesh(FSRCelestialBodyPreparedDynamicMesh& OutPreparedMesh)
{
	OutPreparedMesh = FSRCelestialBodyPreparedDynamicMesh();
	const double TotalStart = SRCelestialNowSeconds();
	double PreBuildValidationMs = 0.0;
	double PreBuildMeshSetupMs = 0.0;
	double PreBuildContainerReserveMs = 0.0;
	double PreBuildEdgeReserveMs = 0.0;
	double PreBuildSetupTotalMs = 0.0;
	double PreBuildStageStart = SRCelestialNowSeconds();
	const uint32 DynamicMeshBuildHash = ComputeDynamicMeshBuildHash();

	if (!IsValid(DynamicMeshBaseDataAsset.Get()))
	{
		return false;
	}

	if (DynamicMeshBaseDataAsset->BaseShape != ESRDynamicMeshBaseShape::CubeSphere)
	{
		UE_LOG(LogStarRoversCelestial, Warning, TEXT("Celestial body '%s' has unsupported DynamicMeshBase shape."), *GetName());
		return false;
	}

	const int32 FaceResolution = DynamicMeshBaseDataAsset->GetClampedFaceResolution();
	const float SourceBodyRadius = DynamicMeshBaseDataAsset->GetSafeBaseRadius(IsValid(StaticMesh.Get()) ? StaticMesh->GetBounds().SphereRadius : 1.0f);
	const int32 CellCount = CubeSphereFaceComponentCount * FaceResolution * FaceResolution;
	const float TerrainHeightStep = ComputeRegularCubeFaceCellEdgeLength(SourceBodyRadius, FaceResolution);
	PreBuildValidationMs = SRCelestialElapsedMilliseconds(PreBuildStageStart);
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("DynamicMesh '%s' BaseMetadata Begin Source='%s' Shape=CubeSphere Resolution=%d Cells=%d Radius=%.3f"),
		*GetName(),
		*GetNameSafe(DynamicMeshBaseDataAsset.Get()),
		FaceResolution,
		CellCount,
		SourceBodyRadius));

	PreBuildStageStart = SRCelestialNowSeconds();
	TArray<UE::Geometry::FDynamicMesh3> FaceDynamicMeshes;
	FaceDynamicMeshes.SetNum(CubeSphereFaceComponentCount);
	for (UE::Geometry::FDynamicMesh3& FaceDynamicMesh : FaceDynamicMeshes)
	{
		FaceDynamicMesh.EnableAttributes();
		FaceDynamicMesh.Attributes()->EnablePrimaryColors();
		FaceDynamicMesh.Attributes()->EnableMaterialID();
	}
	PreBuildMeshSetupMs = SRCelestialElapsedMilliseconds(PreBuildStageStart);

	PreBuildStageStart = SRCelestialNowSeconds();
	TMap<FSRTerrainVertexKey, int32> WeldedVertexIds;
	WeldedVertexIds.Reserve((FaceResolution + 1) * (FaceResolution + 1) * CubeSphereFaceComponentCount);
	TArray<int32> CachedCellIndexByFlatId;
	CachedCellIndexByFlatId.Init(INDEX_NONE, CellCount);
	TArray<FSRCelestialBodyDynamicMeshCellColorData> PreparedColorDataByFlatId;
	PreparedColorDataByFlatId.SetNum(CellCount);
	TArray<FSRPlanetSurfaceGridCell> PreparedSurfaceGridCells;
	PreparedSurfaceGridCells.SetNum(CellCount);
	PreBuildContainerReserveMs = SRCelestialElapsedMilliseconds(PreBuildStageStart);

	auto AppendFlatColoredQuad = [this, &FaceDynamicMeshes, &WeldedVertexIds](
		int32 MeshComponentIndex,
		const FVector& Point0,
		const FVector& Point1,
		const FVector& Point2,
		const FVector& Point3,
		const FLinearColor& SurfaceColor,
		const int32 MaterialId,
		bool bDoubleSided = false,
		const FSRTerrainVertexKey* VertexKeys = nullptr,
		const FVector* NormalReferenceDirectionOverride = nullptr)
	{
		FSRCelestialBodyDynamicMeshQuadRenderData RenderData;
		MeshComponentIndex = 0;
		UE::Geometry::FDynamicMesh3& TargetDynamicMesh = FaceDynamicMeshes[MeshComponentIndex];
		UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay = TargetDynamicMesh.Attributes()->PrimaryNormals();
		auto* ColorOverlay = TargetDynamicMesh.Attributes()->PrimaryColors();
		auto* MaterialIdAttribute = TargetDynamicMesh.Attributes()->GetMaterialID();
		if (!NormalOverlay || !ColorOverlay)
		{
			return RenderData;
		}

		FVector QuadPoints[4] = { Point0, Point1, Point2, Point3 };
		FSRTerrainVertexKey ResolvedVertexKeys[4];
		if (VertexKeys)
		{
			for (int32 CornerIndex = 0; CornerIndex < 4; ++CornerIndex)
			{
				ResolvedVertexKeys[CornerIndex] = VertexKeys[CornerIndex];
			}
		}

		const FVector QuadCenter = (Point0 + Point1 + Point2 + Point3) * 0.25f;
		const FVector OutwardDirection = NormalReferenceDirectionOverride
			? NormalReferenceDirectionOverride->GetSafeNormal()
			: QuadCenter.GetSafeNormal();
		FVector QuadNormal = FVector::CrossProduct(QuadPoints[1] - QuadPoints[0], QuadPoints[2] - QuadPoints[0]).GetSafeNormal();
		if (!OutwardDirection.IsNearlyZero() && FVector::DotProduct(QuadNormal, OutwardDirection) < 0.0f)
		{
			Swap(QuadPoints[1], QuadPoints[3]);
			if (VertexKeys)
			{
				Swap(ResolvedVertexKeys[1], ResolvedVertexKeys[3]);
			}
			QuadNormal *= -1.0f;
		}
		if (QuadNormal.IsNearlyZero())
		{
			QuadNormal = OutwardDirection.IsNearlyZero() ? FVector::UpVector : OutwardDirection;
		}

		auto FindOrAppendVertex = [&TargetDynamicMesh, &WeldedVertexIds](const FVector& Position, const FSRTerrainVertexKey* VertexKey)
		{
			const FSRTerrainVertexKey ResolvedVertexKey = VertexKey ? *VertexKey : MakeTerrainVertexKey(Position);
			if (const int32* ExistingVertexId = WeldedVertexIds.Find(ResolvedVertexKey))
			{
				return *ExistingVertexId;
			}

			const int32 NewVertexId = TargetDynamicMesh.AppendVertex(FVector3d(Position));
			WeldedVertexIds.Add(ResolvedVertexKey, NewVertexId);
			return NewVertexId;
		};

		const int32 Vertex0 = FindOrAppendVertex(QuadPoints[0], VertexKeys ? &ResolvedVertexKeys[0] : nullptr);
		const int32 Vertex1 = FindOrAppendVertex(QuadPoints[1], VertexKeys ? &ResolvedVertexKeys[1] : nullptr);
		const int32 Vertex2 = FindOrAppendVertex(QuadPoints[2], VertexKeys ? &ResolvedVertexKeys[2] : nullptr);
		const int32 Vertex3 = FindOrAppendVertex(QuadPoints[3], VertexKeys ? &ResolvedVertexKeys[3] : nullptr);

		const int32 Normal0 = NormalOverlay->AppendElement(FVector3f(QuadNormal));
		const int32 Normal1 = NormalOverlay->AppendElement(FVector3f(QuadNormal));
		const int32 Normal2 = NormalOverlay->AppendElement(FVector3f(QuadNormal));
		const int32 Normal3 = NormalOverlay->AppendElement(FVector3f(QuadNormal));
		const int32 Color0 = ColorOverlay->AppendElement(FVector4f(SurfaceColor.R, SurfaceColor.G, SurfaceColor.B, SurfaceColor.A));
		const int32 Color1 = ColorOverlay->AppendElement(FVector4f(SurfaceColor.R, SurfaceColor.G, SurfaceColor.B, SurfaceColor.A));
		const int32 Color2 = ColorOverlay->AppendElement(FVector4f(SurfaceColor.R, SurfaceColor.G, SurfaceColor.B, SurfaceColor.A));
		const int32 Color3 = ColorOverlay->AppendElement(FVector4f(SurfaceColor.R, SurfaceColor.G, SurfaceColor.B, SurfaceColor.A));

		auto TrackColorElement = [&RenderData, &SurfaceColor, MeshComponentIndex](int32 ColorElementId)
		{
			if (ColorElementId == INDEX_NONE)
			{
				return;
			}

			FSRCelestialBodyDynamicMeshColorElement ColorElement;
			ColorElement.MeshComponentIndex = MeshComponentIndex;
			ColorElement.ElementId = ColorElementId;
			ColorElement.BaseColor = SurfaceColor;
			RenderData.ColorElements.Add(ColorElement);
		};
		TrackColorElement(Color0);
		TrackColorElement(Color1);
		TrackColorElement(Color2);
		TrackColorElement(Color3);

		const int32 Triangle0 = TargetDynamicMesh.AppendTriangle(Vertex0, Vertex2, Vertex1);
		const int32 Triangle1 = TargetDynamicMesh.AppendTriangle(Vertex0, Vertex3, Vertex2);
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
			const FVector BackNormal = -QuadNormal;
			const int32 BackNormal0 = NormalOverlay->AppendElement(FVector3f(BackNormal));
			const int32 BackNormal1 = NormalOverlay->AppendElement(FVector3f(BackNormal));
			const int32 BackNormal2 = NormalOverlay->AppendElement(FVector3f(BackNormal));
			const int32 BackNormal3 = NormalOverlay->AppendElement(FVector3f(BackNormal));
			const int32 BackColor0 = ColorOverlay->AppendElement(FVector4f(SurfaceColor.R, SurfaceColor.G, SurfaceColor.B, SurfaceColor.A));
			const int32 BackColor1 = ColorOverlay->AppendElement(FVector4f(SurfaceColor.R, SurfaceColor.G, SurfaceColor.B, SurfaceColor.A));
			const int32 BackColor2 = ColorOverlay->AppendElement(FVector4f(SurfaceColor.R, SurfaceColor.G, SurfaceColor.B, SurfaceColor.A));
			const int32 BackColor3 = ColorOverlay->AppendElement(FVector4f(SurfaceColor.R, SurfaceColor.G, SurfaceColor.B, SurfaceColor.A));
			TrackColorElement(BackColor0);
			TrackColorElement(BackColor1);
			TrackColorElement(BackColor2);
			TrackColorElement(BackColor3);

			const int32 BackTriangle0 = TargetDynamicMesh.AppendTriangle(Vertex0, Vertex1, Vertex2);
			const int32 BackTriangle1 = TargetDynamicMesh.AppendTriangle(Vertex0, Vertex2, Vertex3);
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

		return RenderData;
	};

	auto GetPreparedSurfaceGridCellIndex = [&CachedCellIndexByFlatId, FaceResolution](const FSRPlanetSurfaceGridCellId& CellId) -> int32
	{
		if (!CellId.IsValid(FaceResolution))
		{
			return INDEX_NONE;
		}

		const int32 FlatIndex = ((static_cast<int32>(CellId.Face) * FaceResolution) + CellId.CellY) * FaceResolution + CellId.CellX;
		return CachedCellIndexByFlatId.IsValidIndex(FlatIndex) ? CachedCellIndexByFlatId[FlatIndex] : INDEX_NONE;
	};

	auto AddCachedSideLineSegment = [this, &GetPreparedSurfaceGridCellIndex, &PreparedSurfaceGridCells](
		const FSRPlanetSurfaceGridCellId& CellId,
		const FSRPlanetSurfaceGridCellId& AdjacentCellId,
		bool bHasAdjacentCell,
		const FVector& PointA,
		const FVector& PointB)
	{
		const int32 CellIndex = GetPreparedSurfaceGridCellIndex(CellId);
		if (!PreparedSurfaceGridCells.IsValidIndex(CellIndex) || FVector::DistSquared(PointA, PointB) <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		FSRPlanetSurfaceGridLineSegment SideLineSegment;
		SideLineSegment.LocalPointA = PointA * Scale;
		SideLineSegment.LocalPointB = PointB * Scale;
		SideLineSegment.bHasAdjacentCell = bHasAdjacentCell;
		SideLineSegment.AdjacentCellId = AdjacentCellId;
		PreparedSurfaceGridCells[CellIndex].SideLineSegments.Add(SideLineSegment);
	};

	auto AddCachedSideFace = [this, &GetPreparedSurfaceGridCellIndex, &PreparedSurfaceGridCells](
		const FSRPlanetSurfaceGridCellId& CellId,
		const FSRPlanetSurfaceGridCellId& AdjacentCellId,
		bool bHasAdjacentCell,
		const FVector& Point0,
		const FVector& Point1,
		const FVector& Point2,
		const FVector& Point3)
	{
		const int32 CellIndex = GetPreparedSurfaceGridCellIndex(CellId);
		if (!PreparedSurfaceGridCells.IsValidIndex(CellIndex))
		{
			return;
		}

		FSRPlanetSurfaceGridSideFace SideFace;
		SideFace.LocalPoint0 = Point0 * Scale;
		SideFace.LocalPoint1 = Point1 * Scale;
		SideFace.LocalPoint2 = Point2 * Scale;
		SideFace.LocalPoint3 = Point3 * Scale;
		SideFace.bHasAdjacentCell = bHasAdjacentCell;
		SideFace.AdjacentCellId = AdjacentCellId;
		PreparedSurfaceGridCells[CellIndex].SideFaces.Add(SideFace);
	};

	auto AddCachedSideWallOutline = [&AddCachedSideLineSegment](
		const FSRPlanetSurfaceGridCellId& CellId,
		const FSRPlanetSurfaceGridCellId& AdjacentCellId,
		bool bHasAdjacentCell,
		const FVector& Point0,
		const FVector& Point1,
		const FVector& Point2,
		const FVector& Point3)
	{
		AddCachedSideLineSegment(CellId, AdjacentCellId, bHasAdjacentCell, Point0, Point1);
		AddCachedSideLineSegment(CellId, AdjacentCellId, bHasAdjacentCell, Point1, Point2);
		AddCachedSideLineSegment(CellId, AdjacentCellId, bHasAdjacentCell, Point2, Point3);
		AddCachedSideLineSegment(CellId, AdjacentCellId, bHasAdjacentCell, Point3, Point0);
	};

	struct FSRGeneratedBaseTerrainEdge
	{
		FVector PointA = FVector::ZeroVector;
		FVector PointB = FVector::ZeroVector;
		FVector CellCenter = FVector::ZeroVector;
		uint32 SourceHashA = 0;
		uint32 SourceHashB = 0;
		float HeightOffset = 0.0f;
		FLinearColor SurfaceColor = FLinearColor::White;
		int32 MaterialId = 0;
		FSRPlanetSurfaceGridCellId CellId;
	};

	TMap<uint64, FSRGeneratedBaseTerrainEdge> PendingTerrainEdges;
	const int32 PendingTerrainEdgeReserveCount = FMath::Min(CellCount * 2, 8192);
	PreBuildStageStart = SRCelestialNowSeconds();
	PendingTerrainEdges.Reserve(PendingTerrainEdgeReserveCount);
	PreBuildEdgeReserveMs = SRCelestialElapsedMilliseconds(PreBuildStageStart);
	int32 TerrainEdgeRegisterCount = 0;
	int32 TerrainEdgeMatchCount = 0;
	int32 TerrainSideWallCount = 0;
	int32 MaxPendingTerrainEdgeCount = 0;

	auto RegisterTerrainEdge = [
		this,
		&PendingTerrainEdges,
		&AppendFlatColoredQuad,
		&AddCachedSideWallOutline,
		&AddCachedSideFace,
		&PreparedColorDataByFlatId,
		FaceResolution,
		TerrainHeightStep,
		&TerrainEdgeRegisterCount,
		&TerrainEdgeMatchCount,
		&TerrainSideWallCount,
		&MaxPendingTerrainEdgeCount](
		uint32 EndpointHashA,
		uint32 EndpointHashB,
		const FVector& PointA,
		const FVector& PointB,
		const FVector& CellCenter,
		float HeightOffset,
		const FLinearColor& SurfaceColor,
		const int32 MaterialId,
		const FSRPlanetSurfaceGridCellId& CellId)
	{
		++TerrainEdgeRegisterCount;
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
		if (FSRGeneratedBaseTerrainEdge* ExistingEdge = PendingTerrainEdges.Find(EdgeKey))
		{
			++TerrainEdgeMatchCount;
			const float MinMinecraftWallHeight =
				DynamicMeshGeneration.bMinecraft && TerrainHeightStep > KINDA_SMALL_NUMBER
					? TerrainHeightStep * FMath::Max(0.0f, CVarSRDynamicMeshMinecraftSideWallMinHeightStepRatio.GetValueOnGameThread())
					: KINDA_SMALL_NUMBER;
			const bool bSameEdgePosition =
				!DynamicMeshGeneration.bMinecraft
				&& FVector::DistSquared(ExistingEdge->PointA, OrderedPointA) <= KINDA_SMALL_NUMBER
				&& FVector::DistSquared(ExistingEdge->PointB, OrderedPointB) <= KINDA_SMALL_NUMBER;
			const bool bSameMinecraftStep =
				DynamicMeshGeneration.bMinecraft
				&& FMath::Abs(HeightOffset - ExistingEdge->HeightOffset) <= MinMinecraftWallHeight;
			if (!bSameEdgePosition && !bSameMinecraftStep)
			{
				++TerrainSideWallCount;
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
				const FSRCelestialBodyDynamicMeshQuadRenderData SideRenderData = AppendFlatColoredQuad(
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
				AddCachedSideWallOutline(ExistingEdge->CellId, CellId, true, ExistingEdge->PointA, ExistingEdge->PointB, OrderedPointB, OrderedPointA);
				AddCachedSideWallOutline(CellId, ExistingEdge->CellId, true, ExistingEdge->PointA, ExistingEdge->PointB, OrderedPointB, OrderedPointA);
				if (bExistingCellIsHigher)
				{
					AddCachedSideFace(ExistingEdge->CellId, CellId, true, ExistingEdge->PointA, ExistingEdge->PointB, OrderedPointB, OrderedPointA);
					const int32 ExistingCellFlatIndex = ((static_cast<int32>(ExistingEdge->CellId.Face) * FaceResolution) + ExistingEdge->CellId.CellY) * FaceResolution + ExistingEdge->CellId.CellX;
					if (PreparedColorDataByFlatId.IsValidIndex(ExistingCellFlatIndex))
					{
						PreparedColorDataByFlatId[ExistingCellFlatIndex].SideColorElements.Append(SideRenderData.ColorElements);
					}
				}
				else if (bCurrentCellIsHigher)
				{
					AddCachedSideFace(CellId, ExistingEdge->CellId, true, ExistingEdge->PointA, ExistingEdge->PointB, OrderedPointB, OrderedPointA);
					const int32 CellFlatIndex = ((static_cast<int32>(CellId.Face) * FaceResolution) + CellId.CellY) * FaceResolution + CellId.CellX;
					if (PreparedColorDataByFlatId.IsValidIndex(CellFlatIndex))
					{
						PreparedColorDataByFlatId[CellFlatIndex].SideColorElements.Append(SideRenderData.ColorElements);
					}
				}
			}
			PendingTerrainEdges.Remove(EdgeKey);
			return;
		}

		FSRGeneratedBaseTerrainEdge& NewEdge = PendingTerrainEdges.Add(EdgeKey);
		NewEdge.PointA = OrderedPointA;
		NewEdge.PointB = OrderedPointB;
		NewEdge.CellCenter = CellCenter;
		NewEdge.SourceHashA = OrderedHashA;
		NewEdge.SourceHashB = OrderedHashB;
		NewEdge.HeightOffset = HeightOffset;
		NewEdge.SurfaceColor = SurfaceColor;
		NewEdge.MaterialId = MaterialId;
		NewEdge.CellId = CellId;
		MaxPendingTerrainEdgeCount = FMath::Max(MaxPendingTerrainEdgeCount, PendingTerrainEdges.Num());
	};

	double StageStart = SRCelestialNowSeconds();
	TMap<FName, int32> BiomeMaterialSlotIndexById;
	BiomeMaterialSlotIndexById.Reserve(DynamicMeshGeneration.BiomeMaterials.Num());
	for (int32 MaterialIndex = 0; MaterialIndex < DynamicMeshGeneration.BiomeMaterials.Num(); ++MaterialIndex)
	{
		const FSRBiomeMaterialEntry& BiomeMaterialEntry = DynamicMeshGeneration.BiomeMaterials[MaterialIndex];
		if (!BiomeMaterialEntry.BiomeId.IsNone() && IsValid(BiomeMaterialEntry.Material.Get()))
		{
			BiomeMaterialSlotIndexById.Add(BiomeMaterialEntry.BiomeId, MaterialIndex + 1);
		}
	}
	const double BiomeMaterialMapMs = SRCelestialElapsedMilliseconds(StageStart);

	StageStart = SRCelestialNowSeconds();
	const FSRDynamicMeshGenerationSnapshot DynamicMeshGenerationSnapshot = DynamicMeshGeneration.MakeThreadSafeSnapshot();
	const double SnapshotMs = SRCelestialElapsedMilliseconds(StageStart);

	StageStart = SRCelestialNowSeconds();
	TArray<FSRPlanetSurfaceGridCell> GeneratedBaseCells;
	const TArray<FSRDynamicMeshBasePrecomputedCell>* PrecomputedBaseCells =
		DynamicMeshBaseDataAsset->GetValidPrecomputedCells();
	const bool bUsingPrecomputedBaseCells = PrecomputedBaseCells != nullptr;
	const float PrecomputedBaseCellScale =
		bUsingPrecomputedBaseCells ? DynamicMeshBaseDataAsset->GetPrecomputedCellScale(SourceBodyRadius) : 1.0f;
	if (!bUsingPrecomputedBaseCells)
	{
		GeneratedBaseCells = USRPlanetSurfaceGridLibrary::GenerateCubeSphereCells(FaceResolution, SourceBodyRadius);
	}
	const double BaseCellsMs = SRCelestialElapsedMilliseconds(StageStart);

	StageStart = SRCelestialNowSeconds();
	const TArray<FSRDynamicMeshBaseSourceMetadataCell>* BaseSourceMetadataCells =
		bUsingPrecomputedBaseCells ? DynamicMeshBaseDataAsset->GetValidPrecomputedSourceMetadata() : nullptr;
	const bool bUsingPrecomputedSourceMetadata = BaseSourceMetadataCells != nullptr;
	if (!BaseSourceMetadataCells && !bUsingPrecomputedBaseCells)
	{
		BaseSourceMetadataCells = &GetBaseSourceMetadataCacheEntry(FaceResolution, GeneratedBaseCells).Cells;
	}
	const double BaseSourceMetadataMs = SRCelestialElapsedMilliseconds(StageStart);

	const bool bProfileBuildBreakdown = CVarSRDynamicMeshBuildBreakdownTimings.GetValueOnGameThread() != 0;
	double TerrainSampleMs = 0.0;
	double CellTransformMs = 0.0;
	double SourceHashMs = 0.0;
	double SurfaceAppendMs = 0.0;
	double ColorDataMs = 0.0;
	double CacheCellMs = 0.0;
	double TerrainEdgeRegisterMs = 0.0;

	PreBuildSetupTotalMs = SRCelestialElapsedMilliseconds(TotalStart);
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("DynamicMesh '%s' BaseMetadata.PreBuildBreakdown Validation=%.2f ms MeshSetup=%.2f ms ContainerReserve=%.2f ms EdgeReserve=%.2f ms BiomeMap=%.2f ms Snapshot=%.2f ms BaseGrid=%.2f ms BaseSourceMeta=%.2f ms PreBuildTotal=%.2f ms"),
		*GetName(),
		PreBuildValidationMs,
		PreBuildMeshSetupMs,
		PreBuildContainerReserveMs,
		PreBuildEdgeReserveMs,
		BiomeMaterialMapMs,
		SnapshotMs,
		BaseCellsMs,
		BaseSourceMetadataMs,
		PreBuildSetupTotalMs));

	const double BuildCellsStart = SRCelestialNowSeconds();
	int32 ValidCellCount = 0;

	struct FSRDynamicMeshBaseCellView
	{
		FSRPlanetSurfaceGridCellId CellId;
		FVector LocalCenter = FVector::ZeroVector;
		FVector LocalNormal = FVector::UpVector;
		FVector Corner00 = FVector::ZeroVector;
		FVector Corner10 = FVector::ZeroVector;
		FVector Corner11 = FVector::ZeroVector;
		FVector Corner01 = FVector::ZeroVector;
		FVector2D FaceUVMin = FVector2D::ZeroVector;
		FVector2D FaceUVMax = FVector2D::ZeroVector;
		float ApproxSurfaceArea = 0.0f;
		FSRPlanetSurfaceGridCellNeighbors Neighbors;
	};

	auto MakeBaseCellView = [
		bUsingPrecomputedBaseCells,
		PrecomputedBaseCells,
		PrecomputedBaseCellScale,
		&GeneratedBaseCells](int32 BaseCellIndex)
	{
		FSRDynamicMeshBaseCellView CellView;
		if (bUsingPrecomputedBaseCells)
		{
			const FSRDynamicMeshBasePrecomputedCell& PrecomputedCell = (*PrecomputedBaseCells)[BaseCellIndex];
			CellView.CellId = PrecomputedCell.CellId;
			CellView.LocalCenter = PrecomputedCell.LocalCenter * PrecomputedBaseCellScale;
			CellView.LocalNormal = PrecomputedCell.LocalNormal;
			CellView.Corner00 = PrecomputedCell.Corner00 * PrecomputedBaseCellScale;
			CellView.Corner10 = PrecomputedCell.Corner10 * PrecomputedBaseCellScale;
			CellView.Corner11 = PrecomputedCell.Corner11 * PrecomputedBaseCellScale;
			CellView.Corner01 = PrecomputedCell.Corner01 * PrecomputedBaseCellScale;
			CellView.FaceUVMin = PrecomputedCell.FaceUVMin;
			CellView.FaceUVMax = PrecomputedCell.FaceUVMax;
			CellView.ApproxSurfaceArea = PrecomputedCell.ApproxSurfaceArea * PrecomputedBaseCellScale * PrecomputedBaseCellScale;
			CellView.Neighbors = PrecomputedCell.Neighbors;
			return CellView;
		}

		const FSRPlanetSurfaceGridCell& GeneratedCell = GeneratedBaseCells[BaseCellIndex];
		CellView.CellId = GeneratedCell.CellId;
		CellView.LocalCenter = GeneratedCell.LocalCenter;
		CellView.LocalNormal = GeneratedCell.LocalNormal;
		CellView.Corner00 = GeneratedCell.Corner00;
		CellView.Corner10 = GeneratedCell.Corner10;
		CellView.Corner11 = GeneratedCell.Corner11;
		CellView.Corner01 = GeneratedCell.Corner01;
		CellView.FaceUVMin = GeneratedCell.FaceUVMin;
		CellView.FaceUVMax = GeneratedCell.FaceUVMax;
		CellView.ApproxSurfaceArea = GeneratedCell.ApproxSurfaceArea;
		CellView.Neighbors = GeneratedCell.Neighbors;
		return CellView;
	};

	const int32 BaseCellCount = bUsingPrecomputedBaseCells ? PrecomputedBaseCells->Num() : GeneratedBaseCells.Num();
	for (int32 BaseCellIndex = 0; BaseCellIndex < BaseCellCount; ++BaseCellIndex)
	{
		const FSRDynamicMeshBaseCellView BaseCell = MakeBaseCellView(BaseCellIndex);
		const FSRPlanetSurfaceGridCellId CellId = BaseCell.CellId;
		if (!CellId.IsValid(FaceResolution))
		{
			continue;
		}
		const int32 CellFlatIndex = ((static_cast<int32>(CellId.Face) * FaceResolution) + CellId.CellY) * FaceResolution + CellId.CellX;
		if (!PreparedSurfaceGridCells.IsValidIndex(CellFlatIndex) || !CachedCellIndexByFlatId.IsValidIndex(CellFlatIndex))
		{
			continue;
		}

		const FVector CellDirection = BaseCell.LocalCenter.GetSafeNormal();
		if (CellDirection.IsNearlyZero())
		{
			continue;
		}

		FSRBiomeSampleContext TerrainSampleContext;
		TerrainSampleContext.LocalUnitDirection = CellDirection;
		TerrainSampleContext.Face = CellId.Face;
		TerrainSampleContext.CellX = CellId.CellX;
		TerrainSampleContext.CellY = CellId.CellY;
		TerrainSampleContext.FaceResolution = FaceResolution;
		TerrainSampleContext.FaceUV = (BaseCell.FaceUVMin + BaseCell.FaceUVMax) * 0.5f;

		FSRPlanetTerrainSample TerrainSample;
		if (bProfileBuildBreakdown)
		{
			const double InnerStart = SRCelestialNowSeconds();
			TerrainSample = SampleTerrainForDynamicMesh(TerrainSampleContext, DynamicMeshGenerationSnapshot, TerrainHeightStep);
			TerrainSampleMs += SRCelestialElapsedMilliseconds(InnerStart);
		}
		else
		{
			TerrainSample = SampleTerrainForDynamicMesh(TerrainSampleContext, DynamicMeshGenerationSnapshot, TerrainHeightStep);
		}

		FVector TargetPositions[4];
		uint32 SourcePositionHashes[4];
		FVector CellNormal;
		float SourceCellRadius = 0.0f;
		float CellScale = 1.0f;
		FVector TargetCellCenter = FVector::ZeroVector;
		bool bSwappedCellWinding = false;
		{
			const double InnerStart = bProfileBuildBreakdown ? SRCelestialNowSeconds() : 0.0;
			SourceCellRadius = FMath::Max(BaseCell.LocalCenter.Length(), 1.0f);
			CellScale = FMath::Max(0.01f, (SourceCellRadius + TerrainSample.HeightOffset) / SourceCellRadius);
			TargetCellCenter = CellDirection * (SourceCellRadius + TerrainSample.HeightOffset);
			TargetPositions[0] = BaseCell.Corner00 * CellScale;
			TargetPositions[1] = BaseCell.Corner10 * CellScale;
			TargetPositions[2] = BaseCell.Corner11 * CellScale;
			TargetPositions[3] = BaseCell.Corner01 * CellScale;

			CellNormal = FVector::CrossProduct(TargetPositions[1] - TargetPositions[0], TargetPositions[2] - TargetPositions[0]).GetSafeNormal();
			if (FVector::DotProduct(CellNormal, CellDirection) < 0.0f)
			{
				Swap(TargetPositions[1], TargetPositions[3]);
				CellNormal *= -1.0f;
				bSwappedCellWinding = true;
			}
			if (CellNormal.IsNearlyZero())
			{
				CellNormal = CellDirection;
			}
			if (bProfileBuildBreakdown)
			{
				CellTransformMs += SRCelestialElapsedMilliseconds(InnerStart);
			}
		}

		if (bProfileBuildBreakdown)
		{
			const double InnerStart = SRCelestialNowSeconds();
			if (BaseSourceMetadataCells && BaseSourceMetadataCells->IsValidIndex(BaseCellIndex))
			{
				const FSRDynamicMeshBaseSourceMetadataCell& SourceMetadata = (*BaseSourceMetadataCells)[BaseCellIndex];
				SourcePositionHashes[0] = SourceMetadata.CornerHash00;
				SourcePositionHashes[1] = SourceMetadata.CornerHash10;
				SourcePositionHashes[2] = SourceMetadata.CornerHash11;
				SourcePositionHashes[3] = SourceMetadata.CornerHash01;
			}
			else
			{
				SourcePositionHashes[0] = HashSourcePosition(BaseCell.Corner00);
				SourcePositionHashes[1] = HashSourcePosition(BaseCell.Corner10);
				SourcePositionHashes[2] = HashSourcePosition(BaseCell.Corner11);
				SourcePositionHashes[3] = HashSourcePosition(BaseCell.Corner01);
			}
			SourceHashMs += SRCelestialElapsedMilliseconds(InnerStart);
		}
		else
		{
			if (BaseSourceMetadataCells && BaseSourceMetadataCells->IsValidIndex(BaseCellIndex))
			{
				const FSRDynamicMeshBaseSourceMetadataCell& SourceMetadata = (*BaseSourceMetadataCells)[BaseCellIndex];
				SourcePositionHashes[0] = SourceMetadata.CornerHash00;
				SourcePositionHashes[1] = SourceMetadata.CornerHash10;
				SourcePositionHashes[2] = SourceMetadata.CornerHash11;
				SourcePositionHashes[3] = SourceMetadata.CornerHash01;
			}
			else
			{
				SourcePositionHashes[0] = HashSourcePosition(BaseCell.Corner00);
				SourcePositionHashes[1] = HashSourcePosition(BaseCell.Corner10);
				SourcePositionHashes[2] = HashSourcePosition(BaseCell.Corner11);
				SourcePositionHashes[3] = HashSourcePosition(BaseCell.Corner01);
			}
		}
		if (bSwappedCellWinding)
		{
			Swap(SourcePositionHashes[1], SourcePositionHashes[3]);
		}

		FSRTerrainVertexKey SurfaceVertexKeys[4];
		for (int32 CornerIndex = 0; CornerIndex < 4; ++CornerIndex)
		{
			SurfaceVertexKeys[CornerIndex] = MakeTerrainVertexKey(SourcePositionHashes[CornerIndex], TerrainSample.HeightOffset);
		}

		const int32 MaterialId = BiomeMaterialSlotIndexById.FindRef(TerrainSample.BiomeId);
		FSRCelestialBodyDynamicMeshQuadRenderData SurfaceRenderData;
		if (bProfileBuildBreakdown)
		{
			const double InnerStart = SRCelestialNowSeconds();
			SurfaceRenderData = AppendFlatColoredQuad(GetCubeSphereFaceComponentIndex(CellId.Face), TargetPositions[0], TargetPositions[1], TargetPositions[2], TargetPositions[3], TerrainSample.SurfaceColor, MaterialId, false, SurfaceVertexKeys);
			SurfaceAppendMs += SRCelestialElapsedMilliseconds(InnerStart);
		}
		else
		{
			SurfaceRenderData = AppendFlatColoredQuad(GetCubeSphereFaceComponentIndex(CellId.Face), TargetPositions[0], TargetPositions[1], TargetPositions[2], TargetPositions[3], TerrainSample.SurfaceColor, MaterialId, false, SurfaceVertexKeys);
		}

		if (bProfileBuildBreakdown)
		{
			const double InnerStart = SRCelestialNowSeconds();
			if (PreparedColorDataByFlatId.IsValidIndex(CellFlatIndex))
			{
				PreparedColorDataByFlatId[CellFlatIndex].SurfaceColorElements.Append(SurfaceRenderData.ColorElements);
			}
			ColorDataMs += SRCelestialElapsedMilliseconds(InnerStart);
		}
		else
		{
			if (PreparedColorDataByFlatId.IsValidIndex(CellFlatIndex))
			{
				PreparedColorDataByFlatId[CellFlatIndex].SurfaceColorElements.Append(SurfaceRenderData.ColorElements);
			}
		}

		{
			const double InnerStart = bProfileBuildBreakdown ? SRCelestialNowSeconds() : 0.0;
			const int32 CachedCellIndex = CellFlatIndex;
			FSRPlanetSurfaceGridCell& CachedCell = PreparedSurfaceGridCells[CachedCellIndex];
			CachedCell.CellId = CellId;
			CachedCell.FaceUVMin = BaseCell.FaceUVMin;
			CachedCell.FaceUVMax = BaseCell.FaceUVMax;
			CachedCell.LocalCenter = TargetCellCenter * Scale;
			CachedCell.LocalNormal = CellNormal;
			CachedCell.Corner00 = TargetPositions[0] * Scale;
			CachedCell.Corner10 = TargetPositions[1] * Scale;
			CachedCell.Corner11 = TargetPositions[2] * Scale;
			CachedCell.Corner01 = TargetPositions[3] * Scale;
			CachedCell.ApproxSurfaceArea =
				(FVector::CrossProduct(CachedCell.Corner10 - CachedCell.Corner00, CachedCell.Corner11 - CachedCell.Corner00).Size() * 0.5f)
				+ (FVector::CrossProduct(CachedCell.Corner11 - CachedCell.Corner00, CachedCell.Corner01 - CachedCell.Corner00).Size() * 0.5f);
			CachedCell.Biome = TerrainSample.Biome;
			CachedCell.BiomeId = TerrainSample.BiomeId;
			CachedCell.WaterRole = TerrainSample.WaterRole;
			CachedCell.Neighbors = BaseCell.Neighbors;

			CachedCellIndexByFlatId[CellFlatIndex] = CachedCellIndex;
			if (bProfileBuildBreakdown)
			{
				CacheCellMs += SRCelestialElapsedMilliseconds(InnerStart);
			}
		}

		if (bProfileBuildBreakdown)
		{
			const double InnerStart = SRCelestialNowSeconds();
			RegisterTerrainEdge(SourcePositionHashes[0], SourcePositionHashes[1], TargetPositions[0], TargetPositions[1], TargetCellCenter, TerrainSample.HeightOffset, TerrainSample.SurfaceColor, MaterialId, CellId);
			RegisterTerrainEdge(SourcePositionHashes[1], SourcePositionHashes[2], TargetPositions[1], TargetPositions[2], TargetCellCenter, TerrainSample.HeightOffset, TerrainSample.SurfaceColor, MaterialId, CellId);
			RegisterTerrainEdge(SourcePositionHashes[2], SourcePositionHashes[3], TargetPositions[2], TargetPositions[3], TargetCellCenter, TerrainSample.HeightOffset, TerrainSample.SurfaceColor, MaterialId, CellId);
			RegisterTerrainEdge(SourcePositionHashes[3], SourcePositionHashes[0], TargetPositions[3], TargetPositions[0], TargetCellCenter, TerrainSample.HeightOffset, TerrainSample.SurfaceColor, MaterialId, CellId);
			TerrainEdgeRegisterMs += SRCelestialElapsedMilliseconds(InnerStart);
		}
		else
		{
			RegisterTerrainEdge(SourcePositionHashes[0], SourcePositionHashes[1], TargetPositions[0], TargetPositions[1], TargetCellCenter, TerrainSample.HeightOffset, TerrainSample.SurfaceColor, MaterialId, CellId);
			RegisterTerrainEdge(SourcePositionHashes[1], SourcePositionHashes[2], TargetPositions[1], TargetPositions[2], TargetCellCenter, TerrainSample.HeightOffset, TerrainSample.SurfaceColor, MaterialId, CellId);
			RegisterTerrainEdge(SourcePositionHashes[2], SourcePositionHashes[3], TargetPositions[2], TargetPositions[3], TargetCellCenter, TerrainSample.HeightOffset, TerrainSample.SurfaceColor, MaterialId, CellId);
			RegisterTerrainEdge(SourcePositionHashes[3], SourcePositionHashes[0], TargetPositions[3], TargetPositions[0], TargetCellCenter, TerrainSample.HeightOffset, TerrainSample.SurfaceColor, MaterialId, CellId);
		}
		++ValidCellCount;
	}
	if (ValidCellCount != PreparedSurfaceGridCells.Num())
	{
		TArray<FSRPlanetSurfaceGridCell> CompactedSurfaceGridCells;
		CompactedSurfaceGridCells.Reserve(ValidCellCount);
		TArray<int32> CompactedCellIndexByFlatId;
		CompactedCellIndexByFlatId.Init(INDEX_NONE, CachedCellIndexByFlatId.Num());
		for (int32 FlatIndex = 0; FlatIndex < CachedCellIndexByFlatId.Num(); ++FlatIndex)
		{
			const int32 ExistingCellIndex = CachedCellIndexByFlatId[FlatIndex];
			if (!PreparedSurfaceGridCells.IsValidIndex(ExistingCellIndex))
			{
				continue;
			}

			const int32 CompactedCellIndex = CompactedSurfaceGridCells.Add(MoveTemp(PreparedSurfaceGridCells[ExistingCellIndex]));
			CompactedCellIndexByFlatId[FlatIndex] = CompactedCellIndex;
		}

		PreparedSurfaceGridCells = MoveTemp(CompactedSurfaceGridCells);
		CachedCellIndexByFlatId = MoveTemp(CompactedCellIndexByFlatId);
	}
	const double CellLoopMs = SRCelestialElapsedMilliseconds(BuildCellsStart);

	FSRTimingLog::AddLine(FString::Printf(
		TEXT("DynamicMesh '%s' BaseMetadata.BuildCells %.2f ms BaseCells=%d Cells=%d EdgeRegisters=%d EdgeMatches=%d SideWalls=%d PendingEdges=%d MaxPendingEdges=%d PendingReserve=%d"),
		*GetName(),
		BaseCellsMs + BaseSourceMetadataMs + CellLoopMs,
		GeneratedBaseCells.Num(),
		ValidCellCount,
		TerrainEdgeRegisterCount,
		TerrainEdgeMatchCount,
		TerrainSideWallCount,
		PendingTerrainEdges.Num(),
		MaxPendingTerrainEdgeCount,
		PendingTerrainEdgeReserveCount));
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("DynamicMesh '%s' BaseMetadata.BuildBreakdown Profile=%s BaseGridSource=%s BaseSourceMetaSource=%s BiomeMap=%.2f ms Snapshot=%.2f ms BaseGrid=%.2f ms BaseSourceMeta=%.2f ms CellLoop=%.2f ms TerrainSample=%.2f ms Transform=%.2f ms SourceHash=%.2f ms SurfaceAppend=%.2f ms ColorData=%.2f ms CacheCell=%.2f ms EdgeRegister=%.2f ms"),
		*GetName(),
		bProfileBuildBreakdown ? TEXT("true") : TEXT("false"),
		bUsingPrecomputedBaseCells ? TEXT("Precomputed") : TEXT("Generated"),
		bUsingPrecomputedSourceMetadata ? TEXT("Precomputed") : TEXT("Generated"),
		BiomeMaterialMapMs,
		SnapshotMs,
		BaseCellsMs,
		BaseSourceMetadataMs,
		CellLoopMs,
		TerrainSampleMs,
		CellTransformMs,
		SourceHashMs,
		SurfaceAppendMs,
		ColorDataMs,
		CacheCellMs,
		TerrainEdgeRegisterMs));

	const double PostBuildStart = SRCelestialNowSeconds();
	double PendingEdgesCheckMs = 0.0;
	double WeldedMeshCheckMs = 0.0;
	double PreparedMoveMs = 0.0;

	double PostBuildStageStart = SRCelestialNowSeconds();
	if (!PendingTerrainEdges.IsEmpty())
	{
		UE_LOG(
			LogStarRoversCelestial,
			Warning,
			TEXT("Dynamic mesh '%s' code-generated base has %d unmatched source edges."),
			*GetName(),
			PendingTerrainEdges.Num());
	}
	PendingEdgesCheckMs = SRCelestialElapsedMilliseconds(PostBuildStageStart);

	PostBuildStageStart = SRCelestialNowSeconds();
	int32 WeldedBoundaryEdgeCount = 0;
	int32 NonEmptyDynamicMeshCount = 0;
	for (const UE::Geometry::FDynamicMesh3& FaceDynamicMesh : FaceDynamicMeshes)
	{
		if (FaceDynamicMesh.TriangleCount() <= 0)
		{
			continue;
		}

		++NonEmptyDynamicMeshCount;
		WeldedBoundaryEdgeCount += CountDynamicMeshBoundaryEdges(FaceDynamicMesh);
	}
	if (WeldedBoundaryEdgeCount > 0)
	{
		UE_LOG(
			LogStarRoversCelestial,
			Warning,
			TEXT("Dynamic mesh '%s' generated with %d open boundary edges after code-generated base welding."),
			*GetName(),
			WeldedBoundaryEdgeCount);
	}
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("DynamicMesh '%s' BaseMetadata.WeldedMeshCheck BoundaryEdges=%d Meshes=%d Vertices=%d Triangles=%d"),
		*GetName(),
		WeldedBoundaryEdgeCount,
		NonEmptyDynamicMeshCount,
		FaceDynamicMeshes[0].VertexCount(),
		FaceDynamicMeshes[0].TriangleCount()));
	WeldedMeshCheckMs = SRCelestialElapsedMilliseconds(PostBuildStageStart);

	PostBuildStageStart = SRCelestialNowSeconds();
	OutPreparedMesh.bValid = true;
	OutPreparedMesh.BuildHash = DynamicMeshBuildHash;
	OutPreparedMesh.FaceDynamicMeshes = MoveTemp(FaceDynamicMeshes);
	OutPreparedMesh.SurfaceGridCells = MoveTemp(PreparedSurfaceGridCells);
	OutPreparedMesh.CellIndexByFlatId = MoveTemp(CachedCellIndexByFlatId);
	OutPreparedMesh.ColorDataByFlatId = MoveTemp(PreparedColorDataByFlatId);
	PreparedMoveMs = SRCelestialElapsedMilliseconds(PostBuildStageStart);
	OutPreparedMesh.BuildMilliseconds = SRCelestialElapsedMilliseconds(TotalStart);
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("DynamicMesh '%s' BaseMetadata.PostBuildBreakdown PendingEdges=%.2f ms WeldedMeshCheck=%.2f ms PreparedMove=%.2f ms PostBuildTotal=%.2f ms"),
		*GetName(),
		PendingEdgesCheckMs,
		WeldedMeshCheckMs,
		PreparedMoveMs,
		SRCelestialElapsedMilliseconds(PostBuildStart)));
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("DynamicMesh '%s' BaseMetadata.Prepared %.2f ms"),
		*GetName(),
		OutPreparedMesh.BuildMilliseconds));
	return true;
}

bool ASRCelestialBody::ApplyPreparedCelestialBodyDynamicMesh(FSRCelestialBodyPreparedDynamicMesh&& PreparedMesh, double TotalStart)
{
	if (!PreparedMesh.bValid)
	{
		return false;
	}

	ResetDynamicMeshCellColorData();
	DynamicMeshColorDataByFlatId = MoveTemp(PreparedMesh.ColorDataByFlatId);
	CachedSurfaceGridCells = MoveTemp(PreparedMesh.SurfaceGridCells);

	double StageStart = SRCelestialNowSeconds();
	for (int32 FaceIndex = 0; FaceIndex < PreparedMesh.FaceDynamicMeshes.Num(); ++FaceIndex)
	{
		if (UDynamicMeshComponent* FaceDynamicMeshComponent = GetDynamicMeshFaceComponent(FaceIndex))
		{
			FaceDynamicMeshComponent->SetMesh(MoveTemp(PreparedMesh.FaceDynamicMeshes[FaceIndex]));
		}
	}
	const double SetMeshMs = SRCelestialElapsedMilliseconds(StageStart);

	double SurfaceGridApplyMs = 0.0;
	if (USRPlanetSurfaceGrid* SurfaceGrid = GetSurfaceGrid())
	{
		StageStart = SRCelestialNowSeconds();
		TArray<FSRPlanetSurfaceGridCell> GeneratedGridCells = CachedSurfaceGridCells;
		UE::Geometry::FDynamicMesh3 GeneratedGridMesh;
		GeneratedGridMesh.EnableAttributes();
		GeneratedGridMesh.Attributes()->EnablePrimaryColors();
		SurfaceGrid->ApplyGeneratedGridBuild(MoveTemp(GeneratedGridCells), MoveTemp(GeneratedGridMesh), MoveTemp(PreparedMesh.CellIndexByFlatId));
		SurfaceGridApplyMs = SRCelestialElapsedMilliseconds(StageStart);
	}

	CachedDynamicMeshBuildHash = PreparedMesh.BuildHash;
	bHasCachedDynamicMeshBuildHash = true;
	FSRTimingLog::AddLine(FString::Printf(
		TEXT("DynamicMesh '%s' BaseMetadata.Total %.2f ms Build=%.2f ms RuntimeCache=0.00 ms SetMesh=%.2f ms SurfaceGrid=%.2f ms"),
		*GetName(),
		SRCelestialElapsedMilliseconds(TotalStart),
		PreparedMesh.BuildMilliseconds,
		SetMeshMs,
		SurfaceGridApplyMs));
	return true;
}

bool ASRCelestialBody::BuildDynamicMeshFromBaseMetadata(uint32 DynamicMeshBuildHash, double TotalStart)
{
	(void)DynamicMeshBuildHash;
	FSRCelestialBodyPreparedDynamicMesh PreparedMesh;
	if (!BuildPreparedCelestialBodyDynamicMesh(PreparedMesh))
	{
		return false;
	}
	return ApplyPreparedCelestialBodyDynamicMesh(MoveTemp(PreparedMesh), TotalStart);
}

bool ASRCelestialBody::BuildCelestialBodyDynamicMesh()
{
	FSRTimingLogSession TimingLogSession(FString::Printf(TEXT("DynamicMesh '%s'"), *GetName()));
	const double TotalStart = SRCelestialNowSeconds();
	const bool bHasMeshSource = IsValid(StaticMesh.Get()) || IsValid(DynamicMeshBaseDataAsset.Get());
	if (!IsValid(CelestialBodyDynamicMesh.Get()) || !bHasMeshSource)
	{
		return false;
	}

	const uint32 DynamicMeshBuildHash = ComputeDynamicMeshBuildHash();
	if (bHasCachedDynamicMeshBuildHash && CachedDynamicMeshBuildHash == DynamicMeshBuildHash)
	{
		FSRTimingLog::AddLine(FString::Printf(TEXT("DynamicMesh '%s' AlreadyBuilt %.2f ms"), *GetName(), SRCelestialElapsedMilliseconds(TotalStart)));
		return true;
	}

	auto ApplyRuntimeCacheEntry = [this, DynamicMeshBuildHash](const FSRCelestialBodyDynamicMeshRuntimeCacheEntry& CacheEntry)
	{
		const double ApplyStart = SRCelestialNowSeconds();
		ResetDynamicMeshCellColorData();

		DynamicMeshColorDataByFlatId = CacheEntry.ColorDataByFlatId;
		CachedSurfaceGridCells = CacheEntry.SurfaceGridCells;

		for (int32 FaceIndex = 0; FaceIndex < CubeSphereFaceComponentCount; ++FaceIndex)
		{
			if (UDynamicMeshComponent* FaceDynamicMeshComponent = GetDynamicMeshFaceComponent(FaceIndex))
			{
				UE::Geometry::FDynamicMesh3 MeshCopy;
				if (CacheEntry.FaceDynamicMeshes.IsValidIndex(FaceIndex))
				{
					MeshCopy = CacheEntry.FaceDynamicMeshes[FaceIndex];
				}
				else
				{
					MeshCopy.EnableAttributes();
					MeshCopy.Attributes()->EnablePrimaryColors();
				}
				FaceDynamicMeshComponent->SetMesh(MoveTemp(MeshCopy));
			}
		}

		double SurfaceGridApplyMs = 0.0;
		if (USRPlanetSurfaceGrid* SurfaceGrid = GetSurfaceGrid())
		{
			if (!CachedSurfaceGridCells.IsEmpty())
			{
				const double SurfaceGridApplyStart = SRCelestialNowSeconds();
				TArray<FSRPlanetSurfaceGridCell> GeneratedGridCells = CachedSurfaceGridCells;
				UE::Geometry::FDynamicMesh3 EmptyGridMesh;
				EmptyGridMesh.EnableAttributes();
				EmptyGridMesh.Attributes()->EnablePrimaryColors();
				SurfaceGrid->ApplyGeneratedGridBuild(MoveTemp(GeneratedGridCells), MoveTemp(EmptyGridMesh), TMap<FSRPlanetSurfaceGridCellId, int32>());
				SurfaceGridApplyMs = SRCelestialElapsedMilliseconds(SurfaceGridApplyStart);
			}
		}

		CachedDynamicMeshBuildHash = DynamicMeshBuildHash;
		bHasCachedDynamicMeshBuildHash = true;
		FSRTimingLog::AddLine(FString::Printf(TEXT("DynamicMesh.ApplyCache '%s' %.2f ms SurfaceGrid=%.2f ms Meshes=%d Cells=%d"), *GetName(), SRCelestialElapsedMilliseconds(ApplyStart), SurfaceGridApplyMs, CacheEntry.FaceDynamicMeshes.Num(), CacheEntry.SurfaceGridCells.Num()));
		return true;
	};

	if (bEnableGlobalDynamicMeshRuntimeCache)
	{
		if (const FSRCelestialBodyDynamicMeshRuntimeCacheEntry* CacheEntry = GCelestialBodyDynamicMeshRuntimeCache.Find(DynamicMeshBuildHash))
		{
			const bool bApplied = ApplyRuntimeCacheEntry(*CacheEntry);
			FSRTimingLog::AddLine(FString::Printf(TEXT("DynamicMesh '%s' RuntimeCacheHit Total=%.2f ms"), *GetName(), SRCelestialElapsedMilliseconds(TotalStart)));
			return bApplied;
		}
	}

	ResetDynamicMeshCellColorData();

	const bool bShouldGenerateMetadataTerrain =
		(BodyCategory == ESRCelestialBodyCategory::Planet || BodyCategory == ESRCelestialBodyCategory::Moon)
		&& DynamicMeshGeneration.bDynamicMeshGeneration
		&& DynamicMeshGeneration.DynamicMeshHeight > KINDA_SMALL_NUMBER;
	if (bShouldGenerateMetadataTerrain)
	{
		if (IsValid(DynamicMeshBaseDataAsset.Get()))
		{
			return BuildDynamicMeshFromBaseMetadata(DynamicMeshBuildHash, TotalStart);
		}

		UE_LOG(LogStarRoversCelestial, Error, TEXT("Celestial body '%s' requires DynamicMeshBaseDataAsset for metadata terrain generation."), *GetName());
		return false;
	}

	if (!IsValid(StaticMesh.Get()))
	{
		return false;
	}

	const double RenderDataStart = SRCelestialNowSeconds();
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
	FSRTimingLog::AddLine(FString::Printf(TEXT("DynamicMesh '%s' RenderData %.2f ms Vertices=%d Indices=%d"), *GetName(), SRCelestialElapsedMilliseconds(RenderDataStart), VertexCount, IndexCount));

	UE::Geometry::FDynamicMesh3 DynamicMesh;
	DynamicMesh.EnableAttributes();
	DynamicMesh.Attributes()->EnablePrimaryColors();
	DynamicMesh.Attributes()->EnableMaterialID();
	UE::Geometry::FDynamicMeshNormalOverlay* NormalOverlay = DynamicMesh.Attributes()->PrimaryNormals();
	auto* ColorOverlay = DynamicMesh.Attributes()->PrimaryColors();
	auto* MaterialIdAttribute = DynamicMesh.Attributes()->GetMaterialID();

	TArray<int32> DynamicVertexIds;
	DynamicVertexIds.Reserve(VertexCount);
	TArray<int32> DynamicNormalIds;
	DynamicNormalIds.Reserve(VertexCount);
	TArray<int32> DynamicColorIds;
	DynamicColorIds.Reserve(VertexCount);

	double StageStart = SRCelestialNowSeconds();
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
	const double FallbackVertexMs = SRCelestialElapsedMilliseconds(StageStart);

	StageStart = SRCelestialNowSeconds();
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
	const double FallbackTriangleMs = SRCelestialElapsedMilliseconds(StageStart);

	double RuntimeCacheStoreMs = 0.0;
	if (bEnableGlobalDynamicMeshRuntimeCache)
	{
		TArray<UE::Geometry::FDynamicMesh3> CachedFaceDynamicMeshes;
		CachedFaceDynamicMeshes.SetNum(CubeSphereFaceComponentCount);
		CachedFaceDynamicMeshes[0] = DynamicMesh;
		for (int32 FaceIndex = 1; FaceIndex < CubeSphereFaceComponentCount; ++FaceIndex)
		{
			CachedFaceDynamicMeshes[FaceIndex].EnableAttributes();
			CachedFaceDynamicMeshes[FaceIndex].Attributes()->EnablePrimaryColors();
		}
		FSRCelestialBodyDynamicMeshRuntimeCacheEntry GeneratedCacheEntry;
		GeneratedCacheEntry.FaceDynamicMeshes = CachedFaceDynamicMeshes;
		GeneratedCacheEntry.SurfaceGridCells = CachedSurfaceGridCells;
		GeneratedCacheEntry.ColorDataByFlatId = DynamicMeshColorDataByFlatId;
		StageStart = SRCelestialNowSeconds();
		StoreCelestialBodyDynamicMeshRuntimeCache(DynamicMeshBuildHash, MoveTemp(GeneratedCacheEntry));
		RuntimeCacheStoreMs = SRCelestialElapsedMilliseconds(StageStart);
	}

	StageStart = SRCelestialNowSeconds();
	CelestialBodyDynamicMesh->SetMesh(MoveTemp(DynamicMesh));
	for (int32 FaceIndex = 1; FaceIndex < CubeSphereFaceComponentCount; ++FaceIndex)
	{
		if (UDynamicMeshComponent* FaceDynamicMeshComponent = GetDynamicMeshFaceComponent(FaceIndex))
		{
			UE::Geometry::FDynamicMesh3 EmptyMesh;
			EmptyMesh.EnableAttributes();
			EmptyMesh.Attributes()->EnablePrimaryColors();
			FaceDynamicMeshComponent->SetMesh(MoveTemp(EmptyMesh));
		}
	}
	const double SetMeshMs = SRCelestialElapsedMilliseconds(StageStart);
	CachedDynamicMeshBuildHash = DynamicMeshBuildHash;
	bHasCachedDynamicMeshBuildHash = true;
	FSRTimingLog::AddLine(FString::Printf(TEXT("DynamicMesh '%s' FallbackTriangle Total=%.2f ms Vertices=%.2f ms Triangles=%.2f ms RuntimeCache=%.2f ms SetMesh=%.2f ms"), *GetName(), SRCelestialElapsedMilliseconds(TotalStart), FallbackVertexMs, FallbackTriangleMs, RuntimeCacheStoreMs, SetMeshMs));
	return true;
}

UDynamicMeshComponent* ASRCelestialBody::GetDynamicMeshFaceComponent(int32 FaceIndex) const
{
	if (CelestialBodyDynamicMeshFaces.IsValidIndex(FaceIndex) && IsValid(CelestialBodyDynamicMeshFaces[FaceIndex]))
	{
		return CelestialBodyDynamicMeshFaces[FaceIndex];
	}

	return FaceIndex == 0 ? CelestialBodyDynamicMesh.Get() : nullptr;
}

void ASRCelestialBody::SyncDynamicMeshFaceComponentSettings()
{
	if (!IsValid(CelestialBodyDynamicMesh.Get()))
	{
		return;
	}

	for (int32 FaceIndex = 1; FaceIndex < CelestialBodyDynamicMeshFaces.Num(); ++FaceIndex)
	{
		UDynamicMeshComponent* FaceDynamicMesh = CelestialBodyDynamicMeshFaces[FaceIndex];
		if (!IsValid(FaceDynamicMesh))
		{
			continue;
		}

		FaceDynamicMesh->SetMobility(CelestialBodyDynamicMesh->Mobility);
		FaceDynamicMesh->SetCastShadow(CelestialBodyDynamicMesh->CastShadow);
		FaceDynamicMesh->bCastDynamicShadow = CelestialBodyDynamicMesh->bCastDynamicShadow;
		FaceDynamicMesh->bCastStaticShadow = CelestialBodyDynamicMesh->bCastStaticShadow;
		FaceDynamicMesh->bCastVolumetricTranslucentShadow = CelestialBodyDynamicMesh->bCastVolumetricTranslucentShadow;
		FaceDynamicMesh->bCastContactShadow = CelestialBodyDynamicMesh->bCastContactShadow;
		FaceDynamicMesh->bSelfShadowOnly = CelestialBodyDynamicMesh->bSelfShadowOnly;
		FaceDynamicMesh->bCastFarShadow = CelestialBodyDynamicMesh->bCastFarShadow;
		FaceDynamicMesh->bCastInsetShadow = CelestialBodyDynamicMesh->bCastInsetShadow;
		FaceDynamicMesh->bCastCinematicShadow = CelestialBodyDynamicMesh->bCastCinematicShadow;
		FaceDynamicMesh->bCastHiddenShadow = CelestialBodyDynamicMesh->bCastHiddenShadow;
		FaceDynamicMesh->bAffectDynamicIndirectLighting = CelestialBodyDynamicMesh->bAffectDynamicIndirectLighting;
		FaceDynamicMesh->bAffectDistanceFieldLighting = CelestialBodyDynamicMesh->bAffectDistanceFieldLighting;
		FaceDynamicMesh->bReceivesDecals = CelestialBodyDynamicMesh->bReceivesDecals;
		FaceDynamicMesh->bRenderInMainPass = CelestialBodyDynamicMesh->bRenderInMainPass;
		FaceDynamicMesh->bRenderInDepthPass = CelestialBodyDynamicMesh->bRenderInDepthPass;
		FaceDynamicMesh->bVisibleInReflectionCaptures = CelestialBodyDynamicMesh->bVisibleInReflectionCaptures;
		FaceDynamicMesh->bVisibleInRealTimeSkyCaptures = CelestialBodyDynamicMesh->bVisibleInRealTimeSkyCaptures;
		FaceDynamicMesh->bVisibleInRayTracing = CelestialBodyDynamicMesh->bVisibleInRayTracing;
		FaceDynamicMesh->bRenderCustomDepth = CelestialBodyDynamicMesh->bRenderCustomDepth;
		FaceDynamicMesh->CustomDepthStencilValue = CelestialBodyDynamicMesh->CustomDepthStencilValue;
		FaceDynamicMesh->CustomDepthStencilWriteMask = CelestialBodyDynamicMesh->CustomDepthStencilWriteMask;
		FaceDynamicMesh->LightingChannels = CelestialBodyDynamicMesh->LightingChannels;
		FaceDynamicMesh->TranslucencySortPriority = CelestialBodyDynamicMesh->TranslucencySortPriority;
		FaceDynamicMesh->TranslucencySortDistanceOffset = CelestialBodyDynamicMesh->TranslucencySortDistanceOffset;
		FaceDynamicMesh->RuntimeVirtualTextures = CelestialBodyDynamicMesh->RuntimeVirtualTextures;
		FaceDynamicMesh->VirtualTextureLodBias = CelestialBodyDynamicMesh->VirtualTextureLodBias;
		FaceDynamicMesh->VirtualTextureCullMips = CelestialBodyDynamicMesh->VirtualTextureCullMips;
		FaceDynamicMesh->VirtualTextureMinCoverage = CelestialBodyDynamicMesh->VirtualTextureMinCoverage;
		FaceDynamicMesh->VirtualTextureRenderPassType = CelestialBodyDynamicMesh->VirtualTextureRenderPassType;
		FaceDynamicMesh->SetCollisionEnabled(CelestialBodyDynamicMesh->GetCollisionEnabled());
		FaceDynamicMesh->SetCollisionObjectType(CelestialBodyDynamicMesh->GetCollisionObjectType());
		FaceDynamicMesh->SetCollisionResponseToChannels(CelestialBodyDynamicMesh->GetCollisionResponseToChannels());
	}
}

uint32 ASRCelestialBody::ComputeDynamicMeshBuildHash() const
{
	uint32 Hash = ::GetTypeHash(StaticMesh.Get());
	Hash = HashCombine(Hash, PointerHash(DynamicMeshBaseDataAsset.Get()));
	if (IsValid(DynamicMeshBaseDataAsset.Get()))
	{
		Hash = HashCombine(Hash, ::GetTypeHash(static_cast<uint8>(DynamicMeshBaseDataAsset->BaseShape)));
		Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshBaseDataAsset->GetClampedFaceResolution()));
		Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshBaseDataAsset->GetSafeBaseRadius()));
	}
	Hash = HashCombine(Hash, ::GetTypeHash(static_cast<uint8>(BodyCategory)));
	Hash = HashCombine(Hash, ::GetTypeHash(Scale));
	Hash = HashCombine(Hash, ::GetTypeHash(GenerationSeed));
	Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.GenerationSeed));
	Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.bDynamicMeshGeneration ? 1 : 0));
	Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.bMinecraft ? 1 : 0));
	Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.DynamicMeshHeight));
	Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.ContinentFrequency));
	Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.MountainFrequency));
	Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.DetailFrequency));
	Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.MoistureFrequency));
	Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.TemperatureFrequency));
	Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.ValleyStrength));
	Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.MountainStrength));
	Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.NoiseStrength));
	Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.RiverStrength));
	Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.LakeStrength));
	Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.DetailStrength));
	Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.NoiseOctaves));
	Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.NoisePersistence));
	Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.OceanThreshold));
	Hash = HashCombine(Hash, ::GetTypeHash(DynamicMeshGeneration.AtmosphereThreshold));

	for (const FSRBiomeMaterialEntry& BiomeMaterialEntry : DynamicMeshGeneration.BiomeMaterials)
	{
		UMaterialInterface* BiomeMaterialPtr = BiomeMaterialEntry.Material.Get();
		Hash = HashCombine(Hash, FCrc::StrCrc32(*BiomeMaterialEntry.BiomeId.ToString()));
		Hash = HashBiomeDataAssetSettings(Hash, BiomeMaterialEntry.BiomeDataAsset.Get());
		Hash = HashCombine(Hash, ::GetTypeHash(BiomeMaterialPtr));
	}

	return Hash;
}

void ASRCelestialBody::ResetDynamicMeshCellColorData()
{
	DynamicMeshColorDataByFlatId.Reset();
	HighlightedDynamicMeshColorElements.Reset();
	HighlightedDynamicMeshBaseColorByElement.Reset();
	CachedSurfaceGridCells.Reset();
	bHasCachedDynamicMeshBuildHash = false;
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
