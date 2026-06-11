#include "Celestial/SRPlanet.h"

#include "Celestial/SRDynamicMeshBaseDataAsset.h"
#include "Components/LineBatchComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Conveyor/SRConveyorNetworkComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Simulation/SROrbit.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetTerrainGenerator.h"
#include "Surface/SRPlanetSurfaceGrid.h"
#include "UObject/ConstructorHelpers.h"
#include "Visual/SRLineThicknessUtils.h"

namespace
{
	constexpr float SurfaceGridTargetCellSize = 12500.0f;
	constexpr int32 OceanScaleSampleCount = 512;
	constexpr float OceanSurfacePaddingRatio = 0.01f;
	constexpr float RotationAxisSurfaceClearanceRatio = 0.06f;

	void SetRotationAxisSplineVisible(USplineMeshComponent* SplineMesh, const bool bVisible)
	{
		if (IsValid(SplineMesh))
		{
			SplineMesh->SetVisibility(bVisible);
			SplineMesh->SetHiddenInGame(!bVisible);
		}
	}

	void ApplyRotationAxisMaterialParameters(UMaterialInstanceDynamic* MaterialInstance, const FLinearColor& Color)
	{
		if (!IsValid(MaterialInstance))
		{
			return;
		}

		MaterialInstance->SetVectorParameterValue(TEXT("Color"), Color);
		MaterialInstance->SetVectorParameterValue(TEXT("BaseColor"), Color);
		MaterialInstance->SetVectorParameterValue(TEXT("TintColor"), Color);
		MaterialInstance->SetScalarParameterValue(TEXT("Opacity"), Color.A);
		MaterialInstance->SetScalarParameterValue(TEXT("Alpha"), Color.A);
	}

	float ComputeSplineMeshCrossScale(const UStaticMesh* Mesh, const float WorldThickness)
	{
		if (!IsValid(Mesh))
		{
			return 1.0f;
		}

		const FBoxSphereBounds MeshBounds = Mesh->GetBounds();
		const float MeshDiameter = FMath::Max(MeshBounds.BoxExtent.X, MeshBounds.BoxExtent.Y) * 2.0f;
		if (MeshDiameter <= KINDA_SMALL_NUMBER)
		{
			return 1.0f;
		}

		return FMath::Max(0.001f, WorldThickness / MeshDiameter);
	}

	void ConfigureRotationAxisSplineSegment(
		USplineMeshComponent* SplineMesh,
		UStaticMesh* Mesh,
		UMaterialInstanceDynamic* MaterialInstance,
		const FVector& StartPoint,
		const FVector& EndPoint,
		const float WorldThickness)
	{
		if (!IsValid(SplineMesh) || !IsValid(Mesh))
		{
			SetRotationAxisSplineVisible(SplineMesh, false);
			return;
		}

		const FVector Tangent = EndPoint - StartPoint;
		const float CrossScale = ComputeSplineMeshCrossScale(Mesh, WorldThickness);

		SplineMesh->SetStaticMesh(Mesh);
		SplineMesh->SetForwardAxis(ESplineMeshAxis::Z, false);
		SplineMesh->SetStartAndEnd(StartPoint, Tangent, EndPoint, Tangent, false);
		SplineMesh->SetStartScale(FVector2D(CrossScale, CrossScale), false);
		SplineMesh->SetEndScale(FVector2D(CrossScale, CrossScale), true);
		if (IsValid(MaterialInstance))
		{
			SplineMesh->SetMaterial(0, MaterialInstance);
		}
		SetRotationAxisSplineVisible(SplineMesh, true);
	}

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
	OceanMesh = nullptr;
	OceanMaterial = nullptr;
	OceanScaleMultiplier = 1.0f;
	bHasAtmosphere = false;
	AtmosphereMesh = nullptr;
	AtmosphereMaterial = nullptr;
	AtmosphereScaleMultiplier = 1.0f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> RotationAxisCylinderMeshFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (RotationAxisCylinderMeshFinder.Succeeded())
	{
		RotationAxisSplineMesh = RotationAxisCylinderMeshFinder.Object;
	}

