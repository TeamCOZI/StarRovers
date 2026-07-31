#include "SRFacilityOutputPreviewQuery.h"

#include "Automation/SRFacilityDataAsset.h"
#include "Automation/SRFacilityNetworkRuntimeState.h"
#include "SRFacilityMiningTargetResolver.h"
#include "SRFacilityOutputResourceBuilder.h"
#include "SRFacilityProcessingInventoryRouter.h"
#include "SRFacilityRunModifierResolver.h"

bool FSRFacilityOutputPreviewQuery::GetOutputPreview(
	const UActorComponent* OwnerComponent,
	const FSRFacilityNetworkRuntimeState& RuntimeState,
	FName OccupantId,
	FSRResourceInstance& OutPrimaryOutput,
	TArray<FSRResourceInstance>& OutAdditionalOutputs,
	int32& OutOutputCount,
	TArray<FString>& OutOperationTraceTexts)
{
	OutPrimaryOutput = FSRResourceInstance();
	OutAdditionalOutputs.Reset();
	OutOutputCount = 0;
	OutOperationTraceTexts.Reset();

	const FSRFacilityInstance* StoredFacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	if (!StoredFacilityInstance || !IsValid(StoredFacilityInstance->FacilityDataAsset.Get()))
	{
		return false;
	}
	FSRFacilityInstance PreviewFacilityInstance = *StoredFacilityInstance;
	if (!PreviewFacilityInstance.bProcessing)
	{
		FSRFacilityRunModifierResolver::SnapshotCurrentContext(OwnerComponent, PreviewFacilityInstance);
	}
	const FSRFacilityInstance* FacilityInstance = &PreviewFacilityInstance;

	if (FacilityInstance->FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Mine)
	{
		FSRResourceDepositInstance ResourceDeposit;
		if (!FSRFacilityMiningTargetResolver::FindTargetDeposit(OwnerComponent, *FacilityInstance, ResourceDeposit)
			|| !ResourceDeposit.IsPatternSourceValid())
		{
			return false;
		}

		TArray<FSRResourceInstance> PreviewOutputs;
		TArray<FString> PreviewOperationTraceTexts;
		int32 PrimaryOutputCount = 0;
		FSRFacilityOutputResourceBuilder::BuildOutputResourcesFromPrimaryResource(
			*FacilityInstance,
			TArray<FSRResourceInstance>(),
			ResourceDeposit.BuildResourceInstance(),
			PreviewOutputs,
			&PrimaryOutputCount,
			nullptr,
			&PreviewOperationTraceTexts);
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
		OutOperationTraceTexts = MoveTemp(PreviewOperationTraceTexts);
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
	TArray<FString> PreviewOperationTraceTexts;
	int32 PrimaryOutputCount = 0;
	FSRFacilityOutputResourceBuilder::BuildOutputResources(
		*FacilityInstance,
		PreviewInputs,
		PreviewOutputs,
		&PrimaryOutputCount,
		nullptr,
		&PreviewOperationTraceTexts);
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
	OutOperationTraceTexts = MoveTemp(PreviewOperationTraceTexts);
	return true;
}
