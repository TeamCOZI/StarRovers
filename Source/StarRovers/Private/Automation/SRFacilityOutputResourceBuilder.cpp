#include "SRFacilityOutputResourceBuilder.h"

#include "Automation/SRFacilityDataAsset.h"
#include "SRFacilityEffectConditionEvaluator.h"
#include "SRFacilityResourceOperations.h"

namespace
{
	constexpr double HeatResponsiveBaseEnergyBonus = 1.0;
	constexpr double HeatResponsiveHotEnergyBonus = 1.0;
	constexpr double SupercooledEnergyBonus = 3.0;
	constexpr double VolatileEnergyPenalty = 1.0;
	constexpr double HighActivityEnergyBonusPerProcessLimitLoss = 1.0;
	const FName CompoundResourceId(TEXT("Compound"));

	struct FSRFacilityEffectContext
	{
		bool bInvertHeat = false;
		bool bInvertTagEffects = false;
		bool bHasProcessTemperatureOverride = false;
		ESRFacilityTemperatureState ProcessTemperatureOverride = ESRFacilityTemperatureState::Normal;
		bool bHasLastAttachedTag = false;
		ESRResourceProcessTag LastAttachedTag = ESRResourceProcessTag::Responsive;
		int32 EnergyChangeCount = 0;
	};

	ESRFacilityTemperatureState InvertTemperatureState(ESRFacilityTemperatureState TemperatureState)
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

	double ApplyTagEffectDirection(double EnergyDelta, const FSRFacilityEffectContext& EffectContext)
	{
		return EffectContext.bInvertTagEffects ? -EnergyDelta : EnergyDelta;
	}

	ESRFacilityTemperatureState ResolveEffectiveTemperatureState(
		ESRFacilityTemperatureState TemperatureState,
		const FSRFacilityEffectContext& EffectContext)
	{
		const ESRFacilityTemperatureState BaseTemperatureState = EffectContext.bHasProcessTemperatureOverride
			? EffectContext.ProcessTemperatureOverride
			: TemperatureState;
		if (!EffectContext.bInvertHeat)
		{
			return BaseTemperatureState;
		}

		return InvertTemperatureState(BaseTemperatureState);
	}

