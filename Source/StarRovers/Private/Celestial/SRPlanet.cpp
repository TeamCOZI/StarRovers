#include "Celestial/SRPlanet.h"

#include "Automation/SRFacilityNetworkComponent.h"
#include "Celestial/SRDynamicMeshBaseDataAsset.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/LineBatchComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Conveyor/SRConveyorNetworkComponent.h"
#include "Engine/StaticMesh.h"
#include "Simulation/SROrbit.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"
#include "Rendering/SRCelestialRingMeshComponent.h"

namespace
{
	constexpr float SurfaceGridTargetCellSize = 12500.0f;
}

ASRPlanet::ASRPlanet()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	BodyCategory = ESRCelestialBodyCategory::Planet;
	GridLineColor = FLinearColor(0.15f, 0.85f, 1.0f, 1.0f);
	GridLineOpacity = 1.0f;
	GridLineThickness = 1.0f;
	HoveredCellColor = FLinearColor(1.0f, 0.85f, 0.2f, 1.0f);
	SelectedCellColor = FLinearColor(0.25f, 1.0f, 0.35f, 1.0f);
	OccupiedCellColor = FLinearColor(1.0f, 0.35f, 0.35f, 1.0f);
	GridOverlayMaterial = nullptr;
	ShowOrbitLine = true;
	OrbitLineColor = FLinearColor(0.2f, 0.75f, 1.0f, 1.0f);
	OrbitLineOpacity = 0.85f;
	OrbitLineThickness = 20.0f;
	OrbitLineSegments = 96;
	ShowRotationAxisLine = true;
	RotationAxisLineColor = FLinearColor(1.0f, 0.9f, 0.2f, 1.0f);
	RotationAxisLineOpacity = 0.95f;
	RotationAxisLineThickness = 18.0f;
	RotationAxisLineLengthMultiplier = 1.25f;
	RotationAxisMaterial = nullptr;
	CanConstruct = false;
	SurfaceGridHeightOffset = 0.0f;
	bHasOcean = false;
	OceanDynamicMeshBaseDataAsset = nullptr;
	OceanMaterial = nullptr;
	OceanScaleMultiplier = 1.0f;
	bHasAtmosphere = false;
	AtmosphereDynamicMeshBaseDataAsset = nullptr;
	AtmosphereMaterial = nullptr;
	AtmosphereScaleMultiplier = 1.0f;

	Orbit = CreateDefaultSubobject<USROrbit>(TEXT("Orbit"));

	OrbitLineBatch = CreateDefaultSubobject<ULineBatchComponent>(TEXT("OrbitLineBatch"));
	OrbitLineBatch->SetupAttachment(SceneRoot);
	OrbitLineBatch->SetMobility(EComponentMobility::Movable);
	OrbitLineBatch->SetUsingAbsoluteLocation(true);
	OrbitLineBatch->SetUsingAbsoluteRotation(true);
	OrbitLineBatch->SetUsingAbsoluteScale(true);
	OrbitLineBatch->SetVisibility(false);
	OrbitLineBatch->SetHiddenInGame(true);
	OrbitLineBatch->SetComponentTickEnabled(false);
	OrbitLineBatch->ComponentTags.AddUnique(TEXT("StarRovers.OrbitLine"));
	OrbitLineBatch->ComponentTags.AddUnique(TEXT("StarRovers.OrbitLineRoot"));

	OrbitRingVisual = CreateDefaultSubobject<USRCelestialRingMeshComponent>(TEXT("OrbitRingVisual"));
	OrbitRingVisual->SetupAttachment(SceneRoot);
	OrbitRingVisual->ComponentTags.AddUnique(TEXT("StarRovers.OrbitLine"));
	OrbitRingVisual->ComponentTags.AddUnique(TEXT("StarRovers.OrbitLineRoot"));

	RotationAxisNorthSpline = CreateDefaultSubobject<USplineMeshComponent>(TEXT("RotationAxisNorthSpline"));
	RotationAxisNorthSpline->SetupAttachment(SceneRoot);
	RotationAxisNorthSpline->SetMobility(EComponentMobility::Movable);
	RotationAxisNorthSpline->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RotationAxisNorthSpline->ComponentTags.AddUnique(TEXT("StarRovers.RotationAxisLine"));
	RotationAxisNorthSpline->ComponentTags.AddUnique(TEXT("StarRovers.RotationAxisLineRoot"));

	RotationAxisSouthSpline = CreateDefaultSubobject<USplineMeshComponent>(TEXT("RotationAxisSouthSpline"));
	RotationAxisSouthSpline->SetupAttachment(SceneRoot);
	RotationAxisSouthSpline->SetMobility(EComponentMobility::Movable);
	RotationAxisSouthSpline->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RotationAxisSouthSpline->ComponentTags.AddUnique(TEXT("StarRovers.RotationAxisLine"));
	RotationAxisSouthSpline->ComponentTags.AddUnique(TEXT("StarRovers.RotationAxisLineRoot"));

	OceanDynamicMesh = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("OceanDynamicMesh"));
	OceanDynamicMesh->SetupAttachment(SceneRoot);
	OceanDynamicMesh->SetMobility(EComponentMobility::Movable);
	ConfigureShellDynamicMeshComponent(OceanDynamicMesh);
	OceanDynamicMesh->SetVisibility(false);
	OceanDynamicMesh->SetHiddenInGame(true);

	AtmosphereDynamicMesh = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("AtmosphereDynamicMesh"));
	AtmosphereDynamicMesh->SetupAttachment(SceneRoot);
	AtmosphereDynamicMesh->SetMobility(EComponentMobility::Movable);
	ConfigureShellDynamicMeshComponent(AtmosphereDynamicMesh);
	AtmosphereDynamicMesh->SetVisibility(false);
	AtmosphereDynamicMesh->SetHiddenInGame(true);

	SurfaceGrid = CreateDefaultSubobject<USRPlanetSurfaceGrid>(TEXT("SurfaceGrid"));
	SurfaceGrid->SetupAttachment(SceneRoot);

	ConveyorNetwork = CreateDefaultSubobject<USRConveyorNetworkComponent>(TEXT("ConveyorNetwork"));
	ConveyorNetwork->SetupAttachment(SceneRoot);

	StructureInstanceManager = CreateDefaultSubobject<USRStructureInstanceManagerComponent>(TEXT("StructureInstanceManager"));
	StructureInstanceManager->SetupAttachment(SceneRoot);

	FacilityNetwork = CreateDefaultSubobject<USRFacilityNetworkComponent>(TEXT("FacilityNetwork"));
	FacilityNetwork->SetupAttachment(SceneRoot);
}

