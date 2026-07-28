#include "Automation/SRResourceProcessTagEvaluator.h"

namespace
{
	bool DoesTriggerMatch(
		const FSRProcessTagDefinitionV2& Definition,
		const FSRResourceInstance& InputResource,
		const FSRProcessTagTriggerContextV2& Context)
	{
		switch (Definition.Trigger)
		{
		case ESRProcessTagTriggerV2::PositiveFamilyStateActivated:
			return Context.bPositiveFamilyStateActivated;
		case ESRProcessTagTriggerV2::NegativeFamilyStateCleared:
			return Context.bNegativeFamilyStateCleared;
		case ESRProcessTagTriggerV2::ProcessArchetypeChanged:
			return Context.bProcessArchetypeChanged;
		case ESRProcessTagTriggerV2::FirstEnergyChangeAfterImport:
			return Context.bPreTagEnergyChanged
				&& !Context.ProcessingBodyId.IsNone()
				&& InputResource.LogisticsMetadata.LastTransitDestinationBodyId
					== Context.ProcessingBodyId
				&& InputResource.LogisticsMetadata.TransitCount
					> InputResource.ProcessingMemory.TransitCountAtLastEnergyChange;
		case ESRProcessTagTriggerV2::FirstValidProcessOutsideOrigin:
			return !Context.ProcessingBodyId.IsNone()
				&& !InputResource.LogisticsMetadata.OriginBodyId.IsNone()
				&& Context.ProcessingBodyId != InputResource.LogisticsMetadata.OriginBodyId
				&& !InputResource.LogisticsMetadata.bHasBeenProcessedOutsideOrigin;
		default:
			return false;
		}
	}
}

FSRProcessTagEvaluationV2 FSRResourceProcessTagEvaluator::Evaluate(
	const FSRResourceInstance& InputResource,
	const FSRProcessTagTriggerContextV2& TriggerContext)
{
	FSRProcessTagEvaluationV2 Evaluation;
	Evaluation.OutputSlot = InputResource.ProcessTagSlot;
	if (InputResource.ProcessTagSlot.Lifecycle != ESRResourceSlotLifecycle::Primed)
	{
		return Evaluation;
	}

	Evaluation.EvaluatedTagId = InputResource.ProcessTagSlot.TagId;
	FSRProcessTagDefinitionV2 Definition;
	if (InputResource.ProcessTagSlot.TagId.IsNone()
		|| InputResource.ProcessTagSlot.RemainingTriggers <= 0
		|| !FSRResourceSystemContent::TryGetProcessTagDefinition(
			InputResource.ProcessTagSlot.TagId,
			Definition)
		|| Definition.TriggerCount <= 0
		|| !FMath::IsFinite(Definition.EnergyDelta))
	{
		Evaluation.bValid = false;
		Evaluation.FailureReason = TEXT("Primed Process Tag has an unknown or invalid content definition.");
		return Evaluation;
	}

	if (!DoesTriggerMatch(Definition, InputResource, TriggerContext))
	{
		return Evaluation;
	}

	Evaluation.bTriggered = true;
	Evaluation.EnergyDelta = Definition.EnergyDelta;
	Evaluation.OutputSlot.RemainingTriggers = FMath::Max(
		0,
		InputResource.ProcessTagSlot.RemainingTriggers - 1);
	Evaluation.OutputSlot.Lifecycle = Evaluation.OutputSlot.RemainingTriggers > 0
		? ESRResourceSlotLifecycle::Primed
		: ESRResourceSlotLifecycle::Spent;
	return Evaluation;
}