	FSRFacilityEffectContext ResolvePreProcessingEffectContext(
		const USRFacilityDataAsset* FacilityDataAsset,
		const FSRResourceInstance* ConditionResource,
		ESRFacilityTemperatureState TemperatureState)
	{
		FSRFacilityEffectContext EffectContext;
		if (!IsValid(FacilityDataAsset))
		{
			return EffectContext;
		}

		for (const FSRFacilityEffectSpec& EffectSpec : FacilityDataAsset->Effects)
		{
			const ESRFacilityTemperatureState EffectiveTemperatureState = ResolveEffectiveTemperatureState(
				TemperatureState,
				EffectContext);
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
				EffectContext.bInvertHeat = !EffectContext.bInvertHeat;
			}
			else if (EffectSpec.EffectKind == ESRFacilityEffectKind::OverrideProcessTemperature)
			{
				EffectContext.bInvertHeat = false;
				EffectContext.bHasProcessTemperatureOverride = true;
				EffectContext.ProcessTemperatureOverride = EffectSpec.ProcessTemperatureState;
			}
			else if (EffectSpec.EffectKind == ESRFacilityEffectKind::InvertTagEffects)
			{
				EffectContext.bInvertTagEffects = !EffectContext.bInvertTagEffects;
			}
		}
		return EffectContext;
	}

	FSRResourceTagStack* FindTagStack(TArray<FSRResourceTagStack>& Tags, ESRResourceProcessTag Tag)
	{
		for (FSRResourceTagStack& TagStack : Tags)
		{
			if (TagStack.Tag == Tag)
			{
				return &TagStack;
			}
		}

		return nullptr;
	}

	bool HasTag(const TArray<FSRResourceTagStack>& Tags, ESRResourceProcessTag Tag)
	{
		for (const FSRResourceTagStack& TagStack : Tags)
		{
			if (TagStack.Tag == Tag && TagStack.StackCount > 0)
			{
				return true;
			}
		}

		return false;
	}

	bool HasTag(const FSRResourceInstance& ResourceInstance, ESRResourceProcessTag Tag)
	{
		return HasTag(ResourceInstance.Tags, Tag);
	}

	int32 CountTagStacks(const TArray<FSRResourceTagStack>& Tags, ESRResourceProcessTag Tag)
	{
		int32 StackCount = 0;
		for (const FSRResourceTagStack& TagStack : Tags)
		{
			if (TagStack.Tag == Tag && TagStack.StackCount > 0)
			{
				StackCount += TagStack.StackCount;
			}
		}
		return StackCount;
	}

	int32 CountTagStacks(const FSRResourceInstance& ResourceInstance, ESRResourceProcessTag Tag)
	{
		return CountTagStacks(ResourceInstance.Tags, Tag);
	}

	void SetEnergyValue(FSRResourceInstance& ResourceInstance, double NewEnergyValue, FSRFacilityEffectContext& EffectContext)
	{
		if (!FMath::IsNearlyEqual(ResourceInstance.EnergyValue, NewEnergyValue))
		{
			ResourceInstance.EnergyValue = NewEnergyValue;
			++EffectContext.EnergyChangeCount;
		}
	}

	void AddEnergyDelta(FSRResourceInstance& ResourceInstance, double EnergyDelta, FSRFacilityEffectContext& EffectContext)
	{
		SetEnergyValue(ResourceInstance, ResourceInstance.EnergyValue + EnergyDelta, EffectContext);
	}

	void MultiplyEnergyValue(FSRResourceInstance& ResourceInstance, double Multiplier, FSRFacilityEffectContext& EffectContext)
	{
		SetEnergyValue(ResourceInstance, ResourceInstance.EnergyValue * Multiplier, EffectContext);
	}

	bool CanEnergyResourceEnterFacility(
		const FSRResourceInstance& ResourceInstance,
		ESRFacilityTemperatureState TemperatureState)
	{
		return ResourceInstance.RemainingProcessLimit > 0
			&& !HasTag(ResourceInstance, ESRResourceProcessTag::Singularity)
			&& (!HasTag(ResourceInstance, ESRResourceProcessTag::Supercooled)
				|| TemperatureState == ESRFacilityTemperatureState::Cold);
	}

	void MergeTagStacks(TArray<FSRResourceTagStack>& InOutTags, const TArray<FSRResourceTagStack>& TagsToMerge)
	{
		for (const FSRResourceTagStack& TagToMerge : TagsToMerge)
		{
			if (TagToMerge.StackCount <= 0)
			{
				continue;
			}

			if (FSRResourceTagStack* ExistingTag = FindTagStack(InOutTags, TagToMerge.Tag))
			{
				ExistingTag->StackCount += TagToMerge.StackCount;
				if (TagToMerge.RemainingCycles > 0)
				{
					ExistingTag->RemainingCycles = ExistingTag->RemainingCycles > 0
						? FMath::Min(ExistingTag->RemainingCycles, TagToMerge.RemainingCycles)
						: TagToMerge.RemainingCycles;
				}
				else if (TagToMerge.Tag == ESRResourceProcessTag::HalfLife && ExistingTag->RemainingCycles <= 0)
				{
					ExistingTag->RemainingCycles = StarRovers::FacilityResources::HalfLifeDefaultCycles;
				}
				continue;
			}

			FSRResourceTagStack NewTagStack = TagToMerge;
			if (NewTagStack.Tag == ESRResourceProcessTag::HalfLife && NewTagStack.RemainingCycles <= 0)
			{
				NewTagStack.RemainingCycles = StarRovers::FacilityResources::HalfLifeDefaultCycles;
			}
			InOutTags.Add(NewTagStack);
		}
	}

	bool TryCountInputEnergyResources(
		const TArray<FSRResourceInstance>& InputResources,
		ESRFacilityTemperatureState TemperatureState,
		int32& OutEnergyCount)
	{
		OutEnergyCount = 0;
		for (const FSRResourceInstance& ResourceInstance : InputResources)
		{
			if (!CanEnergyResourceEnterFacility(ResourceInstance, TemperatureState))
			{
				return false;
			}
			++OutEnergyCount;
		}

		return true;
	}

	void SetProcessLimitAndApplyHighActivity(
		FSRResourceInstance& ResourceInstance,
		int32 NewProcessLimit,
		FSRFacilityEffectContext& EffectContext)
	{
		const int32 PreviousProcessLimit = FMath::Max(0, ResourceInstance.RemainingProcessLimit);
		const int32 ClampedProcessLimit = FMath::Max(0, NewProcessLimit);
		ResourceInstance.RemainingProcessLimit = ClampedProcessLimit;

		const int32 LostProcessLimit = FMath::Max(0, PreviousProcessLimit - ClampedProcessLimit);
		const int32 HighActivityStackCount = CountTagStacks(ResourceInstance, ESRResourceProcessTag::HighActivity);
		if (LostProcessLimit > 0 && HighActivityStackCount > 0)
		{
			const double EnergyDelta = static_cast<double>(LostProcessLimit * HighActivityStackCount)
				* HighActivityEnergyBonusPerProcessLimitLoss;
			AddEnergyDelta(ResourceInstance, ApplyTagEffectDirection(EnergyDelta, EffectContext), EffectContext);
		}
	}

	void ConsumeEnergyForFacilityPass(
		FSRResourceInstance& ResourceInstance,
		ESRFacilityTemperatureState TemperatureState,
		FSRFacilityEffectContext& EffectContext)
	{
		SetProcessLimitAndApplyHighActivity(ResourceInstance, ResourceInstance.RemainingProcessLimit - 1, EffectContext);
		++ResourceInstance.ProcessCount;
		if (TemperatureState == ESRFacilityTemperatureState::Hot)
		{
			SetProcessLimitAndApplyHighActivity(ResourceInstance, ResourceInstance.RemainingProcessLimit - 1, EffectContext);
		}
	}

	void ConsumeEnergyInputsForFacilityPass(
		TArray<FSRResourceInstance>& InOutResources,
		ESRFacilityTemperatureState TemperatureState,
		FSRFacilityEffectContext& EffectContext)
	{
		for (FSRResourceInstance& ResourceInstance : InOutResources)
		{
			ConsumeEnergyForFacilityPass(ResourceInstance, TemperatureState, EffectContext);
		}
	}

	const FSRResourceInstance* FindFirstResource(const TArray<FSRResourceInstance>& ResourceInstances)
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

	float ResolveFacilityProcessSeconds(
		const FSRFacilityInstance& FacilityInstance,
		ESRFacilityTemperatureState EffectiveTemperatureState,
		const FSRResourceInstance* ConditionResource)
	{
		const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
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

	int32 ResolveChargeStacksToAddForProcessingPass(
		const FSRFacilityInstance& FacilityInstance,
		ESRFacilityTemperatureState EffectiveTemperatureState,
		const FSRResourceInstance* ConditionResource)
	{
		return FMath::Max(0, FMath::FloorToInt(ResolveFacilityProcessSeconds(
			FacilityInstance,
			EffectiveTemperatureState,
			ConditionResource)))
			* StarRovers::FacilityResources::ChargeStacksPerProcessingSecond;
	}

	void ApplyHalfLifeProcessingEffect(
		FSRResourceInstance& ResourceInstance,
		FSRFacilityEffectContext& EffectContext)
	{
		for (FSRResourceTagStack& TagStack : ResourceInstance.Tags)
		{
			if (TagStack.Tag != ESRResourceProcessTag::HalfLife || TagStack.StackCount <= 0)
			{
				continue;
			}

			if (TagStack.RemainingCycles <= 0)
			{
				TagStack.RemainingCycles = StarRovers::FacilityResources::HalfLifeDefaultCycles;
			}

			--TagStack.RemainingCycles;
			if (TagStack.RemainingCycles > 0)
			{
				continue;
			}

			MultiplyEnergyValue(ResourceInstance, EffectContext.bInvertTagEffects ? 2.0 : 0.5, EffectContext);
			TagStack.RemainingCycles = StarRovers::FacilityResources::HalfLifeDefaultCycles;
		}
	}

	int32 CountChargeStacks(const FSRResourceInstance& ResourceInstance)
	{
		int32 ChargeStackCount = 0;
		for (const FSRResourceTagStack& TagStack : ResourceInstance.Tags)
		{
			if (TagStack.Tag == ESRResourceProcessTag::Charge && TagStack.StackCount > 0)
			{
				ChargeStackCount += FMath::Max(0, TagStack.RemainingCycles);
			}
		}
		return ChargeStackCount;
	}

	bool ConsumeChargeStacks(FSRResourceInstance& ResourceInstance, int32 RequiredStackCount)
	{
		int32 RemainingRequiredStackCount = FMath::Max(0, RequiredStackCount);
		if (RemainingRequiredStackCount <= 0 || CountChargeStacks(ResourceInstance) < RemainingRequiredStackCount)
		{
			return false;
		}

		for (FSRResourceTagStack& TagStack : ResourceInstance.Tags)
		{
			if (TagStack.Tag != ESRResourceProcessTag::Charge || TagStack.StackCount <= 0)
			{
				continue;
			}

			const int32 ConsumedStackCount = FMath::Min(RemainingRequiredStackCount, FMath::Max(0, TagStack.RemainingCycles));
			TagStack.RemainingCycles = FMath::Max(0, TagStack.RemainingCycles - ConsumedStackCount);
			RemainingRequiredStackCount -= ConsumedStackCount;
			if (RemainingRequiredStackCount <= 0)
			{
				return true;
			}
		}

		return false;
	}

	void AddChargeStacks(FSRResourceInstance& ResourceInstance, int32 StackCountToAdd)
	{
		const int32 SafeStackCountToAdd = FMath::Max(0, StackCountToAdd);
		if (SafeStackCountToAdd <= 0)
		{
			return;
		}

		for (FSRResourceTagStack& TagStack : ResourceInstance.Tags)
		{
			if (TagStack.Tag == ESRResourceProcessTag::Charge && TagStack.StackCount > 0)
			{
				TagStack.RemainingCycles = FMath::Max(0, TagStack.RemainingCycles) + SafeStackCountToAdd;
				return;
			}
		}
	}

	void ApplyChargeProcessingEffect(
		FSRResourceInstance& ResourceInstance,
		int32 ChargeStacksToAdd,
		FSRFacilityEffectContext& EffectContext)
	{
		if (ConsumeChargeStacks(ResourceInstance, StarRovers::FacilityResources::ChargeRequiredStacks))
		{
			AddEnergyDelta(ResourceInstance, ApplyTagEffectDirection(
				StarRovers::FacilityResources::ChargeEnergyBonus,
				EffectContext), EffectContext);
		}
		AddChargeStacks(ResourceInstance, ChargeStacksToAdd);
	}

	void ApplyTagEffects(
		ESRFacilityTemperatureState TemperatureState,
		int32 ChargeStacksToAdd,
		FSRFacilityEffectContext& EffectContext,
		FSRResourceInstance& ResourceInstance)
	{
		const int32 ResponsiveStackCount = CountTagStacks(ResourceInstance, ESRResourceProcessTag::Responsive);
		if (ResponsiveStackCount > 0)
		{
			AddEnergyDelta(ResourceInstance, ApplyTagEffectDirection(
				static_cast<double>(ResponsiveStackCount) * HeatResponsiveBaseEnergyBonus,
				EffectContext), EffectContext);
			if (TemperatureState == ESRFacilityTemperatureState::Hot)
			{
				AddEnergyDelta(ResourceInstance, ApplyTagEffectDirection(
					static_cast<double>(ResponsiveStackCount) * HeatResponsiveHotEnergyBonus,
					EffectContext), EffectContext);
			}
		}

		const int32 SupercooledStackCount = CountTagStacks(ResourceInstance, ESRResourceProcessTag::Supercooled);
		if (SupercooledStackCount > 0 && TemperatureState == ESRFacilityTemperatureState::Cold)
		{
			AddEnergyDelta(ResourceInstance, ApplyTagEffectDirection(
				static_cast<double>(SupercooledStackCount) * SupercooledEnergyBonus,
				EffectContext), EffectContext);
		}

		const int32 VolatileStackCount = CountTagStacks(ResourceInstance, ESRResourceProcessTag::Volatile);
		if (VolatileStackCount > 0)
		{
			AddEnergyDelta(ResourceInstance, ApplyTagEffectDirection(
				static_cast<double>(VolatileStackCount) * -VolatileEnergyPenalty,
				EffectContext), EffectContext);
		}

		ApplyChargeProcessingEffect(ResourceInstance, ChargeStacksToAdd, EffectContext);
		ApplyHalfLifeProcessingEffect(ResourceInstance, EffectContext);
	}

	double ResolveEnergyAdjustmentValue(
		const FSRFacilityEffectSpec& EffectSpec,
		const FSRResourceInstance& ResourceInstance,
		const FSRFacilityEffectContext& EffectContext)
	{
		switch (EffectSpec.EnergyValueSource)
		{
		case ESRFacilityEnergyAdjustmentValueSource::RemainingProcessLimit:
			return static_cast<double>(FMath::Max(0, ResourceInstance.RemainingProcessLimit));
		case ESRFacilityEnergyAdjustmentValueSource::TagStackCount:
			return static_cast<double>(CountTagStacks(ResourceInstance, EffectSpec.ResourceTag));
		case ESRFacilityEnergyAdjustmentValueSource::EnergyChangeCount:
			return static_cast<double>(FMath::Max(0, EffectContext.EnergyChangeCount));
		case ESRFacilityEnergyAdjustmentValueSource::FixedValue:
		default:
			return EffectSpec.Value;
		}
	}

	bool ResolveSpecificEffectTag(
		const FSRFacilityEffectSpec& EffectSpec,
		const FSRFacilityEffectContext& EffectContext,
		ESRResourceProcessTag& OutTag)
	{
		if (EffectSpec.TagTarget == ESRFacilityEffectTagTarget::SpecificTag)
		{
			OutTag = EffectSpec.ResourceTag;
			return true;
		}
		if (EffectSpec.TagTarget == ESRFacilityEffectTagTarget::LastAttachedTag && EffectContext.bHasLastAttachedTag)
		{
			OutTag = EffectContext.LastAttachedTag;
			return true;
		}
		return false;
	}

	void RemoveTagStack(FSRResourceInstance& ResourceInstance, ESRResourceProcessTag Tag)
	{
		for (int32 TagIndex = ResourceInstance.Tags.Num() - 1; TagIndex >= 0; --TagIndex)
		{
			if (ResourceInstance.Tags[TagIndex].Tag == Tag)
			{
				ResourceInstance.Tags.RemoveAt(TagIndex);
			}
		}
	}

	void RemoveTagsForEffect(
		FSRResourceInstance& ResourceInstance,
		const FSRFacilityEffectSpec& EffectSpec,
		const FSRFacilityEffectContext& EffectContext)
	{
		if (EffectSpec.TagTarget == ESRFacilityEffectTagTarget::AllTags)
		{
			ResourceInstance.Tags.Reset();
			return;
		}

		ESRResourceProcessTag Tag = ESRResourceProcessTag::Responsive;
		if (ResolveSpecificEffectTag(EffectSpec, EffectContext, Tag))
		{
			RemoveTagStack(ResourceInstance, Tag);
		}
	}

	int32 ResolveConsumedProcessLimit(
		const FSRResourceInstance& BaselineResource,
		const FSRResourceInstance& ResourceInstance)
	{
		return FMath::Max(
			0,
			FMath::Max(0, BaselineResource.RemainingProcessLimit)
				- FMath::Max(0, ResourceInstance.RemainingProcessLimit));
	}

	void ApplyImmediateTagEffect(
		FSRResourceInstance& ResourceInstance,
		ESRResourceProcessTag Tag,
		int32 StackCount,
		ESRFacilityTemperatureState TemperatureState,
		const FSRResourceInstance& BaselineResource,
		FSRFacilityEffectContext& EffectContext)
	{
		const int32 SafeStackCount = FMath::Max(1, StackCount);
		switch (Tag)
		{
		case ESRResourceProcessTag::Responsive:
			AddEnergyDelta(
				ResourceInstance,
				ApplyTagEffectDirection(static_cast<double>(SafeStackCount) * HeatResponsiveBaseEnergyBonus, EffectContext),
				EffectContext);
			if (TemperatureState == ESRFacilityTemperatureState::Hot)
			{
				AddEnergyDelta(
					ResourceInstance,
					ApplyTagEffectDirection(static_cast<double>(SafeStackCount) * HeatResponsiveHotEnergyBonus, EffectContext),
					EffectContext);
			}
			break;
		case ESRResourceProcessTag::Supercooled:
			AddEnergyDelta(
				ResourceInstance,
				ApplyTagEffectDirection(static_cast<double>(SafeStackCount) * SupercooledEnergyBonus, EffectContext),
				EffectContext);
			break;
		case ESRResourceProcessTag::Volatile:
			AddEnergyDelta(
				ResourceInstance,
				ApplyTagEffectDirection(static_cast<double>(SafeStackCount) * -VolatileEnergyPenalty, EffectContext),
				EffectContext);
			break;
		case ESRResourceProcessTag::HighActivity:
			AddEnergyDelta(
				ResourceInstance,
				ApplyTagEffectDirection(
					static_cast<double>(ResolveConsumedProcessLimit(BaselineResource, ResourceInstance) * SafeStackCount)
						* HighActivityEnergyBonusPerProcessLimitLoss,
					EffectContext),
				EffectContext);
			break;
		case ESRResourceProcessTag::HalfLife:
			MultiplyEnergyValue(ResourceInstance, EffectContext.bInvertTagEffects ? 2.0 : 0.5, EffectContext);
			break;
		case ESRResourceProcessTag::Charge:
			AddEnergyDelta(
				ResourceInstance,
				ApplyTagEffectDirection(StarRovers::FacilityResources::ChargeEnergyBonus, EffectContext),
				EffectContext);
			break;
		case ESRResourceProcessTag::Waste:
		case ESRResourceProcessTag::Singularity:
		default:
			break;
		}
	}

	void TriggerTagsForEffect(
		FSRResourceInstance& ResourceInstance,
		const FSRFacilityEffectSpec& EffectSpec,
		ESRFacilityTemperatureState TemperatureState,
		const FSRResourceInstance& BaselineResource,
		FSRFacilityEffectContext& EffectContext)
	{
		if (EffectSpec.TagTarget == ESRFacilityEffectTagTarget::AllTags)
		{
			const TArray<FSRResourceTagStack> TagsToTrigger = ResourceInstance.Tags;
			for (const FSRResourceTagStack& TagStack : TagsToTrigger)
			{
				if (TagStack.StackCount > 0)
				{
					ApplyImmediateTagEffect(
						ResourceInstance,
						TagStack.Tag,
						TagStack.StackCount,
						TemperatureState,
						BaselineResource,
						EffectContext);
				}
			}
			return;
		}

		ESRResourceProcessTag Tag = ESRResourceProcessTag::Responsive;
		if (ResolveSpecificEffectTag(EffectSpec, EffectContext, Tag))
		{
			const int32 StackCount = FMath::Max(1, CountTagStacks(ResourceInstance, Tag));
			ApplyImmediateTagEffect(ResourceInstance, Tag, StackCount, TemperatureState, BaselineResource, EffectContext);
		}
	}

	FSRResourceInstance BuildSynthesisProductResource(const TArray<FSRResourceInstance>& ProcessedResources)
	{
		FSRResourceInstance OutputResource;
		OutputResource.ResourceInstanceId = FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		OutputResource.ResourceId = CompoundResourceId;
		OutputResource.ResourceDataAsset = nullptr;
		OutputResource.EnergyValue = 1.0;
		OutputResource.RemainingProcessLimit = TNumericLimits<int32>::Max();
		OutputResource.ProcessCount = 0;
		OutputResource.StackCount = 1;
		OutputResource.Tags.Reset();

		const FSRResourceInstance* FirstInputResource = nullptr;
		bool bAllInputsShareResourceId = true;
		for (const FSRResourceInstance& ProcessedResource : ProcessedResources)
		{
			if (!FirstInputResource)
			{
				FirstInputResource = &ProcessedResource;
			}
			else if (ProcessedResource.ResourceId != FirstInputResource->ResourceId)
			{
				bAllInputsShareResourceId = false;
			}

			OutputResource.EnergyValue *= ProcessedResource.EnergyValue;
			OutputResource.RemainingProcessLimit = FMath::Min(OutputResource.RemainingProcessLimit, ProcessedResource.RemainingProcessLimit);
			OutputResource.ProcessCount += ProcessedResource.ProcessCount;
			MergeTagStacks(OutputResource.Tags, ProcessedResource.Tags);
		}

		if (bAllInputsShareResourceId && FirstInputResource)
		{
			OutputResource.ResourceId = FirstInputResource->ResourceId;
			OutputResource.ResourceDataAsset = FirstInputResource->ResourceDataAsset;
		}

		if (OutputResource.RemainingProcessLimit == TNumericLimits<int32>::Max())
		{
			OutputResource.RemainingProcessLimit = 0;
		}
		return OutputResource;
	}

	int32 CountAdditionalOutputResourcesForInputCount(
		const USRFacilityDataAsset* FacilityDataAsset,
		int32 InputResourceCount)
	{
		if (!IsValid(FacilityDataAsset))
		{
			return 0;
		}

		int32 AdditionalOutputCount = 0;
		const int32 SafeInputResourceCount = FMath::Max(0, InputResourceCount);
		for (const FSRFacilityEffectSpec& EffectSpec : FacilityDataAsset->Effects)
		{
			const int32 Count = FMath::Max(1, EffectSpec.Count);
			if (EffectSpec.EffectKind == ESRFacilityEffectKind::ProduceWaste && IsValid(EffectSpec.ProducedResource.Get()))
			{
				AdditionalOutputCount += Count;
			}
			else if (EffectSpec.EffectKind == ESRFacilityEffectKind::DuplicateInputResource)
			{
				AdditionalOutputCount += Count * SafeInputResourceCount;
			}
		}
		return AdditionalOutputCount;
	}

	bool DoesEffectSequenceRemovePrimaryOutput(const USRFacilityDataAsset* FacilityDataAsset)
	{
		if (!IsValid(FacilityDataAsset))
		{
			return false;
		}

		bool bHasPrimaryOutput = true;
		for (const FSRFacilityEffectSpec& EffectSpec : FacilityDataAsset->Effects)
		{
			if (EffectSpec.EffectKind == ESRFacilityEffectKind::RemoveResource)
			{
				bHasPrimaryOutput = false;
			}
		}
		return !bHasPrimaryOutput;
	}

	int32 ResolvePrimaryOutputCountForAdditionalOutputs(
		const FSRFacilityInstance& FacilityInstance,
		bool bHasPrimaryResource,
		int32 AdditionalOutputCount)
	{
		if (!bHasPrimaryResource)
		{
			return 0;
		}

		const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
		if (!IsValid(FacilityDataAsset))
		{
			return 0;
		}

		const int32 OutputPortCount = FacilityInstance.OutputPortInventories.Num();
		if (FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Split)
		{
			const int32 PrimaryOutputCount = OutputPortCount - FMath::Max(0, AdditionalOutputCount);
			return PrimaryOutputCount >= 2 ? PrimaryOutputCount : 0;
		}

		if (FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Mine)
		{
			return OutputPortCount > 0 ? 1 : 0;
		}

		return OutputPortCount > 0 ? 1 : 0;
	}

	FSRResourceInstance MakeDuplicatedResourceInstance(const FSRResourceInstance& ResourceInstance)
	{
		FSRResourceInstance DuplicatedResource = ResourceInstance;
		DuplicatedResource.ResourceInstanceId = FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		DuplicatedResource.StackCount = FMath::Max(1, DuplicatedResource.StackCount);
		return DuplicatedResource;
	}
}

