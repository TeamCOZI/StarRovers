#include "Celestial/SRCelestialBody.h"

#include "Components/DynamicMeshComponent.h"

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
