#include "SRFacilityOutputResourceBuilder.h"

#include "Automation/SRFacilityDataAsset.h"
#include "Automation/SRFacilityResourceV2Processor.h"
#include "Automation/SROperationalEconomyProcessor.h"
#include "Automation/SRStellarFuelFabricator.h"
#include "SRFacilityEffectConditionEvaluator.h"
#include "SRFacilityProcessContextResolver.h"
#include "SRFacilityResourceOperations.h"

namespace
{
	constexpr double HeatResponsiveBaseEnergy = 1.0;
	constexpr double HeatResponsiveHotEnergy = 1.0;
	constexpr double SupercooledEnergy = 3.0;
	constexpr double VolatileEnergy = -1.0;
	constexpr double HyperReactiveEnergyPerProcessLimit = 1.0;
	const FName CompoundResourceId(TEXT("Compound"));

	struct FSRFacilityFormulaState
	{
		double Base = 0.0;
		double Energy = 0.0;
		double Catalyst = 1.0;
		int32 EnergyChangeCount = 0;
		int32 ProcessLimitLost = 0;
		bool bHasLastAttachedTag = false;
		ESRResourceProcessTag LastAttachedTag = ESRResourceProcessTag::Responsive;
		TArray<FString> EnergyDetails;
		TArray<FString> CatalystDetails;
	};

	struct FSRPreparedFacilityResource
	{
		FSRResourceInstance Resource;
		TArray<int32> ProcessLimitLossByTagIndex;
		int32 TotalProcessLimitLost = 0;
		int32 CompletedProcessCountIncrement = 0;
	};

	FString FormatValue(double Value)
	{
		return FString::Printf(TEXT("%.1f"), Value);
	}

	double ResolveFormulaResult(const FSRFacilityFormulaState& FormulaState)
	{
		return FormulaState.Base + FormulaState.Energy * FormulaState.Catalyst;
	}

	void AddEnergy(
		FSRFacilityFormulaState& FormulaState,
		double Delta,
		const FString& Detail)
	{
		if (FMath::IsNearlyZero(Delta))
		{
			return;
		}

		const double PreviousEnergy = FormulaState.Energy;
		FormulaState.Energy += Delta;
		++FormulaState.EnergyChangeCount;
		FormulaState.EnergyDetails.Add(FString::Printf(
			TEXT("%s: %s %s %s = %s"),
			*Detail,
			*FormatValue(PreviousEnergy),
			Delta < 0.0 ? TEXT("-") : TEXT("+"),
			*FormatValue(FMath::Abs(Delta)),
			*FormatValue(FormulaState.Energy)));
	}

	void SetCatalyst(
		FSRFacilityFormulaState& FormulaState,
		double NewCatalyst,
		const FString& Detail)
	{
		if (FMath::IsNearlyEqual(FormulaState.Catalyst, NewCatalyst))
		{
			return;
		}

		const double PreviousCatalyst = FormulaState.Catalyst;
		FormulaState.Catalyst = NewCatalyst;
		FormulaState.CatalystDetails.Add(FString::Printf(
			TEXT("%s: %s -> %s"),
			*Detail,
			*FormatValue(PreviousCatalyst),
			*FormatValue(FormulaState.Catalyst)));
	}