bool FSRFacilityOutputResourceBuilder::DoesInputSetMatchOperation(
	const USRFacilityDataAsset* FacilityDataAsset,
	const TArray<FSRResourceInstance>& InputResources,
	ESRFacilityTemperatureState TemperatureState)
{
	if (!IsValid(FacilityDataAsset))
	{
		return false;
	}

	if (FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Mine)
	{
		return InputResources.IsEmpty();
	}

	if (InputResources.IsEmpty())
	{
		return false;
	}

	const FSRResourceInstance* ConditionResource = FindFirstResource(InputResources);
	const FSRFacilityEffectContext PreProcessingEffectContext = ResolvePreProcessingEffectContext(
		FacilityDataAsset,
		ConditionResource,
		TemperatureState);
	const ESRFacilityTemperatureState EffectiveTemperatureState = ResolveEffectiveTemperatureState(
		TemperatureState,
		PreProcessingEffectContext);
	int32 EnergyCount = 0;
	if (!TryCountInputEnergyResources(InputResources, EffectiveTemperatureState, EnergyCount))
	{
		return false;
	}

	switch (FacilityDataAsset->OperationKind)
	{
	case ESRFacilityOperationKind::Process:
		return EnergyCount >= 1;
	case ESRFacilityOperationKind::Synthesize:
		return EnergyCount >= 1;
	case ESRFacilityOperationKind::Split:
		return EnergyCount == 1;
	case ESRFacilityOperationKind::Mine:
		return InputResources.IsEmpty();
	default:
		return false;
	}
}

