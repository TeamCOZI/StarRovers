#include "SRFacilityProcessingStepExecutor.h"

#include "Automation/SRFacilityDataAsset.h"
#include "SRFacilityCellTemperatureEffectApplier.h"
#include "SRFacilityOutputResourceBuilder.h"
#include "SRFacilityProcessingInventoryRouter.h"
#include "SRFacilityProcessingRuleEvaluator.h"

namespace
{
	void ResetStandardProcessingState(FSRFacilityInstance& FacilityInstance)
	{
		FacilityInstance.ProcessingInventory.Reset();
		FacilityInstance.bProcessing = false;
		FacilityInstance.ProcessProgressSeconds = 0.0f;
	}
}

bool FSRFacilityProcessingStepExecutor::TryStartProcessing(
	const UActorComponent* OwnerComponent,
	FSRFacilityInstance& FacilityInstance,
	FSRFacilityProcessingStartResult* OutStartResult)
{
	if (OutStartResult)
	{
		*OutStartResult = FSRFacilityProcessingStartResult();
	}

	if (!FSRFacilityProcessingRuleEvaluator::CanRun(OwnerComponent, FacilityInstance))
	{
		return false;
	}

	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (IsValid(FacilityDataAsset) && FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Mine)
	{
		FSRFacilityMiningStartResult MiningResult;
		if (!FSRFacilityMiningProcessor::TryStartMining(OwnerComponent, FacilityInstance, &MiningResult))
		{
			return false;
		}

		if (OutStartResult)
		{
			OutStartResult->StepKind = ESRFacilityProcessingStepKind::Mining;
			OutStartResult->MiningResult = MiningResult;
		}
		return true;
	}

	if (!FSRFacilityProcessingInventoryRouter::TryMoveInputsToProcessingInventory(FacilityInstance))
	{
		return false;
	}

	FacilityInstance.bProcessing = true;
	FacilityInstance.ProcessProgressSeconds = 0.0f;
	if (OutStartResult)
	{
		OutStartResult->StepKind = ESRFacilityProcessingStepKind::Standard;
		OutStartResult->ProcessingInputCount = FacilityInstance.ProcessingInventory.Num();
		OutStartResult->RemainingInputCount = FacilityInstance.InputInventory.Num();
	}
	return true;
}

bool FSRFacilityProcessingStepExecutor::TryCompleteProcessing(
	const UActorComponent* OwnerComponent,
	FSRFacilityInstance& FacilityInstance,
	FSRFacilityProcessingCompletionResult* OutCompletionResult)
{
	if (OutCompletionResult)
	{
		*OutCompletionResult = FSRFacilityProcessingCompletionResult();
	}

	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (IsValid(FacilityDataAsset) && FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Mine)
	{
		FSRFacilityMiningCompletionResult MiningResult;
		if (!FSRFacilityMiningProcessor::TryCompleteMining(OwnerComponent, FacilityInstance, &MiningResult))
		{
			return false;
		}

		if (OutCompletionResult)
		{
			OutCompletionResult->StepKind = ESRFacilityProcessingStepKind::Mining;
			OutCompletionResult->MiningResult = MiningResult;
		}
		return true;
	}

	if (!IsValid(FacilityDataAsset) || FacilityInstance.ProcessingInventory.IsEmpty())
	{
		ResetStandardProcessingState(FacilityInstance);
		return false;
	}

	TArray<FSRResourceInstance> ConsumedResources = FacilityInstance.ProcessingInventory;
	TArray<FSRResourceInstance> OutputResources;
	int32 PrimaryOutputCount = 0;
	FSRResourceInstance BaselineOutputResource;
	FSRFacilityOutputResourceBuilder::BuildOutputResources(
		FacilityInstance,
		ConsumedResources,
		OutputResources,
		&PrimaryOutputCount,
		&BaselineOutputResource);
	if (OutputResources.IsEmpty())
	{
		if (!FSRFacilityOutputResourceBuilder::AllowsEmptyOutput(FacilityInstance))
		{
			FacilityInstance.ProcessProgressSeconds = FSRFacilityProcessingRuleEvaluator::ResolveProcessSeconds(FacilityInstance);
			return false;
		}
	}
	else if (!FSRFacilityProcessingInventoryRouter::CanStoreOutputResources(FacilityInstance, OutputResources))
	{
		FacilityInstance.ProcessProgressSeconds = FSRFacilityProcessingRuleEvaluator::ResolveProcessSeconds(FacilityInstance);
		return false;
	}

	if (!OutputResources.IsEmpty())
	{
		FSRFacilityProcessingInventoryRouter::StoreOutputResources(FacilityInstance, OutputResources);
	}
	const FSRResourceInstance* TemperatureConditionResource = OutputResources.IsEmpty()
		? (BaselineOutputResource.ResourceId.IsNone() ? nullptr : &BaselineOutputResource)
		: &OutputResources[0];
	const FSRResourceInstance* TemperatureBaselineResource = BaselineOutputResource.ResourceId.IsNone()
		? TemperatureConditionResource
		: &BaselineOutputResource;
	const int32 CellTemperatureEffects = FSRFacilityCellTemperatureEffectApplier::ApplyEffects(
		OwnerComponent,
		FacilityInstance,
		TemperatureConditionResource,
		TemperatureBaselineResource);
	ResetStandardProcessingState(FacilityInstance);

	if (OutCompletionResult)
	{
		OutCompletionResult->StepKind = ESRFacilityProcessingStepKind::Standard;
		OutCompletionResult->PrimaryOutputResource = OutputResources.IsEmpty() ? FSRResourceInstance() : OutputResources[0];
		OutCompletionResult->OutputCount = OutputResources.Num();
		OutCompletionResult->AdditionalOutputCount = FMath::Max(
			0,
			OutputResources.Num() - PrimaryOutputCount);
		OutCompletionResult->CellTemperatureEffects = CellTemperatureEffects;
		OutCompletionResult->bShouldRefreshTemperatureFromSurface = CellTemperatureEffects > 0;
	}
	return true;
}
