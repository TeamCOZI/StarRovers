#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityRuntimeData.h"

class UActorComponent;

struct FSRFacilityMiningStartResult
{
	FSRResourceDepositInstance ResourceDeposit;
};

struct FSRFacilityMiningCompletionResult
{
	FName DepositOccupantId = NAME_None;
	FSRResourceInstance MinedResource;
	int32 RemainingAmount = 0;
	int32 TotalAmount = 0;
};

class FSRFacilityMiningProcessor
{
public:
	static bool TryStartMining(
		const UActorComponent* OwnerComponent,
		FSRFacilityInstance& FacilityInstance,
		FSRFacilityMiningStartResult* OutStartResult = nullptr);

	static bool TryCompleteMining(
		const UActorComponent* OwnerComponent,
		FSRFacilityInstance& FacilityInstance,
		FSRFacilityMiningCompletionResult* OutCompletionResult = nullptr);
};