int32 FSRFacilityOutputResourceBuilder::CountProducedOutputResources(const USRFacilityDataAsset* FacilityDataAsset)
{
	const int32 ConfiguredInputCount = IsValid(FacilityDataAsset)
		? FMath::Max(0, FacilityDataAsset->InputInventory.SlotCount)
		: 0;
	return CountAdditionalOutputResourcesForInputCount(FacilityDataAsset, ConfiguredInputCount);
}

int32 FSRFacilityOutputResourceBuilder::ResolvePrimaryOutputCount(const FSRFacilityInstance& FacilityInstance)
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset))
	{
		return 0;
	}

	const int32 OutputPortCount = FacilityInstance.OutputPortInventories.Num();
	const int32 ProducedOutputCount = CountAdditionalOutputResourcesForInputCount(
		FacilityDataAsset,
		FacilityInstance.InputPortInventories.Num());
	if (DoesEffectSequenceRemovePrimaryOutput(FacilityDataAsset))
	{
		return 0;
	}

	if (FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Split)
	{
		const int32 PrimaryOutputCount = OutputPortCount - ProducedOutputCount;
		return PrimaryOutputCount >= 2 ? PrimaryOutputCount : 0;
	}

	if (FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Mine)
	{
		return OutputPortCount > 0 ? 1 : 0;
	}

	return OutputPortCount > 0 ? 1 : 0;
}

