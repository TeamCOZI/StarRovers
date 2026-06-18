#include "Celestial/SRPlanet.h"

#include "Celestial/SRDynamicMeshBaseDataAsset.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Surface/SRPlanetTerrainGenerator.h"
#include "Visual/SRLineThicknessUtils.h"

namespace
{
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
