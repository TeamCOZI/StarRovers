#include "SRPlayerControllerFocusClickResolver.h"

#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Logistics/SRSpaceshipActor.h"

AActor* FSRPlayerControllerFocusClickResolver::ResolveFocusableActor(AActor* HitActor)
{
	return USRCelestialBodyRuntimeLibrary::IsCelestialBodyActor(HitActor) || Cast<ASRSpaceshipActor>(HitActor)
		? HitActor
		: nullptr;
}