int32 FSRFacilityOutputResourceBuilder::ResolveRequiredOutputSlots(const FSRFacilityInstance& FacilityInstance)
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset))
	{
		return 0;
	}

	return ResolvePrimaryOutputCount(FacilityInstance)
		+ CountAdditionalOutputResourcesForInputCount(FacilityDataAsset, FacilityInstance.InputPortInventories.Num());
}

bool FSRFacilityOutputResourceBuilder::AllowsEmptyOutput(const FSRFacilityInstance& FacilityInstance)
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	return DoesEffectSequenceRemovePrimaryOutput(FacilityDataAsset)
		&& CountAdditionalOutputResourcesForInputCount(FacilityDataAsset, FacilityInstance.InputPortInventories.Num()) <= 0;
}

void FSRFacilityOutputResourceBuilder::BuildOutputResources(
	const FSRFacilityInstance& FacilityInstance,
	const TArray<FSRResourceInstance>& InputResources,
	TArray<FSRResourceInstance>& OutOutputResources,
	int32* OutPrimaryOutputCount,
	FSRResourceInstance* OutBaselinePrimaryResource)
{
	OutOutputResources.Reset();
	if (OutPrimaryOutputCount)
	{
		*OutPrimaryOutputCount = 0;
	}
	if (OutBaselinePrimaryResource)
	{
		*OutBaselinePrimaryResource = FSRResourceInstance();
	}
	if (!IsValid(FacilityInstance.FacilityDataAsset.Get()) || InputResources.IsEmpty())
	{
		return;
	}

	int32 InitialEnergyChangeCount = 0;
	FSRResourceInstance OutputResource = BuildBaseOutputResource(FacilityInstance, InputResources, InitialEnergyChangeCount);
	BuildOutputResourcesFromPrimaryResource(
		FacilityInstance,
		InputResources,
		OutputResource,
		OutOutputResources,
		OutPrimaryOutputCount,
		OutBaselinePrimaryResource,
		InitialEnergyChangeCount);
}

