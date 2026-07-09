#include "Celestial/SRCelestialBody.h"

#include "SRCelestialBodyLog.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

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
