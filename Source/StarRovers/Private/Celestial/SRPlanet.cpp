#include "Celestial/SRPlanet.h"

#include "Components/LineBatchComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Simulation/SROrbit.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	constexpr float SurfaceGridTargetCellSize = 12500.0f;
}

ASRPlanet::ASRPlanet()
{
	BodyCategory = ESRCelestialBodyCategory::Planet;
	ConstructionHeightOffset = 15.0f;
	GridLineColor = FLinearColor(0.15f, 0.85f, 1.0f, 1.0f);
	GridLineOpacity = 1.0f;
	GridLineThickness = 1.0f;
	HoveredCellColor = FLinearColor(1.0f, 0.85f, 0.2f, 1.0f);
	SelectedCellColor = FLinearColor(0.25f, 1.0f, 0.35f, 1.0f);
	OccupiedCellColor = FLinearColor(1.0f, 0.35f, 0.35f, 1.0f);
	bShowOrbitLine = true;
	OrbitLineColor = FLinearColor(0.2f, 0.75f, 1.0f, 1.0f);
	OrbitLineOpacity = 0.85f;
	OrbitLineThickness = 20.0f;
	OrbitLineSegments = 96;
	CanConstruct = false;
	SurfaceGridSurfaceOffset = 0.0f;
	bHasOcean = false;
	OceanMesh = nullptr;
	OceanMaterial = nullptr;
	OceanScaleMultiplier = 0.97f;

	Orbit = CreateDefaultSubobject<USROrbit>(TEXT("Orbit"));

	OrbitLineBatch = CreateDefaultSubobject<ULineBatchComponent>(TEXT("OrbitLineBatch"));
	OrbitLineBatch->SetupAttachment(SceneRoot);
	OrbitLineBatch->SetMobility(EComponentMobility::Movable);
	OrbitLineBatch->SetUsingAbsoluteLocation(true);
	OrbitLineBatch->SetUsingAbsoluteRotation(true);
	OrbitLineBatch->SetUsingAbsoluteScale(true);
	OrbitLineBatch->ComponentTags.AddUnique(TEXT("StarRovers.OrbitLine"));
	OrbitLineBatch->ComponentTags.AddUnique(TEXT("StarRovers.OrbitLineRoot"));

	OceanStaticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OceanStaticMesh"));
	OceanStaticMesh->SetupAttachment(SceneRoot);
	OceanStaticMesh->SetMobility(EComponentMobility::Movable);
	OceanStaticMesh->SetVisibility(false);
	OceanStaticMesh->SetHiddenInGame(true);

	SurfaceGrid = CreateDefaultSubobject<USRPlanetSurfaceGrid>(TEXT("SurfaceGrid"));
	SurfaceGrid->SetupAttachment(SceneRoot);
}

void ASRPlanet::ApplySpec(const FSRCelestialBodySpec& NewSpec)
{
	CanConstruct = NewSpec.bCanConstruct;
	SurfaceGridSurfaceOffset = NewSpec.SurfaceGridSurfaceOffset;
	bHasOcean = NewSpec.bHasOcean;
	OceanMesh = NewSpec.OceanMesh;
	OceanMaterial = NewSpec.OceanMaterial;
	OceanScaleMultiplier = NewSpec.OceanScaleMultiplier;
	bShowOrbitLine = NewSpec.bShowOrbitLine;
	OrbitLineColor = NewSpec.OrbitLineColor;
	OrbitLineOpacity = NewSpec.OrbitLineOpacity;
	OrbitLineThickness = NewSpec.OrbitLineThickness;
	OrbitLineSegments = NewSpec.OrbitLineSegments;
	ConstructionHeightOffset = NewSpec.ConstructionHeightOffset;
	GridLineColor = NewSpec.GridLineColor;
	GridLineOpacity = NewSpec.GridLineOpacity;
	GridLineThickness = NewSpec.GridLineThickness;
	HoveredCellColor = NewSpec.HoveredCellColor;
	SelectedCellColor = NewSpec.SelectedCellColor;
	OccupiedCellColor = NewSpec.OccupiedCellColor;

	if (IsValid(Orbit))
	{
		Orbit->ConfigureOrbit(
			NewSpec.ParentBody,
			NewSpec.OrbitRadius,
			NewSpec.OrbitPeriodInPeriods,
			NewSpec.StartingPhase);
	}

	Super::ApplySpec(NewSpec);
}