void ASRPlanet::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshRotationAxisLineVisual();
}

void ASRPlanet::SetData(const FSRCelestialBodyData& NewData)
{
	CanConstruct = NewData.bCanConstruct;
	SurfaceGridHeightOffset = NewData.SurfaceGridHeightOffset;
	bHasOcean = NewData.bHasOcean;
	OceanDynamicMeshBaseDataAsset = NewData.OceanDynamicMeshBaseDataAsset;
	OceanMaterial = NewData.OceanMaterial;
	OceanScaleMultiplier = NewData.OceanScaleMultiplier;
	bHasAtmosphere = NewData.bHasAtmosphere;
	AtmosphereDynamicMeshBaseDataAsset = NewData.AtmosphereDynamicMeshBaseDataAsset;
	AtmosphereMaterial = NewData.AtmosphereMaterial;
	AtmosphereScaleMultiplier = NewData.AtmosphereScaleMultiplier;
	ShowOrbitLine = NewData.ShowOrbitLine;
	OrbitLineColor = NewData.OrbitLineColor;
	OrbitLineOpacity = NewData.OrbitLineOpacity;
	OrbitLineThickness = NewData.OrbitLineThickness;
	OrbitLineSegments = NewData.OrbitLineSegments;
	ShowRotationAxisLine = NewData.ShowRotationAxisLine;
	RotationAxisLineColor = NewData.RotationAxisLineColor;
	RotationAxisLineOpacity = NewData.RotationAxisLineOpacity;
	RotationAxisLineThickness = NewData.RotationAxisLineThickness;
	RotationAxisLineLengthMultiplier = NewData.RotationAxisLineLengthMultiplier;
	GridLineColor = NewData.GridLineColor;
	GridLineOpacity = NewData.GridLineOpacity;
	GridLineThickness = NewData.GridLineThickness;
	HoveredCellColor = NewData.HoveredCellColor;
	SelectedCellColor = NewData.SelectedCellColor;
	OccupiedCellColor = NewData.OccupiedCellColor;

	if (IsValid(Orbit))
	{
		Orbit->ConfigureOrbit(
			NewData.ParentBody,
			NewData.OrbitRadius,
			NewData.OrbitPeriod,
			NewData.InitialAngle);
	}

	Super::SetData(NewData);
}

