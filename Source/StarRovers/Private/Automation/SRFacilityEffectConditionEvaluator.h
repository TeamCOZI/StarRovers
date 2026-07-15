#pragma once

#include "CoreMinimal.h"
#include "Automation/SRFacilityDataAsset.h"

namespace StarRovers::FacilityEffects
{
	struct FSRFacilityEffectConditionContext
	{
		const FSRResourceInstance* CurrentResource = nullptr;
		const FSRResourceInstance* BaselineResource = nullptr;
		ESRFacilityTemperatureState TemperatureState = ESRFacilityTemperatureState::Normal;
	};

	inline int32 CountTagStacks(const FSRResourceInstance& ResourceInstance, ESRResourceProcessTag Tag)
	{
		int32 StackCount = 0;
		for (const FSRResourceTagStack& TagStack : ResourceInstance.Tags)
		{
			if (TagStack.Tag == Tag && TagStack.StackCount > 0)
			{
				StackCount += TagStack.StackCount;
			}
		}
		return StackCount;
	}

	inline bool HasTag(const FSRResourceInstance& ResourceInstance, ESRResourceProcessTag Tag)
	{
		return CountTagStacks(ResourceInstance, Tag) > 0;
	}

	inline int32 CountAllTagStacks(const FSRResourceInstance& ResourceInstance)
	{
		int32 StackCount = 0;
		for (const FSRResourceTagStack& TagStack : ResourceInstance.Tags)
		{
			if (TagStack.StackCount > 0)
			{
				StackCount += TagStack.StackCount;
			}
		}
		return StackCount;
	}

	inline bool IsPrimeEnergyValue(double EnergyValue)
	{
		if (!FMath::IsFinite(EnergyValue))
		{
			return false;
		}

		const int64 Candidate = FMath::RoundToInt64(EnergyValue);
		if (Candidate < 2 || FMath::Abs(EnergyValue - static_cast<double>(Candidate)) > UE_DOUBLE_KINDA_SMALL_NUMBER)
		{
			return false;
		}
		if (Candidate == 2)
		{
			return true;
		}
		if (Candidate % 2 == 0)
		{
			return false;
		}

		const int64 MaxDivisor = FMath::FloorToInt64(FMath::Sqrt(static_cast<double>(Candidate)));
		for (int64 Divisor = 3; Divisor <= MaxDivisor; Divisor += 2)
		{
			if (Candidate % Divisor == 0)
			{
				return false;
			}
		}
		return true;
	}

	inline bool DoesTagConditionPass(
		const FSRFacilityEffectConditionSpec& ConditionSpec,
		const FSRResourceInstance& ResourceInstance)
	{
		const int32 StackCount = ConditionSpec.TagTarget == ESRFacilityTagConditionTarget::AllTags
			? CountAllTagStacks(ResourceInstance)
			: CountTagStacks(ResourceInstance, ConditionSpec.ResourceTag);
		switch (ConditionSpec.TagMode)
		{
		case ESRFacilityTagConditionMode::HasTag:
			return StackCount > 0;
		case ESRFacilityTagConditionMode::MissingTag:
			return StackCount <= 0;
		case ESRFacilityTagConditionMode::StackCountAtLeast:
			return StackCount >= FMath::Max(1, ConditionSpec.TagStackCount);
		default:
			return false;
		}
	}

	inline bool DoesEffectConditionPass(
		const FSRFacilityEffectConditionSpec& ConditionSpec,
		const FSRFacilityEffectConditionContext& Context)
	{
		const FSRResourceInstance* CurrentResource = Context.CurrentResource;
		const FSRResourceInstance* BaselineResource = Context.BaselineResource;

		switch (ConditionSpec.ConditionKind)
		{
		case ESRFacilityEffectConditionKind::EnergyAtLeast:
			return CurrentResource && CurrentResource->EnergyValue >= ConditionSpec.EnergyValue;
		case ESRFacilityEffectConditionKind::EnergyAtMost:
			return CurrentResource && CurrentResource->EnergyValue <= ConditionSpec.EnergyValue;
		case ESRFacilityEffectConditionKind::EnergyIncreased:
			return CurrentResource && BaselineResource && CurrentResource->EnergyValue > BaselineResource->EnergyValue;
		case ESRFacilityEffectConditionKind::EnergyDecreased:
			return CurrentResource && BaselineResource && CurrentResource->EnergyValue < BaselineResource->EnergyValue;
		case ESRFacilityEffectConditionKind::Tag:
			return CurrentResource && DoesTagConditionPass(ConditionSpec, *CurrentResource);
		case ESRFacilityEffectConditionKind::TemperatureState:
			return Context.TemperatureState == ConditionSpec.TemperatureState;
		case ESRFacilityEffectConditionKind::ProcessCountEquals:
			return CurrentResource && CurrentResource->ProcessCount == FMath::Max(0, ConditionSpec.ProcessCount);
		case ESRFacilityEffectConditionKind::PrimeEnergy:
			return CurrentResource && IsPrimeEnergyValue(CurrentResource->EnergyValue);
		default:
			return false;
		}
	}

	inline bool DoEffectConditionsPass(
		const FSRFacilityEffectSpec& EffectSpec,
		const FSRFacilityEffectConditionContext& Context)
	{
		for (const FSRFacilityEffectConditionSpec& ConditionSpec : EffectSpec.Conditions)
		{
			if (!DoesEffectConditionPass(ConditionSpec, Context))
			{
				return false;
			}
		}
		return true;
	}
}
