#include "SRFacilityProcessingStepExecutor.h"

#include "Automation/SRFacilityDataAsset.h"
#include "Automation/SRFacilityResourceV2Processor.h"
#include "Automation/SROperationalEconomyProcessor.h"
#include "Automation/SRResourceInstanceOperations.h"
#include "Automation/SRStellarFuelFabricator.h"
#include "Components/ActorComponent.h"
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
		FSRFacilityProcessingRuleEvaluator::ClearProcessSecondsSnapshot(FacilityInstance);
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
		FSRFacilityProcessingRuleEvaluator::CaptureProcessSecondsSnapshot(FacilityInstance);

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
	FSRFacilityProcessingRuleEvaluator::CaptureProcessSecondsSnapshot(FacilityInstance);
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
	FSRResourceProcessResult ResourceV2ProcessResult;
	FSRStellarFuelFabricationResultV2 StellarFuelFabricationResult;
	const FName ProcessedBodyId = StarRovers::Resources::ResolveCelestialBodyResourceId(
		IsValid(OwnerComponent) ? OwnerComponent->GetOwner() : nullptr);
	const bool bUsesResourceV2ProcessRoute =
		FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Process
		&& FSRFacilityResourceV2Processor::ShouldRouteStandardProcessThroughResourceV2(FacilityDataAsset);
	const bool bUsesStellarFuelFabricatorV2Route =
		FSRStellarFuelFabricator::ShouldRouteThroughResourceV2(FacilityDataAsset);
	const bool bUsesOperationalEconomyV2Route =
		FSROperationalEconomyProcessor::ShouldRouteThroughResourceV2(FacilityDataAsset);
	const FSROperationalEconomyEvaluationV2 OperationalEconomyEvaluation =
		bUsesOperationalEconomyV2Route
			? FSROperationalEconomyProcessor::Evaluate(FacilityInstance, ConsumedResources)
			: FSROperationalEconomyEvaluationV2();
	if (bUsesOperationalEconomyV2Route && !OperationalEconomyEvaluation.IsSuccess())
	{
		FacilityInstance.ProcessProgressSeconds =
			FSRFacilityProcessingRuleEvaluator::ResolveProcessSeconds(FacilityInstance);
		return false;
	}
	const bool bUsesResourceV2Route = bUsesResourceV2ProcessRoute
		|| bUsesStellarFuelFabricatorV2Route
		|| bUsesOperationalEconomyV2Route;
	FSRFacilityOutputResourceBuilder::BuildOutputResources(
		FacilityInstance,
		ConsumedResources,
		OutputResources,
		&PrimaryOutputCount,
		&BaselineOutputResource,
		nullptr,
		&ResourceV2ProcessResult,
		&StellarFuelFabricationResult,
		ProcessedBodyId);
	if (!bUsesResourceV2Route)
	{
		for (FSRResourceInstance& OutputResource : OutputResources)
		{
			StarRovers::Resources::RecordResourceProcessedOnBody(OutputResource, ProcessedBodyId);
		}
	}
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
	ResetStandardProcessingState(FacilityInstance);

	if (OutCompletionResult)
	{
		OutCompletionResult->StepKind = ESRFacilityProcessingStepKind::Standard;
		OutCompletionResult->PrimaryOutputResource = OutputResources.IsEmpty() ? FSRResourceInstance() : OutputResources[0];
		OutCompletionResult->ResourceV2ProcessResult = ResourceV2ProcessResult;
		OutCompletionResult->StellarFuelFabricationResult = StellarFuelFabricationResult;
		OutCompletionResult->bUsedResourceV2Process = bUsesResourceV2ProcessRoute
			&& ResourceV2ProcessResult.IsSuccess();
		OutCompletionResult->bUsedStellarFuelFabricatorV2 = bUsesStellarFuelFabricatorV2Route
			&& StellarFuelFabricationResult.IsSuccess();
		OutCompletionResult->bUsedOperationalEconomyV2 = bUsesOperationalEconomyV2Route
			&& OperationalEconomyEvaluation.IsSuccess();
		OutCompletionResult->bUsedResourceV2 = OutCompletionResult->bUsedResourceV2Process
			|| OutCompletionResult->bUsedStellarFuelFabricatorV2
			|| OutCompletionResult->bUsedOperationalEconomyV2;
		OutCompletionResult->OutputCount = OutputResources.Num();
		OutCompletionResult->AdditionalOutputCount = FMath::Max(
			0,
			OutputResources.Num() - PrimaryOutputCount);
	}
	return true;
}