void FSRFacilityOutputResourceBuilder::BuildOutputResourcesFromPrimaryResource(
	const FSRFacilityInstance& FacilityInstance,
	const TArray<FSRResourceInstance>& InputResources,
	const FSRResourceInstance& PrimaryResource,
	TArray<FSRResourceInstance>& OutOutputResources,
	int32* OutPrimaryOutputCount,
	FSRResourceInstance* OutBaselinePrimaryResource,
	int32 InitialEnergyChangeCount)
{
	OutOutputResources.Reset();
	if (OutPrimaryOutputCount)
	{
		*OutPrimaryOutputCount = 0;
	}
	if (OutBaselinePrimaryResource)
	{
		*OutBaselinePrimaryResource = FSRResourceInstance();
	}
	if (!IsValid(FacilityInstance.FacilityDataAsset.Get()) || PrimaryResource.ResourceId.IsNone())
	{
		return;
	}

	if (OutBaselinePrimaryResource)
	{
		*OutBaselinePrimaryResource = PrimaryResource;
	}
	FSRResourceInstance OutputResource = PrimaryResource;
	TArray<FSRResourceInstance> AdditionalOutputs;
	AdditionalOutputs.Reserve(CountAdditionalOutputResourcesForInputCount(
		FacilityInstance.FacilityDataAsset.Get(),
		InputResources.Num()));
	bool bHasPrimaryResource = true;
	ApplyFacilityEffects(
		FacilityInstance,
		InputResources,
		OutputResource,
		bHasPrimaryResource,
		AdditionalOutputs,
		InitialEnergyChangeCount);

	const int32 OutputCount = ResolvePrimaryOutputCountForAdditionalOutputs(
		FacilityInstance,
		bHasPrimaryResource,
		AdditionalOutputs.Num());
	if (OutPrimaryOutputCount)
	{
		*OutPrimaryOutputCount = OutputCount;
	}
	OutOutputResources.Reserve(OutputCount + AdditionalOutputs.Num());
	for (int32 OutputIndex = 0; OutputIndex < OutputCount; ++OutputIndex)
	{
		OutOutputResources.Add(OutputResource);
	}
	OutOutputResources.Append(AdditionalOutputs);
}

