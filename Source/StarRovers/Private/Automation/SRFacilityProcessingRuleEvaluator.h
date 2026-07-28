#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityRuntimeData.h"
#include "Automation/SRRefinementResistanceV2.h"

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
	static float CaptureProcessSecondsSnapshot(FSRFacilityInstance& FacilityInstance);
	static void ClearProcessSecondsSnapshot(FSRFacilityInstance& FacilityInstance);

	static FSRRefinementResistanceResultV2 ResolveRefinementResistance(
		const FSRFacilityInstance& FacilityInstance);
};
