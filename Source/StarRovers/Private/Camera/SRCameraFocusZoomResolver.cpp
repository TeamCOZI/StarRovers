#include "SRCameraFocusZoomResolver.h"

#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

float FSRCameraFocusZoomResolver::ResolveActorFocusZoomDistance(
	const AActor* Actor,
	float CameraFieldOfViewDegrees,
	float FocusZoomMultiplier,
	float SmallActorFocusZoomDistance)
{
	if (!IsValid(Actor))
	{
		return 0.0f;
	}

	if (USRCelestialBodyRuntimeLibrary::IsCelestialBodyActor(Actor))
	{
		return USRCelestialBodyRuntimeLibrary::GetCelestialFocusZoomDistance(
			Actor,
			CameraFieldOfViewDegrees,
			FocusZoomMultiplier);
	}

	const float SafeFallbackDistance = FMath::Max(0.0f, SmallActorFocusZoomDistance);
	const float VisiblePrimitiveRadius = ComputeActorVisiblePrimitiveRadius(Actor);
	if (VisiblePrimitiveRadius <= KINDA_SMALL_NUMBER)
	{
		return SafeFallbackDistance;
	}

	const float SafeFieldOfViewDegrees = FMath::Clamp(CameraFieldOfViewDegrees, 5.0f, 170.0f);
	const float HalfFieldOfViewRadians = FMath::DegreesToRadians(SafeFieldOfViewDegrees * 0.5f);
	const float FramedDistance = VisiblePrimitiveRadius / FMath::Tan(HalfFieldOfViewRadians);
	return FMath::Max(SafeFallbackDistance, FramedDistance * FMath::Max(0.0f, FocusZoomMultiplier));
}

float FSRCameraFocusZoomResolver::ComputeActorVisiblePrimitiveRadius(const AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return 0.0f;
	}

	float LargestRadius = 0.0f;
	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(Actor);
	Actor->GetComponents(PrimitiveComponents);
	for (const UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!IsValid(PrimitiveComponent)
			|| !PrimitiveComponent->IsVisible()
			|| PrimitiveComponent->ComponentHasTag(TEXT("StarRovers.FocusCollision")))
		{
			continue;
		}

		LargestRadius = FMath::Max(LargestRadius, PrimitiveComponent->Bounds.SphereRadius);
	}

	return LargestRadius;
}
