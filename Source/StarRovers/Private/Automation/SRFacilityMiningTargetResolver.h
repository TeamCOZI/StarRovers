#pragma once

#include "CoreMinimal.h"

class UActorComponent;
struct FSRFacilityInstance;
struct FSRResourceDepositInstance;
struct FSRResourceInstance;

class FSRFacilityMiningTargetResolver
{
public:
	static bool FindTargetDeposit(
		const UActorComponent* OwnerComponent,
		const FSRFacilityInstance& FacilityInstance,
		FSRResourceDepositInstance& OutResourceDeposit);

	static bool TryHarvestTargetDeposit(
		const UActorComponent* OwnerComponent,
		const FSRResourceDepositInstance& ResourceDeposit,
		FSRResourceInstance& OutMinedResource,
		FSRResourceDepositInstance& OutUpdatedResourceDeposit);
};