	FString BuildFormulaText(const FSRFacilityFormulaState& FormulaState)
	{
		FString FormulaText = FString::Printf(
			TEXT("Base (A): %s\nEnergy (B): %s"),
			*FormatValue(FormulaState.Base),
			*FormatValue(FormulaState.Energy));
		for (const FString& Detail : FormulaState.EnergyDetails)
		{
			FormulaText += TEXT("\n  ");
			FormulaText += Detail;
		}

		FormulaText += FString::Printf(TEXT("\nCatalyst (C): %s"), *FormatValue(FormulaState.Catalyst));
		for (const FString& Detail : FormulaState.CatalystDetails)
		{
			FormulaText += TEXT("\n  ");
			FormulaText += Detail;
		}

		FormulaText += FString::Printf(
			TEXT("\nFinal: %s + %s * %s = %s"),
			*FormatValue(FormulaState.Base),
			*FormatValue(FormulaState.Energy),
			*FormatValue(FormulaState.Catalyst),
			*FormatValue(ResolveFormulaResult(FormulaState)));
		return FormulaText;
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

	FSRResourceTagStack* FindTagStack(
		TArray<FSRResourceTagStack>& Tags,
		ESRResourceProcessTag Tag)
	{
		for (FSRResourceTagStack& TagStack : Tags)
		{
			if (TagStack.Tag == Tag && TagStack.StackCount > 0)
			{
				return &TagStack;
			}
		}
		return nullptr;
	}

	int32 CountTagStacks(const FSRResourceInstance& ResourceInstance, ESRResourceProcessTag Tag)
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

	int32 CountAllTagStacks(const FSRResourceInstance& ResourceInstance)
	{
		int32 StackCount = 0;
		for (const FSRResourceTagStack& TagStack : ResourceInstance.Tags)
		{
			StackCount += FMath::Max(0, TagStack.StackCount);
		}
		return StackCount;
	}

	int32 CountTagKinds(const FSRResourceInstance& ResourceInstance)
	{
		constexpr ESRResourceProcessTag TagKinds[] =
		{
			ESRResourceProcessTag::Responsive,
			ESRResourceProcessTag::HalfLife,
			ESRResourceProcessTag::Volatile,
			ESRResourceProcessTag::Supercooled,
			ESRResourceProcessTag::HyperReactive,
			ESRResourceProcessTag::Charge,
		};

		int32 KindCount = 0;
		for (const ESRResourceProcessTag Tag : TagKinds)
		{
			KindCount += CountTagStacks(ResourceInstance, Tag) > 0 ? 1 : 0;
		}
		return KindCount;
	}

	const TCHAR* GetTagLabel(ESRResourceProcessTag Tag)
	{
		switch (Tag)
		{
		case ESRResourceProcessTag::Responsive:
			return TEXT("He");
		case ESRResourceProcessTag::HalfLife:
			return TEXT("H");
		case ESRResourceProcessTag::Volatile:
			return TEXT("V");
		case ESRResourceProcessTag::Supercooled:
			return TEXT("Su");
		case ESRResourceProcessTag::HyperReactive:
			return TEXT("Hy");
		case ESRResourceProcessTag::Charge:
			return TEXT("C");
		default:
			return TEXT("Tag");
		}
	}

	FString BuildTagDetail(ESRResourceProcessTag Tag, int32 StackCount, const TCHAR* Detail)
	{
		return FString::Printf(
			TEXT("Tag %s x%d %s"),
			GetTagLabel(Tag),
			FMath::Max(1, StackCount),
			Detail);
	}

	void NormalizeTagStack(FSRResourceTagStack& TagStack)
	{
		if (TagStack.Tag == ESRResourceProcessTag::HalfLife
			&& TagStack.StackCount > 0
			&& TagStack.RemainingCycles <= 0)
		{
			TagStack.RemainingCycles = StarRovers::FacilityResources::HalfLifeDefaultCycles;
		}
	}

	void AddTagStack(FSRResourceInstance& ResourceInstance, ESRResourceProcessTag Tag, int32 Count)
	{
		FSRResourceTagStack& NewTagStack = ResourceInstance.Tags.AddDefaulted_GetRef();
		NewTagStack.Tag = Tag;
		NewTagStack.StackCount = FMath::Max(1, Count);
		NormalizeTagStack(NewTagStack);
	}

	bool TryResolveLastActiveTag(
		const TArray<FSRResourceTagStack>& Tags,
		ESRResourceProcessTag& OutTag)
	{
		for (int32 TagIndex = Tags.Num() - 1; TagIndex >= 0; --TagIndex)
		{
			if (Tags[TagIndex].StackCount > 0)
			{
				OutTag = Tags[TagIndex].Tag;
				return true;
			}
		}
		return false;
	}

	void SeedLastAttachedTag(
		FSRFacilityFormulaState& FormulaState,
		const FSRResourceInstance& ResourceInstance)
	{
		FormulaState.bHasLastAttachedTag = TryResolveLastActiveTag(
			ResourceInstance.Tags,
			FormulaState.LastAttachedTag);
	}

	int32 ConsumeBaseProcessLimit(FSRResourceInstance& ResourceInstance)
	{
		const int32 PreviousLimit = FMath::Max(0, ResourceInstance.RemainingProcessLimit);
		ResourceInstance.RemainingProcessLimit = FMath::Max(0, PreviousLimit - 1);
		return PreviousLimit - ResourceInstance.RemainingProcessLimit;
	}

	int32 ApplyTemperatureToProcessLimit(
		FSRResourceInstance& ResourceInstance,
		ESRFacilityTemperatureState TemperatureState)
	{
		if (TemperatureState != ESRFacilityTemperatureState::Hot)
		{
			return 0;
		}

		const int32 PreviousLimit = FMath::Max(0, ResourceInstance.RemainingProcessLimit);
		ResourceInstance.RemainingProcessLimit = FMath::Max(0, PreviousLimit - 1);
		return PreviousLimit - ResourceInstance.RemainingProcessLimit;
	}

	int32 ConsumeProcessLimit(
		FSRResourceInstance& ResourceInstance,
		ESRFacilityTemperatureState TemperatureState)
	{
		const int32 BaseLoss = ConsumeBaseProcessLimit(ResourceInstance);
		const int32 TemperatureLoss = ApplyTemperatureToProcessLimit(ResourceInstance, TemperatureState);
		return BaseLoss + TemperatureLoss;
	}

	int32 CountCharge(const FSRResourceInstance& ResourceInstance)
	{
		int32 ChargeCount = 0;
		for (const FSRResourceTagStack& TagStack : ResourceInstance.Tags)
		{
			if (TagStack.Tag == ESRResourceProcessTag::Charge && TagStack.StackCount > 0)
			{
				ChargeCount += FMath::Max(0, TagStack.RemainingCycles);
			}
		}
		return ChargeCount;
	}

	bool ConsumeCharge(FSRResourceInstance& ResourceInstance, int32 RequiredCharge)
	{
		int32 RemainingCharge = FMath::Max(0, RequiredCharge);
		if (RemainingCharge <= 0 || CountCharge(ResourceInstance) < RemainingCharge)
		{
			return false;
		}

		for (FSRResourceTagStack& TagStack : ResourceInstance.Tags)
		{
			if (TagStack.Tag != ESRResourceProcessTag::Charge || TagStack.StackCount <= 0)
			{
				continue;
			}

			const int32 ConsumedCharge = FMath::Min(RemainingCharge, FMath::Max(0, TagStack.RemainingCycles));
			TagStack.RemainingCycles -= ConsumedCharge;
			RemainingCharge -= ConsumedCharge;
			if (RemainingCharge <= 0)
			{
				return true;
			}
		}
		return false;
	}

	void AddCharge(FSRResourceInstance& ResourceInstance, int32 ChargeToAdd)
	{
		if (ChargeToAdd <= 0)
		{
			return;
		}

		if (FSRResourceTagStack* ChargeTag = FindTagStack(ResourceInstance.Tags, ESRResourceProcessTag::Charge))
		{
			ChargeTag->RemainingCycles = FMath::Max(0, ChargeTag->RemainingCycles) + ChargeToAdd;
		}
	}

	void ResetHalfLifeCycles(FSRResourceInstance& ResourceInstance)
	{
		for (FSRResourceTagStack& TagStack : ResourceInstance.Tags)
		{
			if (TagStack.Tag == ESRResourceProcessTag::HalfLife && TagStack.StackCount > 0)
			{
				TagStack.RemainingCycles = StarRovers::FacilityResources::HalfLifeDefaultCycles;
			}
		}
	}

	void ApplyAutomaticTagEffects(
		FSRResourceInstance& ResourceInstance,
		const TArray<int32>& ProcessLimitLossByTagIndex,
		ESRFacilityTemperatureState TemperatureState,
		int32 ChargeToAdd,
		FSRFacilityFormulaState& FormulaState)
	{
		bool bHandledCharge = false;
		for (int32 TagIndex = 0; TagIndex < ResourceInstance.Tags.Num(); ++TagIndex)
		{
			FSRResourceTagStack& TagStack = ResourceInstance.Tags[TagIndex];
			if (TagStack.StackCount <= 0)
			{
				continue;
			}

			NormalizeTagStack(TagStack);
			const int32 StackCount = FMath::Max(1, TagStack.StackCount);
			switch (TagStack.Tag)
			{
			case ESRResourceProcessTag::Responsive:
				AddEnergy(
					FormulaState,
					static_cast<double>(StackCount) * HeatResponsiveBaseEnergy,
					BuildTagDetail(TagStack.Tag, StackCount, TEXT("base")));
				if (TemperatureState == ESRFacilityTemperatureState::Hot)
				{
					AddEnergy(
						FormulaState,
						static_cast<double>(StackCount) * HeatResponsiveHotEnergy,
						BuildTagDetail(TagStack.Tag, StackCount, TEXT("hot")));
				}
				break;

			case ESRResourceProcessTag::Supercooled:
				if (TemperatureState == ESRFacilityTemperatureState::Cold)
				{
					AddEnergy(
						FormulaState,
						static_cast<double>(StackCount) * SupercooledEnergy,
						BuildTagDetail(TagStack.Tag, StackCount, TEXT("cold")));
				}
				break;

			case ESRResourceProcessTag::Volatile:
				AddEnergy(
					FormulaState,
					static_cast<double>(StackCount) * VolatileEnergy,
					BuildTagDetail(TagStack.Tag, StackCount, TEXT("penalty")));
				break;

			case ESRResourceProcessTag::HalfLife:
				--TagStack.RemainingCycles;
				if (TagStack.RemainingCycles <= 0)
				{
					AddEnergy(
						FormulaState,
						FormulaState.Base * -0.5,
						BuildTagDetail(TagStack.Tag, StackCount, TEXT("half base")));
					TagStack.RemainingCycles = StarRovers::FacilityResources::HalfLifeDefaultCycles;
				}
				break;

			case ESRResourceProcessTag::HyperReactive:
			{
				const int32 ProcessLimitLost = ProcessLimitLossByTagIndex.IsValidIndex(TagIndex)
					? FMath::Max(0, ProcessLimitLossByTagIndex[TagIndex])
					: FMath::Max(0, FormulaState.ProcessLimitLost);
				AddEnergy(
					FormulaState,
					static_cast<double>(ProcessLimitLost * StackCount) * HyperReactiveEnergyPerProcessLimit,
					BuildTagDetail(TagStack.Tag, StackCount, TEXT("process limit")));
				break;
			}

			case ESRResourceProcessTag::Charge:
				if (!bHandledCharge)
				{
					if (ConsumeCharge(ResourceInstance, StarRovers::FacilityResources::ChargeRequiredStacks))
					{
						AddEnergy(
							FormulaState,
							StarRovers::FacilityResources::ChargeEnergyBonus,
							BuildTagDetail(TagStack.Tag, StackCount, TEXT("discharge")));
					}
					AddCharge(ResourceInstance, ChargeToAdd);
					bHandledCharge = true;
				}
				break;

			default:
				break;
			}
		}
	}

	bool ResolveSpecificEffectTag(
		const FSRFacilityEffectSpec& EffectSpec,
		const FSRFacilityFormulaState& FormulaState,
		ESRResourceProcessTag& OutTag)
	{
		if (EffectSpec.TagTarget == ESRFacilityEffectTagTarget::SpecificTag)
		{
			OutTag = EffectSpec.ResourceTag;
			return true;
		}
		if (EffectSpec.TagTarget == ESRFacilityEffectTagTarget::LastAttachedTag
			&& FormulaState.bHasLastAttachedTag)
		{
			OutTag = FormulaState.LastAttachedTag;
			return true;
		}
		return false;
	}

	void ApplyTriggeredTagEffect(
		FSRResourceInstance& ResourceInstance,
		ESRResourceProcessTag Tag,
		int32 StackCount,
		ESRFacilityTemperatureState TemperatureState,
		FSRFacilityFormulaState& FormulaState)
	{
		const int32 SafeStackCount = FMath::Max(1, StackCount);
		switch (Tag)
		{
		case ESRResourceProcessTag::Responsive:
			AddEnergy(
				FormulaState,
				static_cast<double>(SafeStackCount) * HeatResponsiveBaseEnergy,
				BuildTagDetail(Tag, SafeStackCount, TEXT("trigger")));
			if (TemperatureState == ESRFacilityTemperatureState::Hot)
			{
				AddEnergy(
					FormulaState,
					static_cast<double>(SafeStackCount) * HeatResponsiveHotEnergy,
					BuildTagDetail(Tag, SafeStackCount, TEXT("trigger hot")));
			}
			break;

		case ESRResourceProcessTag::Supercooled:
			AddEnergy(
				FormulaState,
				static_cast<double>(SafeStackCount) * SupercooledEnergy,
				BuildTagDetail(Tag, SafeStackCount, TEXT("trigger")));
			break;

		case ESRResourceProcessTag::Volatile:
			AddEnergy(
				FormulaState,
				static_cast<double>(SafeStackCount) * VolatileEnergy,
				BuildTagDetail(Tag, SafeStackCount, TEXT("trigger")));
			break;

		case ESRResourceProcessTag::HyperReactive:
			AddEnergy(
				FormulaState,
				static_cast<double>(FMath::Max(0, FormulaState.ProcessLimitLost) * SafeStackCount)
					* HyperReactiveEnergyPerProcessLimit,
				BuildTagDetail(Tag, SafeStackCount, TEXT("trigger process limit")));
			break;

		case ESRResourceProcessTag::HalfLife:
			AddEnergy(
				FormulaState,
				FormulaState.Base * -0.5,
				BuildTagDetail(Tag, SafeStackCount, TEXT("trigger half base")));
			ResetHalfLifeCycles(ResourceInstance);
			break;

		case ESRResourceProcessTag::Charge:
			AddEnergy(
				FormulaState,
				StarRovers::FacilityResources::ChargeEnergyBonus,
				BuildTagDetail(Tag, SafeStackCount, TEXT("trigger")));
			break;

		default:
			break;
		}
	}

	void TriggerTags(
		FSRResourceInstance& ResourceInstance,
		const FSRFacilityEffectSpec& EffectSpec,
		ESRFacilityTemperatureState TemperatureState,
		FSRFacilityFormulaState& FormulaState)
	{
		if (EffectSpec.TagTarget == ESRFacilityEffectTagTarget::AllTags)
		{
			const TArray<FSRResourceTagStack> TagsToTrigger = ResourceInstance.Tags;
			for (const FSRResourceTagStack& TagStack : TagsToTrigger)
			{
				if (TagStack.StackCount > 0)
				{
					ApplyTriggeredTagEffect(
						ResourceInstance,
						TagStack.Tag,
						TagStack.StackCount,
						TemperatureState,
						FormulaState);
				}
			}
			return;
		}

		ESRResourceProcessTag Tag = ESRResourceProcessTag::Responsive;
		if (ResolveSpecificEffectTag(EffectSpec, FormulaState, Tag))
		{
			ApplyTriggeredTagEffect(
				ResourceInstance,
				Tag,
				FMath::Max(1, CountTagStacks(ResourceInstance, Tag)),
				TemperatureState,
				FormulaState);
		}
	}

	int32 ResolveAdjustedProcessLimit(
		const FSRResourceInstance& ResourceInstance,
		const FSRFacilityEffectSpec& EffectSpec)
	{
		const int32 CurrentLimit = FMath::Max(0, ResourceInstance.RemainingProcessLimit);
		switch (EffectSpec.ProcessLimitMode)
		{
		case ESRFacilityProcessLimitAdjustmentMode::SetValue:
			return FMath::Max(0, FMath::RoundToInt(EffectSpec.Value));
		case ESRFacilityProcessLimitAdjustmentMode::Multiply:
			return FMath::Max(0, FMath::RoundToInt(static_cast<double>(CurrentLimit) * EffectSpec.Value));
		case ESRFacilityProcessLimitAdjustmentMode::AddValue:
		default:
			return FMath::Max(0, CurrentLimit + FMath::RoundToInt(EffectSpec.Value));
		}
	}

	double ResolveCatalystValue(
		const FSRFacilityEffectSpec& EffectSpec,
		const FSRResourceInstance& ResourceInstance,
		const FSRFacilityFormulaState& FormulaState)
	{
		double BaseValue = EffectSpec.Value;
		switch (EffectSpec.EnergyValueSource)
		{
		case ESRFacilityEnergyAdjustmentValueSource::RemainingProcessLimit:
			BaseValue = static_cast<double>(FMath::Max(0, ResourceInstance.RemainingProcessLimit));
			break;
		case ESRFacilityEnergyAdjustmentValueSource::TagStackCount:
			BaseValue = static_cast<double>(
				EffectSpec.TagStackCountTarget == ESRFacilityTagStackCountTarget::All
					? CountAllTagStacks(ResourceInstance)
					: CountTagStacks(ResourceInstance, EffectSpec.ResourceTag));
			break;
		case ESRFacilityEnergyAdjustmentValueSource::EnergyChangeCount:
			BaseValue = static_cast<double>(FMath::Max(0, FormulaState.EnergyChangeCount));
			break;
		case ESRFacilityEnergyAdjustmentValueSource::TagEffectEnergyChangeAmount:
			BaseValue = FormulaState.Energy;
			break;
		case ESRFacilityEnergyAdjustmentValueSource::ProcessCount:
			BaseValue = static_cast<double>(FMath::Max(0, ResourceInstance.ProcessCount));
			break;
		case ESRFacilityEnergyAdjustmentValueSource::TagKindCount:
			BaseValue = static_cast<double>(CountTagKinds(ResourceInstance));
			break;
		case ESRFacilityEnergyAdjustmentValueSource::FixedValue:
		default:
			return EffectSpec.Value;
		}

		return BaseValue * EffectSpec.EnergyValueMultiplier;
	}

	FString BuildCatalystValueLabel(
		const FSRFacilityEffectSpec& EffectSpec,
		double Value)
	{
		const TCHAR* SourceLabel = TEXT("Fixed");
		switch (EffectSpec.EnergyValueSource)
		{
		case ESRFacilityEnergyAdjustmentValueSource::RemainingProcessLimit:
			SourceLabel = TEXT("RemainingProcessLimit");
			break;
		case ESRFacilityEnergyAdjustmentValueSource::TagStackCount:
			SourceLabel = TEXT("TagStackCount");
			break;
		case ESRFacilityEnergyAdjustmentValueSource::EnergyChangeCount:
			SourceLabel = TEXT("EnergyChangeCount");
			break;
		case ESRFacilityEnergyAdjustmentValueSource::TagEffectEnergyChangeAmount:
			SourceLabel = TEXT("Energy (B)");
			break;
		case ESRFacilityEnergyAdjustmentValueSource::ProcessCount:
			SourceLabel = TEXT("ProcessCount");
			break;
		case ESRFacilityEnergyAdjustmentValueSource::TagKindCount:
			SourceLabel = TEXT("TagKindCount");
			break;
		default:
			break;
		}
		return FString::Printf(TEXT("AdjustCatalyst %s %s"), SourceLabel, *FormatValue(Value));
	}

	void RemoveTagCount(
		FSRResourceInstance& ResourceInstance,
		ESRResourceProcessTag Tag,
		int32 CountToRemove)
	{
		int32 RemainingCount = FMath::Max(0, CountToRemove);
		for (int32 TagIndex = ResourceInstance.Tags.Num() - 1; TagIndex >= 0 && RemainingCount > 0; --TagIndex)
		{
			FSRResourceTagStack& TagStack = ResourceInstance.Tags[TagIndex];
			if (TagStack.Tag != Tag || TagStack.StackCount <= 0)
			{
				continue;
			}

			const int32 RemovedCount = FMath::Min(RemainingCount, TagStack.StackCount);
			TagStack.StackCount -= RemovedCount;
			RemainingCount -= RemovedCount;
			if (TagStack.StackCount <= 0)
			{
				ResourceInstance.Tags.RemoveAt(TagIndex);
			}
		}
	}

	void RemoveAnyTagCount(FSRResourceInstance& ResourceInstance, int32 CountToRemove)
	{
		int32 RemainingCount = FMath::Max(0, CountToRemove);
		for (int32 TagIndex = ResourceInstance.Tags.Num() - 1; TagIndex >= 0 && RemainingCount > 0; --TagIndex)
		{
			FSRResourceTagStack& TagStack = ResourceInstance.Tags[TagIndex];
			if (TagStack.StackCount <= 0)
			{
				continue;
			}

			const int32 RemovedCount = FMath::Min(RemainingCount, TagStack.StackCount);
			TagStack.StackCount -= RemovedCount;
			RemainingCount -= RemovedCount;
			if (TagStack.StackCount <= 0)
			{
				ResourceInstance.Tags.RemoveAt(TagIndex);
			}
		}
	}

	void RemoveTags(
		FSRResourceInstance& ResourceInstance,
		const FSRFacilityEffectSpec& EffectSpec,
		const FSRFacilityFormulaState& FormulaState)
	{
		const bool bRemoveCountOnly = EffectSpec.EffectKind == ESRFacilityEffectKind::RemoveTag
			&& EffectSpec.RemoveTagAmountMode == ESRFacilityRemoveTagAmountMode::Count;
		const int32 CountToRemove = FMath::Max(1, EffectSpec.Count);
		if (EffectSpec.TagTarget == ESRFacilityEffectTagTarget::AllTags)
		{
			if (bRemoveCountOnly)
			{
				RemoveAnyTagCount(ResourceInstance, CountToRemove);
			}
			else
			{
				ResourceInstance.Tags.Reset();
			}
			return;
		}

		ESRResourceProcessTag Tag = ESRResourceProcessTag::Responsive;
		if (ResolveSpecificEffectTag(EffectSpec, FormulaState, Tag))
		{
			RemoveTagCount(
				ResourceInstance,
				Tag,
				bRemoveCountOnly ? CountToRemove : TNumericLimits<int32>::Max());
		}
	}

	void CollectTags(
		const FSRResourceInstance& ResourceInstance,
		const FSRFacilityEffectSpec& EffectSpec,
		const FSRFacilityFormulaState& FormulaState,
		TArray<FSRResourceTagStack>& OutTags)
	{
		OutTags.Reset();
		if (EffectSpec.TagTarget == ESRFacilityEffectTagTarget::AllTags)
		{
			for (const FSRResourceTagStack& TagStack : ResourceInstance.Tags)
			{
				if (TagStack.StackCount > 0)
				{
					OutTags.Add(TagStack);
				}
			}
			return;
		}

		ESRResourceProcessTag Tag = ESRResourceProcessTag::Responsive;
		if (!ResolveSpecificEffectTag(EffectSpec, FormulaState, Tag))
		{
			return;
		}
		for (const FSRResourceTagStack& TagStack : ResourceInstance.Tags)
		{
			if (TagStack.Tag == Tag && TagStack.StackCount > 0)
			{
				OutTags.Add(TagStack);
			}
		}
	}

	FSRResourceInstance MakeDuplicatedResource(const FSRResourceInstance& ResourceInstance)
	{
		FSRResourceInstance DuplicatedResource = ResourceInstance;
		DuplicatedResource.ResourceInstanceId = FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		DuplicatedResource.StackCount = FMath::Max(1, DuplicatedResource.StackCount);
		return DuplicatedResource;
	}

	FSRResourceInstance MakeConditionResource(
		const FSRResourceInstance& ResourceInstance,
		const FSRFacilityFormulaState& FormulaState)
	{
		FSRResourceInstance ConditionResource = ResourceInstance;
		ConditionResource.EnergyValue = ResolveFormulaResult(FormulaState);
		ConditionResource.EnergyChangeCount = FMath::Max(0, FormulaState.EnergyChangeCount);
		return ConditionResource;
	}

	void ApplyFacilityEffects(
		const FSRFacilityInstance& FacilityInstance,
		const TArray<FSRResourceInstance>& InputResources,
		ESRFacilityTemperatureState EffectiveTemperatureState,
		FSRResourceInstance& ResourceInstance,
		FSRFacilityFormulaState& FormulaState,
		bool& bHasPrimaryResource,
		TArray<FSRResourceInstance>& OutAdditionalOutputs,
		TArray<FString>& OutAdditionalFormulaTexts)
	{
		const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
		if (!IsValid(FacilityDataAsset))
		{
			return;
		}

		const FSRResourceInstance BaselineResource = ResourceInstance;
		SeedLastAttachedTag(FormulaState, ResourceInstance);
		for (const FSRFacilityEffectSpec& EffectSpec : FacilityDataAsset->Effects)
		{
			const FSRResourceInstance ConditionResource = MakeConditionResource(ResourceInstance, FormulaState);
			const FSRResourceInstance ConditionBaseline = [&BaselineResource, &FormulaState]()
			{
				FSRResourceInstance Result = BaselineResource;
				Result.EnergyValue = FormulaState.Base;
				return Result;
			}();
			const StarRovers::FacilityEffects::FSRFacilityEffectConditionContext ConditionContext =
			{
				bHasPrimaryResource ? &ConditionResource : nullptr,
				bHasPrimaryResource ? &ConditionBaseline : nullptr,
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
					const double AdjustmentValue = ResolveCatalystValue(
						EffectSpec,
						ConditionResource,
						FormulaState);
					const FString Detail = BuildCatalystValueLabel(EffectSpec, AdjustmentValue);
					if (EffectSpec.EnergyAdjustmentMode == ESRFacilityEnergyAdjustmentMode::Multiply)
					{
						SetCatalyst(FormulaState, FormulaState.Catalyst * AdjustmentValue, Detail);
					}
					else if (EffectSpec.EnergyAdjustmentMode == ESRFacilityEnergyAdjustmentMode::Subtract)
					{
						SetCatalyst(FormulaState, FormulaState.Catalyst - AdjustmentValue, Detail);
					}
					else
					{
						SetCatalyst(FormulaState, FormulaState.Catalyst + AdjustmentValue, Detail);
					}
				}
				break;

			case ESRFacilityEffectKind::AdjustProcessLimit:
				if (bHasPrimaryResource)
				{
					const int32 PreviousLimit = FMath::Max(0, ResourceInstance.RemainingProcessLimit);
					ResourceInstance.RemainingProcessLimit = ResolveAdjustedProcessLimit(ResourceInstance, EffectSpec);
					const int32 AdditionalLimitLost = FMath::Max(
						0,
						PreviousLimit - ResourceInstance.RemainingProcessLimit);
					FormulaState.ProcessLimitLost += AdditionalLimitLost;
					const int32 HyperReactiveStacks = CountTagStacks(
						ResourceInstance,
						ESRResourceProcessTag::HyperReactive);
					if (AdditionalLimitLost > 0 && HyperReactiveStacks > 0)
					{
						AddEnergy(
							FormulaState,
							static_cast<double>(AdditionalLimitLost * HyperReactiveStacks)
								* HyperReactiveEnergyPerProcessLimit,
							BuildTagDetail(
								ESRResourceProcessTag::HyperReactive,
								HyperReactiveStacks,
								TEXT("facility process limit")));
					}
				}
				break;

			case ESRFacilityEffectKind::RemoveResource:
				bHasPrimaryResource = false;
				break;

			case ESRFacilityEffectKind::AttachTag:
				if (!bHasPrimaryResource)
				{
					break;
				}
				if (EffectSpec.AttachTagSource == ESRFacilityAttachTagSource::MissingTags)
				{
					constexpr ESRResourceProcessTag TagCandidates[] =
					{
						ESRResourceProcessTag::Responsive,
						ESRResourceProcessTag::HalfLife,
						ESRResourceProcessTag::Volatile,
						ESRResourceProcessTag::Supercooled,
						ESRResourceProcessTag::HyperReactive,
						ESRResourceProcessTag::Charge,
					};
					for (const ESRResourceProcessTag Tag : TagCandidates)
					{
						if (CountTagStacks(ResourceInstance, Tag) <= 0)
						{
							AddTagStack(ResourceInstance, Tag, 1);
							FormulaState.LastAttachedTag = Tag;
							FormulaState.bHasLastAttachedTag = true;
						}
					}
					break;
				}
				if (EffectSpec.AttachTagSource == ESRFacilityAttachTagSource::AttachedTags)
				{
					const TArray<FSRResourceTagStack> TagsToDuplicate = ResourceInstance.Tags;
					for (FSRResourceTagStack TagStack : TagsToDuplicate)
					{
						if (TagStack.StackCount > 0)
						{
							NormalizeTagStack(TagStack);
							ResourceInstance.Tags.Add(TagStack);
							FormulaState.LastAttachedTag = TagStack.Tag;
							FormulaState.bHasLastAttachedTag = true;
						}
					}
					break;
				}

				if (EffectSpec.AttachTagSource == ESRFacilityAttachTagSource::SpecificTag)
				{
					AddTagStack(ResourceInstance, EffectSpec.ResourceTag, Count);
					FormulaState.LastAttachedTag = EffectSpec.ResourceTag;
					FormulaState.bHasLastAttachedTag = true;
				}
				else if (FormulaState.bHasLastAttachedTag)
				{
					AddTagStack(ResourceInstance, FormulaState.LastAttachedTag, Count);
				}
				break;

			case ESRFacilityEffectKind::ProduceWaste:
				if (IsValid(EffectSpec.ProducedResource.Get()))
				{
					for (int32 OutputIndex = 0; OutputIndex < Count; ++OutputIndex)
					{
						const FSRResourceInstance WasteResource = EffectSpec.ProducedResource->BuildDefaultInstance();
						OutAdditionalOutputs.Add(WasteResource);
						OutAdditionalFormulaTexts.Add(FString::Printf(
							TEXT("Waste base Energy: %s"),
							*FormatValue(WasteResource.EnergyValue)));
					}
				}
				break;

			case ESRFacilityEffectKind::TransferTagsToWaste:
				if (bHasPrimaryResource && IsValid(EffectSpec.ProducedResource.Get()))
				{
					TArray<FSRResourceTagStack> TransferredTags;
					CollectTags(ResourceInstance, EffectSpec, FormulaState, TransferredTags);
					if (!TransferredTags.IsEmpty())
					{
						FSRResourceInstance WasteResource = EffectSpec.ProducedResource->BuildDefaultInstance();
						WasteResource.Tags = TransferredTags;
						WasteResource.StackCount = 1;
						RemoveTags(ResourceInstance, EffectSpec, FormulaState);
						OutAdditionalOutputs.Add(WasteResource);
						OutAdditionalFormulaTexts.Add(FString::Printf(
							TEXT("Tag transfer waste: %s"),
							*FormatValue(WasteResource.EnergyValue)));
					}
				}
				break;

			case ESRFacilityEffectKind::InvertTagEffects:
				SetCatalyst(FormulaState, FormulaState.Catalyst * -1.0, TEXT("InvertTagEffects"));
				break;

			case ESRFacilityEffectKind::DoubleTagEffects:
				SetCatalyst(FormulaState, FormulaState.Catalyst * 2.0, TEXT("DoubleTagEffects"));
				break;

			case ESRFacilityEffectKind::DuplicateInputResource:
				for (int32 DuplicateIndex = 0; DuplicateIndex < Count; ++DuplicateIndex)
				{
					for (const FSRResourceInstance& InputResource : InputResources)
					{
						if (!InputResource.ResourceId.IsNone() && InputResource.StackCount > 0)
						{
							OutAdditionalOutputs.Add(MakeDuplicatedResource(InputResource));
							OutAdditionalFormulaTexts.Add(FString::Printf(
								TEXT("Duplicate input Energy: %s"),
								*FormatValue(InputResource.EnergyValue)));
						}
					}
				}
				break;

			case ESRFacilityEffectKind::TriggerTagEffect:
				if (bHasPrimaryResource)
				{
					TriggerTags(ResourceInstance, EffectSpec, EffectiveTemperatureState, FormulaState);
				}
				break;

			case ESRFacilityEffectKind::RemoveTag:
				if (bHasPrimaryResource)
				{
					RemoveTags(ResourceInstance, EffectSpec, FormulaState);
				}
				break;

			case ESRFacilityEffectKind::ChangeResourceType:
				if (bHasPrimaryResource && IsValid(EffectSpec.TargetResource.Get()))
				{
					ResourceInstance.ResourceDataAsset = EffectSpec.TargetResource;
					ResourceInstance.ResourceId = EffectSpec.TargetResource->ResourceId;
				}
				break;

			case ESRFacilityEffectKind::AdjustCellTemperature:
			case ESRFacilityEffectKind::InvertHeat:
			case ESRFacilityEffectKind::OverrideProcessTemperature:
			case ESRFacilityEffectKind::AdjustProcessTime:
			default:
				break;
			}
		}
	}

