#include "SRFacilityOutputResourceBuilder.h"

#include "Automation/SRFacilityDataAsset.h"
#include "SRFacilityEffectConditionEvaluator.h"
#include "SRFacilityProcessContextResolver.h"
#include "SRFacilityResourceOperations.h"

namespace
{
	constexpr double HeatResponsiveBaseEnergyBonus = 1.0;
	constexpr double HeatResponsiveHotEnergyBonus = 1.0;
	constexpr double SupercooledEnergyBonus = 3.0;
	constexpr double VolatileEnergyPenalty = 1.0;
	constexpr double HyperReactiveEnergyBonusPerProcessLimitLoss = 1.0;
	const FName CompoundResourceId(TEXT("Compound"));

	struct FSRFacilityEffectContext
	{
		bool bInvertTagEffects = false;
		bool bHasLastAttachedTag = false;
		ESRResourceProcessTag LastAttachedTag = ESRResourceProcessTag::Responsive;
		int32 EnergyChangeCount = 0;
		double TagEffectEnergyChangeAmount = 0.0;
	};

	double ApplyTagEffectDirection(double EnergyDelta, const FSRFacilityEffectContext& EffectContext)
	{
		return EffectContext.bInvertTagEffects ? -EnergyDelta : EnergyDelta;
	}

