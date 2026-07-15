#include "SRFacilityProcessingRuleEvaluator.h"

#include "Automation/SRFacilityDataAsset.h"
#include "Automation/SRResourceDataAsset.h"
#include "SRFacilityMiningTargetResolver.h"
#include "SRFacilityEffectConditionEvaluator.h"
#include "SRFacilityOutputResourceBuilder.h"
#include "SRFacilityProcessingInventoryRouter.h"

namespace
{
	const FSRResourceInstance* FindFirstProcessingRuleResource(const TArray<FSRResourceInstance>& ResourceInstances)
	{
		for (const FSRResourceInstance& ResourceInstance : ResourceInstances)
		{
			if (!ResourceInstance.ResourceId.IsNone())
			{
				return &ResourceInstance;
			}
		}
		return nullptr;
	}

	ESRFacilityTemperatureState InvertProcessingRuleTemperatureState(ESRFacilityTemperatureState TemperatureState)
	{
		switch (TemperatureState)
		{
		case ESRFacilityTemperatureState::Frozen:
			return ESRFacilityTemperatureState::Overheated;
		case ESRFacilityTemperatureState::Cold:
			return ESRFacilityTemperatureState::Hot;
		case ESRFacilityTemperatureState::Hot:
			return ESRFacilityTemperatureState::Cold;
		case ESRFacilityTemperatureState::Overheated:
			return ESRFacilityTemperatureState::Frozen;
		case ESRFacilityTemperatureState::Normal:
		default:
			return ESRFacilityTemperatureState::Normal;
		}
	}

	ESRFacilityTemperatureState ResolveEffectiveProcessingTemperature(
		const FSRFacilityInstance& FacilityInstance,
		const TArray<FSRResourceInstance>& ResourceInstances)
	{
		const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
		if (!IsValid(FacilityDataAsset))
		{
			return FacilityInstance.TemperatureState;
		}

		const FSRResourceInstance* ConditionResource = FindFirstProcessingRuleResource(ResourceInstances);
		ESRFacilityTemperatureState EffectiveTemperatureState = FacilityInstance.TemperatureState;
		for (const FSRFacilityEffectSpec& EffectSpec : FacilityDataAsset->Effects)
		{
			const StarRovers::FacilityEffects::FSRFacilityEffectConditionContext ConditionContext =
			{
				ConditionResource,
				ConditionResource,
				EffectiveTemperatureState
			};
			if (!StarRovers::FacilityEffects::DoEffectConditionsPass(EffectSpec, ConditionContext))
			{
				continue;
			}

			if (EffectSpec.EffectKind == ESRFacilityEffectKind::InvertHeat)
			{
				EffectiveTemperatureState = InvertProcessingRuleTemperatureState(EffectiveTemperatureState);
			}
			else if (EffectSpec.EffectKind == ESRFacilityEffectKind::OverrideProcessTemperature)
			{
				EffectiveTemperatureState = EffectSpec.ProcessTemperatureState;
			}
		}
		return EffectiveTemperatureState;
	}

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

		const ESRFacilityTemperatureState EffectiveTemperatureState = ResolveEffectiveProcessingTemperature(
			FacilityInstance,
			ResourceInstances);
		if (EffectiveTemperatureState == ESRFacilityTemperatureState::Frozen
			|| EffectiveTemperatureState == ESRFacilityTemperatureState::Overheated)
		{
			return false;
		}

		if (FacilityDataAsset->bRequiresColdTemperature && EffectiveTemperatureState != ESRFacilityTemperatureState::Cold)
		{
			return false;
		}

		if (FacilityDataAsset->bRequiresHotTemperature && EffectiveTemperatureState != ESRFacilityTemperatureState::Hot)
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
	const ESRFacilityTemperatureState EffectiveTemperatureState = ResolveEffectiveProcessingTemperature(
		FacilityInstance,
		FacilityInstance.ProcessingInventory);
	if (EffectiveTemperatureState == ESRFacilityTemperatureState::Cold)
	{
		ProcessSeconds *= 2.0f;
	}
	if (!IsValid(FacilityDataAsset))
	{
		return ProcessSeconds;
	}

	const FSRResourceInstance* ConditionResource = FindFirstProcessingRuleResource(FacilityInstance.ProcessingInventory);
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