	Orbit = CreateDefaultSubobject<USROrbit>(TEXT("Orbit"));

	OrbitLineBatch = CreateDefaultSubobject<ULineBatchComponent>(TEXT("OrbitLineBatch"));
	OrbitLineBatch->SetupAttachment(SceneRoot);
	OrbitLineBatch->SetMobility(EComponentMobility::Movable);
	OrbitLineBatch->SetUsingAbsoluteLocation(true);
	OrbitLineBatch->SetUsingAbsoluteRotation(true);
	OrbitLineBatch->SetUsingAbsoluteScale(true);
	OrbitLineBatch->ComponentTags.AddUnique(TEXT("StarRovers.OrbitLine"));
	OrbitLineBatch->ComponentTags.AddUnique(TEXT("StarRovers.OrbitLineRoot"));

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

	OceanStaticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OceanStaticMesh"));
	OceanStaticMesh->SetupAttachment(SceneRoot);
	OceanStaticMesh->SetMobility(EComponentMobility::Movable);
	OceanStaticMesh->SetVisibility(false);
	OceanStaticMesh->SetHiddenInGame(true);

	AtmosphereStaticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AtmosphereStaticMesh"));
	AtmosphereStaticMesh->SetupAttachment(SceneRoot);
	AtmosphereStaticMesh->SetMobility(EComponentMobility::Movable);
	AtmosphereStaticMesh->SetVisibility(false);
	AtmosphereStaticMesh->SetHiddenInGame(true);

	SurfaceGrid = CreateDefaultSubobject<USRPlanetSurfaceGrid>(TEXT("SurfaceGrid"));
	SurfaceGrid->SetupAttachment(SceneRoot);

	ConveyorNetwork = CreateDefaultSubobject<USRConveyorNetworkComponent>(TEXT("ConveyorNetwork"));
	ConveyorNetwork->SetupAttachment(SceneRoot);

	StructureInstanceManager = CreateDefaultSubobject<USRStructureInstanceManagerComponent>(TEXT("StructureInstanceManager"));
	StructureInstanceManager->SetupAttachment(SceneRoot);
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
	OceanMesh = NewData.OceanMesh;
	OceanMaterial = NewData.OceanMaterial;
	OceanScaleMultiplier = NewData.OceanScaleMultiplier;
	bHasAtmosphere = NewData.bHasAtmosphere;
	AtmosphereMesh = NewData.AtmosphereMesh;
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

	if (IsValid(OceanStaticMesh))
	{
		OceanStaticMesh->SetRelativeLocation(FVector::ZeroVector);
		OceanStaticMesh->SetRelativeRotation(FRotator::ZeroRotator);
		OceanStaticMesh->SetRelativeScale3D(FVector(ResolveOceanScale()));
	}

	if (IsValid(AtmosphereStaticMesh))
	{
		AtmosphereStaticMesh->SetRelativeLocation(FVector::ZeroVector);
		AtmosphereStaticMesh->SetRelativeRotation(FRotator::ZeroRotator);
		AtmosphereStaticMesh->SetRelativeScale3D(FVector(ResolveAtmosphereScale()));
	}