FSRResourceInstance FSRFacilityOutputResourceBuilder::BuildBaseOutputResource(
	const FSRFacilityInstance& FacilityInstance,
	const TArray<FSRResourceInstance>& ConsumedResources,
	int32& OutEnergyChangeCount)
{
	OutEnergyChangeCount = 0;
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset) || ConsumedResources.IsEmpty())
	{
		return !ConsumedResources.IsEmpty() ? ConsumedResources[0] : FSRResourceInstance();
	}

	TArray<FSRResourceInstance> ProcessedResources = ConsumedResources;
	const FSRResourceInstance* ConditionResource = FindFirstResource(ConsumedResources);
	FSRFacilityEffectContext PreProcessingEffectContext = ResolvePreProcessingEffectContext(
		FacilityDataAsset,
		ConditionResource,
		FacilityInstance.TemperatureState);
	const ESRFacilityTemperatureState EffectiveTemperatureState = ResolveEffectiveTemperatureState(
		FacilityInstance.TemperatureState,
		PreProcessingEffectContext);
	ConsumeEnergyInputsForFacilityPass(ProcessedResources, EffectiveTemperatureState, PreProcessingEffectContext);
	const int32 ChargeStacksToAdd = ResolveChargeStacksToAddForProcessingPass(
		FacilityInstance,
		EffectiveTemperatureState,
		ConditionResource);
	for (FSRResourceInstance& ProcessedResource : ProcessedResources)
	{
		ApplyTagEffects(EffectiveTemperatureState, ChargeStacksToAdd, PreProcessingEffectContext, ProcessedResource);
	}
	OutEnergyChangeCount = PreProcessingEffectContext.EnergyChangeCount;

	if (FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Process)
	{
		if (const FSRResourceInstance* EnergyResource = FindFirstResource(ProcessedResources))
		{
			return *EnergyResource;
		}
		return ProcessedResources[0];
	}

	if (FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Split)
	{
		const int32 PrimaryOutputCount = FMath::Max(1, ResolvePrimaryOutputCount(FacilityInstance));
		if (const FSRResourceInstance* EnergyResource = FindFirstResource(ProcessedResources))
		{
			FSRResourceInstance OutputResource = *EnergyResource;
			OutputResource.EnergyValue = EnergyResource->EnergyValue / static_cast<double>(PrimaryOutputCount);
			return OutputResource;
		}
		return ProcessedResources[0];
	}

	return BuildSynthesisProductResource(ProcessedResources);
}

