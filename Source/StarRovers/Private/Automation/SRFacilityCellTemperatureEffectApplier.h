#pragma once

#include "CoreMinimal.h"

class UActorComponent;
struct FSRFacilityInstance;
struct FSRResourceInstance;

class FSRFacilityCellTemperatureEffectApplier
{
public:
	static int32 ApplyEffects(
		const UActorComponent* OwnerComponent,
		const FSRFacilityInstance& FacilityInstance,
		const FSRResourceInstance* ConditionResource,
		const FSRResourceInstance* BaselineResource);
};
