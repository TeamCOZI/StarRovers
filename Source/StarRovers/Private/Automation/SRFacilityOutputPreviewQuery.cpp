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

		OutPrimaryOutput = ResourceDeposit.ResourceDataAsset->BuildDefaultInstance();
		OutAdditionalOutputs.Reset();
		OutOutputCount = FSRFacilityOutputResourceBuilder::ResolvePrimaryOutputCount(*FacilityInstance);
		return OutOutputCount > 0 && !OutPrimaryOutput.ResourceId.IsNone();
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

	if (!FSRFacilityOutputResourceBuilder::DoesInputSetMatchOperation(FacilityInstance->FacilityDataAsset.Get(), PreviewInputs))
	{
		return false;
	}

	TArray<FSRResourceInstance> PreviewOutputs;
	FSRFacilityOutputResourceBuilder::BuildOutputResources(*FacilityInstance, PreviewInputs, PreviewOutputs);
	if (PreviewOutputs.IsEmpty())
	{
		return false;
	}

	OutPrimaryOutput = PreviewOutputs[0];
	const int32 PrimaryOutputCount = FMath::Max(0, FSRFacilityOutputResourceBuilder::ResolvePrimaryOutputCount(*FacilityInstance));
	for (int32 OutputIndex = PrimaryOutputCount; OutputIndex < PreviewOutputs.Num(); ++OutputIndex)
	{
		OutAdditionalOutputs.Add(PreviewOutputs[OutputIndex]);
	}
	OutOutputCount = FSRFacilityOutputResourceBuilder::ResolvePrimaryOutputCount(*FacilityInstance);
	return true;
}