void ASRPlanet::ApplyData()
{
	Super::ApplyData();
	GridLineThickness = FMath::Clamp(GridLineThickness, 0.0f, 2.0f);
	GridLineOpacity = FMath::Clamp(GridLineOpacity, 0.0f, 1.0f);
	SurfaceGridHeightOffset = FMath::Clamp(SurfaceGridHeightOffset, 0.0f, 1.0f);
	OceanScaleMultiplier = FMath::Max(0.01f, OceanScaleMultiplier);
	AtmosphereScaleMultiplier = FMath::Max(0.01f, AtmosphereScaleMultiplier);
	OrbitLineOpacity = FMath::Clamp(OrbitLineOpacity, 0.0f, 1.0f);
	OrbitLineSegments = FMath::Max(3, OrbitLineSegments);
	OrbitLineThickness = FMath::Max(0.0f, OrbitLineThickness);
	RotationAxisLineOpacity = FMath::Clamp(RotationAxisLineOpacity, 0.0f, 1.0f);
	RotationAxisLineThickness = FMath::Max(0.0f, RotationAxisLineThickness);
	RotationAxisLineLengthMultiplier = FMath::Max(0.0f, RotationAxisLineLengthMultiplier);

	if (IsValid(OceanDynamicMesh))
	{
		OceanDynamicMesh->SetRelativeLocation(FVector::ZeroVector);
		OceanDynamicMesh->SetRelativeRotation(FRotator::ZeroRotator);
		OceanDynamicMesh->SetRelativeScale3D(FVector(ResolveOceanDynamicMeshScale()));
	}

	if (IsValid(AtmosphereDynamicMesh))
	{
		AtmosphereDynamicMesh->SetRelativeLocation(FVector::ZeroVector);
		AtmosphereDynamicMesh->SetRelativeRotation(FRotator::ZeroRotator);
		AtmosphereDynamicMesh->SetRelativeScale3D(FVector(ResolveAtmosphereDynamicMeshScale()));
	}

	ApplyOceanMeshSettings();
	ApplyAtmosphereMeshSettings();
	if (IsValid(ClickSphereCollision))
	{
		const float BodyRadius = IsValid(StaticMesh.Get())
			? StaticMesh->GetBounds().SphereRadius * Scale
			: IsValid(DynamicMeshBaseDataAsset.Get())
				? DynamicMeshBaseDataAsset->GetSafeBaseRadius() * Scale
				: 0.0f;
		const float OceanDynamicMeshRadius = bHasOcean && IsValid(OceanDynamicMeshBaseDataAsset.Get())
			? OceanDynamicMeshBaseDataAsset->GetSafeBaseRadius() * ResolveOceanDynamicMeshScale()
			: 0.0f;
		const float AtmosphereDynamicMeshRadius = bHasAtmosphere && IsValid(AtmosphereDynamicMeshBaseDataAsset.Get())
			? AtmosphereDynamicMeshBaseDataAsset->GetSafeBaseRadius() * ResolveAtmosphereDynamicMeshScale()
			: 0.0f;
		const float OceanRadius = OceanDynamicMeshRadius;
		const float AtmosphereRadius = AtmosphereDynamicMeshRadius;
		const float VisualRadius = FMath::Max(FMath::Max(BodyRadius, OceanRadius), AtmosphereRadius);
		ClickSphereCollision->SetSphereRadius(FMath::Max(VisualRadius, 1.0f));
	}

	if (SupportsSurfaceGrid())
	{
		EnsureSurfaceGrid();
		if (IsValid(SurfaceGrid))
		{
			const float BodyRadius = IsValid(StaticMesh.Get())
				? StaticMesh->GetBounds().SphereRadius * Scale
				: IsValid(DynamicMeshBaseDataAsset.Get())
					? DynamicMeshBaseDataAsset->GetSafeBaseRadius() * Scale
					: 0.0f;
			const float SurfaceGridPlanetRadius = FMath::Max(BodyRadius, 1.0f);
			const int32 ResolvedSurfaceGridResolution = FMath::Clamp(
				FMath::RoundToInt((SurfaceGridPlanetRadius * 2.0f) / SurfaceGridTargetCellSize),
				1,
				256);
			SurfaceGrid->SetFaceResolution(ResolvedSurfaceGridResolution);
			SurfaceGrid->SetPlanetRadius(SurfaceGridPlanetRadius);
			SurfaceGrid->ConfigureDebugGrid(
				GridLineColor,
				GridLineOpacity,
				GridLineThickness,
				HoveredCellColor,
				SelectedCellColor,
				OccupiedCellColor,
				SurfaceGridHeightOffset);
			SurfaceGrid->SetGridOverlayMaterial(GridOverlayMaterial);
			SurfaceGrid->ConfigureTerrain(DynamicMeshGeneration);
		}
	}
	else
	{
		HideSurfaceGrid();
	}

	if (IsValid(Orbit))
	{
		Orbit->ConfigureOrbitLineVisual(
			ShowOrbitLine,
			OrbitLineColor,
			OrbitLineOpacity,
			OrbitLineSegments,
			OrbitLineThickness);
		Orbit->ResetOrbitSimulation();
		Orbit->RefreshOrbitLineVisual();
	}

	RefreshRotationAxisLineVisual();
}

