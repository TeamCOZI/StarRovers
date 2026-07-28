#pragma once

#include "CoreMinimal.h"
#include "Automation/SRResourceSystemContent.h"

struct STARROVERS_API FSRProcessTagTriggerContextV2
{
	bool bPositiveFamilyStateActivated = false;
	bool bNegativeFamilyStateCleared = false;
	bool bProcessArchetypeChanged = false;
	bool bPreTagEnergyChanged = false;
	FName ProcessingBodyId = NAME_None;
};

struct STARROVERS_API FSRProcessTagEvaluationV2
{
	bool bValid = true;
	FString FailureReason;
	FName EvaluatedTagId = NAME_None;
	bool bTriggered = false;
	double EnergyDelta = 0.0;
	FSRResourceProcessTagSlot OutputSlot;
};

class STARROVERS_API FSRResourceProcessTagEvaluator final
{
public:
	// Pure and deterministic. Slot mutation is returned in OutputSlot and is never
	// applied directly to InputResource.
	static FSRProcessTagEvaluationV2 Evaluate(
		const FSRResourceInstance& InputResource,
		const FSRProcessTagTriggerContextV2& TriggerContext);
};