	int32 CountAdditionalOutputs(
		const USRFacilityDataAsset* FacilityDataAsset,
		int32 InputResourceCount)
	{
		if (!IsValid(FacilityDataAsset))
		{
			return 0;
		}

		int32 OutputCount = 0;
		for (const FSRFacilityEffectSpec& EffectSpec : FacilityDataAsset->Effects)
		{
			const int32 Count = FMath::Max(1, EffectSpec.Count);
			if (EffectSpec.EffectKind == ESRFacilityEffectKind::ProduceWaste
				&& IsValid(EffectSpec.ProducedResource.Get()))
			{
				OutputCount += Count;
			}
			else if (EffectSpec.EffectKind == ESRFacilityEffectKind::TransferTagsToWaste
				&& IsValid(EffectSpec.ProducedResource.Get()))
			{
				++OutputCount;
			}
			else if (EffectSpec.EffectKind == ESRFacilityEffectKind::DuplicateInputResource)
			{
				OutputCount += Count * FMath::Max(0, InputResourceCount);
			}
		}
		return OutputCount;
	}

	bool RemovesPrimaryOutput(const USRFacilityDataAsset* FacilityDataAsset)
	{
		if (!IsValid(FacilityDataAsset))
		{
			return false;
		}

		for (const FSRFacilityEffectSpec& EffectSpec : FacilityDataAsset->Effects)
		{
			if (EffectSpec.EffectKind == ESRFacilityEffectKind::RemoveResource)
			{
				return true;
			}
		}
		return false;
	}

