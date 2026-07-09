#include "Celestial/SRCelestialBody.h"

#include "Celestial/SRCelestialBodyDynamicMeshPipeline.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/LineBatchComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Gravity/SRGravityParent.h"
#include "Rendering/SRCelestialRingMeshComponent.h"

void ASRCelestialBody::InitializeCelestialBodyComponents()
{
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	InitializeDynamicMeshComponents();

	CelestialBodyStaticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CelestialBodyStaticMesh"));
	CelestialBodyStaticMesh->SetupAttachment(SceneRoot);

	ClickSphereCollision = CreateDefaultSubobject<USphereComponent>(TEXT("ClickSphereCollision"));
	ClickSphereCollision->SetupAttachment(SceneRoot);
	ClickSphereCollision->SetMobility(EComponentMobility::Movable);
	ClickSphereCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ClickSphereCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	ClickSphereCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ClickSphereCollision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	InitializeGravityComponents();
}

void ASRCelestialBody::InitializeDynamicMeshComponents()
{
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
}

void ASRCelestialBody::InitializeGravityComponents()
{
	GravityParent = CreateDefaultSubobject<USRGravityParent>(TEXT("GravityParent"));

	GravityLineBatch = CreateDefaultSubobject<ULineBatchComponent>(TEXT("GravityLineBatch"));
	GravityLineBatch->SetupAttachment(SceneRoot);
	GravityLineBatch->SetMobility(EComponentMobility::Movable);
	GravityLineBatch->SetUsingAbsoluteLocation(true);
	GravityLineBatch->SetUsingAbsoluteRotation(true);
	GravityLineBatch->SetUsingAbsoluteScale(true);
	GravityLineBatch->SetVisibility(false);
	GravityLineBatch->SetHiddenInGame(true);
	GravityLineBatch->SetComponentTickEnabled(false);
	GravityLineBatch->ComponentTags.AddUnique(TEXT("StarRovers.GravityLine"));
	GravityLineBatch->ComponentTags.AddUnique(TEXT("StarRovers.GravityLineRoot"));

	GravityRingVisual = CreateDefaultSubobject<USRCelestialRingMeshComponent>(TEXT("GravityRingVisual"));
	GravityRingVisual->SetupAttachment(SceneRoot);
	GravityRingVisual->ComponentTags.AddUnique(TEXT("StarRovers.GravityLine"));
	GravityRingVisual->ComponentTags.AddUnique(TEXT("StarRovers.GravityLineRoot"));
}

void ASRCelestialBody::InitializeCelestialBodyDefaults()
{
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
	ToonOutlineSettings = FSRToonOutlineSettings();
	DynamicMeshBaseDataAsset = nullptr;
	GravityRatio = 1.0f;
	GravityRadiusRatio = 10.0f;
	ShowGravityLine = true;
	GravityLineColor = FLinearColor(0.45f, 1.0f, 0.45f, 1.0f);
	GravityLineOpacity = 0.85f;
	GravityLineSegments = 96;
	GravityLineThickness = 20.0f;
}
