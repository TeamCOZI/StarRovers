#include "Celestial/SRCelestialBody.h"

#include "SRCelestialBodyLog.h"
#include "Celestial/SRCelestialBodyDynamicMeshInternal.h"
#include "Celestial/SRDynamicMeshBaseDataAsset.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/LineBatchComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Gravity/SRGravityParent.h"
#include "Materials/MaterialInterface.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "Surface/SRPlanetTerrainProfileDataAsset.h"
#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

DEFINE_LOG_CATEGORY(LogStarRoversCelestial);

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
	for (int32 FaceIndex = 1; FaceIndex < StarRovers::Celestial::DynamicMesh::CubeSphereFaceComponentCount; ++FaceIndex)
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

	if (UWorld* World = GetWorld(); World && World->IsGameWorld() && GetDynamicMeshRuntimeCacheWorld() != World)
	{
		ClearDynamicMeshRuntimeCaches(TEXT("BeginPlay.NewGameWorld"));
		SetDynamicMeshRuntimeCacheWorld(World);
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
	if (UWorld* World = GetWorld(); World && World->IsGameWorld() && GetDynamicMeshRuntimeCacheWorld() == World)
	{
		ClearDynamicMeshRuntimeCaches(TEXT("EndPlay.GameWorld"));
		SetDynamicMeshRuntimeCacheWorld(nullptr);
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
