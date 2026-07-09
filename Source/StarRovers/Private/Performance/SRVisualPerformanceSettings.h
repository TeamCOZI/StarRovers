#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"

namespace StarRovers::Performance
{
	inline void ApplyLightweightVisualPrimitiveSettings(UPrimitiveComponent* PrimitiveComponent)
	{
		if (!IsValid(PrimitiveComponent))
		{
			return;
		}

		PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PrimitiveComponent->SetGenerateOverlapEvents(false);
		PrimitiveComponent->SetCanEverAffectNavigation(false);
		PrimitiveComponent->SetCastShadow(false);
		PrimitiveComponent->bCastDynamicShadow = false;
		PrimitiveComponent->bCastStaticShadow = false;
		PrimitiveComponent->bCastVolumetricTranslucentShadow = false;
		PrimitiveComponent->bCastContactShadow = false;
		PrimitiveComponent->bSelfShadowOnly = false;
		PrimitiveComponent->bCastFarShadow = false;
		PrimitiveComponent->bCastInsetShadow = false;
		PrimitiveComponent->bCastCinematicShadow = false;
		PrimitiveComponent->bCastHiddenShadow = false;
		PrimitiveComponent->bAffectDynamicIndirectLighting = false;
		PrimitiveComponent->bAffectDistanceFieldLighting = false;
		PrimitiveComponent->bReceivesDecals = false;
		PrimitiveComponent->bVisibleInReflectionCaptures = false;
		PrimitiveComponent->bVisibleInRealTimeSkyCaptures = false;
		PrimitiveComponent->bVisibleInRayTracing = false;
	}
}
