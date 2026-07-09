#include "Celestial/SRCelestialBodyRuntimeLibrary.h"

#include "SRCelestialBodyRuntimeLibraryReflection.h"

#include "Celestial/SRCelestialBody.h"
#include "GameFramework/Actor.h"
#include "Simulation/SROrbit.h"

using namespace StarRovers::CelestialBodyRuntime;

bool USRCelestialBodyRuntimeLibrary::GetCelestialParentBody(const AActor* Actor, AActor*& OutParentBody)
{
	if (const ASRCelestialBody* ProceduralBody = Cast<ASRCelestialBody>(Actor))
	{
		if (const USROrbit* OrbitComponent = ProceduralBody->GetOrbit())
		{
			OutParentBody = OrbitComponent->GetParentBody();
			return true;
		}
	}

	return TryGetActorPropertyValue(Actor, PropertyNames::ParentBody, OutParentBody);
}

bool USRCelestialBodyRuntimeLibrary::GetCelestialRootBody(const AActor* Actor, AActor*& OutRootBody)
{
	OutRootBody = nullptr;

	if (!IsCelestialBodyActor(Actor))
	{
		return false;
	}

	constexpr int32 MaxHierarchyDepth = 32;
	AActor* CurrentBody = const_cast<AActor*>(Actor);
	for (int32 DepthIndex = 0; DepthIndex < MaxHierarchyDepth; ++DepthIndex)
	{
		AActor* ParentBody = nullptr;
		if (!GetCelestialParentBody(CurrentBody, ParentBody) || !IsValid(ParentBody))
		{
			OutRootBody = CurrentBody;
			return IsValid(OutRootBody);
		}

		if (ParentBody == CurrentBody)
		{
			break;
		}

		CurrentBody = ParentBody;
	}

	OutRootBody = CurrentBody;
	return IsValid(OutRootBody);
}

bool USRCelestialBodyRuntimeLibrary::GetCelestialPrimaryStar(const AActor* Actor, AActor*& OutPrimaryStar)
{
	OutPrimaryStar = nullptr;

	if (!IsCelestialBodyActor(Actor))
	{
		return false;
	}

	if (IsCelestialStarActor(Actor))
	{
		OutPrimaryStar = const_cast<AActor*>(Actor);
		return true;
	}

	AActor* RootBody = nullptr;
	if (!GetCelestialRootBody(Actor, RootBody) || !IsValid(RootBody) || !IsCelestialStarActor(RootBody))
	{
		return false;
	}

	OutPrimaryStar = RootBody;
	return true;
}

bool USRCelestialBodyRuntimeLibrary::GetCelestialOrbitRadius(const AActor* Actor, float& OutOrbitRadius)
{
	if (const ASRCelestialBody* ProceduralBody = Cast<ASRCelestialBody>(Actor))
	{
		if (const USROrbit* OrbitComponent = ProceduralBody->GetOrbit())
		{
			OutOrbitRadius = OrbitComponent->GetOrbitRadius();
			return true;
		}
	}

	return TryGetFloatPropertyValue(Actor, PropertyNames::OrbitRadius, OutOrbitRadius);
}

bool USRCelestialBodyRuntimeLibrary::GetCelestialOrbitPeriod(const AActor* Actor, float& OutOrbitPeriod)
{
	if (const ASRCelestialBody* ProceduralBody = Cast<ASRCelestialBody>(Actor))
	{
		if (const USROrbit* OrbitComponent = ProceduralBody->GetOrbit())
		{
			OutOrbitPeriod = OrbitComponent->GetOrbitPeriodSeconds();
			return true;
		}
	}

	return TryGetFloatPropertyValue(Actor, PropertyNames::OrbitPeriod, OutOrbitPeriod);
}

bool USRCelestialBodyRuntimeLibrary::GetCelestialInitialAngle(const AActor* Actor, float& OutInitialAngleDegrees)
{
	if (const ASRCelestialBody* ProceduralBody = Cast<ASRCelestialBody>(Actor))
	{
		if (const USROrbit* OrbitComponent = ProceduralBody->GetOrbit())
		{
			OutInitialAngleDegrees = OrbitComponent->GetInitialAngleDegrees();
			return true;
		}
	}

	return TryGetFloatPropertyValue(Actor, PropertyNames::InitialAngle, OutInitialAngleDegrees);
}
