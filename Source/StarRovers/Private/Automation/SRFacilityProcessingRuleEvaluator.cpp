#include "SRFacilityProcessingRuleEvaluator.h"

#include "Automation/SRFacilityDataAsset.h"
#include "Automation/SRResourceDataAsset.h"
#include "SRFacilityMiningTargetResolver.h"
#include "SRFacilityOutputResourceBuilder.h"
#include "SRFacilityProcessingInventoryRouter.h"

namespace
{
	bool DoesProcessingResourceRequireColdTemperature(const FSRResourceInstance& ResourceInstance)
	{
		for (const FSRResourceTagStack& TagStack : ResourceInstance.Tags)
		{
			if (TagStack.Tag == ESRResourceProcessTag::Supercooled && TagStack.StackCount > 0)
			{
				return true;
			}
		}

		return false;
	}

	bool CanProcessingInventoryAdvanceAtTemperature(const FSRFacilityInstance& FacilityInstance)
	{
		if (FacilityInstance.TemperatureState == ESRFacilityTemperatureState::Cold)
		{
			return true;
		}

		for (const FSRResourceInstance& ResourceInstance : FacilityInstance.ProcessingInventory)
		{
			if (DoesProcessingResourceRequireColdTemperature(ResourceInstance))
			{
				return false;
			}
		}

		return true;
	}
}

bool FSRFacilityProcessingRuleEvaluator::CanAdvanceProcessing(const FSRFacilityInstance& FacilityInstance)
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset))
	{
		return false;
	}

	if (FacilityDataAsset->FacilityKind == ESRFacilityKind::Hub)
	{
		return false;
	}

	if (!FacilityInstance.bProcessEnabled)
	{
		return false;
	}

	if (FacilityInstance.TemperatureState == ESRFacilityTemperatureState::Frozen
		|| FacilityInstance.TemperatureState == ESRFacilityTemperatureState::Overheated)
	{
		return false;
	}

	if (FacilityDataAsset->bRequiresColdTemperature && FacilityInstance.TemperatureState != ESRFacilityTemperatureState::Cold)
	{
		return false;
	}

	if (FacilityDataAsset->bRequiresHotTemperature && FacilityInstance.TemperatureState != ESRFacilityTemperatureState::Hot)
	{
		return false;
	}

	return CanProcessingInventoryAdvanceAtTemperature(FacilityInstance);
}

bool FSRFacilityProcessingRuleEvaluator::CanRun(
	const UActorComponent* OwnerComponent,
	const FSRFacilityInstance& FacilityInstance)
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!CanAdvanceProcessing(FacilityInstance) || !IsValid(FacilityDataAsset))
	{
		return false;
	}

	if (FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Mine)
	{
		return CanMiningRun(OwnerComponent, FacilityInstance);
	}

	TArray<FSRResourceInstance> InputResources;
	if (!FSRFacilityProcessingInventoryRouter::GatherPendingInputResources(FacilityInstance, InputResources))
	{
		return false;
	}

	if (!FSRFacilityOutputResourceBuilder::DoesInputSetMatchOperation(FacilityDataAsset, InputResources, FacilityInstance.TemperatureState))
	{
		return false;
	}

	TArray<FSRResourceInstance> OutputResources;
	FSRFacilityOutputResourceBuilder::BuildOutputResources(FacilityInstance, InputResources, OutputResources);
	return FSRFacilityProcessingInventoryRouter::CanStoreOutputResources(FacilityInstance, OutputResources);
}

bool FSRFacilityProcessingRuleEvaluator::CanMiningRun(
	const UActorComponent* OwnerComponent,
	const FSRFacilityInstance& FacilityInstance)
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!CanAdvanceProcessing(FacilityInstance)
		|| !IsValid(FacilityDataAsset)
		|| FacilityDataAsset->OperationKind != ESRFacilityOperationKind::Mine)
	{
		return false;
	}

	FSRResourceDepositInstance ResourceDeposit;
	if (!FSRFacilityMiningTargetResolver::FindTargetDeposit(OwnerComponent, FacilityInstance, ResourceDeposit))
	{
		return false;
	}

	if (!IsValid(ResourceDeposit.ResourceDataAsset.Get()))
	{
		return false;
	}

	const int32 OutputCount = FSRFacilityOutputResourceBuilder::ResolvePrimaryOutputCount(FacilityInstance);
	TArray<FSRResourceInstance> OutputResources;
	OutputResources.Reserve(OutputCount);
	const FSRResourceInstance MinedResource = ResourceDeposit.ResourceDataAsset->BuildDefaultInstance();
	for (int32 OutputIndex = 0; OutputIndex < OutputCount; ++OutputIndex)
	{
		OutputResources.Add(MinedResource);
	}
	return FSRFacilityProcessingInventoryRouter::CanStoreOutputResources(FacilityInstance, OutputResources);
}

float FSRFacilityProcessingRuleEvaluator::ResolveProcessSeconds(const FSRFacilityInstance& FacilityInstance)
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	float ProcessSeconds = IsValid(FacilityDataAsset) ? FMath::Max(0.01f, FacilityDataAsset->BaseProcessSeconds) : 1.0f;
	if (FacilityInstance.TemperatureState == ESRFacilityTemperatureState::Cold)
	{
		ProcessSeconds *= 2.0f;
	}
	return ProcessSeconds;
}
