#include "Celestial/SRCelestialBody.h"

#include "SRCelestialBodyLog.h"
#include "Celestial/SRDynamicMeshBaseDataAsset.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	const FName PlanetCenterMaterialParameterName(TEXT("PlanetCenterWS"));
}

void ASRCelestialBody::RefreshMaterialParameters()
{
	const FVector PlanetCenterWS = GetActorLocation();
	if (UMaterialInstanceDynamic* ActiveDynamicMaterial = GetActiveBodyDynamicMaterial())
	{
		ActiveDynamicMaterial->SetVectorParameterValue(PlanetCenterMaterialParameterName, FLinearColor(PlanetCenterWS));
	}
	if (IsValid(CelestialBodyStaticMesh.Get()))
	{
		if (UMaterialInstanceDynamic* StaticDynamicMaterial = Cast<UMaterialInstanceDynamic>(CelestialBodyStaticMesh->GetMaterial(0)))
		{
			StaticDynamicMaterial->SetVectorParameterValue(PlanetCenterMaterialParameterName, FLinearColor(PlanetCenterWS));
		}
	}
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
	return bHasCachedDynamicMeshBuildHash;
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

	UMaterialInstanceDynamic* ActiveDynamicMaterial = GetActiveBodyDynamicMaterial();
	const UMaterialInstance* ActiveMaterialInstance = ActiveDynamicMaterial;
	if (!IsValid(ActiveDynamicMaterial) || ActiveMaterialInstance->Parent != DesiredBaseMaterial)
	{
		ActiveDynamicMaterial = UMaterialInstanceDynamic::Create(DesiredBaseMaterial, this);
	}

	if (IsValid(CelestialBodyStaticMesh.Get()))
	{
		CelestialBodyStaticMesh->SetMaterial(0, IsValid(ActiveDynamicMaterial) ? ActiveDynamicMaterial : DesiredBaseMaterial);
	}

	for (UDynamicMeshComponent* DynamicMeshComponent : CelestialBodyDynamicMeshFaces)
	{
		if (!IsValid(DynamicMeshComponent))
		{
			continue;
		}

		DynamicMeshComponent->SetMaterial(0, IsValid(ActiveDynamicMaterial) ? ActiveDynamicMaterial : DesiredBaseMaterial);
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
					DynamicMeshComponent->SetMaterial(MaterialSlotIndex, BiomeMaterial);
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
	DynamicMeshColorDataByFlatId.Reset();
	HighlightedDynamicMeshColorElements.Reset();
	HighlightedDynamicMeshBaseColorByElement.Reset();
	CachedSurfaceGridCells.Reset();
	bHasCachedDynamicMeshBuildHash = false;
}

UMaterialInstanceDynamic* ASRCelestialBody::GetActiveBodyDynamicMaterial() const
{
	return IsValid(CelestialBodyDynamicMesh.Get())
		? Cast<UMaterialInstanceDynamic>(CelestialBodyDynamicMesh->GetMaterial(0))
		: nullptr;
}
