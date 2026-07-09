#include "SRFacilityMiningProcessor.h"

#include "Automation/SRFacilityDataAsset.h"
#include "Automation/SRResourceDataAsset.h"
#include "SRFacilityMiningTargetResolver.h"
#include "SRFacilityOutputResourceBuilder.h"
#include "SRFacilityProcessingInventoryRouter.h"
#include "SRFacilityProcessingRuleEvaluator.h"

namespace
{
	void ResetProcessingState(FSRFacilityInstance& FacilityInstance)
	{
		FacilityInstance.bProcessing = false;
		FacilityInstance.ProcessProgressSeconds = 0.0f;
	}

	void ResetMiningState(FSRFacilityInstance& FacilityInstance)
	{
		FacilityInstance.MiningTargetDepositOccupantId = NAME_None;
		ResetProcessingState(FacilityInstance);
	}
}

bool FSRFacilityMiningProcessor::TryStartMining(
	const UActorComponent* OwnerComponent,
	FSRFacilityInstance& FacilityInstance,
	FSRFacilityMiningStartResult* OutStartResult)
{
	if (OutStartResult)
	{
		*OutStartResult = FSRFacilityMiningStartResult();
	}

	FSRResourceDepositInstance ResourceDeposit;
	if (!FSRFacilityMiningTargetResolver::FindTargetDeposit(OwnerComponent, FacilityInstance, ResourceDeposit))
	{
		return false;
	}

	FacilityInstance.MiningTargetDepositOccupantId = ResourceDeposit.OccupantId;
	FacilityInstance.ProcessingInventory.Reset();
	FacilityInstance.bProcessing = true;
	FacilityInstance.ProcessProgressSeconds = 0.0f;
	if (OutStartResult)
	{
		OutStartResult->ResourceDeposit = ResourceDeposit;
	}
	return true;
}

bool FSRFacilityMiningProcessor::TryCompleteMining(
	const UActorComponent* OwnerComponent,
	FSRFacilityInstance& FacilityInstance,
	FSRFacilityMiningCompletionResult* OutCompletionResult)
{
	if (OutCompletionResult)
	{
		*OutCompletionResult = FSRFacilityMiningCompletionResult();
	}

	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset) || FacilityDataAsset->OperationKind != ESRFacilityOperationKind::Mine)
	{
		ResetMiningState(FacilityInstance);
		return false;
	}

	FSRResourceDepositInstance ResourceDeposit;
	if (!FSRFacilityMiningTargetResolver::FindTargetDeposit(OwnerComponent, FacilityInstance, ResourceDeposit))
	{
		ResetMiningState(FacilityInstance);
		return false;
	}

	const int32 OutputCount = FSRFacilityOutputResourceBuilder::ResolvePrimaryOutputCount(FacilityInstance);
	if (!IsValid(ResourceDeposit.ResourceDataAsset.Get()))
	{
		ResetMiningState(FacilityInstance);
		return false;
	}

	const FSRResourceInstance PreviewMinedResource = ResourceDeposit.ResourceDataAsset->BuildDefaultInstance();
	TArray<FSRResourceInstance> PreviewOutputResources;
	PreviewOutputResources.Reserve(OutputCount);
	for (int32 OutputIndex = 0; OutputIndex < OutputCount; ++OutputIndex)
	{
		PreviewOutputResources.Add(PreviewMinedResource);
	}
	if (!FSRFacilityProcessingInventoryRouter::CanStoreOutputResources(FacilityInstance, PreviewOutputResources))
	{
		FacilityInstance.ProcessProgressSeconds = FSRFacilityProcessingRuleEvaluator::ResolveProcessSeconds(FacilityInstance);
		return false;
	}

	FSRResourceInstance MinedResource;
	FSRResourceDepositInstance UpdatedResourceDeposit;
	if (!FSRFacilityMiningTargetResolver::TryHarvestTargetDeposit(OwnerComponent, ResourceDeposit, MinedResource, UpdatedResourceDeposit))
	{
		ResetMiningState(FacilityInstance);
		return false;
	}

	TArray<FSRResourceInstance> OutputResources;
	OutputResources.Reserve(OutputCount);
	for (int32 OutputIndex = 0; OutputIndex < OutputCount; ++OutputIndex)
	{
		OutputResources.Add(MinedResource);
	}

	FSRFacilityProcessingInventoryRouter::StoreOutputResources(FacilityInstance, OutputResources);
	FacilityInstance.MiningTargetDepositOccupantId = UpdatedResourceDeposit.RemainingAmount > 0
		? UpdatedResourceDeposit.OccupantId
		: NAME_None;
	FacilityInstance.ProcessingInventory.Reset();
	ResetProcessingState(FacilityInstance);

	if (OutCompletionResult)
	{
		OutCompletionResult->DepositOccupantId = ResourceDeposit.OccupantId;
		OutCompletionResult->MinedResource = MinedResource;
		OutCompletionResult->RemainingAmount = UpdatedResourceDeposit.RemainingAmount;
		OutCompletionResult->TotalAmount = UpdatedResourceDeposit.TotalAmount;
	}
	return true;
}
