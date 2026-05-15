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
	GridLineThickness = 10.0f;
	HoveredCellColor = FLinearColor(1.0f, 0.85f, 0.2f, 1.0f);
	SelectedCellColor = FLinearColor(0.25f, 1.0f, 0.35f, 1.0f);
	OccupiedCellColor = FLinearColor(1.0f, 0.35f, 0.35f, 1.0f);
	bShowOrbitLine = true;
	OrbitLineColor = FLinearColor(0.2f, 0.75f, 1.0f, 1.0f);
	OrbitLineOpacity = 0.85f;
	OrbitLineThickness = 20.0f;
	OrbitLineSegments = 96;
	CanConstruct = false;
	SurfaceGridSurfaceOffset = 8.0f;
	CastShadowScaleMultiplier = 0.95f;
	bHasOcean = false;
	OceanMesh = nullptr;
	OceanMaterial = nullptr;
	OceanScaleMultiplier = 0.97f;

	Orbit = CreateDefaultSubobject<USROrbit>(TEXT("Orbit"));

	OrbitLineBatch = CreateDefaultSubobject<ULineBatchComponent>(TEXT("Orbit Line Batch"));
	OrbitLineBatch->SetupAttachment(SceneRoot);
	OrbitLineBatch->SetMobility(EComponentMobility::Movable);
	OrbitLineBatch->SetUsingAbsoluteLocation(true);
	OrbitLineBatch->SetUsingAbsoluteRotation(true);
	OrbitLineBatch->SetUsingAbsoluteScale(true);
	OrbitLineBatch->ComponentTags.AddUnique(TEXT("StarRovers.OrbitLine"));
	OrbitLineBatch->ComponentTags.AddUnique(TEXT("StarRovers.OrbitLineRoot"));

	CastShadowStaticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cast Shadow Static Mesh"));
	CastShadowStaticMesh->SetupAttachment(SceneRoot);
	CastShadowStaticMesh->SetMobility(EComponentMobility::Movable);
	CastShadowStaticMesh->SetVisibility(false);
	CastShadowStaticMesh->SetHiddenInGame(true);

	OceanStaticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ocean Static Mesh"));
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
	CastShadowScaleMultiplier = NewSpec.ShadowCasterScaleMultiplier;
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
	GridLineThickness = FMath::Max(0.0f, GridLineThickness);
	GridLineOpacity = FMath::Clamp(GridLineOpacity, 0.0f, 1.0f);
	SurfaceGridSurfaceOffset = FMath::Max(0.0f, SurfaceGridSurfaceOffset);
	CastShadowScaleMultiplier = FMath::Max(0.01f, CastShadowScaleMultiplier);
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
	ApplyCastShadowStaticMeshSettings();
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
			const float SurfaceGridPlanetRadius = FMath::Max(ComputeCelestialBodyStaticMeshRadius(), 1.0f);
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
	CurrentSpec.ShadowCasterScaleMultiplier = CastShadowScaleMultiplier;
	CurrentSpec.TerrainSeed = GenerationSeed;
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

void ASRPlanet::ApplyCastShadowStaticMeshSettings()
{
	if (!IsValid(CastShadowStaticMesh))
	{
		return;
	}

	const bool bEnableShadowCaster =
		IsValid(CelestialBodyStaticMesh_)
		&& CelestialBodyStaticMesh_->CastShadow
		&& BodyCategory != ESRCelestialBodyCategory::Star;

	UStaticMesh* DesiredCasterMesh = nullptr;
	if (IsValid(CelestialBodyStaticMesh))
	{
		DesiredCasterMesh = CelestialBodyStaticMesh.Get();
	}

	if (!bEnableShadowCaster || !IsValid(DesiredCasterMesh))
	{
		if (bEnableShadowCaster)
		{
			UE_LOG(LogTemp, Error, TEXT("Planet '%s' requires CelestialBodyStaticMesh for shadow caster."), *GetName());
		}

		CastShadowStaticMesh->SetVisibility(false);
		CastShadowStaticMesh->SetHiddenInGame(true);
		return;
	}

	if (CastShadowStaticMesh->GetStaticMesh() != DesiredCasterMesh)
	{
		CastShadowStaticMesh->SetStaticMesh(DesiredCasterMesh);
	}

	CastShadowStaticMesh->SetMaterial(0, nullptr);
	CastShadowStaticMesh->SetRelativeLocation(FVector::ZeroVector);
	CastShadowStaticMesh->SetRelativeRotation(FRotator::ZeroRotator);
	CastShadowStaticMesh->SetRelativeScale3D(FVector(FMath::Max(0.01f, BodyScale * CastShadowScaleMultiplier)));
	CastShadowStaticMesh->SetVisibility(false);
	CastShadowStaticMesh->SetHiddenInGame(true);
	CastShadowStaticMesh->LightingChannels.bChannel0 = true;
	CastShadowStaticMesh->LightingChannels.bChannel1 = false;
	CastShadowStaticMesh->LightingChannels.bChannel2 = false;
}

USROrbit* ASRPlanet::GetOrbitComponent() const
{
	return Orbit;
}

USRPlanetSurfaceGrid* ASRPlanet::GetSurfaceGrid() const
{
	return SurfaceGrid;
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
	OceanStaticMesh->LightingChannels.bChannel0 = true;
	OceanStaticMesh->LightingChannels.bChannel1 = false;
	OceanStaticMesh->LightingChannels.bChannel2 = false;
}

void ASRPlanet::SyncApproximateRadiusFromPlanetVisuals()
{
	ApproximateRadius = ComputeCelestialBodyStaticMeshRadius();
	if (bHasOcean)
	{
		ApproximateRadius = FMath::Max(ApproximateRadius, ComputeCelestialBodyStaticMeshRadius() * OceanScaleMultiplier);
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