void FSRFacilityOutputResourceBuilder::ApplyFacilityEffects(
	const FSRFacilityInstance& FacilityInstance,
	const TArray<FSRResourceInstance>& InputResources,
	FSRResourceInstance& ResourceInstance,
	bool& bHasPrimaryResource,
	TArray<FSRResourceInstance>& OutAdditionalOutputs,
	int32 InitialEnergyChangeCount)
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset))
	{
		return;
	}

	FSRFacilityEffectContext EffectContext;
	EffectContext.EnergyChangeCount = FMath::Max(0, InitialEnergyChangeCount);
	const FSRResourceInstance BaselineResource = ResourceInstance;
	for (const FSRFacilityEffectSpec& EffectSpec : FacilityDataAsset->Effects)
	{
		const ESRFacilityTemperatureState EffectiveTemperatureState = ResolveEffectiveTemperatureState(
			FacilityInstance.TemperatureState,
			EffectContext);
		const StarRovers::FacilityEffects::FSRFacilityEffectConditionContext ConditionContext =
		{
			bHasPrimaryResource ? &ResourceInstance : nullptr,
			bHasPrimaryResource ? &BaselineResource : nullptr,
			EffectiveTemperatureState
		};
		if (!StarRovers::FacilityEffects::DoEffectConditionsPass(EffectSpec, ConditionContext))
		{
			continue;
		}

		const int32 Count = FMath::Max(1, EffectSpec.Count);
		switch (EffectSpec.EffectKind)
		{
		case ESRFacilityEffectKind::AdjustEnergy:
			if (bHasPrimaryResource)
			{
				AddEnergyDelta(
					ResourceInstance,
					ResolveEnergyAdjustmentValue(EffectSpec, ResourceInstance, EffectContext),
					EffectContext);
			}
			break;
		case ESRFacilityEffectKind::AdjustProcessLimit:
			if (bHasPrimaryResource)
			{
				const int32 TargetProcessLimit = EffectSpec.ProcessLimitMode == ESRFacilityProcessLimitAdjustmentMode::SetValue
					? FMath::RoundToInt(EffectSpec.Value)
					: ResourceInstance.RemainingProcessLimit + FMath::RoundToInt(EffectSpec.Value);
				SetProcessLimitAndApplyHighActivity(
					ResourceInstance,
					TargetProcessLimit,
					EffectContext);
			}
			break;
		case ESRFacilityEffectKind::RemoveResource:
			bHasPrimaryResource = false;
			break;
		case ESRFacilityEffectKind::AttachTag:
			if (bHasPrimaryResource)
			{
				ESRResourceProcessTag TagToAttach = EffectSpec.ResourceTag;
				const bool bCanAttachTag = EffectSpec.AttachTagSource == ESRFacilityAttachTagSource::SpecificTag
					|| EffectContext.bHasLastAttachedTag;
				if (EffectSpec.AttachTagSource == ESRFacilityAttachTagSource::LastAttachedTag)
				{
					TagToAttach = EffectContext.LastAttachedTag;
				}
				if (bCanAttachTag)
				{
					AddTagStack(ResourceInstance, TagToAttach, Count);
					EffectContext.LastAttachedTag = TagToAttach;
					EffectContext.bHasLastAttachedTag = true;
				}
			}
			break;
		case ESRFacilityEffectKind::ProduceWaste:
			if (IsValid(EffectSpec.ProducedResource.Get()))
			{
				for (int32 OutputIndex = 0; OutputIndex < Count; ++OutputIndex)
				{
					FSRResourceInstance WasteResource = EffectSpec.ProducedResource->BuildDefaultInstance();
					AddTagStack(WasteResource, ESRResourceProcessTag::Waste, 1);
					OutAdditionalOutputs.Add(WasteResource);
				}
			}
			break;
		case ESRFacilityEffectKind::AdjustCellTemperature:
			break;
		case ESRFacilityEffectKind::InvertHeat:
			EffectContext.bInvertHeat = !EffectContext.bInvertHeat;
			break;
		case ESRFacilityEffectKind::InvertTagEffects:
			EffectContext.bInvertTagEffects = !EffectContext.bInvertTagEffects;
			break;
		case ESRFacilityEffectKind::DuplicateInputResource:
			for (int32 DuplicateIndex = 0; DuplicateIndex < Count; ++DuplicateIndex)
			{
				for (const FSRResourceInstance& InputResource : InputResources)
				{
					if (!InputResource.ResourceId.IsNone() && InputResource.StackCount > 0)
					{
						OutAdditionalOutputs.Add(MakeDuplicatedResourceInstance(InputResource));
					}
				}
			}
			break;
		case ESRFacilityEffectKind::OverrideProcessTemperature:
			EffectContext.bInvertHeat = false;
			EffectContext.bHasProcessTemperatureOverride = true;
			EffectContext.ProcessTemperatureOverride = EffectSpec.ProcessTemperatureState;
			break;
		case ESRFacilityEffectKind::TriggerTagEffect:
			if (bHasPrimaryResource)
			{
				TriggerTagsForEffect(
					ResourceInstance,
					EffectSpec,
					EffectiveTemperatureState,
					BaselineResource,
					EffectContext);
			}
			break;
		case ESRFacilityEffectKind::AdjustProcessTime:
			break;
		case ESRFacilityEffectKind::RemoveTag:
			if (bHasPrimaryResource)
			{
				RemoveTagsForEffect(ResourceInstance, EffectSpec, EffectContext);
			}
			break;
		case ESRFacilityEffectKind::MultiplyEnergyByConsumedProcessLimit:
			if (bHasPrimaryResource)
			{
				MultiplyEnergyValue(
					ResourceInstance,
					static_cast<double>(ResolveConsumedProcessLimit(BaselineResource, ResourceInstance)),
					EffectContext);
			}
			break;
		case ESRFacilityEffectKind::ChangeResourceType:
			if (bHasPrimaryResource && IsValid(EffectSpec.TargetResource.Get()))
			{
				ResourceInstance.ResourceDataAsset = EffectSpec.TargetResource;
				ResourceInstance.ResourceId = EffectSpec.TargetResource->ResourceId;
			}
			break;
		default:
			break;
		}
	}
}

void FSRFacilityOutputResourceBuilder::AddTagStack(FSRResourceInstance& ResourceInstance, ESRResourceProcessTag Tag, int32 Count)
{
	const int32 SafeCount = FMath::Max(1, Count);
	for (FSRResourceTagStack& TagStack : ResourceInstance.Tags)
	{
		if (TagStack.Tag == Tag)
		{
			TagStack.StackCount += SafeCount;
			if (Tag == ESRResourceProcessTag::HalfLife && TagStack.RemainingCycles <= 0)
			{
				TagStack.RemainingCycles = StarRovers::FacilityResources::HalfLifeDefaultCycles;
			}
			return;
		}
	}

	FSRResourceTagStack& NewTagStack = ResourceInstance.Tags.AddDefaulted_GetRef();
	NewTagStack.Tag = Tag;
	NewTagStack.StackCount = SafeCount;
	if (Tag == ESRResourceProcessTag::HalfLife)
	{
		NewTagStack.RemainingCycles = StarRovers::FacilityResources::HalfLifeDefaultCycles;
	}
}
