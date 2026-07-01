#include "Celestial/SRCelestialBody.h"

#include "SRCelestialBodyLog.h"
#include "Celestial/SRDynamicMeshBaseDataAsset.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	const FName PlanetCenterMaterialParameterName(TEXT("PlanetCenterWS"));
	const FName ToonLineEnabledMaterialParameterName(TEXT("ToonLineEnabled"));
	const FName ToonLineColorMaterialParameterName(TEXT("ToonLineColor"));
	const FName ToonLineThicknessMaterialParameterName(TEXT("ToonLineThickness"));

	void ApplyCelestialBodyMaterialParameters(
		UMaterialInstanceDynamic* MaterialInstance,
		const FSRToonOutlineSettings& ToonOutlineSettings,
		const FVector& PlanetCenterWS)
	{
		if (!IsValid(MaterialInstance))
		{
			return;
		}

		MaterialInstance->SetVectorParameterValue(PlanetCenterMaterialParameterName, FLinearColor(PlanetCenterWS));
		MaterialInstance->SetScalarParameterValue(ToonLineEnabledMaterialParameterName, ToonOutlineSettings.bEnableToonOutline ? 1.0f : 0.0f);
		MaterialInstance->SetVectorParameterValue(ToonLineColorMaterialParameterName, ToonOutlineSettings.ToonLineColor);
		MaterialInstance->SetScalarParameterValue(ToonLineThicknessMaterialParameterName, FMath::Max(0.0f, ToonOutlineSettings.ToonLineThickness));
	}

	UMaterialInstanceDynamic* ResolveComponentMaterialInstance(
		UObject* Outer,
		UMeshComponent* MeshComponent,
		int32 MaterialSlotIndex,
		UMaterialInterface* DesiredBaseMaterial)
	{
		if (!IsValid(Outer) || !IsValid(MeshComponent) || !IsValid(DesiredBaseMaterial))
		{
			return nullptr;
		}

		UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(MeshComponent->GetMaterial(MaterialSlotIndex));
		const UMaterialInstance* DynamicMaterialInstance = DynamicMaterial;
		if (!IsValid(DynamicMaterial) || DynamicMaterialInstance->Parent != DesiredBaseMaterial)
		{
			DynamicMaterial = UMaterialInstanceDynamic::Create(DesiredBaseMaterial, Outer);
			MeshComponent->SetMaterial(MaterialSlotIndex, DynamicMaterial);
		}
		return DynamicMaterial;
	}

	void RefreshComponentMaterialParameters(
		UMeshComponent* MeshComponent,
		const FSRToonOutlineSettings& ToonOutlineSettings,
		const FVector& PlanetCenterWS)
	{
		if (!IsValid(MeshComponent))
		{
			return;
		}

		const int32 MaterialCount = MeshComponent->GetNumMaterials();
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
		{
			ApplyCelestialBodyMaterialParameters(
				Cast<UMaterialInstanceDynamic>(MeshComponent->GetMaterial(MaterialIndex)),
				ToonOutlineSettings,
				PlanetCenterWS);
		}
	}
}

void ASRCelestialBody::RefreshMaterialParameters()
{
	const FVector PlanetCenterWS = GetActorLocation();
	for (UDynamicMeshComponent* DynamicMeshComponent : CelestialBodyDynamicMeshFaces)
	{
		RefreshComponentMaterialParameters(DynamicMeshComponent, ToonOutlineSettings, PlanetCenterWS);
	}
	RefreshComponentMaterialParameters(CelestialBodyStaticMesh.Get(), ToonOutlineSettings, PlanetCenterWS);
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
	return DynamicMeshState.HasBuild();
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

	if (IsValid(CelestialBodyStaticMesh.Get()))
	{
		ResolveComponentMaterialInstance(this, CelestialBodyStaticMesh.Get(), 0, DesiredBaseMaterial);
	}

	for (UDynamicMeshComponent* DynamicMeshComponent : CelestialBodyDynamicMeshFaces)
	{
		if (!IsValid(DynamicMeshComponent))
		{
			continue;
		}

		ResolveComponentMaterialInstance(this, DynamicMeshComponent, 0, DesiredBaseMaterial);
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
					ResolveComponentMaterialInstance(this, DynamicMeshComponent, MaterialSlotIndex, BiomeMaterial);
				}
			}
		}
	}

	RefreshMaterialParameters();
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

void ASRCelestialBody::ResetDynamicMeshCellColorData()
{
	DynamicMeshState.Reset();
}

UMaterialInstanceDynamic* ASRCelestialBody::GetActiveBodyDynamicMaterial() const
{
	return IsValid(CelestialBodyDynamicMesh.Get())
		? Cast<UMaterialInstanceDynamic>(CelestialBodyDynamicMesh->GetMaterial(0))
		: nullptr;
}

bool ASRCelestialBody::ApplyToonOutlineToPrimitive(
	UPrimitiveComponent* PrimitiveComponent,
	bool bEnableToonOutline) const
{
	if (!IsValid(PrimitiveComponent))
	{
		return false;
	}

	const int32 StencilValue = FMath::Clamp(ToonOutlineSettings.ToonOutlineStencilValue, 1, 255);
	PrimitiveComponent->SetRenderCustomDepth(bEnableToonOutline);
	PrimitiveComponent->SetCustomDepthStencilValue(StencilValue);
	return true;
}

int32 ASRCelestialBody::ApplyToonOutlineToBodyMeshComponents()
{
	int32 AppliedComponentCount = 0;
	const bool bEnableToonOutline = ToonOutlineSettings.bEnableToonOutline;
	if (ApplyToonOutlineToPrimitive(CelestialBodyStaticMesh.Get(), bEnableToonOutline))
	{
		++AppliedComponentCount;
	}

	for (UDynamicMeshComponent* DynamicMeshComponent : CelestialBodyDynamicMeshFaces)
	{
		if (ApplyToonOutlineToPrimitive(DynamicMeshComponent, bEnableToonOutline))
		{
			++AppliedComponentCount;
		}
	}

	return AppliedComponentCount;
}

void ASRCelestialBody::ApplyToonOutlineSettings()
{
	const int32 BodyMeshComponentCount = ApplyToonOutlineToBodyMeshComponents();
	if (UWorld* World = GetWorld(); World && World->IsGameWorld())
	{
		UE_LOG(
			LogStarRoversCelestial,
			Log,
			TEXT("ToonOutline Body='%s' Enabled=%s Stencil=%d BodyComponents=%d Ocean=false Atmosphere=false"),
			*GetName(),
			ToonOutlineSettings.bEnableToonOutline ? TEXT("true") : TEXT("false"),
			FMath::Clamp(ToonOutlineSettings.ToonOutlineStencilValue, 1, 255),
			BodyMeshComponentCount);
	}
}
