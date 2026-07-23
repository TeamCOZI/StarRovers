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
			if (EffectSpec.EffectKind != ESRFacilityEffectKind::InvertHeat
				&& EffectSpec.EffectKind != ESRFacilityEffectKind::OverrideProcessTemperature)
			{
				continue;
			}

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
			ProcessContext.EffectiveTemperatureState = ResolveEffectiveTemperatureState(ProcessContext);
		}

		return ProcessContext;
	}

	inline double ResolveProcessTimeAdjustmentValue(
		const FSRFacilityEffectSpec& EffectSpec,
		const FSRResourceInstance* ConditionResource,
		const TArray<FSRResourceInstance>* ProcessResources = nullptr)
	{
		if (EffectSpec.ProcessTimeValueSource == ESRFacilityProcessTimeAdjustmentValueSource::TagStackCount)
		{
			if (ProcessResources)
			{
				int32 StackCount = 0;
				for (const FSRResourceInstance& ProcessResource : *ProcessResources)
				{
					if (ProcessResource.ResourceId.IsNone())
					{
						continue;
					}
					StackCount += EffectSpec.TagStackCountTarget == ESRFacilityTagStackCountTarget::All
						? StarRovers::FacilityEffects::CountAllTagStacks(ProcessResource)
						: StarRovers::FacilityEffects::CountTagStacks(ProcessResource, EffectSpec.ResourceTag);
				}
				return static_cast<double>(StackCount);
			}
			if (!ConditionResource)
			{
				return 0.0;
			}
			return static_cast<double>(
				EffectSpec.TagStackCountTarget == ESRFacilityTagStackCountTarget::All
					? StarRovers::FacilityEffects::CountAllTagStacks(*ConditionResource)
					: StarRovers::FacilityEffects::CountTagStacks(*ConditionResource, EffectSpec.ResourceTag));
		}
		return EffectSpec.Value;
	}

	inline float ApplyProcessTimeAdjustment(
		float ProcessSeconds,
		const FSRFacilityEffectSpec& EffectSpec,
		const FSRResourceInstance* ConditionResource,
		const TArray<FSRResourceInstance>* ProcessResources = nullptr)
	{
		const double AdjustmentValue = ResolveProcessTimeAdjustmentValue(EffectSpec, ConditionResource, ProcessResources);
		if (EffectSpec.ProcessTimeMode == ESRFacilityProcessTimeAdjustmentMode::Multiply)
		{
			ProcessSeconds *= static_cast<float>(AdjustmentValue);
		}
		else
		{
			ProcessSeconds += static_cast<float>(AdjustmentValue);
		}
		return FMath::Max(0.01f, ProcessSeconds);
	}

	inline float ResolveFacilityProcessSeconds(
		const USRFacilityDataAsset* FacilityDataAsset,
		ESRFacilityTemperatureState EffectiveTemperatureState,
		const FSRResourceInstance* ConditionResource,
		const TArray<FSRResourceInstance>* ProcessResources = nullptr)
	{
		float ProcessSeconds = IsValid(FacilityDataAsset)
			? FMath::Max(0.01f, FacilityDataAsset->BaseProcessSeconds)
			: 1.0f;
		if (EffectiveTemperatureState == ESRFacilityTemperatureState::Cold)
		{
			ProcessSeconds *= 2.0f;
		}
		if (!IsValid(FacilityDataAsset))
		{
			return ProcessSeconds;
		}

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

			ProcessSeconds = ApplyProcessTimeAdjustment(ProcessSeconds, EffectSpec, ConditionResource, ProcessResources);
		}
		return FMath::Max(0.01f, ProcessSeconds);
	}

	inline float ResolveFacilityProcessSeconds(
		const USRFacilityDataAsset* FacilityDataAsset,
		ESRFacilityTemperatureState EffectiveTemperatureState,
		const TArray<FSRResourceInstance>& ProcessResources)
	{
		return ResolveFacilityProcessSeconds(
			FacilityDataAsset,
			EffectiveTemperatureState,
			FindFirstProcessResource(ProcessResources),
			&ProcessResources);
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
