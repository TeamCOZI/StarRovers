#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityRuntimeData.h"

class UActorComponent;

class FSRFacilityProcessingRuleEvaluator
{
public:
	static bool CanAdvanceProcessing(const FSRFacilityInstance& FacilityInstance);

	static bool CanRun(
		const UActorComponent* OwnerComponent,
		const FSRFacilityInstance& FacilityInstance);

	static bool CanMiningRun(
		const UActorComponent* OwnerComponent,
		const FSRFacilityInstance& FacilityInstance);

	static float ResolveProcessSeconds(const FSRFacilityInstance& FacilityInstance);
};
