#include "SRFacilityOutputPreviewQuery.h"

#include "Automation/SRFacilityDataAsset.h"
#include "Automation/SRFacilityNetworkRuntimeState.h"
#include "SRFacilityMiningTargetResolver.h"
#include "SRFacilityOutputResourceBuilder.h"
#include "SRFacilityProcessingInventoryRouter.h"

bool FSRFacilityOutputPreviewQuery::GetOutputPreview(
	const UActorComponent* OwnerComponent,
	const FSRFacilityNetworkRuntimeState& RuntimeState,
	FName OccupantId,
	FSRResourceInstance& OutPrimaryOutput,
	TArray<FSRResourceInstance>& OutAdditionalOutputs,
	int32& OutOutputCount)
{
	OutPrimaryOutput = FSRResourceInstance();
	OutAdditionalOutputs.Reset();
	OutOutputCount = 0;

	const FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	if (!FacilityInstance || !IsValid(FacilityInstance->FacilityDataAsset.Get()))
	{
		return false;
	}

	if (FacilityInstance->FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Mine)
	{
		FSRResourceDepositInstance ResourceDeposit;
		if (!FSRFacilityMiningTargetResolver::FindTargetDeposit(OwnerComponent, *FacilityInstance, ResourceDeposit)
			|| !IsValid(ResourceDeposit.ResourceDataAsset.Get()))
		{
			return false;
		}

		TArray<FSRResourceInstance> PreviewOutputs;
		int32 PrimaryOutputCount = 0;
		FSRFacilityOutputResourceBuilder::BuildOutputResourcesFromPrimaryResource(
			*FacilityInstance,
			TArray<FSRResourceInstance>(),
			ResourceDeposit.ResourceDataAsset->BuildDefaultInstance(),
			PreviewOutputs,
			&PrimaryOutputCount);
		if (PreviewOutputs.IsEmpty())
		{
			OutOutputCount = 0;
			return FSRFacilityOutputResourceBuilder::AllowsEmptyOutput(*FacilityInstance);
		}

		OutPrimaryOutput = PreviewOutputs[0];
		for (int32 OutputIndex = PrimaryOutputCount; OutputIndex < PreviewOutputs.Num(); ++OutputIndex)
		{
			OutAdditionalOutputs.Add(PreviewOutputs[OutputIndex]);
		}
		OutOutputCount = PrimaryOutputCount;
		return !OutPrimaryOutput.ResourceId.IsNone();
	}

	TArray<FSRResourceInstance> PreviewInputs;
	if (!FacilityInstance->ProcessingInventory.IsEmpty())
	{
		PreviewInputs = FacilityInstance->ProcessingInventory;
	}
	else
	{
		if (!FSRFacilityProcessingInventoryRouter::GatherPendingInputResources(*FacilityInstance, PreviewInputs))
		{
			return false;
		}
	}

	if (!FSRFacilityOutputResourceBuilder::DoesInputSetMatchOperation(
		FacilityInstance->FacilityDataAsset.Get(),
		PreviewInputs,
		FacilityInstance->TemperatureState))
	{
		return false;
	}

	TArray<FSRResourceInstance> PreviewOutputs;
	int32 PrimaryOutputCount = 0;
	FSRFacilityOutputResourceBuilder::BuildOutputResources(
		*FacilityInstance,
		PreviewInputs,
		PreviewOutputs,
		&PrimaryOutputCount);
	if (PreviewOutputs.IsEmpty())
	{
		if (FSRFacilityOutputResourceBuilder::AllowsEmptyOutput(*FacilityInstance))
		{
			OutOutputCount = 0;
			return true;
		}
		return false;
	}

	OutPrimaryOutput = PreviewOutputs[0];
	for (int32 OutputIndex = PrimaryOutputCount; OutputIndex < PreviewOutputs.Num(); ++OutputIndex)
	{
		OutAdditionalOutputs.Add(PreviewOutputs[OutputIndex]);
	}
	OutOutputCount = PrimaryOutputCount;
	return true;
}
