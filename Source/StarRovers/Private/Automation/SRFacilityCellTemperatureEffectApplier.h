#pragma once

#include "CoreMinimal.h"

class UActorComponent;
struct FSRFacilityInstance;

class FSRFacilityCellTemperatureEffectApplier
{
public:
	static int32 ApplyEffects(
		const UActorComponent* OwnerComponent,
		const FSRFacilityInstance& FacilityInstance);
};