FSRCelestialBodyData ASRPlanet::GetData() const
{
	FSRCelestialBodyData CurrentData = Super::GetData();
	CurrentData.ParentBody = IsValid(Orbit) ? Orbit->GetParentBody() : nullptr;
	CurrentData.OrbitRadius = IsValid(Orbit) ? Orbit->GetOrbitRadius() : 0.0f;
	CurrentData.OrbitPeriod = IsValid(Orbit) ? Orbit->GetOrbitPeriod() : 0.0f;
	CurrentData.InitialAngle = IsValid(Orbit) ? Orbit->GetInitialAngleDegrees() : 0.0f;
	CurrentData.bCanConstruct = CanConstruct;
	CurrentData.GridLineThickness = GridLineThickness;
	CurrentData.GridLineColor = GridLineColor;
	CurrentData.GridLineOpacity = GridLineOpacity;
	CurrentData.HoveredCellColor = HoveredCellColor;
	CurrentData.SelectedCellColor = SelectedCellColor;
	CurrentData.OccupiedCellColor = OccupiedCellColor;
	CurrentData.SurfaceGridHeightOffset = SurfaceGridHeightOffset;
	CurrentData.GenerationSeed = GenerationSeed;
	CurrentData.DynamicMeshGeneration = DynamicMeshGeneration;
	CurrentData.bHasOcean = bHasOcean;
	CurrentData.OceanDynamicMeshBaseDataAsset = OceanDynamicMeshBaseDataAsset;
	CurrentData.OceanMaterial = OceanMaterial;
	CurrentData.OceanScaleMultiplier = OceanScaleMultiplier;
	CurrentData.bHasAtmosphere = bHasAtmosphere;
	CurrentData.AtmosphereDynamicMeshBaseDataAsset = AtmosphereDynamicMeshBaseDataAsset;
	CurrentData.AtmosphereMaterial = AtmosphereMaterial;
	CurrentData.AtmosphereScaleMultiplier = AtmosphereScaleMultiplier;
	CurrentData.ShowOrbitLine = ShowOrbitLine;
	CurrentData.OrbitLineColor = OrbitLineColor;
	CurrentData.OrbitLineOpacity = OrbitLineOpacity;
	CurrentData.OrbitLineSegments = OrbitLineSegments;
	CurrentData.OrbitLineThickness = OrbitLineThickness;
	CurrentData.ShowRotationAxisLine = ShowRotationAxisLine;
	CurrentData.RotationAxisLineColor = RotationAxisLineColor;
	CurrentData.RotationAxisLineOpacity = RotationAxisLineOpacity;
	CurrentData.RotationAxisLineThickness = RotationAxisLineThickness;
	CurrentData.RotationAxisLineLengthMultiplier = RotationAxisLineLengthMultiplier;
	return CurrentData;
}

