#pragma once

#include "CoreMinimal.h"

class UActorComponent;
struct FSRFacilityInstance;
struct FSRResourceInstance;

class FSRFacilityCellTemperatureEffectApplier
{
public:
	static int32 ApplyInstallationEffects(
		const UActorComponent* OwnerComponent,
		FSRFacilityInstance& FacilityInstance);

	static int32 RemoveInstallationEffects(
		const UActorComponent* OwnerComponent,
		FSRFacilityInstance& FacilityInstance);
};
