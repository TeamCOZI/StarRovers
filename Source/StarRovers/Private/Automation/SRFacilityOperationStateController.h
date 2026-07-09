#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityNetworkRuntimeState.h"
#include "Automation/SRFacilityRuntimeData.h"

class UActorComponent;

class FSRFacilityOperationStateController
{
public:
	static bool SetTemperatureState(
		FSRFacilityNetworkRuntimeState& RuntimeState,
		FName OccupantId,
		ESRFacilityTemperatureState TemperatureState);

	static bool SetProcessEnabled(
		UActorComponent* OwnerComponent,
		FSRFacilityNetworkRuntimeState& RuntimeState,
		FName OccupantId,
		bool bEnabled,
		bool bAutoProcessFacilities);

	static bool SetDeliverEnabled(
		UActorComponent* OwnerComponent,
		FSRFacilityNetworkRuntimeState& RuntimeState,
		FName OccupantId,
		bool bEnabled);
};