USROrbit* ASRPlanet::GetOrbit() const
{
	return Orbit;
}

USRPlanetSurfaceGrid* ASRPlanet::GetSurfaceGrid() const
{
	return SurfaceGrid;
}

USRConveyorNetworkComponent* ASRPlanet::GetConveyorNetwork() const
{
	return ConveyorNetwork;
}

USRStructureInstanceManagerComponent* ASRPlanet::GetStructureInstanceManager() const
{
	return StructureInstanceManager;
}

USRFacilityNetworkComponent* ASRPlanet::GetFacilityNetwork() const
{
	return FacilityNetwork;
}

void ASRPlanet::SetCelestialBodyMesh(bool bUseDynamicMesh)
{
	Super::SetCelestialBodyMesh(bUseDynamicMesh);

	if (IsValid(OceanDynamicMesh))
	{
		const bool bEnableOcean = bUseDynamicMesh && bHasOcean && IsValid(OceanDynamicMeshBaseDataAsset.Get()) && IsValid(OceanMaterial.Get());
		OceanDynamicMesh->SetVisibility(bEnableOcean);
		OceanDynamicMesh->SetHiddenInGame(!bEnableOcean);
	}

	if (IsValid(AtmosphereDynamicMesh))
	{
		const bool bEnableAtmosphere = bUseDynamicMesh && bHasAtmosphere && IsValid(AtmosphereDynamicMeshBaseDataAsset.Get()) && IsValid(AtmosphereMaterial.Get());
		AtmosphereDynamicMesh->SetVisibility(bEnableAtmosphere);
		AtmosphereDynamicMesh->SetHiddenInGame(!bEnableAtmosphere);
	}

	RefreshRotationAxisLineVisual();
}

void ASRPlanet::EnsureSurfaceGrid()
{
	if (IsValid(SurfaceGrid))
	{
		SurfaceGrid->SetVisibility(SurfaceGrid->IsGridVisible());
		SurfaceGrid->SetHiddenInGame(!SurfaceGrid->IsGridVisible());
	}
}

void ASRPlanet::HideSurfaceGrid()
{
	if (IsValid(SurfaceGrid))
	{
		SurfaceGrid->SetVisibility(false);
		SurfaceGrid->SetHiddenInGame(true);
	}
}

bool ASRPlanet::SupportsSurfaceGrid() const
{
	return BodyCategory == ESRCelestialBodyCategory::Planet;
}