	FSRFacilityEffectContext MakeEffectContext(
		const StarRovers::FacilityProcessing::FSRFacilityProcessContext& ProcessContext,
		int32 InitialEnergyChangeCount = 0,
		double InitialTagEffectEnergyChangeAmount = 0.0)
	{
		FSRFacilityEffectContext EffectContext;
		EffectContext.bInvertTagEffects = ProcessContext.bInvertTagEffects;
		EffectContext.EnergyChangeCount = FMath::Max(0, InitialEnergyChangeCount);
		EffectContext.TagEffectEnergyChangeAmount = FMath::Max(0.0, InitialTagEffectEnergyChangeAmount);
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

	bool TryResolveLastActiveTag(const TArray<FSRResourceTagStack>& Tags, ESRResourceProcessTag& OutTag)
	{
		for (int32 TagIndex = Tags.Num() - 1; TagIndex >= 0; --TagIndex)
		{
			const FSRResourceTagStack& TagStack = Tags[TagIndex];
			if (TagStack.StackCount > 0)
			{
				OutTag = TagStack.Tag;
				return true;
			}
		}

		return false;
	}

	void SeedLastAttachedTagFromResource(
		FSRFacilityEffectContext& EffectContext,
		const FSRResourceInstance& ResourceInstance)
	{
		ESRResourceProcessTag LastActiveTag = ESRResourceProcessTag::Responsive;
		if (TryResolveLastActiveTag(ResourceInstance.Tags, LastActiveTag))
		{
			EffectContext.LastAttachedTag = LastActiveTag;
			EffectContext.bHasLastAttachedTag = true;
		}
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

	void RecordDirectEnergyChangeIfNeeded(
		const FSRResourceInstance* ResourceInstance,
		double EnergyValueBeforeEffect,
		int32 EnergyChangeCountBeforeEffect,
		FSRFacilityEffectContext& EffectContext)
	{
		if (ResourceInstance
			&& EffectContext.EnergyChangeCount == EnergyChangeCountBeforeEffect
			&& !FMath::IsNearlyEqual(ResourceInstance->EnergyValue, EnergyValueBeforeEffect))
		{
			++EffectContext.EnergyChangeCount;
		}
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

	void SetProcessLimitAndApplyHyperReactive(
		FSRResourceInstance& ResourceInstance,
		int32 NewProcessLimit,
		FSRFacilityEffectContext& EffectContext)
	{
		const int32 PreviousProcessLimit = FMath::Max(0, ResourceInstance.RemainingProcessLimit);
		const int32 ClampedProcessLimit = FMath::Max(0, NewProcessLimit);
		ResourceInstance.RemainingProcessLimit = ClampedProcessLimit;

		const int32 LostProcessLimit = FMath::Max(0, PreviousProcessLimit - ClampedProcessLimit);
		const int32 HyperReactiveStackCount = CountTagStacks(ResourceInstance, ESRResourceProcessTag::HyperReactive);
		if (LostProcessLimit > 0 && HyperReactiveStackCount > 0)
		{
			const double EnergyDelta = static_cast<double>(LostProcessLimit * HyperReactiveStackCount)
				* HyperReactiveEnergyBonusPerProcessLimitLoss;
			AddEnergyDelta(ResourceInstance, ApplyTagEffectDirection(EnergyDelta, EffectContext), EffectContext);
		}
	}

	void ConsumeEnergyForFacilityPass(
		FSRResourceInstance& ResourceInstance,
		ESRFacilityTemperatureState TemperatureState,
		FSRFacilityEffectContext& EffectContext)
	{
		SetProcessLimitAndApplyHyperReactive(ResourceInstance, ResourceInstance.RemainingProcessLimit - 1, EffectContext);
		++ResourceInstance.ProcessCount;
		if (TemperatureState == ESRFacilityTemperatureState::Hot)
		{
			SetProcessLimitAndApplyHyperReactive(ResourceInstance, ResourceInstance.RemainingProcessLimit - 1, EffectContext);
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

	int32 FindFirstResourceIndex(const TArray<FSRResourceInstance>& ResourceInstances)
	{
		for (int32 ResourceIndex = 0; ResourceIndex < ResourceInstances.Num(); ++ResourceIndex)
		{
			if (!ResourceInstances[ResourceIndex].ResourceId.IsNone())
			{
				return ResourceIndex;
			}
		}
		return INDEX_NONE;
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

	void ResetHalfLifeTagCycles(FSRResourceInstance& ResourceInstance)
	{
		for (FSRResourceTagStack& TagStack : ResourceInstance.Tags)
		{
			if (TagStack.Tag == ESRResourceProcessTag::HalfLife && TagStack.StackCount > 0)
			{
				TagStack.RemainingCycles = StarRovers::FacilityResources::HalfLifeDefaultCycles;
			}
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
		case ESRFacilityEnergyAdjustmentValueSource::TagEffectEnergyChangeAmount:
			return EffectContext.TagEffectEnergyChangeAmount;
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
		case ESRResourceProcessTag::HyperReactive:
			AddEnergyDelta(
				ResourceInstance,
				ApplyTagEffectDirection(
					static_cast<double>(ResolveConsumedProcessLimit(BaselineResource, ResourceInstance) * SafeStackCount)
						* HyperReactiveEnergyBonusPerProcessLimitLoss,
					EffectContext),
				EffectContext);
			break;
		case ESRResourceProcessTag::HalfLife:
			MultiplyEnergyValue(ResourceInstance, EffectContext.bInvertTagEffects ? 2.0 : 0.5, EffectContext);
			ResetHalfLifeTagCycles(ResourceInstance);
			break;
		case ESRResourceProcessTag::Charge:
			AddEnergyDelta(
				ResourceInstance,
				ApplyTagEffectDirection(StarRovers::FacilityResources::ChargeEnergyBonus, EffectContext),
				EffectContext);
			break;
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
	const StarRovers::FacilityProcessing::FSRFacilityProcessContext ProcessContext =
		StarRovers::FacilityProcessing::ResolveProcessContext(
		FacilityDataAsset,
		TemperatureState,
		ConditionResource);
	const ESRFacilityTemperatureState EffectiveTemperatureState = ProcessContext.EffectiveTemperatureState;
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

	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Synthesize)
	{
		int32 InitialEnergyChangeCount = 0;
		TArray<FSRResourceInstance> ProcessedResources;
		TArray<FSRResourceInstance> ResourcesBeforeTagEffects;
		TArray<double> TagEffectEnergyChangeAmounts;
		BuildProcessedResourcesBeforeFacilityEffects(
			FacilityInstance,
			InputResources,
			ProcessedResources,
			InitialEnergyChangeCount,
			TagEffectEnergyChangeAmounts,
			&ResourcesBeforeTagEffects);

		TArray<FSRResourceInstance> SynthesisResources;
		SynthesisResources.Reserve(ProcessedResources.Num());
		TArray<FSRResourceInstance> AdditionalOutputs;
		AdditionalOutputs.Reserve(CountAdditionalOutputResourcesForInputCount(FacilityDataAsset, InputResources.Num()));
		for (int32 ResourceIndex = 0; ResourceIndex < ProcessedResources.Num(); ++ResourceIndex)
		{
			const FSRResourceInstance& ProcessedResource = ProcessedResources[ResourceIndex];
			if (ProcessedResource.ResourceId.IsNone())
			{
				continue;
			}

			FSRResourceInstance SynthesisResource = ProcessedResource;
			bool bHasSynthesisResource = true;
			TArray<FSRResourceInstance> IgnoredAdditionalOutputs;
			const FSRResourceInstance* ConditionBaselineResource = ResourcesBeforeTagEffects.IsValidIndex(ResourceIndex)
				? &ResourcesBeforeTagEffects[ResourceIndex]
				: &ProcessedResource;
			ApplyFacilityEffects(
				FacilityInstance,
				InputResources,
				SynthesisResource,
				bHasSynthesisResource,
				IgnoredAdditionalOutputs,
				InitialEnergyChangeCount,
				TagEffectEnergyChangeAmounts.IsValidIndex(ResourceIndex) ? TagEffectEnergyChangeAmounts[ResourceIndex] : 0.0,
				true,
				false,
				ConditionBaselineResource);
			if (bHasSynthesisResource && !SynthesisResource.ResourceId.IsNone())
			{
				SynthesisResources.Add(SynthesisResource);
			}
		}

		FSRResourceInstance BaselineOutputResource = ResourcesBeforeTagEffects.IsEmpty()
			? FSRResourceInstance()
			: BuildSynthesisProductResource(ResourcesBeforeTagEffects);
		FSRResourceInstance OutputResource = SynthesisResources.IsEmpty()
			? FSRResourceInstance()
			: BuildSynthesisProductResource(SynthesisResources);
		bool bHasPrimaryResource = !OutputResource.ResourceId.IsNone();
		ApplyFacilityEffects(
			FacilityInstance,
			InputResources,
			OutputResource,
			bHasPrimaryResource,
			AdditionalOutputs,
			InitialEnergyChangeCount,
			0.0,
			false,
			true,
			&BaselineOutputResource);

		if (OutBaselinePrimaryResource)
		{
			*OutBaselinePrimaryResource = BaselineOutputResource;
		}
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
		return;
	}

	int32 InitialEnergyChangeCount = 0;
	double InitialTagEffectEnergyChangeAmount = 0.0;
	FSRResourceInstance ConditionBaselineResource;
	FSRResourceInstance OutputResource = BuildBaseOutputResource(
		FacilityInstance,
		InputResources,
		InitialEnergyChangeCount,
		InitialTagEffectEnergyChangeAmount,
		&ConditionBaselineResource);
	BuildOutputResourcesFromPrimaryResource(
		FacilityInstance,
		InputResources,
		OutputResource,
		OutOutputResources,
		OutPrimaryOutputCount,
		OutBaselinePrimaryResource,
		InitialEnergyChangeCount,
		InitialTagEffectEnergyChangeAmount,
		&ConditionBaselineResource);
}

void FSRFacilityOutputResourceBuilder::BuildOutputResourcesFromPrimaryResource(
	const FSRFacilityInstance& FacilityInstance,
	const TArray<FSRResourceInstance>& InputResources,
	const FSRResourceInstance& PrimaryResource,
	TArray<FSRResourceInstance>& OutOutputResources,
	int32* OutPrimaryOutputCount,
	FSRResourceInstance* OutBaselinePrimaryResource,
	int32 InitialEnergyChangeCount,
	double InitialTagEffectEnergyChangeAmount,
	const FSRResourceInstance* ConditionBaselineResource)
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
		*OutBaselinePrimaryResource = ConditionBaselineResource ? *ConditionBaselineResource : PrimaryResource;
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
		InitialEnergyChangeCount,
		InitialTagEffectEnergyChangeAmount,
		true,
		true,
		ConditionBaselineResource);

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

void FSRFacilityOutputResourceBuilder::BuildProcessedResourcesBeforeFacilityEffects(
	const FSRFacilityInstance& FacilityInstance,
	const TArray<FSRResourceInstance>& ConsumedResources,
	TArray<FSRResourceInstance>& OutProcessedResources,
	int32& OutEnergyChangeCount,
	TArray<double>& OutTagEffectEnergyChangeAmounts,
	TArray<FSRResourceInstance>* OutResourcesBeforeTagEffects)
{
	OutEnergyChangeCount = 0;
	OutProcessedResources = ConsumedResources;
	OutTagEffectEnergyChangeAmounts.Reset();
	OutTagEffectEnergyChangeAmounts.SetNumZeroed(OutProcessedResources.Num());
	if (OutResourcesBeforeTagEffects)
	{
		OutResourcesBeforeTagEffects->Reset();
	}
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset) || OutProcessedResources.IsEmpty())
	{
		return;
	}

	const FSRResourceInstance* ConditionResource = FindFirstResource(ConsumedResources);
	const StarRovers::FacilityProcessing::FSRFacilityProcessContext ProcessContext =
		StarRovers::FacilityProcessing::ResolveProcessContext(
		FacilityDataAsset,
		FacilityInstance.TemperatureState,
		ConditionResource);
	FSRFacilityEffectContext PreProcessingEffectContext = MakeEffectContext(ProcessContext);
	const ESRFacilityTemperatureState EffectiveTemperatureState = ProcessContext.EffectiveTemperatureState;
	ConsumeEnergyInputsForFacilityPass(OutProcessedResources, EffectiveTemperatureState, PreProcessingEffectContext);
	const int32 ChargeStacksToAdd = ResolveChargeStacksToAddForProcessingPass(
		FacilityInstance,
		EffectiveTemperatureState,
		ConditionResource);
	if (OutResourcesBeforeTagEffects)
	{
		*OutResourcesBeforeTagEffects = OutProcessedResources;
	}
	for (int32 ResourceIndex = 0; ResourceIndex < OutProcessedResources.Num(); ++ResourceIndex)
	{
		FSRResourceInstance& ProcessedResource = OutProcessedResources[ResourceIndex];
		const double EnergyBeforeTagEffects = ProcessedResource.EnergyValue;
		ApplyTagEffects(EffectiveTemperatureState, ChargeStacksToAdd, PreProcessingEffectContext, ProcessedResource);
		OutTagEffectEnergyChangeAmounts[ResourceIndex] = FMath::Abs(ProcessedResource.EnergyValue - EnergyBeforeTagEffects);
	}
	OutEnergyChangeCount = PreProcessingEffectContext.EnergyChangeCount;
}

FSRResourceInstance FSRFacilityOutputResourceBuilder::BuildBaseOutputResource(
	const FSRFacilityInstance& FacilityInstance,
	const TArray<FSRResourceInstance>& ConsumedResources,
	int32& OutEnergyChangeCount,
	double& OutTagEffectEnergyChangeAmount,
	FSRResourceInstance* OutConditionBaselineResource)
{
	OutEnergyChangeCount = 0;
	OutTagEffectEnergyChangeAmount = 0.0;
	if (OutConditionBaselineResource)
	{
		*OutConditionBaselineResource = FSRResourceInstance();
	}
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset) || ConsumedResources.IsEmpty())
	{
		return !ConsumedResources.IsEmpty() ? ConsumedResources[0] : FSRResourceInstance();
	}

	TArray<FSRResourceInstance> ProcessedResources;
	TArray<FSRResourceInstance> ResourcesBeforeTagEffects;
	TArray<double> TagEffectEnergyChangeAmounts;
	BuildProcessedResourcesBeforeFacilityEffects(
		FacilityInstance,
		ConsumedResources,
		ProcessedResources,
		OutEnergyChangeCount,
		TagEffectEnergyChangeAmounts,
		&ResourcesBeforeTagEffects);

	if (FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Process)
	{
		const int32 ResourceIndex = FindFirstResourceIndex(ProcessedResources);
		if (ProcessedResources.IsValidIndex(ResourceIndex))
		{
			if (OutConditionBaselineResource)
			{
				*OutConditionBaselineResource = ResourcesBeforeTagEffects.IsValidIndex(ResourceIndex)
					? ResourcesBeforeTagEffects[ResourceIndex]
					: ProcessedResources[ResourceIndex];
			}
			OutTagEffectEnergyChangeAmount = TagEffectEnergyChangeAmounts.IsValidIndex(ResourceIndex)
				? TagEffectEnergyChangeAmounts[ResourceIndex]
				: 0.0;
			return ProcessedResources[ResourceIndex];
		}
		if (OutConditionBaselineResource)
		{
			*OutConditionBaselineResource = ResourcesBeforeTagEffects.IsValidIndex(0)
				? ResourcesBeforeTagEffects[0]
				: ProcessedResources[0];
		}
		OutTagEffectEnergyChangeAmount = TagEffectEnergyChangeAmounts.IsValidIndex(0)
			? TagEffectEnergyChangeAmounts[0]
			: 0.0;
		return ProcessedResources[0];
	}

	if (FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Split)
	{
		const int32 PrimaryOutputCount = FMath::Max(1, ResolvePrimaryOutputCount(FacilityInstance));
		const int32 ResourceIndex = FindFirstResourceIndex(ProcessedResources);
		if (ProcessedResources.IsValidIndex(ResourceIndex))
		{
			OutTagEffectEnergyChangeAmount = TagEffectEnergyChangeAmounts.IsValidIndex(ResourceIndex)
				? TagEffectEnergyChangeAmounts[ResourceIndex] / static_cast<double>(PrimaryOutputCount)
				: 0.0;
			FSRResourceInstance OutputResource = ProcessedResources[ResourceIndex];
			OutputResource.EnergyValue = ProcessedResources[ResourceIndex].EnergyValue / static_cast<double>(PrimaryOutputCount);
			if (OutConditionBaselineResource)
			{
				*OutConditionBaselineResource = ResourcesBeforeTagEffects.IsValidIndex(ResourceIndex)
					? ResourcesBeforeTagEffects[ResourceIndex]
					: ProcessedResources[ResourceIndex];
				OutConditionBaselineResource->EnergyValue =
					OutConditionBaselineResource->EnergyValue / static_cast<double>(PrimaryOutputCount);
			}
			return OutputResource;
		}
		if (OutConditionBaselineResource)
		{
			*OutConditionBaselineResource = ResourcesBeforeTagEffects.IsValidIndex(0)
				? ResourcesBeforeTagEffects[0]
				: ProcessedResources[0];
			OutConditionBaselineResource->EnergyValue =
				OutConditionBaselineResource->EnergyValue / static_cast<double>(PrimaryOutputCount);
		}
		OutTagEffectEnergyChangeAmount = TagEffectEnergyChangeAmounts.IsValidIndex(0)
			? TagEffectEnergyChangeAmounts[0] / static_cast<double>(PrimaryOutputCount)
			: 0.0;
		return ProcessedResources[0];
	}

	for (const double TagEffectEnergyChangeAmount : TagEffectEnergyChangeAmounts)
	{
		OutTagEffectEnergyChangeAmount += FMath::Max(0.0, TagEffectEnergyChangeAmount);
	}
	if (OutConditionBaselineResource)
	{
		*OutConditionBaselineResource = ResourcesBeforeTagEffects.IsEmpty()
			? BuildSynthesisProductResource(ProcessedResources)
			: BuildSynthesisProductResource(ResourcesBeforeTagEffects);
	}
	return BuildSynthesisProductResource(ProcessedResources);
}

void FSRFacilityOutputResourceBuilder::ApplyFacilityEffects(
	const FSRFacilityInstance& FacilityInstance,
	const TArray<FSRResourceInstance>& InputResources,
	FSRResourceInstance& ResourceInstance,
	bool& bHasPrimaryResource,
	TArray<FSRResourceInstance>& OutAdditionalOutputs,
	int32 InitialEnergyChangeCount,
	double InitialTagEffectEnergyChangeAmount,
	bool bApplyResourceEffects,
	bool bApplyAdditionalOutputEffects,
	const FSRResourceInstance* ConditionBaselineResource)
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset))
	{
		return;
	}

	const FSRResourceInstance FacilityEffectBaselineResource = ResourceInstance;
	const FSRResourceInstance& ConditionBaseline = ConditionBaselineResource
		? *ConditionBaselineResource
		: FacilityEffectBaselineResource;
	const StarRovers::FacilityProcessing::FSRFacilityProcessContext ProcessContext =
		StarRovers::FacilityProcessing::ResolveProcessContext(
			FacilityDataAsset,
			FacilityInstance.TemperatureState,
			bHasPrimaryResource ? &ResourceInstance : nullptr,
			bHasPrimaryResource ? &ConditionBaseline : nullptr);
	FSRFacilityEffectContext EffectContext = MakeEffectContext(
		ProcessContext,
		InitialEnergyChangeCount,
		InitialTagEffectEnergyChangeAmount);
	if (bHasPrimaryResource)
	{
		SeedLastAttachedTagFromResource(EffectContext, ResourceInstance);
	}
	const ESRFacilityTemperatureState EffectiveTemperatureState = ProcessContext.EffectiveTemperatureState;
	for (const FSRFacilityEffectSpec& EffectSpec : FacilityDataAsset->Effects)
	{
		const StarRovers::FacilityEffects::FSRFacilityEffectConditionContext ConditionContext =
		{
			bHasPrimaryResource ? &ResourceInstance : nullptr,
			bHasPrimaryResource ? &ConditionBaseline : nullptr,
			EffectiveTemperatureState
		};
		if (!StarRovers::FacilityEffects::DoEffectConditionsPass(EffectSpec, ConditionContext))
		{
			continue;
		}

		const FSRResourceInstance* ResourceBeforeEffect = bHasPrimaryResource ? &ResourceInstance : nullptr;
		const double EnergyValueBeforeEffect = ResourceBeforeEffect ? ResourceBeforeEffect->EnergyValue : 0.0;
		const int32 EnergyChangeCountBeforeEffect = EffectContext.EnergyChangeCount;
		const int32 Count = FMath::Max(1, EffectSpec.Count);
		switch (EffectSpec.EffectKind)
		{
		case ESRFacilityEffectKind::AdjustEnergy:
			if (!bApplyResourceEffects)
			{
				break;
			}
			if (bHasPrimaryResource)
			{
				const double EnergyAdjustmentValue = ResolveEnergyAdjustmentValue(
					EffectSpec,
					ResourceInstance,
					EffectContext);
				if (EffectSpec.EnergyAdjustmentMode == ESRFacilityEnergyAdjustmentMode::Multiply)
				{
					MultiplyEnergyValue(ResourceInstance, EnergyAdjustmentValue, EffectContext);
				}
				else if (EffectSpec.EnergyAdjustmentMode == ESRFacilityEnergyAdjustmentMode::Subtract)
				{
					AddEnergyDelta(ResourceInstance, -EnergyAdjustmentValue, EffectContext);
				}
				else
				{
					AddEnergyDelta(ResourceInstance, EnergyAdjustmentValue, EffectContext);
				}
			}
			break;
		case ESRFacilityEffectKind::AdjustProcessLimit:
			if (!bApplyResourceEffects)
			{
				break;
			}
			if (bHasPrimaryResource)
			{
				const int32 TargetProcessLimit = EffectSpec.ProcessLimitMode == ESRFacilityProcessLimitAdjustmentMode::SetValue
					? FMath::RoundToInt(EffectSpec.Value)
					: ResourceInstance.RemainingProcessLimit + FMath::RoundToInt(EffectSpec.Value);
				SetProcessLimitAndApplyHyperReactive(
					ResourceInstance,
					TargetProcessLimit,
					EffectContext);
			}
			break;
		case ESRFacilityEffectKind::RemoveResource:
			if (!bApplyResourceEffects)
			{
				break;
			}
			bHasPrimaryResource = false;
			break;
		case ESRFacilityEffectKind::AttachTag:
			if (!bApplyResourceEffects)
			{
				break;
			}
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
			if (!bApplyAdditionalOutputEffects)
			{
				break;
			}
			if (IsValid(EffectSpec.ProducedResource.Get()))
			{
				for (int32 OutputIndex = 0; OutputIndex < Count; ++OutputIndex)
				{
					FSRResourceInstance WasteResource = EffectSpec.ProducedResource->BuildDefaultInstance();
					OutAdditionalOutputs.Add(WasteResource);
				}
			}
			break;
		case ESRFacilityEffectKind::AdjustCellTemperature:
			break;
		case ESRFacilityEffectKind::InvertHeat:
		case ESRFacilityEffectKind::InvertTagEffects:
			break;
		case ESRFacilityEffectKind::DuplicateInputResource:
			if (!bApplyAdditionalOutputEffects)
			{
				break;
			}
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
			break;
		case ESRFacilityEffectKind::TriggerTagEffect:
			if (!bApplyResourceEffects)
			{
				break;
			}
			if (bHasPrimaryResource)
			{
				TriggerTagsForEffect(
					ResourceInstance,
					EffectSpec,
					EffectiveTemperatureState,
					FacilityEffectBaselineResource,
					EffectContext);
			}
			break;
		case ESRFacilityEffectKind::AdjustProcessTime:
			break;
		case ESRFacilityEffectKind::RemoveTag:
			if (!bApplyResourceEffects)
			{
				break;
			}
			if (bHasPrimaryResource)
			{
				RemoveTagsForEffect(ResourceInstance, EffectSpec, EffectContext);
			}
			break;
		case ESRFacilityEffectKind::MultiplyEnergyByConsumedProcessLimit:
			if (!bApplyResourceEffects)
			{
				break;
			}
			if (bHasPrimaryResource)
			{
				MultiplyEnergyValue(
					ResourceInstance,
					static_cast<double>(ResolveConsumedProcessLimit(FacilityEffectBaselineResource, ResourceInstance)),
					EffectContext);
			}
			break;
		case ESRFacilityEffectKind::ChangeResourceType:
			if (!bApplyResourceEffects)
			{
				break;
			}
			if (bHasPrimaryResource && IsValid(EffectSpec.TargetResource.Get()))
			{
				ResourceInstance.ResourceDataAsset = EffectSpec.TargetResource;
				ResourceInstance.ResourceId = EffectSpec.TargetResource->ResourceId;
			}
			break;
		default:
			break;
		}
		RecordDirectEnergyChangeIfNeeded(
			bHasPrimaryResource ? &ResourceInstance : nullptr,
			EnergyValueBeforeEffect,
			EnergyChangeCountBeforeEffect,
			EffectContext);
	}
}

void FSRFacilityOutputResourceBuilder::AddTagStack(FSRResourceInstance& ResourceInstance, ESRResourceProcessTag Tag, int32 Count)
{
	const int32 SafeCount = FMath::Max(1, Count);
	for (int32 TagIndex = 0; TagIndex < ResourceInstance.Tags.Num(); ++TagIndex)
	{
		FSRResourceTagStack& TagStack = ResourceInstance.Tags[TagIndex];
		if (TagStack.Tag == Tag)
		{
			FSRResourceTagStack UpdatedTagStack = TagStack;
			UpdatedTagStack.StackCount += SafeCount;
			if (Tag == ESRResourceProcessTag::HalfLife && UpdatedTagStack.RemainingCycles <= 0)
			{
				UpdatedTagStack.RemainingCycles = StarRovers::FacilityResources::HalfLifeDefaultCycles;
			}
			ResourceInstance.Tags.RemoveAt(TagIndex);
			ResourceInstance.Tags.Add(UpdatedTagStack);
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