	bool CanResourceEnterFacility(
		const FSRResourceInstance& ResourceInstance,
		ESRFacilityTemperatureState TemperatureState)
	{
		return !ResourceInstance.ResourceId.IsNone()
			&& ResourceInstance.RemainingProcessLimit > 0
			&& (CountTagStacks(ResourceInstance, ESRResourceProcessTag::Supercooled) <= 0
				|| TemperatureState == ESRFacilityTemperatureState::Cold);
	}

	FSRPreparedFacilityResource PrepareProcessResource(
		const FSRResourceInstance& InputResource,
		ESRFacilityTemperatureState TemperatureState)
	{
		FSRPreparedFacilityResource PreparedResource;
		PreparedResource.Resource = InputResource;
		PreparedResource.Resource.StackCount = 1;
		PreparedResource.TotalProcessLimitLost = ConsumeProcessLimit(
			PreparedResource.Resource,
			TemperatureState);
		PreparedResource.ProcessLimitLossByTagIndex.Init(
			PreparedResource.TotalProcessLimitLost,
			PreparedResource.Resource.Tags.Num());
		PreparedResource.CompletedProcessCountIncrement = 1;
		return PreparedResource;
	}

	FSRPreparedFacilityResource PrepareSynthesisResource(
		const TArray<FSRResourceInstance>& InputResources,
		ESRFacilityTemperatureState TemperatureState)
	{
		FSRPreparedFacilityResource PreparedResource;
		PreparedResource.Resource.ResourceInstanceId = FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		PreparedResource.Resource.ResourceId = CompoundResourceId;
		PreparedResource.Resource.ResourceDataAsset = nullptr;
		PreparedResource.Resource.RemainingProcessLimit = TNumericLimits<int32>::Max();
		PreparedResource.Resource.StackCount = 1;

		const FSRResourceInstance* FirstResource = nullptr;
		bool bSameResourceType = true;
		for (const FSRResourceInstance& InputResource : InputResources)
		{
			if (InputResource.ResourceId.IsNone())
			{
				continue;
			}

			FSRResourceInstance ProcessedInput = InputResource;
			ProcessedInput.StackCount = 1;
			const int32 ProcessLimitLost = ConsumeProcessLimit(ProcessedInput, TemperatureState);
			PreparedResource.TotalProcessLimitLost += ProcessLimitLost;
			PreparedResource.CompletedProcessCountIncrement += 1;
			if (!FirstResource)
			{
				FirstResource = &InputResource;
			}
			else if (InputResource.ResourceId != FirstResource->ResourceId)
			{
				bSameResourceType = false;
			}

			PreparedResource.Resource.EnergyValue += ProcessedInput.EnergyValue;
			PreparedResource.Resource.RemainingProcessLimit = FMath::Min(
				PreparedResource.Resource.RemainingProcessLimit,
				ProcessedInput.RemainingProcessLimit);
			PreparedResource.Resource.ProcessCount += FMath::Max(0, ProcessedInput.ProcessCount);
			PreparedResource.Resource.EnergyChangeCount += FMath::Max(0, ProcessedInput.EnergyChangeCount);
			for (FSRResourceTagStack TagStack : ProcessedInput.Tags)
			{
				if (TagStack.StackCount <= 0)
				{
					continue;
				}
				NormalizeTagStack(TagStack);
				PreparedResource.Resource.Tags.Add(TagStack);
				PreparedResource.ProcessLimitLossByTagIndex.Add(ProcessLimitLost);
			}
		}

		if (bSameResourceType && FirstResource)
		{
			PreparedResource.Resource.ResourceId = FirstResource->ResourceId;
			PreparedResource.Resource.ResourceDataAsset = FirstResource->ResourceDataAsset;
		}
		if (PreparedResource.Resource.RemainingProcessLimit == TNumericLimits<int32>::Max())
		{
			PreparedResource.Resource.RemainingProcessLimit = 0;
		}
		return PreparedResource;
	}

