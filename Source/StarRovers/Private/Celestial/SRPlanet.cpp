#include "Celestial/SRPlanet.h"

#include "Components/LineBatchComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Simulation/SROrbit.h"
#include "Surface/SRPlanetTerrainGenerator.h"
#include "Surface/SRPlanetSurfaceGrid.h"

namespace
{
	constexpr float SurfaceGridTargetCellSize = 12500.0f;
	constexpr int32 OceanScaleSampleCount = 512;
	constexpr float OceanSurfacePaddingRatio = 0.01f;

	FVector BuildFibonacciSphereDirection(const int32 Index, const int32 Count)
	{
		constexpr float GoldenAngle = PI * (3.0f - 2.2360679775f);
		const float SafeCount = FMath::Max(1.0f, static_cast<float>(Count));
		const float Z = 1.0f - ((static_cast<float>(Index) + 0.5f) * 2.0f / SafeCount);
		const float Radius = FMath::Sqrt(FMath::Max(0.0f, 1.0f - (Z * Z)));
		const float Theta = GoldenAngle * static_cast<float>(Index);
		return FVector(FMath::Cos(Theta) * Radius, FMath::Sin(Theta) * Radius, Z).GetSafeNormal();
	}
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
	GridOverlayMaterial = nullptr;
	ShowOrbitLine = true;
	OrbitLineColor = FLinearColor(0.2f, 0.75f, 1.0f, 1.0f);
	OrbitLineOpacity = 0.85f;
	OrbitLineThickness = 20.0f;
	OrbitLineSegments = 96;
	CanConstruct = false;
	SurfaceGridHeightOffset = 0.0f;
	bHasOcean = false;
	OceanMesh = nullptr;
	OceanMaterial = nullptr;
	OceanScaleMultiplier = 1.0f;

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

void ASRPlanet::SetData(const FSRCelestialBodyData& NewData)
{
	CanConstruct = NewData.bCanConstruct;
	SurfaceGridHeightOffset = NewData.SurfaceGridHeightOffset;
	bHasOcean = NewData.bHasOcean;
	OceanMesh = NewData.OceanMesh;
	OceanMaterial = NewData.OceanMaterial;
	OceanScaleMultiplier = NewData.OceanScaleMultiplier;
	ShowOrbitLine = NewData.ShowOrbitLine;
	OrbitLineColor = NewData.OrbitLineColor;
	OrbitLineOpacity = NewData.OrbitLineOpacity;
	OrbitLineThickness = NewData.OrbitLineThickness;
	OrbitLineSegments = NewData.OrbitLineSegments;
	ConstructionHeightOffset = NewData.ConstructionHeightOffset;
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
	ConstructionHeightOffset = FMath::Max(0.0f, ConstructionHeightOffset);
	GridLineThickness = FMath::Clamp(GridLineThickness, 0.0f, 2.0f);
	GridLineOpacity = FMath::Clamp(GridLineOpacity, 0.0f, 1.0f);
	SurfaceGridHeightOffset = FMath::Clamp(SurfaceGridHeightOffset, 0.0f, 1.0f);
	OceanScaleMultiplier = FMath::Max(0.01f, OceanScaleMultiplier);
	OrbitLineOpacity = FMath::Clamp(OrbitLineOpacity, 0.0f, 1.0f);
	OrbitLineSegments = FMath::Max(3, OrbitLineSegments);
	OrbitLineThickness = FMath::Max(0.0f, OrbitLineThickness);

	if (IsValid(OceanStaticMesh))
	{
		OceanStaticMesh->SetRelativeLocation(FVector::ZeroVector);
		OceanStaticMesh->SetRelativeRotation(FRotator::ZeroRotator);
		OceanStaticMesh->SetRelativeScale3D(FVector(ResolveOceanScale()));
	}

	ApplyOceanStaticMeshSettings();
	if (IsValid(ClickSphereCollision))
	{
		const float BodyRadius = IsValid(StaticMesh.Get())
			? StaticMesh->GetBounds().SphereRadius * Scale
			: 0.0f;
		const float OceanRadius = bHasOcean && IsValid(OceanMesh.Get())
			? OceanMesh->GetBounds().SphereRadius * ResolveOceanScale()
			: 0.0f;
		ClickSphereCollision->SetSphereRadius(FMath::Max(FMath::Max(BodyRadius, OceanRadius), 1.0f));
	}

	if (SupportsSurfaceGrid())
	{
		EnsureSurfaceGrid();
		if (IsValid(SurfaceGrid))
		{
			const float BodyRadius = IsValid(StaticMesh.Get())
				? StaticMesh->GetBounds().SphereRadius * Scale
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
			SurfaceGrid->ConfigureConstructionHeightOffset(ConstructionHeightOffset);
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
	CurrentData.ConstructionHeightOffset = ConstructionHeightOffset;
	CurrentData.GenerationSeed = GenerationSeed;
	CurrentData.DynamicMeshGeneration = DynamicMeshGeneration;
	CurrentData.bHasOcean = bHasOcean;
	CurrentData.OceanMesh = OceanMesh;
	CurrentData.OceanMaterial = OceanMaterial;
	CurrentData.OceanScaleMultiplier = OceanScaleMultiplier;
	CurrentData.ShowOrbitLine = ShowOrbitLine;
	CurrentData.OrbitLineColor = OrbitLineColor;
	CurrentData.OrbitLineOpacity = OrbitLineOpacity;
	CurrentData.OrbitLineSegments = OrbitLineSegments;
	CurrentData.OrbitLineThickness = OrbitLineThickness;
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

void ASRPlanet::SetCelestialBodyMesh(bool bUseDynamicMesh)
{
	Super::SetCelestialBodyMesh(bUseDynamicMesh);

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

	const float OceanScale = ResolveOceanScale();
	OceanStaticMesh->SetRelativeLocation(FVector::ZeroVector);
	OceanStaticMesh->SetRelativeRotation(FRotator::ZeroRotator);
	OceanStaticMesh->SetRelativeScale3D(FVector(OceanScale));
	OceanStaticMesh->SetVisibility(true);
	OceanStaticMesh->SetHiddenInGame(false);
}

float ASRPlanet::ResolveOceanScale() const
{
	const float AutoOceanScaleMultiplier = EstimateProceduralOceanScaleMultiplier();
	const float ResolvedOceanScaleMultiplier = AutoOceanScaleMultiplier > KINDA_SMALL_NUMBER
		? AutoOceanScaleMultiplier
		: OceanScaleMultiplier;
	return FMath::Max(0.01f, Scale * ResolvedOceanScaleMultiplier);
}

float ASRPlanet::EstimateProceduralOceanScaleMultiplier() const
{
	if (!DynamicMeshGeneration.bDynamicMeshGeneration || DynamicMeshGeneration.DynamicMeshHeight <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	if (!IsValid(StaticMesh.Get()) || !IsValid(OceanMesh.Get()))
	{
		return 0.0f;
	}

	const float BodyMeshRadius = StaticMesh->GetBounds().SphereRadius;
	const float OceanMeshRadius = OceanMesh->GetBounds().SphereRadius;
	if (BodyMeshRadius <= KINDA_SMALL_NUMBER || OceanMeshRadius <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	bool bHasWaterSample = false;
	float HighestWaterHeightOffset = -BIG_NUMBER;
	for (int32 SampleIndex = 0; SampleIndex < OceanScaleSampleCount; ++SampleIndex)
	{
		const FVector Direction = BuildFibonacciSphereDirection(SampleIndex, OceanScaleSampleCount);
		const FSRPlanetTerrainSample TerrainSample = FSRPlanetTerrainGenerator::SampleTerrain(Direction, DynamicMeshGeneration);
		const bool bWaterBiome = TerrainSample.Biome == ESRPlanetBiome::Ocean
			|| (TerrainSample.Biome == ESRPlanetBiome::Coast && (TerrainSample.RiverMask > 0.58f || TerrainSample.LakeMask > 0.38f));
		if (!bWaterBiome)
		{
			continue;
		}

		bHasWaterSample = true;
		HighestWaterHeightOffset = FMath::Max(HighestWaterHeightOffset, TerrainSample.HeightOffset);
	}

	if (!bHasWaterSample)
	{
		return 0.0f;
	}

	const float SurfacePadding = FMath::Max(0.0f, DynamicMeshGeneration.DynamicMeshHeight) * OceanSurfacePaddingRatio;
	const float DesiredOceanRadius = FMath::Max(1.0f, BodyMeshRadius + HighestWaterHeightOffset + SurfacePadding);
	return DesiredOceanRadius / OceanMeshRadius;
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
