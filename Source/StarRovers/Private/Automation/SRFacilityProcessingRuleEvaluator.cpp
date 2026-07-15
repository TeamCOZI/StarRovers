#include "SRFacilityProcessingRuleEvaluator.h"

#include "Automation/SRFacilityDataAsset.h"
#include "Automation/SRResourceDataAsset.h"
#include "SRFacilityMiningTargetResolver.h"
#include "SRFacilityEffectConditionEvaluator.h"
#include "SRFacilityOutputResourceBuilder.h"
#include "SRFacilityProcessContextResolver.h"
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

	bool CanResourcesAdvanceAtTemperature(
		const TArray<FSRResourceInstance>& ResourceInstances,
		ESRFacilityTemperatureState TemperatureState)
	{
		if (TemperatureState == ESRFacilityTemperatureState::Cold)
		{
			return true;
		}

		for (const FSRResourceInstance& ResourceInstance : ResourceInstances)
		{
			if (DoesProcessingResourceRequireColdTemperature(ResourceInstance))
			{
				return false;
			}
		}

		return true;
	}

	bool CanAdvanceProcessingWithResources(
		const FSRFacilityInstance& FacilityInstance,
		const TArray<FSRResourceInstance>& ResourceInstances)
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

		const StarRovers::FacilityProcessing::FSRFacilityProcessContext ProcessContext =
			StarRovers::FacilityProcessing::ResolveProcessContext(FacilityInstance, ResourceInstances);
		const ESRFacilityTemperatureState EffectiveTemperatureState = ProcessContext.EffectiveTemperatureState;
		if (EffectiveTemperatureState == ESRFacilityTemperatureState::Frozen
			|| EffectiveTemperatureState == ESRFacilityTemperatureState::Overheated)
		{
			return false;
		}

		return CanResourcesAdvanceAtTemperature(ResourceInstances, EffectiveTemperatureState);
	}
}

bool FSRFacilityProcessingRuleEvaluator::CanAdvanceProcessing(const FSRFacilityInstance& FacilityInstance)
{
	return CanAdvanceProcessingWithResources(FacilityInstance, FacilityInstance.ProcessingInventory);
}

bool FSRFacilityProcessingRuleEvaluator::CanRun(
	const UActorComponent* OwnerComponent,
	const FSRFacilityInstance& FacilityInstance)
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset))
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

	if (!CanAdvanceProcessingWithResources(FacilityInstance, InputResources))
	{
		return false;
	}

	if (!FSRFacilityOutputResourceBuilder::DoesInputSetMatchOperation(FacilityDataAsset, InputResources, FacilityInstance.TemperatureState))
	{
		return false;
	}

	TArray<FSRResourceInstance> OutputResources;
	FSRFacilityOutputResourceBuilder::BuildOutputResources(FacilityInstance, InputResources, OutputResources);
	if (OutputResources.IsEmpty())
	{
		return FSRFacilityOutputResourceBuilder::AllowsEmptyOutput(FacilityInstance);
	}
	return FSRFacilityProcessingInventoryRouter::CanStoreOutputResources(FacilityInstance, OutputResources);
}

bool FSRFacilityProcessingRuleEvaluator::CanMiningRun(
	const UActorComponent* OwnerComponent,
	const FSRFacilityInstance& FacilityInstance)
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset)
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

	TArray<FSRResourceInstance> MiningConditionResources;
	MiningConditionResources.Add(ResourceDeposit.ResourceDataAsset->BuildDefaultInstance());
	if (!CanAdvanceProcessingWithResources(FacilityInstance, MiningConditionResources))
	{
		return false;
	}

	TArray<FSRResourceInstance> OutputResources;
	const FSRResourceInstance MinedResource = MiningConditionResources[0];
	FSRFacilityOutputResourceBuilder::BuildOutputResourcesFromPrimaryResource(
		FacilityInstance,
		TArray<FSRResourceInstance>(),
		MinedResource,
		OutputResources);
	if (OutputResources.IsEmpty())
	{
		return FSRFacilityOutputResourceBuilder::AllowsEmptyOutput(FacilityInstance);
	}
	return FSRFacilityProcessingInventoryRouter::CanStoreOutputResources(FacilityInstance, OutputResources);
}

float FSRFacilityProcessingRuleEvaluator::ResolveProcessSeconds(const FSRFacilityInstance& FacilityInstance)
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	float ProcessSeconds = IsValid(FacilityDataAsset) ? FMath::Max(0.01f, FacilityDataAsset->BaseProcessSeconds) : 1.0f;
	const StarRovers::FacilityProcessing::FSRFacilityProcessContext ProcessContext =
		StarRovers::FacilityProcessing::ResolveProcessContext(FacilityInstance, FacilityInstance.ProcessingInventory);
	const ESRFacilityTemperatureState EffectiveTemperatureState = ProcessContext.EffectiveTemperatureState;
	if (EffectiveTemperatureState == ESRFacilityTemperatureState::Cold)
	{
		ProcessSeconds *= 2.0f;
	}
	if (!IsValid(FacilityDataAsset))
	{
		return ProcessSeconds;
	}

	const FSRResourceInstance* ConditionResource =
		StarRovers::FacilityProcessing::FindFirstProcessResource(FacilityInstance.ProcessingInventory);
	for (const FSRFacilityEffectSpec& EffectSpec : FacilityDataAsset->Effects)
	{
		const StarRovers::FacilityEffects::FSRFacilityEffectConditionContext ConditionContext =
		{
			ConditionResource,
			ConditionResource,
			EffectiveTemperatureState
		};
		if (EffectSpec.EffectKind != ESRFacilityEffectKind::AdjustProcessTime
			|| !StarRovers::FacilityEffects::DoEffectConditionsPass(EffectSpec, ConditionContext))
		{
			continue;
		}

		if (EffectSpec.ProcessTimeMode == ESRFacilityProcessTimeAdjustmentMode::Multiply)
		{
			ProcessSeconds *= static_cast<float>(EffectSpec.Value);
		}
		else
		{
			ProcessSeconds += static_cast<float>(EffectSpec.Value);
		}
		ProcessSeconds = FMath::Max(0.01f, ProcessSeconds);
	}
	return FMath::Max(0.01f, ProcessSeconds);
}