	void BuildOutputsFromPreparedResource(
		const FSRFacilityInstance& FacilityInstance,
		const TArray<FSRResourceInstance>& InputResources,
		ESRFacilityTemperatureState EffectiveTemperatureState,
		FSRPreparedFacilityResource PreparedResource,
		TArray<FSRResourceInstance>& OutOutputResources,
		int32* OutPrimaryOutputCount,
		FSRResourceInstance* OutBaselinePrimaryResource,
		TArray<FString>* OutEnergyFormulaTexts)
	{
		if (OutBaselinePrimaryResource)
		{
			*OutBaselinePrimaryResource = PreparedResource.Resource;
		}

		FSRFacilityFormulaState FormulaState;
		FormulaState.Base = PreparedResource.Resource.EnergyValue;
		FormulaState.EnergyChangeCount = FMath::Max(0, PreparedResource.Resource.EnergyChangeCount);
		FormulaState.ProcessLimitLost = FMath::Max(0, PreparedResource.TotalProcessLimitLost);
		TArray<FSRResourceInstance> FallbackProcessResources;
		const TArray<FSRResourceInstance>* ProcessResources = &InputResources;
		if (InputResources.IsEmpty())
		{
			FallbackProcessResources.Add(PreparedResource.Resource);
			ProcessResources = &FallbackProcessResources;
		}
		const float ProcessSeconds = StarRovers::FacilityProcessing::ResolveFacilityProcessSeconds(
			FacilityInstance.FacilityDataAsset.Get(),
			EffectiveTemperatureState,
			*ProcessResources);
		const int32 ChargeToAdd = FMath::Max(0, FMath::FloorToInt(ProcessSeconds))
			* StarRovers::FacilityResources::ChargeStacksPerProcessingSecond;
		ApplyAutomaticTagEffects(
			PreparedResource.Resource,
			PreparedResource.ProcessLimitLossByTagIndex,
			EffectiveTemperatureState,
			ChargeToAdd,
			FormulaState);

		bool bHasPrimaryResource = !PreparedResource.Resource.ResourceId.IsNone();
		TArray<FSRResourceInstance> AdditionalOutputs;
		TArray<FString> AdditionalFormulaTexts;
		ApplyFacilityEffects(
			FacilityInstance,
			InputResources,
			EffectiveTemperatureState,
			PreparedResource.Resource,
			FormulaState,
			bHasPrimaryResource,
			AdditionalOutputs,
			AdditionalFormulaTexts);

		if (bHasPrimaryResource)
		{
			PreparedResource.Resource.EnergyValue = ResolveFormulaResult(FormulaState);
			PreparedResource.Resource.EnergyChangeCount = FMath::Max(0, FormulaState.EnergyChangeCount);
			PreparedResource.Resource.ProcessCount = FMath::Max(0, PreparedResource.Resource.ProcessCount)
				+ FMath::Max(0, PreparedResource.CompletedProcessCountIncrement);
			StarRovers::Resources::SynchronizeLegacyRuntimeStateToResourceV2(PreparedResource.Resource);
		}
		for (FSRResourceInstance& AdditionalOutput : AdditionalOutputs)
		{
			StarRovers::Resources::SynchronizeLegacyRuntimeStateToResourceV2(AdditionalOutput);
		}

		const int32 PrimaryOutputCount = bHasPrimaryResource
			&& !FacilityInstance.OutputPortInventories.IsEmpty()
			? 1
			: 0;
		if (OutPrimaryOutputCount)
		{
			*OutPrimaryOutputCount = PrimaryOutputCount;
		}
		if (PrimaryOutputCount > 0)
		{
			OutOutputResources.Add(PreparedResource.Resource);
			if (OutEnergyFormulaTexts)
			{
				OutEnergyFormulaTexts->Add(BuildFormulaText(FormulaState));
			}
		}

		OutOutputResources.Append(AdditionalOutputs);
		if (OutEnergyFormulaTexts)
		{
			OutEnergyFormulaTexts->Append(AdditionalFormulaTexts);
		}
	}

