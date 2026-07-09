#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityNetworkRuntimeState.h"

class UActorComponent;

class FSRFacilityTemperatureSynchronizer
{
public:
	static bool RefreshFacilityFromSurface(
		const UActorComponent* OwnerComponent,
		FSRFacilityNetworkRuntimeState& RuntimeState,
		FName OccupantId);

	static int32 RefreshFacilitiesFromSurface(
		const UActorComponent* OwnerComponent,
		FSRFacilityNetworkRuntimeState& RuntimeState);
};
