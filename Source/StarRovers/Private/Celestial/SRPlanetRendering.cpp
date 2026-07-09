#include "Celestial/SRPlanet.h"

#include "SRCelestialBodyLog.h"
#include "Celestial/SRCelestialBodyDynamicMeshPipeline.h"
#include "Celestial/SRDynamicMeshBaseDataAsset.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Surface/SRPlanetSurfaceGridLibrary.h"
#include "Surface/SRPlanetTerrainGenerator.h"
#include "Rendering/SRScreenSpaceLineThickness.h"

using namespace StarRovers::Celestial::DynamicMesh;

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

	FSRScreenSpaceLineViewInfo CameraInfo;
	FSRScreenSpaceLineThickness::TryBuildPrimaryCameraViewInfo(GetWorld(), CameraInfo);

	float ReferenceViewDepth = FSRScreenSpaceLineThickness::DefaultReferenceViewDepth;
	float ReferenceFieldOfViewDegrees = FSRScreenSpaceLineThickness::DefaultReferenceFieldOfViewDegrees;
	FSRScreenSpaceLineThickness::ResolveReferenceViewParameters(GetWorld(), ReferenceViewDepth, ReferenceFieldOfViewDegrees);

	const float CenterThickness = FSRScreenSpaceLineThickness::ComputeWorldThicknessForScreenSpaceLine(
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
		return FSRScreenSpaceLineThickness::ComputeWorldThicknessForScreenSpaceLine(
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

uint32 ASRPlanet::ComputeShellDynamicMeshBuildHash(const USRDynamicMeshBaseDataAsset* ShellBaseDataAsset) const
{
	if (!IsValid(ShellBaseDataAsset))
	{
		return 0;
	}

	uint32 Hash = PointerHash(ShellBaseDataAsset);
	Hash = HashCombine(Hash, ::GetTypeHash(static_cast<uint8>(ShellBaseDataAsset->BaseShape)));
	Hash = HashCombine(Hash, ::GetTypeHash(ShellBaseDataAsset->GetClampedFaceResolution()));
	Hash = HashCombine(Hash, ::GetTypeHash(ShellBaseDataAsset->GetSafeBaseRadius()));
	Hash = HashCombine(Hash, ::GetTypeHash(ShellBaseDataAsset->PrecomputedFaceResolution));
	Hash = HashCombine(Hash, ::GetTypeHash(ShellBaseDataAsset->PrecomputedCells.Num()));
	return Hash;
}

bool ASRPlanet::BuildShellDynamicMesh(
	UDynamicMeshComponent* TargetComponent,
	USRDynamicMeshBaseDataAsset* ShellBaseDataAsset,
	TObjectPtr<USRDynamicMeshBaseDataAsset>& InOutCachedBaseDataAsset,
	uint32& InOutCachedBuildHash,
	const TCHAR* ShellName)
{
	if (!IsValid(TargetComponent) || !IsValid(ShellBaseDataAsset))
	{
		return false;
	}
	if (ShellBaseDataAsset->BaseShape != ESRDynamicMeshBaseShape::CubeSphere)
	{
		UE_LOG(LogTemp, Warning, TEXT("Planet '%s' cannot build %s shell from unsupported shape."), *GetName(), ShellName ? ShellName : TEXT("dynamic"));
		return false;
	}

	const uint32 BuildHash = ComputeShellDynamicMeshBuildHash(ShellBaseDataAsset);
	if (InOutCachedBaseDataAsset.Get() == ShellBaseDataAsset && InOutCachedBuildHash == BuildHash)
	{
		return true;
	}

	const int32 FaceResolution = ShellBaseDataAsset->GetClampedFaceResolution();
	const float SourceBodyRadius = ShellBaseDataAsset->GetSafeBaseRadius();
	const TArray<FSRDynamicMeshBasePrecomputedCell>* PrecomputedBaseCells = ShellBaseDataAsset->GetValidPrecomputedCells();
	const bool bUsingPrecomputedBaseCells = PrecomputedBaseCells != nullptr;
	const float PrecomputedBaseCellScale = bUsingPrecomputedBaseCells
		? ShellBaseDataAsset->GetPrecomputedCellScale(SourceBodyRadius)
		: 1.0f;

	TArray<FSRPlanetSurfaceGridCell> GeneratedBaseCells;
	if (!bUsingPrecomputedBaseCells)
	{
		GeneratedBaseCells = USRPlanetSurfaceGridLibrary::GenerateCubeSphereCells(FaceResolution, SourceBodyRadius);
	}

	const int32 BaseCellCount = bUsingPrecomputedBaseCells ? PrecomputedBaseCells->Num() : GeneratedBaseCells.Num();
	TArray<UE::Geometry::FDynamicMesh3> ShellMeshes;
	ShellMeshes.SetNum(1);
	ShellMeshes[0].EnableAttributes();
	ShellMeshes[0].Attributes()->EnablePrimaryColors();
	ShellMeshes[0].Attributes()->SetNumUVLayers(2);
	ShellMeshes[0].Attributes()->EnableMaterialID();

	TMap<FSRCelestialBodyDynamicMeshTerrainVertexKey, int32> WeldedVertexIds;
	WeldedVertexIds.Reserve((FaceResolution + 1) * (FaceResolution + 1) * CubeSphereFaceComponentCount);

	for (int32 BaseCellIndex = 0; BaseCellIndex < BaseCellCount; ++BaseCellIndex)
	{
		const FSRCelestialBodyDynamicMeshBaseCellView BaseCell = MakeDynamicMeshBaseCellView(
			BaseCellIndex,
			PrecomputedBaseCells,
			PrecomputedBaseCellScale,
			GeneratedBaseCells);
		if (!BaseCell.CellId.IsValid(FaceResolution))
		{
			continue;
		}

		const FVector NormalReferenceDirection = BaseCell.LocalCenter.GetSafeNormal();
		AppendFlatColoredDynamicMeshQuad(
			ShellMeshes,
			WeldedVertexIds,
			0,
			BaseCell.Corner00,
			BaseCell.Corner10,
			BaseCell.Corner11,
			BaseCell.Corner01,
			FLinearColor::White,
			0,
			false,
			nullptr,
			&NormalReferenceDirection);
	}

	TargetComponent->SetMesh(MoveTemp(ShellMeshes[0]));
	InOutCachedBaseDataAsset = ShellBaseDataAsset;
	InOutCachedBuildHash = BuildHash;
	return true;
}

void ASRPlanet::ApplyOceanMeshSettings()
{
	ApplyOceanDynamicMeshSettings();
}

void ASRPlanet::ApplyOceanDynamicMeshSettings()
{
	if (!IsValid(OceanDynamicMesh))
	{
		return;
	}

	ConfigureShellDynamicMeshComponent(OceanDynamicMesh.Get());

	if (!bHasOcean)
	{
		OceanDynamicMesh->SetVisibility(false);
		OceanDynamicMesh->SetHiddenInGame(true);
		return;
	}

	if (!IsValid(OceanDynamicMeshBaseDataAsset.Get()))
	{
		UE_LOG(LogTemp, Error, TEXT("Planet '%s' requires OceanDynamicMeshBaseDataAsset while ocean is enabled."), *GetName());
		OceanDynamicMesh->SetVisibility(false);
		OceanDynamicMesh->SetHiddenInGame(true);
		return;
	}

	UMaterialInterface* DesiredOceanMaterial = OceanMaterial.Get();
	if (!IsValid(DesiredOceanMaterial))
	{
		UE_LOG(LogTemp, Error, TEXT("Planet '%s' requires OceanMaterial while ocean is enabled."), *GetName());
		OceanDynamicMesh->SetVisibility(false);
		OceanDynamicMesh->SetHiddenInGame(true);
		return;
	}

	if (!BuildShellDynamicMesh(
			OceanDynamicMesh.Get(),
			OceanDynamicMeshBaseDataAsset.Get(),
			CachedOceanDynamicMeshBaseDataAsset,
			CachedOceanDynamicMeshBuildHash,
			TEXT("ocean")))
	{
		OceanDynamicMesh->SetVisibility(false);
		OceanDynamicMesh->SetHiddenInGame(true);
		return;
	}

	OceanDynamicMesh->SetMaterial(0, DesiredOceanMaterial);
	OceanDynamicMesh->SetRelativeLocation(FVector::ZeroVector);
	OceanDynamicMesh->SetRelativeRotation(FRotator::ZeroRotator);
	OceanDynamicMesh->SetRelativeScale3D(FVector(ResolveOceanDynamicMeshScale()));
	OceanDynamicMesh->SetVisibility(true);
	OceanDynamicMesh->SetHiddenInGame(false);
}

void ASRPlanet::ConfigureShellDynamicMeshComponent(UDynamicMeshComponent* ShellMesh) const
{
	if (!IsValid(ShellMesh))
	{
		return;
	}

	ShellMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShellMesh->SetGenerateOverlapEvents(false);
	ShellMesh->SetCastShadow(false);
	ShellMesh->bCastDynamicShadow = false;
	ShellMesh->bCastStaticShadow = false;
	ShellMesh->bCastVolumetricTranslucentShadow = false;
	ShellMesh->bCastContactShadow = false;
	ShellMesh->bSelfShadowOnly = false;
	ShellMesh->bCastFarShadow = false;
	ShellMesh->bCastInsetShadow = false;
	ShellMesh->bCastCinematicShadow = false;
	ShellMesh->bCastHiddenShadow = false;
	ShellMesh->bAffectDynamicIndirectLighting = false;
	ShellMesh->bAffectDistanceFieldLighting = false;
	ShellMesh->bReceivesDecals = false;
	ShellMesh->bVisibleInReflectionCaptures = false;
	ShellMesh->bVisibleInRealTimeSkyCaptures = false;
	ShellMesh->bVisibleInRayTracing = false;
}

float ASRPlanet::ResolveOceanDynamicMeshScale() const
{
	const USRDynamicMeshBaseDataAsset* ShellBase = OceanDynamicMeshBaseDataAsset.Get();
	if (!IsValid(ShellBase))
	{
		return FMath::Max(0.01f, Scale * OceanScaleMultiplier);
	}

	const float ShellBaseRadius = ShellBase->GetSafeBaseRadius();
	if (ShellBaseRadius <= KINDA_SMALL_NUMBER)
	{
		return FMath::Max(0.01f, Scale * OceanScaleMultiplier);
	}

	const float BodyMeshRadius = IsValid(StaticMesh.Get())
		? StaticMesh->GetBounds().SphereRadius
		: IsValid(DynamicMeshBaseDataAsset.Get())
			? DynamicMeshBaseDataAsset->GetSafeBaseRadius()
			: 0.0f;
	const float ProceduralOceanRadius = ComputeProceduralOceanLocalRadius();
	const float DesiredOceanRadius = ProceduralOceanRadius > KINDA_SMALL_NUMBER
		? ProceduralOceanRadius
		: BodyMeshRadius * OceanScaleMultiplier;
	return FMath::Max(0.01f, Scale * (DesiredOceanRadius / ShellBaseRadius));
}

float ASRPlanet::ComputeProceduralOceanLocalRadius() const
{
	if (!DynamicMeshGeneration.bDynamicMeshGeneration || DynamicMeshGeneration.DynamicMeshHeight <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const float BodyMeshRadius = IsValid(StaticMesh.Get())
		? StaticMesh->GetBounds().SphereRadius
		: IsValid(DynamicMeshBaseDataAsset.Get())
			? DynamicMeshBaseDataAsset->GetSafeBaseRadius()
			: 0.0f;
	if (BodyMeshRadius <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const float TerrainHeightStep =
		DynamicMeshGeneration.bMinecraft && IsValid(DynamicMeshBaseDataAsset.Get())
			? ComputeRegularCubeFaceCellEdgeLength(DynamicMeshBaseDataAsset->GetSafeBaseRadius(BodyMeshRadius), DynamicMeshBaseDataAsset->GetClampedFaceResolution())
			: 0.0f;

	float HighestWaterHeightOffset = 0.0f;
	if (!FSRPlanetTerrainGenerator::TryResolveOceanLevelHeightOffset(
			DynamicMeshGeneration,
			OceanScaleSampleCount,
			TerrainHeightStep,
			HighestWaterHeightOffset))
	{
		return 0.0f;
	}

	const float SurfacePadding = FMath::Max(0.0f, DynamicMeshGeneration.DynamicMeshHeight) * OceanSurfacePaddingRatio;
	return FMath::Max(1.0f, BodyMeshRadius + HighestWaterHeightOffset + SurfacePadding);
}

void ASRPlanet::ApplyAtmosphereMeshSettings()
{
	ApplyAtmosphereDynamicMeshSettings();
}

void ASRPlanet::ApplyToonOutlineSettings()
{
	const int32 BodyMeshComponentCount = ApplyToonOutlineToBodyMeshComponents();
	const bool bEnableOceanToonOutline =
		ToonOutlineSettings.bEnableToonOutline
		&& ToonOutlineSettings.bApplyToonOutlineToOcean
		&& bHasOcean;
	const bool bEnableAtmosphereToonOutline =
		ToonOutlineSettings.bEnableToonOutline
		&& ToonOutlineSettings.bApplyToonOutlineToAtmosphere
		&& bHasAtmosphere;

	const bool bAppliedOceanComponent = ApplyToonOutlineToPrimitive(OceanDynamicMesh.Get(), bEnableOceanToonOutline);
	const bool bAppliedAtmosphereComponent = ApplyToonOutlineToPrimitive(AtmosphereDynamicMesh.Get(), bEnableAtmosphereToonOutline);
	if (UWorld* World = GetWorld(); World && World->IsGameWorld())
	{
		UE_LOG(
			LogStarRoversCelestial,
			Log,
			TEXT("ToonOutline Body='%s' Enabled=%s Stencil=%d BodyComponents=%d Ocean=%s OceanComponent=%s Atmosphere=%s AtmosphereComponent=%s"),
			*GetName(),
			ToonOutlineSettings.bEnableToonOutline ? TEXT("true") : TEXT("false"),
			FMath::Clamp(ToonOutlineSettings.ToonOutlineStencilValue, 1, 255),
			BodyMeshComponentCount,
			bEnableOceanToonOutline ? TEXT("true") : TEXT("false"),
			bAppliedOceanComponent ? TEXT("true") : TEXT("false"),
			bEnableAtmosphereToonOutline ? TEXT("true") : TEXT("false"),
			bAppliedAtmosphereComponent ? TEXT("true") : TEXT("false"));
	}
}

void ASRPlanet::ApplyAtmosphereDynamicMeshSettings()
{
	if (!IsValid(AtmosphereDynamicMesh))
	{
		return;
	}

	ConfigureShellDynamicMeshComponent(AtmosphereDynamicMesh.Get());

	if (!bHasAtmosphere)
	{
		AtmosphereDynamicMesh->SetVisibility(false);
		AtmosphereDynamicMesh->SetHiddenInGame(true);
		return;
	}

	if (!IsValid(AtmosphereDynamicMeshBaseDataAsset.Get()))
	{
		UE_LOG(LogTemp, Error, TEXT("Planet '%s' requires AtmosphereDynamicMeshBaseDataAsset while atmosphere is enabled."), *GetName());
		AtmosphereDynamicMesh->SetVisibility(false);
		AtmosphereDynamicMesh->SetHiddenInGame(true);
		return;
	}

	UMaterialInterface* DesiredAtmosphereMaterial = AtmosphereMaterial.Get();
	if (!IsValid(DesiredAtmosphereMaterial))
	{
		UE_LOG(LogTemp, Error, TEXT("Planet '%s' requires AtmosphereMaterial while atmosphere is enabled."), *GetName());
		AtmosphereDynamicMesh->SetVisibility(false);
		AtmosphereDynamicMesh->SetHiddenInGame(true);
		return;
	}

	if (!BuildShellDynamicMesh(
			AtmosphereDynamicMesh.Get(),
			AtmosphereDynamicMeshBaseDataAsset.Get(),
			CachedAtmosphereDynamicMeshBaseDataAsset,
			CachedAtmosphereDynamicMeshBuildHash,
			TEXT("atmosphere")))
	{
		AtmosphereDynamicMesh->SetVisibility(false);
		AtmosphereDynamicMesh->SetHiddenInGame(true);
		return;
	}

	AtmosphereDynamicMesh->SetMaterial(0, DesiredAtmosphereMaterial);
	AtmosphereDynamicMesh->SetRelativeLocation(FVector::ZeroVector);
	AtmosphereDynamicMesh->SetRelativeRotation(FRotator::ZeroRotator);
	AtmosphereDynamicMesh->SetRelativeScale3D(FVector(ResolveAtmosphereDynamicMeshScale()));
	AtmosphereDynamicMesh->SetVisibility(true);
	AtmosphereDynamicMesh->SetHiddenInGame(false);
}

float ASRPlanet::ResolveAtmosphereDynamicMeshScale() const
{
	const USRDynamicMeshBaseDataAsset* ShellBase = AtmosphereDynamicMeshBaseDataAsset.Get();
	if (!IsValid(ShellBase))
	{
		const float AtmosphereThreshold = FMath::Max(0.01f, DynamicMeshGeneration.AtmosphereThreshold);
		return FMath::Max(0.01f, Scale * AtmosphereScaleMultiplier * AtmosphereThreshold);
	}

	const float ShellBaseRadius = ShellBase->GetSafeBaseRadius();
	const float BodyMeshRadius = IsValid(StaticMesh.Get())
		? StaticMesh->GetBounds().SphereRadius
		: IsValid(DynamicMeshBaseDataAsset.Get())
			? DynamicMeshBaseDataAsset->GetSafeBaseRadius()
			: 0.0f;
	if (ShellBaseRadius <= KINDA_SMALL_NUMBER || BodyMeshRadius <= KINDA_SMALL_NUMBER)
	{
		return FMath::Max(0.01f, Scale * AtmosphereScaleMultiplier);
	}

	const float AtmosphereThreshold = FMath::Max(0.01f, DynamicMeshGeneration.AtmosphereThreshold);
	const float DesiredAtmosphereRadius = BodyMeshRadius * AtmosphereThreshold;
	return FMath::Max(0.01f, Scale * AtmosphereScaleMultiplier * (DesiredAtmosphereRadius / ShellBaseRadius));
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