	void BuildResourceV2Output(
		const FSRFacilityInstance& FacilityInstance,
		const FSRResourceInstance& InputResource,
		TArray<FSRResourceInstance>& OutOutputResources,
		int32* OutPrimaryOutputCount,
		FSRResourceInstance* OutBaselinePrimaryResource,
		TArray<FString>* OutEnergyFormulaTexts,
		FSRResourceProcessResult* OutResourceV2ProcessResult,
		FName ProcessingBodyId)
	{
		if (OutBaselinePrimaryResource)
		{
			*OutBaselinePrimaryResource = InputResource;
		}

		const FSRFacilityResourceV2Evaluation Evaluation = FSRFacilityResourceV2Processor::Evaluate(
			FacilityInstance,
			InputResource,
			ProcessingBodyId);
		if (OutResourceV2ProcessResult)
		{
			*OutResourceV2ProcessResult = Evaluation.ResourceProcessResult;
		}
		if (!Evaluation.IsSuccess() || FacilityInstance.OutputPortInventories.IsEmpty())
		{
			return;
		}

		FSRResourceInstance OutputResource = Evaluation.ResourceProcessResult.OutputResource;
		OutputResource.StackCount = 1;
		OutOutputResources.Add(OutputResource);
		if (OutPrimaryOutputCount)
		{
			*OutPrimaryOutputCount = 1;
		}
		if (OutEnergyFormulaTexts)
		{
			OutEnergyFormulaTexts->Add(FSRFacilityResourceV2Processor::BuildPreviewSummary(Evaluation));
		}
	}