	ApplyOceanStaticMeshSettings();
	ApplyAtmosphereStaticMeshSettings();
	if (IsValid(ClickSphereCollision))
	{
		const float BodyRadius = IsValid(StaticMesh.Get())
			? StaticMesh->GetBounds().SphereRadius * Scale
			: IsValid(DynamicMeshBaseDataAsset.Get())
				? DynamicMeshBaseDataAsset->GetSafeBaseRadius() * Scale
				: 0.0f;
		const float OceanRadius = bHasOcean && IsValid(OceanMesh.Get())
			? OceanMesh->GetBounds().SphereRadius * ResolveOceanScale()
			: 0.0f;
		const float AtmosphereRadius = bHasAtmosphere && IsValid(AtmosphereMesh.Get())
			? AtmosphereMesh->GetBounds().SphereRadius * ResolveAtmosphereScale()
			: 0.0f;
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
	CurrentData.OceanMesh = OceanMesh;
	CurrentData.OceanMaterial = OceanMaterial;
	CurrentData.OceanScaleMultiplier = OceanScaleMultiplier;
	CurrentData.bHasAtmosphere = bHasAtmosphere;
	CurrentData.AtmosphereMesh = AtmosphereMesh;
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

void ASRPlanet::SetCelestialBodyMesh(bool bUseDynamicMesh)
{
	Super::SetCelestialBodyMesh(bUseDynamicMesh);

	if (IsValid(OceanStaticMesh))
	{
		const bool bEnableOcean = bUseDynamicMesh && bHasOcean && IsValid(OceanMesh.Get()) && IsValid(OceanMaterial.Get());
		OceanStaticMesh->SetVisibility(bEnableOcean);
		OceanStaticMesh->SetHiddenInGame(!bEnableOcean);
	}

	if (IsValid(AtmosphereStaticMesh))
	{
		const bool bEnableAtmosphere = bUseDynamicMesh && bHasAtmosphere && IsValid(AtmosphereMesh.Get()) && IsValid(AtmosphereMaterial.Get());
		AtmosphereStaticMesh->SetVisibility(bEnableAtmosphere);
		AtmosphereStaticMesh->SetHiddenInGame(!bEnableAtmosphere);
	}

	RefreshRotationAxisLineVisual();
}

void ASRPlanet::RefreshRotationAxisLineVisual()
{
	if (!IsValid(RotationAxisNorthSpline) || !IsValid(RotationAxisSouthSpline))
	{
		return;
	}

	if (!ShowRotationAxisLine || RotationAxisLineOpacity <= KINDA_SMALL_NUMBER || RotationAxisLineThickness <= KINDA_SMALL_NUMBER)
	{
		SetRotationAxisSplineVisible(RotationAxisNorthSpline, false);
		SetRotationAxisSplineVisible(RotationAxisSouthSpline, false);
		return;
	}

	const float SurfaceRadius = ComputeRotationAxisSurfaceRadius();
	const float AxisRadius = ComputeRotationAxisLineRadius();
	if (SurfaceRadius <= KINDA_SMALL_NUMBER || AxisRadius <= SurfaceRadius)
	{
		SetRotationAxisSplineVisible(RotationAxisNorthSpline, false);
		SetRotationAxisSplineVisible(RotationAxisSouthSpline, false);
		return;
	}

	UStaticMesh* AxisMesh = RotationAxisSplineMesh.Get();
	if (!IsValid(AxisMesh))
	{
		SetRotationAxisSplineVisible(RotationAxisNorthSpline, false);
		SetRotationAxisSplineVisible(RotationAxisSouthSpline, false);
		return;
	}

	FSRCameraInfo CameraInfo;
	FSRLineThicknessUtils::TryBuildPrimaryCameraInfo(GetWorld(), CameraInfo);

	float ReferenceViewDepth = FSRLineThicknessUtils::DefaultReferenceViewDepth;
	float ReferenceFieldOfViewDegrees = FSRLineThicknessUtils::DefaultReferenceFieldOfViewDegrees;
	FSRLineThicknessUtils::ResolveReferenceView(GetWorld(), ReferenceViewDepth, ReferenceFieldOfViewDegrees);

	const float CenterThickness = FSRLineThicknessUtils::ComputeWorldThicknessAtLocation(
		CameraInfo,
		GetActorLocation(),
		RotationAxisLineThickness,
		ReferenceViewDepth,
		ReferenceFieldOfViewDegrees);
	const float SurfaceClearance = FMath::Max(
		SurfaceRadius * RotationAxisSurfaceClearanceRatio,
		FMath::Max(0.0f, CenterThickness) * 4.0f);
	const float SegmentStartRadius = SurfaceRadius + SurfaceClearance;
	if (AxisRadius <= SegmentStartRadius)
	{
		SetRotationAxisSplineVisible(RotationAxisNorthSpline, false);
		SetRotationAxisSplineVisible(RotationAxisSouthSpline, false);
		return;
	}

	const FLinearColor AxisColor(
		RotationAxisLineColor.R,
		RotationAxisLineColor.G,
		RotationAxisLineColor.B,
		FMath::Clamp(RotationAxisLineOpacity, 0.0f, 1.0f));

	UMaterialInterface* AxisBaseMaterial = RotationAxisMaterial.Get();
	if (!IsValid(AxisBaseMaterial))
	{
		AxisBaseMaterial = AxisMesh->GetMaterial(0);
	}

	if (IsValid(AxisBaseMaterial) && !IsValid(RotationAxisNorthMaterialInstance))
	{
		RotationAxisNorthMaterialInstance = UMaterialInstanceDynamic::Create(AxisBaseMaterial, this);
	}
	if (IsValid(AxisBaseMaterial) && !IsValid(RotationAxisSouthMaterialInstance))
	{
		RotationAxisSouthMaterialInstance = UMaterialInstanceDynamic::Create(AxisBaseMaterial, this);
	}
	ApplyRotationAxisMaterialParameters(RotationAxisNorthMaterialInstance, AxisColor);
	ApplyRotationAxisMaterialParameters(RotationAxisSouthMaterialInstance, AxisColor);

	auto ComputeAdaptiveThickness = [&](const FVector& LocalStart, const FVector& LocalEnd)
	{
		const FVector WorldMidpoint = GetActorTransform().TransformPosition((LocalStart + LocalEnd) * 0.5f);
		return FSRLineThicknessUtils::ComputeWorldThicknessAtLocation(
			CameraInfo,
			WorldMidpoint,
			RotationAxisLineThickness,
			ReferenceViewDepth,
			ReferenceFieldOfViewDegrees);
	};

	const FVector NorthStart = FVector::UpVector * SegmentStartRadius;
	const FVector NorthEnd = FVector::UpVector * AxisRadius;
	const FVector SouthStart = -FVector::UpVector * SegmentStartRadius;
	const FVector SouthEnd = -FVector::UpVector * AxisRadius;

	ConfigureRotationAxisSplineSegment(
		RotationAxisNorthSpline,
		AxisMesh,
		RotationAxisNorthMaterialInstance,
		NorthStart,
		NorthEnd,
		ComputeAdaptiveThickness(NorthStart, NorthEnd));
	ConfigureRotationAxisSplineSegment(
		RotationAxisSouthSpline,
		AxisMesh,
		RotationAxisSouthMaterialInstance,
		SouthStart,
		SouthEnd,
		ComputeAdaptiveThickness(SouthStart, SouthEnd));
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

	if (!IsValid(OceanMesh.Get()))
	{
		return 0.0f;
	}

	const float BodyMeshRadius = IsValid(StaticMesh.Get())
		? StaticMesh->GetBounds().SphereRadius
		: IsValid(DynamicMeshBaseDataAsset.Get())
			? DynamicMeshBaseDataAsset->GetSafeBaseRadius()
			: 0.0f;
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
		const bool bWaterBiome = TerrainSample.WaterRole == ESRBiomeWaterRole::Ocean
			|| TerrainSample.WaterRole == ESRBiomeWaterRole::River
			|| TerrainSample.WaterRole == ESRBiomeWaterRole::Lake
			|| (TerrainSample.WaterRole == ESRBiomeWaterRole::Coast && (TerrainSample.RiverMask > 0.58f || TerrainSample.LakeMask > 0.38f));
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

void ASRPlanet::ApplyAtmosphereStaticMeshSettings()
{
	if (!IsValid(AtmosphereStaticMesh))
	{
		return;
	}

	UStaticMesh* DesiredAtmosphereMesh = AtmosphereMesh.Get();
	if (!IsValid(DesiredAtmosphereMesh) && bHasAtmosphere)
	{
		UE_LOG(LogTemp, Error, TEXT("Planet '%s' requires AtmosphereMesh while atmosphere is enabled."), *GetName());
	}

	const bool bEnableAtmosphere = bHasAtmosphere && IsValid(DesiredAtmosphereMesh);
	if (!bEnableAtmosphere)
	{
		AtmosphereStaticMesh->SetVisibility(false);
		AtmosphereStaticMesh->SetHiddenInGame(true);
		return;
	}

	if (AtmosphereStaticMesh->GetStaticMesh() != DesiredAtmosphereMesh)
	{
		AtmosphereStaticMesh->SetStaticMesh(DesiredAtmosphereMesh);
	}

	UMaterialInterface* DesiredAtmosphereMaterial = AtmosphereMaterial.Get();
	if (!IsValid(DesiredAtmosphereMaterial))
	{
		UE_LOG(LogTemp, Error, TEXT("Planet '%s' requires AtmosphereMaterial while atmosphere is enabled."), *GetName());
		AtmosphereStaticMesh->SetVisibility(false);
		AtmosphereStaticMesh->SetHiddenInGame(true);
		return;
	}

	AtmosphereStaticMesh->SetMaterial(0, DesiredAtmosphereMaterial);

	const float ResolvedAtmosphereScale = ResolveAtmosphereScale();
	AtmosphereStaticMesh->SetRelativeLocation(FVector::ZeroVector);
	AtmosphereStaticMesh->SetRelativeRotation(FRotator::ZeroRotator);
	AtmosphereStaticMesh->SetRelativeScale3D(FVector(ResolvedAtmosphereScale));
	AtmosphereStaticMesh->SetVisibility(true);
	AtmosphereStaticMesh->SetHiddenInGame(false);
}

float ASRPlanet::ResolveAtmosphereScale() const
{
	const float AtmosphereThreshold = FMath::Max(0.01f, DynamicMeshGeneration.AtmosphereThreshold);
	if (IsValid(AtmosphereMesh.Get()))
	{
		const float BodyMeshRadius = IsValid(StaticMesh.Get())
			? StaticMesh->GetBounds().SphereRadius
			: IsValid(DynamicMeshBaseDataAsset.Get())
				? DynamicMeshBaseDataAsset->GetSafeBaseRadius()
				: 0.0f;
		const float AtmosphereMeshRadius = AtmosphereMesh->GetBounds().SphereRadius;
		if (BodyMeshRadius > KINDA_SMALL_NUMBER && AtmosphereMeshRadius > KINDA_SMALL_NUMBER)
		{
			const float DesiredAtmosphereRadius = BodyMeshRadius * AtmosphereThreshold;
			return FMath::Max(0.01f, Scale * AtmosphereScaleMultiplier * (DesiredAtmosphereRadius / AtmosphereMeshRadius));
		}
	}

	return FMath::Max(0.01f, Scale * AtmosphereScaleMultiplier * AtmosphereThreshold);
}

float ASRPlanet::ComputeRotationAxisSurfaceRadius() const
{
	const float BodyRadius = IsValid(StaticMesh.Get())
		? StaticMesh->GetBounds().SphereRadius * Scale
		: IsValid(DynamicMeshBaseDataAsset.Get())
			? DynamicMeshBaseDataAsset->GetSafeBaseRadius() * Scale
			: Scale;
	const float TerrainPadding = DynamicMeshGeneration.bDynamicMeshGeneration
		? FMath::Max(0.0f, DynamicMeshGeneration.DynamicMeshHeight) * Scale
		: 0.0f;
	return FMath::Max(BodyRadius + TerrainPadding, 1.0f);
}

float ASRPlanet::ComputeRotationAxisLineRadius() const
{
	return ComputeRotationAxisSurfaceRadius() * FMath::Max(0.0f, RotationAxisLineLengthMultiplier);
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
