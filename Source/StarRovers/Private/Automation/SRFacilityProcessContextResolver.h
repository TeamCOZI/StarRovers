#pragma once

#include "Automation/SRFacilityRuntimeData.h"
#include "SRFacilityEffectConditionEvaluator.h"

namespace StarRovers::FacilityProcessing
{
	struct FSRFacilityProcessContext
	{
		ESRFacilityTemperatureState PhysicalTemperatureState = ESRFacilityTemperatureState::Normal;
		ESRFacilityTemperatureState EffectiveTemperatureState = ESRFacilityTemperatureState::Normal;
		bool bInvertHeat = false;
		bool bInvertTagEffects = false;
		bool bHasProcessTemperatureOverride = false;
		ESRFacilityTemperatureState ProcessTemperatureOverride = ESRFacilityTemperatureState::Normal;
	};

	inline ESRFacilityTemperatureState InvertTemperatureState(ESRFacilityTemperatureState TemperatureState)
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

	inline ESRFacilityTemperatureState ResolveEffectiveTemperatureState(const FSRFacilityProcessContext& ProcessContext)
	{
		const ESRFacilityTemperatureState BaseTemperatureState = ProcessContext.bHasProcessTemperatureOverride
			? ProcessContext.ProcessTemperatureOverride
			: ProcessContext.PhysicalTemperatureState;
		return ProcessContext.bInvertHeat ? InvertTemperatureState(BaseTemperatureState) : BaseTemperatureState;
	}

	inline const FSRResourceInstance* FindFirstProcessResource(const TArray<FSRResourceInstance>& ResourceInstances)
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

	inline FSRFacilityProcessContext ResolveProcessContext(
		const USRFacilityDataAsset* FacilityDataAsset,
		ESRFacilityTemperatureState PhysicalTemperatureState,
		const FSRResourceInstance* ConditionResource,
		const FSRResourceInstance* BaselineResource = nullptr)
	{
		FSRFacilityProcessContext ProcessContext;
		ProcessContext.PhysicalTemperatureState = PhysicalTemperatureState;
		ProcessContext.EffectiveTemperatureState = ResolveEffectiveTemperatureState(ProcessContext);

		if (!IsValid(FacilityDataAsset))
		{
			return ProcessContext;
		}

		const FSRResourceInstance* ConditionBaselineResource = BaselineResource ? BaselineResource : ConditionResource;
		for (const FSRFacilityEffectSpec& EffectSpec : FacilityDataAsset->Effects)
		{
			const StarRovers::FacilityEffects::FSRFacilityEffectConditionContext ConditionContext =
			{
				ConditionResource,
				ConditionBaselineResource,
				ProcessContext.EffectiveTemperatureState
			};
			if (!StarRovers::FacilityEffects::DoEffectConditionsPass(EffectSpec, ConditionContext))
			{
				continue;
			}

			if (EffectSpec.EffectKind == ESRFacilityEffectKind::InvertHeat)
			{
				ProcessContext.bInvertHeat = !ProcessContext.bInvertHeat;
			}
			else if (EffectSpec.EffectKind == ESRFacilityEffectKind::OverrideProcessTemperature)
			{
				ProcessContext.bInvertHeat = false;
				ProcessContext.bHasProcessTemperatureOverride = true;
				ProcessContext.ProcessTemperatureOverride = EffectSpec.ProcessTemperatureState;
			}
			else if (EffectSpec.EffectKind == ESRFacilityEffectKind::InvertTagEffects)
			{
				ProcessContext.bInvertTagEffects = !ProcessContext.bInvertTagEffects;
			}

			ProcessContext.EffectiveTemperatureState = ResolveEffectiveTemperatureState(ProcessContext);
		}

		return ProcessContext;
	}

	inline FSRFacilityProcessContext ResolveProcessContext(
		const FSRFacilityInstance& FacilityInstance,
		const TArray<FSRResourceInstance>& ResourceInstances)
	{
		return ResolveProcessContext(
			FacilityInstance.FacilityDataAsset.Get(),
			FacilityInstance.TemperatureState,
			FindFirstProcessResource(ResourceInstances));
	}
}