	void BuildStellarFuelV2Output(
		const FSRFacilityInstance& FacilityInstance,
		const TArray<FSRResourceInstance>& InputResources,
		TArray<FSRResourceInstance>& OutOutputResources,
		int32* OutPrimaryOutputCount,
		TArray<FString>* OutEnergyFormulaTexts,
		FSRStellarFuelFabricationResultV2* OutStellarFuelFabricationResult,
		FName ProcessingBodyId)
	{
		const FSRStellarFuelFabricationResultV2 Evaluation = FSRStellarFuelFabricator::Evaluate(
			FacilityInstance,
			InputResources,
			ProcessingBodyId);
		if (OutStellarFuelFabricationResult)
		{
			*OutStellarFuelFabricationResult = Evaluation;
		}
		if (!Evaluation.IsSuccess() || FacilityInstance.OutputPortInventories.IsEmpty())
		{
			return;
		}

		OutOutputResources.Add(Evaluation.OutputFuel);
		if (OutPrimaryOutputCount)
		{
			*OutPrimaryOutputCount = 1;
		}
		if (OutEnergyFormulaTexts)
		{
			OutEnergyFormulaTexts->Add(FSRStellarFuelFabricator::BuildPreviewSummary(Evaluation));
		}
	}

	void BuildOperationalEconomyV2Output(
		const FSRFacilityInstance& FacilityInstance,
		const TArray<FSRResourceInstance>& InputResources,
		TArray<FSRResourceInstance>& OutOutputResources,
		int32* OutPrimaryOutputCount,
		TArray<FString>* OutEnergyFormulaTexts)
	{
		const FSROperationalEconomyEvaluationV2 Evaluation =
			FSROperationalEconomyProcessor::Evaluate(FacilityInstance, InputResources);
		if (!Evaluation.IsSuccess())
		{
			return;
		}

		if (!Evaluation.OutputResources.IsEmpty()
			&& FacilityInstance.OutputPortInventories.IsEmpty())
		{
			return;
		}
		OutOutputResources = Evaluation.OutputResources;
		if (OutPrimaryOutputCount)
		{
			*OutPrimaryOutputCount = OutOutputResources.IsEmpty() ? 0 : 1;
		}
		if (OutEnergyFormulaTexts)
		{
			OutEnergyFormulaTexts->Add(
				FSROperationalEconomyProcessor::BuildPreviewSummary(Evaluation));
		}
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
	if (FacilityDataAsset->FacilityKind == ESRFacilityKind::Hub)
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
	if (FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Process
		&& FSRFacilityResourceV2Processor::ShouldRouteStandardProcessThroughResourceV2(FacilityDataAsset))
	{
		if (InputResources.Num() != 1)
		{
			return false;
		}

		FSRFacilityInstance PreviewFacility;
		PreviewFacility.FacilityDataAsset = const_cast<USRFacilityDataAsset*>(FacilityDataAsset);
		PreviewFacility.TemperatureState = TemperatureState;
		return FSRFacilityResourceV2Processor::Evaluate(PreviewFacility, InputResources[0]).IsSuccess();
	}
	if (FSRStellarFuelFabricator::ShouldRouteThroughResourceV2(FacilityDataAsset))
	{
		FSRFacilityInstance PreviewFacility;
		PreviewFacility.FacilityDataAsset = const_cast<USRFacilityDataAsset*>(FacilityDataAsset);
		PreviewFacility.TemperatureState = TemperatureState;
		return FSRStellarFuelFabricator::Evaluate(PreviewFacility, InputResources).IsSuccess();
	}
	if (FSROperationalEconomyProcessor::ShouldRouteThroughResourceV2(FacilityDataAsset))
	{
		FSRFacilityInstance PreviewFacility;
		PreviewFacility.FacilityDataAsset = const_cast<USRFacilityDataAsset*>(FacilityDataAsset);
		PreviewFacility.TemperatureState = TemperatureState;
		return FSROperationalEconomyProcessor::Evaluate(PreviewFacility, InputResources).IsSuccess();
	}

	const StarRovers::FacilityProcessing::FSRFacilityProcessContext ProcessContext =
		StarRovers::FacilityProcessing::ResolveProcessContext(
			FacilityDataAsset,
			TemperatureState,
			FindFirstResource(InputResources));
	for (const FSRResourceInstance& InputResource : InputResources)
	{
		if (!CanResourceEnterFacility(InputResource, ProcessContext.EffectiveTemperatureState))
		{
			return false;
		}
	}

	if (FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Process)
	{
		return InputResources.Num() == 1;
	}
	return FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Synthesize;
}

int32 FSRFacilityOutputResourceBuilder::CountProducedOutputResources(
	const USRFacilityDataAsset* FacilityDataAsset)
{
	if (FSRFacilityResourceV2Processor::ShouldRouteStandardProcessThroughResourceV2(FacilityDataAsset))
	{
		return 0;
	}
	if (FSRStellarFuelFabricator::ShouldRouteThroughResourceV2(FacilityDataAsset))
	{
		return 0;
	}
	if (FSROperationalEconomyProcessor::ShouldRouteThroughResourceV2(FacilityDataAsset))
	{
		return 0;
	}

	const int32 InputCount = IsValid(FacilityDataAsset)
		? FMath::Max(0, FacilityDataAsset->InputInventory.SlotCount)
		: 0;
	return CountAdditionalOutputs(FacilityDataAsset, InputCount);
}

int32 FSRFacilityOutputResourceBuilder::ResolvePrimaryOutputCount(
	const FSRFacilityInstance& FacilityInstance)
{
	if (FSRFacilityResourceV2Processor::ShouldRouteStandardProcessThroughResourceV2(
		FacilityInstance.FacilityDataAsset.Get()))
	{
		FString DefinitionFailure;
		return !FacilityInstance.OutputPortInventories.IsEmpty()
			&& FSRFacilityResourceV2Processor::ValidateProcessDefinition(
				FacilityInstance.FacilityDataAsset.Get(),
				DefinitionFailure)
			? 1
			: 0;
	}
	if (FSRStellarFuelFabricator::ShouldRouteThroughResourceV2(
		FacilityInstance.FacilityDataAsset.Get()))
	{
		FString DefinitionFailure;
		return !FacilityInstance.OutputPortInventories.IsEmpty()
			&& FSRStellarFuelFabricator::ValidateFacilityDefinition(
				FacilityInstance.FacilityDataAsset.Get(),
				DefinitionFailure)
			? 1
			: 0;
	}
	if (FSROperationalEconomyProcessor::ShouldRouteThroughResourceV2(
		FacilityInstance.FacilityDataAsset.Get()))
	{
		FString DefinitionFailure;
		if (!FSROperationalEconomyProcessor::ValidateFacilityDefinition(
			FacilityInstance.FacilityDataAsset.Get(),
			DefinitionFailure))
		{
			return 0;
		}
		return FSROperationalEconomyProcessor::AllowsEmptyOutput(
			FacilityInstance.FacilityDataAsset.Get())
			? 0
			: (!FacilityInstance.OutputPortInventories.IsEmpty() ? 1 : 0);
	}

	return IsValid(FacilityInstance.FacilityDataAsset.Get())
		&& !FacilityInstance.OutputPortInventories.IsEmpty()
		&& !RemovesPrimaryOutput(FacilityInstance.FacilityDataAsset.Get())
		? 1
		: 0;
}

int32 FSRFacilityOutputResourceBuilder::ResolveRequiredOutputSlots(
	const FSRFacilityInstance& FacilityInstance)
{
	if (FSRFacilityResourceV2Processor::ShouldRouteStandardProcessThroughResourceV2(
		FacilityInstance.FacilityDataAsset.Get()))
	{
		return ResolvePrimaryOutputCount(FacilityInstance);
	}
	if (FSRStellarFuelFabricator::ShouldRouteThroughResourceV2(
		FacilityInstance.FacilityDataAsset.Get()))
	{
		return ResolvePrimaryOutputCount(FacilityInstance);
	}
	if (FSROperationalEconomyProcessor::ShouldRouteThroughResourceV2(
		FacilityInstance.FacilityDataAsset.Get()))
	{
		return ResolvePrimaryOutputCount(FacilityInstance);
	}

	return ResolvePrimaryOutputCount(FacilityInstance)
		+ CountAdditionalOutputs(
			FacilityInstance.FacilityDataAsset.Get(),
			FacilityInstance.InputPortInventories.Num());
}

bool FSRFacilityOutputResourceBuilder::AllowsEmptyOutput(
	const FSRFacilityInstance& FacilityInstance)
{
	if (FSRFacilityResourceV2Processor::ShouldRouteStandardProcessThroughResourceV2(
		FacilityInstance.FacilityDataAsset.Get()))
	{
		return false;
	}
	if (FSRStellarFuelFabricator::ShouldRouteThroughResourceV2(
		FacilityInstance.FacilityDataAsset.Get()))
	{
		return false;
	}
	if (FSROperationalEconomyProcessor::ShouldRouteThroughResourceV2(
		FacilityInstance.FacilityDataAsset.Get()))
	{
		return FSROperationalEconomyProcessor::AllowsEmptyOutput(
			FacilityInstance.FacilityDataAsset.Get());
	}

	return RemovesPrimaryOutput(FacilityInstance.FacilityDataAsset.Get())
		&& CountAdditionalOutputs(
			FacilityInstance.FacilityDataAsset.Get(),
			FacilityInstance.InputPortInventories.Num()) <= 0;
}

void FSRFacilityOutputResourceBuilder::BuildOutputResources(
	const FSRFacilityInstance& FacilityInstance,
	const TArray<FSRResourceInstance>& InputResources,
	TArray<FSRResourceInstance>& OutOutputResources,
	int32* OutPrimaryOutputCount,
	FSRResourceInstance* OutBaselinePrimaryResource,
	TArray<FString>* OutEnergyFormulaTexts,
	FSRResourceProcessResult* OutResourceV2ProcessResult,
	FSRStellarFuelFabricationResultV2* OutStellarFuelFabricationResult,
	FName ProcessingBodyId)
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
	if (OutEnergyFormulaTexts)
	{
		OutEnergyFormulaTexts->Reset();
	}
	if (OutResourceV2ProcessResult)
	{
		*OutResourceV2ProcessResult = FSRResourceProcessResult();
	}
	if (OutStellarFuelFabricationResult)
	{
		*OutStellarFuelFabricationResult = FSRStellarFuelFabricationResultV2();
	}

	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!DoesInputSetMatchOperation(FacilityDataAsset, InputResources, FacilityInstance.TemperatureState))
	{
		return;
	}
	if (FSRStellarFuelFabricator::ShouldRouteThroughResourceV2(FacilityDataAsset))
	{
		BuildStellarFuelV2Output(
			FacilityInstance,
			InputResources,
			OutOutputResources,
			OutPrimaryOutputCount,
			OutEnergyFormulaTexts,
			OutStellarFuelFabricationResult,
			ProcessingBodyId);
		return;
	}
	if (FSROperationalEconomyProcessor::ShouldRouteThroughResourceV2(FacilityDataAsset))
	{
		BuildOperationalEconomyV2Output(
			FacilityInstance,
			InputResources,
			OutOutputResources,
			OutPrimaryOutputCount,
			OutEnergyFormulaTexts);
		return;
	}
	if (FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Process
		&& FSRFacilityResourceV2Processor::ShouldRouteStandardProcessThroughResourceV2(FacilityDataAsset))
	{
		BuildResourceV2Output(
			FacilityInstance,
			InputResources[0],
			OutOutputResources,
			OutPrimaryOutputCount,
			OutBaselinePrimaryResource,
			OutEnergyFormulaTexts,
			OutResourceV2ProcessResult,
			ProcessingBodyId);
		return;
	}

	const StarRovers::FacilityProcessing::FSRFacilityProcessContext ProcessContext =
		StarRovers::FacilityProcessing::ResolveProcessContext(
			FacilityDataAsset,
			FacilityInstance.TemperatureState,
			FindFirstResource(InputResources));
	FSRPreparedFacilityResource PreparedResource = FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Synthesize
		? PrepareSynthesisResource(InputResources, ProcessContext.EffectiveTemperatureState)
		: PrepareProcessResource(InputResources[0], ProcessContext.EffectiveTemperatureState);
	BuildOutputsFromPreparedResource(
		FacilityInstance,
		InputResources,
		ProcessContext.EffectiveTemperatureState,
		MoveTemp(PreparedResource),
		OutOutputResources,
		OutPrimaryOutputCount,
		OutBaselinePrimaryResource,
		OutEnergyFormulaTexts);
}