void ASRPlanet::ApplyConfiguredBodyState()
{
	Super::ApplyConfiguredBodyState();
	ConstructionHeightOffset = FMath::Max(0.0f, ConstructionHeightOffset);
	GridLineThickness = FMath::Clamp(GridLineThickness, 0.0f, 2.0f);
	GridLineOpacity = FMath::Clamp(GridLineOpacity, 0.0f, 1.0f);
	SurfaceGridSurfaceOffset = FMath::Clamp(SurfaceGridSurfaceOffset, 0.0f, 1.0f);
	OceanScaleMultiplier = FMath::Max(0.01f, OceanScaleMultiplier);
	OrbitLineOpacity = FMath::Clamp(OrbitLineOpacity, 0.0f, 1.0f);
	OrbitLineSegments = FMath::Max(3, OrbitLineSegments);
	OrbitLineThickness = FMath::Max(0.0f, OrbitLineThickness);

	if (IsValid(OceanStaticMesh))
	{
		OceanStaticMesh->SetRelativeLocation(FVector::ZeroVector);
		OceanStaticMesh->SetRelativeRotation(FRotator::ZeroRotator);
		OceanStaticMesh->SetRelativeScale3D(FVector(BodyScale * OceanScaleMultiplier));
	}

	ApplyOceanStaticMeshSettings();
	SyncApproximateRadiusFromPlanetVisuals();
	if (IsValid(ClickSphereCollision))
	{
		ClickSphereCollision->SetSphereRadius(FMath::Max(ApproximateRadius, 1.0f));
	}

	if (SupportsSurfaceGrid())
	{
		EnsureSurfaceGrid();
		if (IsValid(SurfaceGrid))
		{
			const float SurfaceGridPlanetRadius = FMath::Max(ComputeCelestialBodyDynamicMeshRadius(), 1.0f);
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
				SurfaceGridSurfaceOffset);
			SurfaceGrid->ConfigureConstructionHeightOffset(ConstructionHeightOffset);
			SurfaceGrid->ConfigureTerrain(TerrainSettings);
			SurfaceGrid->RebuildGrid();
		}
	}
	else
	{
		HideSurfaceGrid();
	}

	if (IsValid(Orbit))
	{
		Orbit->ConfigureOrbitLineVisual(
			bShowOrbitLine,
			OrbitLineColor,
			OrbitLineOpacity,
			OrbitLineSegments,
			OrbitLineThickness);
		Orbit->ResetOrbitSimulation();
		Orbit->RefreshOrbitLineVisual();
	}
}

FSRCelestialBodySpec ASRPlanet::GetSpec() const
{
	FSRCelestialBodySpec CurrentSpec = Super::GetSpec();
	CurrentSpec.ParentBody = IsValid(Orbit) ? Orbit->GetParentBody() : nullptr;
	CurrentSpec.OrbitRadius = IsValid(Orbit) ? Orbit->GetOrbitRadius() : 0.0f;
	CurrentSpec.OrbitPeriodInPeriods = IsValid(Orbit) ? Orbit->GetOrbitPeriodInPeriods() : 0.0f;
	CurrentSpec.StartingPhase = IsValid(Orbit) ? Orbit->GetStartingPhaseDegrees() : 0.0f;
	CurrentSpec.bCanConstruct = CanConstruct;
	CurrentSpec.GridLineThickness = GridLineThickness;
	CurrentSpec.GridLineColor = GridLineColor;
	CurrentSpec.GridLineOpacity = GridLineOpacity;
	CurrentSpec.HoveredCellColor = HoveredCellColor;
	CurrentSpec.SelectedCellColor = SelectedCellColor;
	CurrentSpec.OccupiedCellColor = OccupiedCellColor;
	CurrentSpec.SurfaceGridSurfaceOffset = SurfaceGridSurfaceOffset;
	CurrentSpec.ConstructionHeightOffset = ConstructionHeightOffset;
	CurrentSpec.TerrainSeed = GenerationSeed;
	CurrentSpec.TerrainSettings = TerrainSettings;
	CurrentSpec.bHasOcean = bHasOcean;
	CurrentSpec.OceanMesh = OceanMesh;
	CurrentSpec.OceanMaterial = OceanMaterial;
	CurrentSpec.OceanScaleMultiplier = OceanScaleMultiplier;
	CurrentSpec.bShowOrbitLine = bShowOrbitLine;
	CurrentSpec.OrbitLineColor = OrbitLineColor;
	CurrentSpec.OrbitLineOpacity = OrbitLineOpacity;
	CurrentSpec.OrbitLineSegments = OrbitLineSegments;
	CurrentSpec.OrbitLineThickness = OrbitLineThickness;
	return CurrentSpec;
}

