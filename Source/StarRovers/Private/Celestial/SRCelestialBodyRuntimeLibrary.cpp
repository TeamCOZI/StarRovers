#include "Celestial/SRCelestialBodyRuntimeLibrary.h"

#include "SRCelestialBodyRuntimeLibraryReflection.h"

#include "Celestial/SRCelestialBody.h"
#include "GameFramework/Actor.h"

using namespace StarRovers::CelestialBodyRuntime;

bool USRCelestialBodyRuntimeLibrary::IsCelestialBodyActor(const AActor* Actor)
{
	if (Cast<ASRCelestialBody>(Actor))
	{
		return true;
	}

	float UnusedFloat = 0.0f;
	AActor* UnusedParent = nullptr;

	return IsLikelyCelestialClass(Actor)
		|| (TryGetActorPropertyValue(Actor, PropertyNames::ParentBody, UnusedParent)
			&& TryGetFloatPropertyValue(Actor, PropertyNames::OrbitRadius, UnusedFloat)
			&& TryGetFloatPropertyValue(Actor, PropertyNames::OrbitPeriod, UnusedFloat)
			&& TryGetFloatPropertyValue(Actor, PropertyNames::InitialAngle, UnusedFloat)
			&& TryGetFloatPropertyValue(Actor, PropertyNames::FocusZoomMultiplier, UnusedFloat));
}

bool USRCelestialBodyRuntimeLibrary::IsCelestialStarActor(const AActor* Actor)
{
	if (!IsCelestialBodyActor(Actor))
	{
		return false;
	}

	if (const ASRCelestialBody* ProceduralBody = Cast<ASRCelestialBody>(Actor))
	{
		return ProceduralBody->GetBodyCategory() == ESRCelestialBodyCategory::Star;
	}

	if (IsLikelyStarClass(Actor))
	{
		return true;
	}

	return false;
}