void FSRFacilityOutputResourceBuilder::BuildOutputResourcesFromPrimaryResource(
	const FSRFacilityInstance& FacilityInstance,
	const TArray<FSRResourceInstance>& InputResources,
	const FSRResourceInstance& PrimaryResource,
	TArray<FSRResourceInstance>& OutOutputResources,
	int32* OutPrimaryOutputCount,
	FSRResourceInstance* OutBaselinePrimaryResource,
	TArray<FString>* OutEnergyFormulaTexts,
	FSRResourceProcessResult* OutResourceV2ProcessResult,
	FName ProcessingBodyId)
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
	if (OutEnergyFormulaTexts)
	{
		OutEnergyFormulaTexts->Reset();
	}
	if (OutResourceV2ProcessResult)
	{
		*OutResourceV2ProcessResult = FSRResourceProcessResult();
	}
	if (!IsValid(FacilityInstance.FacilityDataAsset.Get()) || PrimaryResource.ResourceId.IsNone())
	{
		return;
	}
	if (FacilityInstance.FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Process
		&& FSRFacilityResourceV2Processor::ShouldRouteStandardProcessThroughResourceV2(
			FacilityInstance.FacilityDataAsset.Get()))
	{
		BuildResourceV2Output(
			FacilityInstance,
			PrimaryResource,
			OutOutputResources,
			OutPrimaryOutputCount,
			OutBaselinePrimaryResource,
			OutEnergyFormulaTexts,
			OutResourceV2ProcessResult,
			ProcessingBodyId);
		return;
	}

	const StarRovers::FacilityProcessing::FSRFacilityProcessContext ProcessContext =
		StarRovers::FacilityProcessing::ResolveProcessContext(
			FacilityInstance.FacilityDataAsset.Get(),
			FacilityInstance.TemperatureState,
			&PrimaryResource);
	FSRPreparedFacilityResource PreparedResource;
	PreparedResource.Resource = PrimaryResource;
	PreparedResource.Resource.StackCount = 1;
	PreparedResource.ProcessLimitLossByTagIndex.Init(0, PreparedResource.Resource.Tags.Num());
	PreparedResource.CompletedProcessCountIncrement = 1;
	BuildOutputsFromPreparedResource(
		FacilityInstance,
		InputResources,
		ProcessContext.EffectiveTemperatureState,
		MoveTemp(PreparedResource),
		OutOutputResources,
		OutPrimaryOutputCount,
		OutBaselinePrimaryResource,
		OutEnergyFormulaTexts);
}