USROrbit* ASRPlanet::GetOrbitComponent() const
{
	return Orbit;
}

USRPlanetSurfaceGrid* ASRPlanet::GetSurfaceGrid() const
{
	return SurfaceGrid;
}

void ASRPlanet::SetDynamicMeshEnabled(bool bUseDynamicMesh)
{
	Super::SetDynamicMeshEnabled(bUseDynamicMesh);

	if (IsValid(OceanStaticMesh))
	{
		const bool bEnableOcean = bUseDynamicMesh && bHasOcean && IsValid(OceanMesh.Get()) && IsValid(OceanMaterial.Get());
		OceanStaticMesh->SetVisibility(bEnableOcean);
		OceanStaticMesh->SetHiddenInGame(!bEnableOcean);
	}
}

void ASRPlanet::ApplyOceanStaticMeshSettings()
{
	if (!IsValid(OceanStaticMesh))
	{
		return;
	}

	UStaticMesh* DesiredOceanMesh = OceanMesh.Get();
	if (!IsValid(DesiredOceanMesh) && bHasOcean)
	{
		UE_LOG(LogTemp, Error, TEXT("Planet '%s' requires OceanMesh while ocean is enabled."), *GetName());
	}

	const bool bEnableOcean = bHasOcean && IsValid(DesiredOceanMesh);
	if (!bEnableOcean)
	{
		OceanStaticMesh->SetVisibility(false);
		OceanStaticMesh->SetHiddenInGame(true);
		return;
	}

	if (OceanStaticMesh->GetStaticMesh() != DesiredOceanMesh)
	{
		OceanStaticMesh->SetStaticMesh(DesiredOceanMesh);
	}

	UMaterialInterface* DesiredOceanMaterial = OceanMaterial.Get();
	if (!IsValid(DesiredOceanMaterial))
	{
		UE_LOG(LogTemp, Error, TEXT("Planet '%s' requires OceanMaterial while ocean is enabled."), *GetName());
		OceanStaticMesh->SetVisibility(false);
		OceanStaticMesh->SetHiddenInGame(true);
		return;
	}

	OceanStaticMesh->SetMaterial(0, DesiredOceanMaterial);

	const float OceanScale = FMath::Max(0.01f, BodyScale * OceanScaleMultiplier);
	OceanStaticMesh->SetRelativeLocation(FVector::ZeroVector);
	OceanStaticMesh->SetRelativeRotation(FRotator::ZeroRotator);
	OceanStaticMesh->SetRelativeScale3D(FVector(OceanScale));
	OceanStaticMesh->SetVisibility(true);
	OceanStaticMesh->SetHiddenInGame(false);
}

void ASRPlanet::SyncApproximateRadiusFromPlanetVisuals()
{
	ApproximateRadius = ComputeCelestialBodyDynamicMeshRadius();
	if (bHasOcean)
	{
		ApproximateRadius = FMath::Max(ApproximateRadius, ComputeCelestialBodyDynamicMeshRadius() * OceanScaleMultiplier);
	}
}

void ASRPlanet::EnsureSurfaceGrid()
{
	if (IsValid(SurfaceGrid))
	{
		SurfaceGrid->SetVisibility(true);
		SurfaceGrid->SetHiddenInGame(false);
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
