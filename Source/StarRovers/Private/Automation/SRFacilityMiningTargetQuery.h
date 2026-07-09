#pragma once

#include "CoreMinimal.h"

class UActorComponent;
struct FSRFacilityNetworkRuntimeState;
struct FSRResourceDepositInstance;

class FSRFacilityMiningTargetQuery
{
public:
	static bool GetMiningTarget(
		const UActorComponent* OwnerComponent,
		const FSRFacilityNetworkRuntimeState& RuntimeState,
		FName OccupantId,
		FSRResourceDepositInstance& OutResourceDeposit);
};
