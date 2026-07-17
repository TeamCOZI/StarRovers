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
		int32 TagEffectApplicationCount = 1;
	};

	int32 GetTagEffectApplicationCount(const FSRFacilityEffectContext& EffectContext)
	{
		return FMath::Max(1, EffectContext.TagEffectApplicationCount);
	}

	FString AddTagEffectApplicationDetail(const FString& Detail, const FSRFacilityEffectContext& EffectContext)
	{
		const int32 ApplicationCount = GetTagEffectApplicationCount(EffectContext);
		if (ApplicationCount <= 1)
		{
			return Detail;
		}
		return Detail.IsEmpty()
			? FString::Printf(TEXT("effect x%d"), ApplicationCount)
			: FString::Printf(TEXT("%s effect x%d"), *Detail, ApplicationCount);
	}

	double ResolveDirectedTagEnergyDelta(double EnergyDelta, const FSRFacilityEffectContext& EffectContext)
	{
		return EffectContext.bInvertTagEffects ? -EnergyDelta : EnergyDelta;
	}

	FString FormatEnergyFormulaValue(double Value)
	{
		return FString::Printf(TEXT("%.1f"), Value);
	}

	void AppendEnergyFormulaLine(FString* FormulaText, const FString& Line)
	{
		if (!FormulaText || Line.IsEmpty())
		{
			return;
		}

		if (!FormulaText->IsEmpty())
		{
			FormulaText->Append(TEXT("\n"));
		}
		FormulaText->Append(Line);
	}

	void AppendEnergyTransitionFormula(
		FString* FormulaText,
		const TCHAR* Label,
		double BeforeEnergy,
		double AfterEnergy)
	{
		if (!FormulaText || FMath::IsNearlyEqual(BeforeEnergy, AfterEnergy))
		{
			return;
		}

		AppendEnergyFormulaLine(
			FormulaText,
			FString::Printf(
				TEXT("%s: %s -> %s"),
				Label,
				*FormatEnergyFormulaValue(BeforeEnergy),
				*FormatEnergyFormulaValue(AfterEnergy)));
	}

	void AppendEnergyDeltaFormula(
		FString* FormulaText,
		const TCHAR* Label,
		double BeforeEnergy,
		double Delta,
		double AfterEnergy)
	{
		if (!FormulaText || FMath::IsNearlyEqual(BeforeEnergy, AfterEnergy))
		{
			return;
		}

		AppendEnergyFormulaLine(
			FormulaText,
			FString::Printf(
				TEXT("%s: %s %s %s = %s"),
				Label,
				*FormatEnergyFormulaValue(BeforeEnergy),
				Delta < 0.0 ? TEXT("-") : TEXT("+"),
				*FormatEnergyFormulaValue(FMath::Abs(Delta)),
				*FormatEnergyFormulaValue(AfterEnergy)));
	}

	void AppendEnergyMultiplyFormula(
		FString* FormulaText,
		const TCHAR* Label,
		double BeforeEnergy,
		double Multiplier,
		double AfterEnergy)
	{
		if (!FormulaText || FMath::IsNearlyEqual(BeforeEnergy, AfterEnergy))
		{
			return;
		}

		AppendEnergyFormulaLine(
			FormulaText,
			FString::Printf(
				TEXT("%s: %s * %s = %s"),
				Label,
				*FormatEnergyFormulaValue(BeforeEnergy),
				*FormatEnergyFormulaValue(Multiplier),
				*FormatEnergyFormulaValue(AfterEnergy)));
	}

	const TCHAR* GetEnergyFormulaEffectLabel(ESRFacilityEffectKind EffectKind)
	{
		switch (EffectKind)
		{
		case ESRFacilityEffectKind::AdjustEnergy:
			return TEXT("AdjustEnergy");
		case ESRFacilityEffectKind::AdjustProcessLimit:
			return TEXT("AdjustProcessLimit");
		case ESRFacilityEffectKind::TriggerTagEffect:
			return TEXT("TriggerTagEffect");
		default:
			return TEXT("Effect");
		}
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
		EffectContext.TagEffectApplicationCount = FMath::Max(1, ProcessContext.TagEffectApplicationCount);
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

	int32 CountAllTagStacks(const FSRResourceInstance& ResourceInstance)
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
			if (CountTagStacks(ResourceInstance, Tag) > 0)
			{
				++KindCount;
			}
		}
		return KindCount;
	}

	const TCHAR* GetResourceProcessTagFormulaLabel(ESRResourceProcessTag Tag)
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
			return TEXT("T");
		}
	}

	FString BuildTagFormulaLabel(ESRResourceProcessTag Tag, int32 StackCount, const FString& Detail)
	{
		FString Label = FString::Printf(
			TEXT("Tag %s x%d"),
			GetResourceProcessTagFormulaLabel(Tag),
			FMath::Max(1, StackCount));
		if (!Detail.IsEmpty())
		{
			Label += TEXT(" ");
			Label += Detail;
		}
		return Label;
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

	double ApplyTagEnergyDelta(
		FSRResourceInstance& ResourceInstance,
		double EnergyDelta,
		FSRFacilityEffectContext& EffectContext)
	{
		const double EnergyValueBeforeEffect = ResourceInstance.EnergyValue;
		const double DirectedEnergyDelta = ResolveDirectedTagEnergyDelta(EnergyDelta, EffectContext);
		const int32 ApplicationCount = GetTagEffectApplicationCount(EffectContext);
		for (int32 ApplicationIndex = 0; ApplicationIndex < ApplicationCount; ++ApplicationIndex)
		{
			AddEnergyDelta(ResourceInstance, DirectedEnergyDelta, EffectContext);
		}
		return ResourceInstance.EnergyValue - EnergyValueBeforeEffect;
	}

	double ApplyTagEnergyMultiplier(
		FSRResourceInstance& ResourceInstance,
		double Multiplier,
		FSRFacilityEffectContext& EffectContext)
	{
		const int32 ApplicationCount = GetTagEffectApplicationCount(EffectContext);
		for (int32 ApplicationIndex = 0; ApplicationIndex < ApplicationCount; ++ApplicationIndex)
		{
			MultiplyEnergyValue(ResourceInstance, Multiplier, EffectContext);
		}
		return FMath::Pow(Multiplier, static_cast<double>(ApplicationCount));
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

	void IncrementProcessCount(FSRResourceInstance& ResourceInstance, int32 CountToAdd = 1)
	{
		ResourceInstance.ProcessCount = FMath::Max(0, ResourceInstance.ProcessCount) + FMath::Max(0, CountToAdd);
	}

	bool CanEnergyResourceEnterFacility(
		const FSRResourceInstance& ResourceInstance,
		ESRFacilityTemperatureState TemperatureState)
	{
		return ResourceInstance.RemainingProcessLimit > 0
			&& (!HasTag(ResourceInstance, ESRResourceProcessTag::Supercooled)
				|| TemperatureState == ESRFacilityTemperatureState::Cold);
	}

	int32 ResolveAdjustedProcessLimit(
		const FSRResourceInstance& ResourceInstance,
		const FSRFacilityEffectSpec& EffectSpec)
	{
		const int32 CurrentProcessLimit = FMath::Max(0, ResourceInstance.RemainingProcessLimit);
		switch (EffectSpec.ProcessLimitMode)
		{
		case ESRFacilityProcessLimitAdjustmentMode::SetValue:
			return FMath::RoundToInt(EffectSpec.Value);
		case ESRFacilityProcessLimitAdjustmentMode::Multiply:
			return FMath::RoundToInt(static_cast<double>(CurrentProcessLimit) * EffectSpec.Value);
		case ESRFacilityProcessLimitAdjustmentMode::AddValue:
		default:
			return CurrentProcessLimit + FMath::RoundToInt(EffectSpec.Value);
		}
	}

	void MergeTagStacks(TArray<FSRResourceTagStack>& InOutTags, const TArray<FSRResourceTagStack>& TagsToMerge)
	{
		for (const FSRResourceTagStack& TagToMerge : TagsToMerge)
		{
			if (TagToMerge.StackCount <= 0)
			{
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
		FSRFacilityEffectContext& EffectContext,
		FString* EnergyFormulaText = nullptr)
	{
		const int32 PreviousProcessLimit = FMath::Max(0, ResourceInstance.RemainingProcessLimit);
		const int32 ClampedProcessLimit = FMath::Max(0, NewProcessLimit);
		ResourceInstance.RemainingProcessLimit = ClampedProcessLimit;

		const int32 LostProcessLimit = FMath::Max(0, PreviousProcessLimit - ClampedProcessLimit);
		const int32 HyperReactiveStackCount = CountTagStacks(ResourceInstance, ESRResourceProcessTag::HyperReactive);
		if (LostProcessLimit > 0 && HyperReactiveStackCount > 0)
		{
			const double EnergyBeforeHyperReactive = ResourceInstance.EnergyValue;
			const double EnergyDelta = static_cast<double>(LostProcessLimit * HyperReactiveStackCount)
				* HyperReactiveEnergyBonusPerProcessLimitLoss;
			const double AppliedEnergyDelta = ApplyTagEnergyDelta(ResourceInstance, EnergyDelta, EffectContext);
			const FString Label = BuildTagFormulaLabel(
				ESRResourceProcessTag::HyperReactive,
				HyperReactiveStackCount,
				AddTagEffectApplicationDetail(
					FString::Printf(
						TEXT("lost limit %d * %s"),
						LostProcessLimit,
						*FormatEnergyFormulaValue(HyperReactiveEnergyBonusPerProcessLimitLoss)),
					EffectContext));
			AppendEnergyDeltaFormula(
				EnergyFormulaText,
				*Label,
				EnergyBeforeHyperReactive,
				AppliedEnergyDelta,
				ResourceInstance.EnergyValue);
		}
	}

	void ConsumeEnergyForFacilityPass(
		FSRResourceInstance& ResourceInstance,
		ESRFacilityTemperatureState TemperatureState,
		FSRFacilityEffectContext& EffectContext,
		FString* EnergyFormulaText = nullptr)
	{
		SetProcessLimitAndApplyHyperReactive(
			ResourceInstance,
			ResourceInstance.RemainingProcessLimit - 1,
			EffectContext,
			EnergyFormulaText);
		if (TemperatureState == ESRFacilityTemperatureState::Hot)
		{
			SetProcessLimitAndApplyHyperReactive(
				ResourceInstance,
				ResourceInstance.RemainingProcessLimit - 1,
				EffectContext,
				EnergyFormulaText);
		}
	}

	void ConsumeEnergyInputsForFacilityPass(
		TArray<FSRResourceInstance>& InOutResources,
		ESRFacilityTemperatureState TemperatureState,
		FSRFacilityEffectContext& EffectContext,
		TArray<FString>* EnergyFormulaTexts = nullptr)
	{
		for (int32 ResourceIndex = 0; ResourceIndex < InOutResources.Num(); ++ResourceIndex)
		{
			ConsumeEnergyForFacilityPass(
				InOutResources[ResourceIndex],
				TemperatureState,
				EffectContext,
				EnergyFormulaTexts && EnergyFormulaTexts->IsValidIndex(ResourceIndex)
					? &(*EnergyFormulaTexts)[ResourceIndex]
					: nullptr);
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

	int32 ResolveChargeStacksToAddForProcessingPass(
		const FSRFacilityInstance& FacilityInstance,
		ESRFacilityTemperatureState EffectiveTemperatureState,
		const TArray<FSRResourceInstance>& ConditionResources)
	{
		return FMath::Max(0, FMath::FloorToInt(StarRovers::FacilityProcessing::ResolveFacilityProcessSeconds(
			FacilityInstance.FacilityDataAsset.Get(),
			EffectiveTemperatureState,
			ConditionResources)))
			* StarRovers::FacilityResources::ChargeStacksPerProcessingSecond;
	}

	void ApplyHalfLifeProcessingEffect(
		FSRResourceInstance& ResourceInstance,
		FSRResourceTagStack& TagStack,
		FSRFacilityEffectContext& EffectContext,
		FString* EnergyFormulaText = nullptr)
	{
		if (TagStack.Tag != ESRResourceProcessTag::HalfLife || TagStack.StackCount <= 0)
		{
			return;
		}

		if (TagStack.RemainingCycles <= 0)
		{
			TagStack.RemainingCycles = StarRovers::FacilityResources::HalfLifeDefaultCycles;
		}

		--TagStack.RemainingCycles;
		if (TagStack.RemainingCycles > 0)
		{
			return;
		}

		const double EnergyBeforeHalfLife = ResourceInstance.EnergyValue;
		const double Multiplier = EffectContext.bInvertTagEffects ? 2.0 : 0.5;
		const double AppliedMultiplier = ApplyTagEnergyMultiplier(ResourceInstance, Multiplier, EffectContext);
		const FString Label = BuildTagFormulaLabel(
			ESRResourceProcessTag::HalfLife,
			TagStack.StackCount,
			AddTagEffectApplicationDetail(
				FString::Printf(
					TEXT("cycle reset multiplier %s"),
					*FormatEnergyFormulaValue(Multiplier)),
				EffectContext));
		AppendEnergyMultiplyFormula(
			EnergyFormulaText,
			*Label,
			EnergyBeforeHalfLife,
			AppliedMultiplier,
			ResourceInstance.EnergyValue);
		TagStack.RemainingCycles = StarRovers::FacilityResources::HalfLifeDefaultCycles;
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
		FSRFacilityEffectContext& EffectContext,
		FString* EnergyFormulaText = nullptr)
	{
		if (ConsumeChargeStacks(ResourceInstance, StarRovers::FacilityResources::ChargeRequiredStacks))
		{
			const double EnergyBeforeCharge = ResourceInstance.EnergyValue;
			const double EnergyDelta = ApplyTagEnergyDelta(
				ResourceInstance,
				StarRovers::FacilityResources::ChargeEnergyBonus,
				EffectContext);
			const FString Label = BuildTagFormulaLabel(
				ESRResourceProcessTag::Charge,
				1,
				AddTagEffectApplicationDetail(
					FString::Printf(
						TEXT("consumed %d charge"),
						StarRovers::FacilityResources::ChargeRequiredStacks),
					EffectContext));
			AppendEnergyDeltaFormula(
				EnergyFormulaText,
				*Label,
				EnergyBeforeCharge,
				EnergyDelta,
				ResourceInstance.EnergyValue);
		}
		AddChargeStacks(ResourceInstance, ChargeStacksToAdd);
	}

	void ApplyTagEffects(
		ESRFacilityTemperatureState TemperatureState,
		int32 ChargeStacksToAdd,
		FSRFacilityEffectContext& EffectContext,
		FSRResourceInstance& ResourceInstance,
		FString* EnergyFormulaText = nullptr)
	{
		bool bAppliedChargeProcessingEffect = false;
		for (FSRResourceTagStack& TagStack : ResourceInstance.Tags)
		{
			if (TagStack.StackCount <= 0)
			{
				continue;
			}

			const int32 StackCount = FMath::Max(1, TagStack.StackCount);
			switch (TagStack.Tag)
			{
			case ESRResourceProcessTag::Responsive:
			{
				const double EnergyBeforeResponsive = ResourceInstance.EnergyValue;
				const double EnergyDelta = ApplyTagEnergyDelta(
					ResourceInstance,
					static_cast<double>(StackCount) * HeatResponsiveBaseEnergyBonus,
					EffectContext);
				const FString Label = BuildTagFormulaLabel(
					ESRResourceProcessTag::Responsive,
					StackCount,
					AddTagEffectApplicationDetail(
						FString::Printf(TEXT("base %s each"), *FormatEnergyFormulaValue(HeatResponsiveBaseEnergyBonus)),
						EffectContext));
				AppendEnergyDeltaFormula(
					EnergyFormulaText,
					*Label,
					EnergyBeforeResponsive,
					EnergyDelta,
					ResourceInstance.EnergyValue);
				if (TemperatureState == ESRFacilityTemperatureState::Hot)
				{
					const double EnergyBeforeHotResponsive = ResourceInstance.EnergyValue;
					const double HotEnergyDelta = ApplyTagEnergyDelta(
						ResourceInstance,
						static_cast<double>(StackCount) * HeatResponsiveHotEnergyBonus,
						EffectContext);
					const FString HotLabel = BuildTagFormulaLabel(
						ESRResourceProcessTag::Responsive,
						StackCount,
						AddTagEffectApplicationDetail(
							FString::Printf(TEXT("hot bonus %s each"), *FormatEnergyFormulaValue(HeatResponsiveHotEnergyBonus)),
							EffectContext));
					AppendEnergyDeltaFormula(
						EnergyFormulaText,
						*HotLabel,
						EnergyBeforeHotResponsive,
						HotEnergyDelta,
						ResourceInstance.EnergyValue);
				}
				break;
			}

			case ESRResourceProcessTag::Supercooled:
				if (TemperatureState == ESRFacilityTemperatureState::Cold)
				{
					const double EnergyBeforeSupercooled = ResourceInstance.EnergyValue;
					const double EnergyDelta = ApplyTagEnergyDelta(
						ResourceInstance,
						static_cast<double>(StackCount) * SupercooledEnergyBonus,
						EffectContext);
					const FString Label = BuildTagFormulaLabel(
						ESRResourceProcessTag::Supercooled,
						StackCount,
						AddTagEffectApplicationDetail(
							FString::Printf(TEXT("cold bonus %s each"), *FormatEnergyFormulaValue(SupercooledEnergyBonus)),
							EffectContext));
					AppendEnergyDeltaFormula(
						EnergyFormulaText,
						*Label,
						EnergyBeforeSupercooled,
						EnergyDelta,
						ResourceInstance.EnergyValue);
				}
				break;

			case ESRResourceProcessTag::Volatile:
			{
				const double EnergyBeforeVolatile = ResourceInstance.EnergyValue;
				const double EnergyDelta = ApplyTagEnergyDelta(
					ResourceInstance,
					static_cast<double>(StackCount) * -VolatileEnergyPenalty,
					EffectContext);
				const FString Label = BuildTagFormulaLabel(
					ESRResourceProcessTag::Volatile,
					StackCount,
					AddTagEffectApplicationDetail(
						FString::Printf(TEXT("penalty -%s each"), *FormatEnergyFormulaValue(VolatileEnergyPenalty)),
						EffectContext));
				AppendEnergyDeltaFormula(
					EnergyFormulaText,
					*Label,
					EnergyBeforeVolatile,
					EnergyDelta,
					ResourceInstance.EnergyValue);
				break;
			}

			case ESRResourceProcessTag::Charge:
				if (!bAppliedChargeProcessingEffect)
				{
					ApplyChargeProcessingEffect(
						ResourceInstance,
						ChargeStacksToAdd,
						EffectContext,
						EnergyFormulaText);
					bAppliedChargeProcessingEffect = true;
				}
				break;

			case ESRResourceProcessTag::HalfLife:
				ApplyHalfLifeProcessingEffect(
					ResourceInstance,
					TagStack,
					EffectContext,
					EnergyFormulaText);
				break;

			case ESRResourceProcessTag::HyperReactive:
			default:
				break;
			}
		}
	}

	bool DoesEnergyValueSourceUseMultiplier(ESRFacilityEnergyAdjustmentValueSource ValueSource)
	{
		return ValueSource != ESRFacilityEnergyAdjustmentValueSource::FixedValue;
	}

	double ResolveEnergyAdjustmentBaseValue(
		const FSRFacilityEffectSpec& EffectSpec,
		const FSRResourceInstance& ResourceInstance,
		const FSRFacilityEffectContext& EffectContext)
	{
		switch (EffectSpec.EnergyValueSource)
		{
		case ESRFacilityEnergyAdjustmentValueSource::RemainingProcessLimit:
			return static_cast<double>(FMath::Max(0, ResourceInstance.RemainingProcessLimit));
		case ESRFacilityEnergyAdjustmentValueSource::TagStackCount:
			return static_cast<double>(
				EffectSpec.TagStackCountTarget == ESRFacilityTagStackCountTarget::All
					? CountAllTagStacks(ResourceInstance)
					: CountTagStacks(ResourceInstance, EffectSpec.ResourceTag));
		case ESRFacilityEnergyAdjustmentValueSource::EnergyChangeCount:
			return static_cast<double>(FMath::Max(0, EffectContext.EnergyChangeCount));
		case ESRFacilityEnergyAdjustmentValueSource::TagEffectEnergyChangeAmount:
			return EffectContext.TagEffectEnergyChangeAmount;
		case ESRFacilityEnergyAdjustmentValueSource::ProcessCount:
			return static_cast<double>(FMath::Max(0, ResourceInstance.ProcessCount));
		case ESRFacilityEnergyAdjustmentValueSource::TagKindCount:
			return static_cast<double>(CountTagKinds(ResourceInstance));
		case ESRFacilityEnergyAdjustmentValueSource::FixedValue:
		default:
			return EffectSpec.Value;
		}
	}

	double ResolveEnergyAdjustmentValue(
		const FSRFacilityEffectSpec& EffectSpec,
		const FSRResourceInstance& ResourceInstance,
		const FSRFacilityEffectContext& EffectContext)
	{
		const double BaseValue = ResolveEnergyAdjustmentBaseValue(EffectSpec, ResourceInstance, EffectContext);
		return DoesEnergyValueSourceUseMultiplier(EffectSpec.EnergyValueSource)
			? BaseValue * EffectSpec.EnergyValueMultiplier
			: BaseValue;
	}

	FString BuildEnergyAdjustmentResolvedValueText(
		const FSRFacilityEffectSpec& EffectSpec,
		double ResolvedValue)
	{
		if (DoesEnergyValueSourceUseMultiplier(EffectSpec.EnergyValueSource)
			&& !FMath::IsNearlyEqual(EffectSpec.EnergyValueMultiplier, 1.0))
		{
			return FString::Printf(
				TEXT("* %s => %s"),
				*FormatEnergyFormulaValue(EffectSpec.EnergyValueMultiplier),
				*FormatEnergyFormulaValue(ResolvedValue));
		}
		return FString::Printf(TEXT("=> %s"), *FormatEnergyFormulaValue(ResolvedValue));
	}

	const TCHAR* GetEnergyAdjustmentModeFormulaLabel(ESRFacilityEnergyAdjustmentMode AdjustmentMode)
	{
		switch (AdjustmentMode)
		{
		case ESRFacilityEnergyAdjustmentMode::Multiply:
			return TEXT("Multiply");
		case ESRFacilityEnergyAdjustmentMode::Subtract:
			return TEXT("Subtract");
		case ESRFacilityEnergyAdjustmentMode::Add:
		default:
			return TEXT("Add");
		}
	}

	FString BuildEnergyAdjustmentValueSourceFormulaLabel(
		const FSRFacilityEffectSpec& EffectSpec,
		const FSRResourceInstance& ResourceInstance,
		const FSRFacilityEffectContext& EffectContext,
		double ResolvedValue)
	{
		const double BaseValue = ResolveEnergyAdjustmentBaseValue(EffectSpec, ResourceInstance, EffectContext);
		const FString ResolvedValueText = BuildEnergyAdjustmentResolvedValueText(EffectSpec, ResolvedValue);
		switch (EffectSpec.EnergyValueSource)
		{
		case ESRFacilityEnergyAdjustmentValueSource::RemainingProcessLimit:
			return FString::Printf(
				TEXT("RemainingProcessLimit %d %s"),
				FMath::Max(0, ResourceInstance.RemainingProcessLimit),
				*ResolvedValueText);
		case ESRFacilityEnergyAdjustmentValueSource::TagStackCount:
			if (EffectSpec.TagStackCountTarget == ESRFacilityTagStackCountTarget::All)
			{
				return FString::Printf(
					TEXT("TagStackCount All x%d %s"),
					CountAllTagStacks(ResourceInstance),
					*ResolvedValueText);
			}
			return FString::Printf(
				TEXT("TagStackCount %s x%d %s"),
				GetResourceProcessTagFormulaLabel(EffectSpec.ResourceTag),
				CountTagStacks(ResourceInstance, EffectSpec.ResourceTag),
				*ResolvedValueText);
		case ESRFacilityEnergyAdjustmentValueSource::EnergyChangeCount:
			return FString::Printf(
				TEXT("EnergyChangeCount %d %s"),
				FMath::Max(0, EffectContext.EnergyChangeCount),
				*ResolvedValueText);
		case ESRFacilityEnergyAdjustmentValueSource::TagEffectEnergyChangeAmount:
			return FString::Printf(
				TEXT("TagEffectEnergyChangeAmount %s %s"),
				*FormatEnergyFormulaValue(BaseValue),
				*ResolvedValueText);
		case ESRFacilityEnergyAdjustmentValueSource::ProcessCount:
			return FString::Printf(
				TEXT("ProcessCount %d %s"),
				FMath::Max(0, ResourceInstance.ProcessCount),
				*ResolvedValueText);
		case ESRFacilityEnergyAdjustmentValueSource::TagKindCount:
			return FString::Printf(
				TEXT("TagKindCount %d %s"),
				CountTagKinds(ResourceInstance),
				*ResolvedValueText);
		case ESRFacilityEnergyAdjustmentValueSource::FixedValue:
		default:
			return FString::Printf(TEXT("FixedValue %s"), *FormatEnergyFormulaValue(ResolvedValue));
		}
	}

	FString BuildAdjustEnergyFormulaLabel(
		const FSRFacilityEffectSpec& EffectSpec,
		const FSRResourceInstance& ResourceInstance,
		const FSRFacilityEffectContext& EffectContext,
		double ResolvedValue)
	{
		return FString::Printf(
			TEXT("AdjustEnergy %s %s"),
			GetEnergyAdjustmentModeFormulaLabel(EffectSpec.EnergyAdjustmentMode),
			*BuildEnergyAdjustmentValueSourceFormulaLabel(
				EffectSpec,
				ResourceInstance,
				EffectContext,
				ResolvedValue));
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

	void RemoveTagStackCount(FSRResourceInstance& ResourceInstance, ESRResourceProcessTag Tag, int32 CountToRemove)
	{
		int32 RemainingCountToRemove = FMath::Max(0, CountToRemove);
		if (RemainingCountToRemove <= 0)
		{
			return;
		}

		for (int32 TagIndex = ResourceInstance.Tags.Num() - 1; TagIndex >= 0; --TagIndex)
		{
			FSRResourceTagStack& TagStack = ResourceInstance.Tags[TagIndex];
			if (TagStack.Tag != Tag || TagStack.StackCount <= 0)
			{
				continue;
			}

			const int32 RemovedCount = FMath::Min(RemainingCountToRemove, TagStack.StackCount);
			TagStack.StackCount -= RemovedCount;
			RemainingCountToRemove -= RemovedCount;
			if (TagStack.StackCount <= 0)
			{
				ResourceInstance.Tags.RemoveAt(TagIndex);
			}
			if (RemainingCountToRemove <= 0)
			{
				return;
			}
		}
	}

	void RemoveAllTagStackCount(FSRResourceInstance& ResourceInstance, int32 CountToRemove)
	{
		int32 RemainingCountToRemove = FMath::Max(0, CountToRemove);
		if (RemainingCountToRemove <= 0)
		{
			return;
		}

		for (int32 TagIndex = ResourceInstance.Tags.Num() - 1; TagIndex >= 0; --TagIndex)
		{
			FSRResourceTagStack& TagStack = ResourceInstance.Tags[TagIndex];
			if (TagStack.StackCount <= 0)
			{
				continue;
			}

			const int32 RemovedCount = FMath::Min(RemainingCountToRemove, TagStack.StackCount);
			TagStack.StackCount -= RemovedCount;
			RemainingCountToRemove -= RemovedCount;
			if (TagStack.StackCount <= 0)
			{
				ResourceInstance.Tags.RemoveAt(TagIndex);
			}
			if (RemainingCountToRemove <= 0)
			{
				return;
			}
		}
	}

	void RemoveTagsForEffect(
		FSRResourceInstance& ResourceInstance,
		const FSRFacilityEffectSpec& EffectSpec,
		const FSRFacilityEffectContext& EffectContext)
	{
		const bool bRemoveCountOnly = EffectSpec.EffectKind == ESRFacilityEffectKind::RemoveTag
			&& EffectSpec.RemoveTagAmountMode == ESRFacilityRemoveTagAmountMode::Count;
		const int32 CountToRemove = FMath::Max(1, EffectSpec.Count);
		if (EffectSpec.TagTarget == ESRFacilityEffectTagTarget::AllTags)
		{
			if (bRemoveCountOnly)
			{
				RemoveAllTagStackCount(ResourceInstance, CountToRemove);
				return;
			}
			ResourceInstance.Tags.Reset();
			return;
		}

		ESRResourceProcessTag Tag = ESRResourceProcessTag::Responsive;
		if (ResolveSpecificEffectTag(EffectSpec, EffectContext, Tag))
		{
			if (bRemoveCountOnly)
			{
				RemoveTagStackCount(ResourceInstance, Tag, CountToRemove);
				return;
			}
			RemoveTagStackCount(ResourceInstance, Tag, TNumericLimits<int32>::Max());
		}
	}

	void CollectTagStacksForEffect(
		const FSRResourceInstance& ResourceInstance,
		const FSRFacilityEffectSpec& EffectSpec,
		const FSRFacilityEffectContext& EffectContext,
		TArray<FSRResourceTagStack>& OutTagStacks)
	{
		OutTagStacks.Reset();
		if (EffectSpec.TagTarget == ESRFacilityEffectTagTarget::AllTags)
		{
			for (const FSRResourceTagStack& TagStack : ResourceInstance.Tags)
			{
				if (TagStack.StackCount > 0)
				{
					OutTagStacks.Add(TagStack);
				}
			}
			return;
		}

		ESRResourceProcessTag Tag = ESRResourceProcessTag::Responsive;
		if (!ResolveSpecificEffectTag(EffectSpec, EffectContext, Tag))
		{
			return;
		}

		for (const FSRResourceTagStack& TagStack : ResourceInstance.Tags)
		{
			if (TagStack.Tag == Tag && TagStack.StackCount > 0)
			{
				OutTagStacks.Add(TagStack);
			}
		}
	}

	int32 CountTagStackEntries(const TArray<FSRResourceTagStack>& TagStacks)
	{
		int32 StackCount = 0;
		for (const FSRResourceTagStack& TagStack : TagStacks)
		{
			StackCount += FMath::Max(0, TagStack.StackCount);
		}
		return StackCount;
	}

	void TransferTagsToWasteForEffect(
		FSRResourceInstance& ResourceInstance,
		const FSRFacilityEffectSpec& EffectSpec,
		FSRFacilityEffectContext& EffectContext,
		TArray<FSRResourceInstance>& OutAdditionalOutputs,
		TArray<FString>* OutAdditionalOutputEnergyFormulaTexts)
	{
		if (!IsValid(EffectSpec.ProducedResource.Get()))
		{
			return;
		}

		TArray<FSRResourceTagStack> TagStacksToTransfer;
		CollectTagStacksForEffect(ResourceInstance, EffectSpec, EffectContext, TagStacksToTransfer);
		if (TagStacksToTransfer.IsEmpty())
		{
			return;
		}

		FSRResourceInstance WasteResource = EffectSpec.ProducedResource->BuildDefaultInstance();
		WasteResource.Tags = MoveTemp(TagStacksToTransfer);
		WasteResource.StackCount = 1;
		RemoveTagsForEffect(ResourceInstance, EffectSpec, EffectContext);

		if (OutAdditionalOutputEnergyFormulaTexts)
		{
			OutAdditionalOutputEnergyFormulaTexts->Add(FString::Printf(
				TEXT("Tag transfer waste: %s\nTransferred tag stacks: %d"),
				*FormatEnergyFormulaValue(WasteResource.EnergyValue),
				CountTagStackEntries(WasteResource.Tags)));
		}
		OutAdditionalOutputs.Add(WasteResource);
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
		FSRFacilityEffectContext& EffectContext,
		FString* EnergyFormulaText = nullptr)
	{
		const int32 SafeStackCount = FMath::Max(1, StackCount);
		switch (Tag)
		{
		case ESRResourceProcessTag::Responsive:
		{
			const double EnergyBeforeResponsive = ResourceInstance.EnergyValue;
			const double EnergyDelta = ApplyTagEnergyDelta(
				ResourceInstance,
				static_cast<double>(SafeStackCount) * HeatResponsiveBaseEnergyBonus,
				EffectContext);
			const FString Label = BuildTagFormulaLabel(
				Tag,
				SafeStackCount,
				AddTagEffectApplicationDetail(
					FString::Printf(TEXT("trigger base %s each"), *FormatEnergyFormulaValue(HeatResponsiveBaseEnergyBonus)),
					EffectContext));
			AppendEnergyDeltaFormula(
				EnergyFormulaText,
				*Label,
				EnergyBeforeResponsive,
				EnergyDelta,
				ResourceInstance.EnergyValue);
			if (TemperatureState == ESRFacilityTemperatureState::Hot)
			{
				const double EnergyBeforeHotResponsive = ResourceInstance.EnergyValue;
				const double HotEnergyDelta = ApplyTagEnergyDelta(
					ResourceInstance,
					static_cast<double>(SafeStackCount) * HeatResponsiveHotEnergyBonus,
					EffectContext);
				const FString HotLabel = BuildTagFormulaLabel(
					Tag,
					SafeStackCount,
					AddTagEffectApplicationDetail(
						FString::Printf(TEXT("trigger hot bonus %s each"), *FormatEnergyFormulaValue(HeatResponsiveHotEnergyBonus)),
						EffectContext));
				AppendEnergyDeltaFormula(
					EnergyFormulaText,
					*HotLabel,
					EnergyBeforeHotResponsive,
					HotEnergyDelta,
					ResourceInstance.EnergyValue);
			}
			break;
		}
		case ESRResourceProcessTag::Supercooled:
		{
			const double EnergyBeforeSupercooled = ResourceInstance.EnergyValue;
			const double EnergyDelta = ApplyTagEnergyDelta(
				ResourceInstance,
				static_cast<double>(SafeStackCount) * SupercooledEnergyBonus,
				EffectContext);
			const FString Label = BuildTagFormulaLabel(
				Tag,
				SafeStackCount,
				AddTagEffectApplicationDetail(
					FString::Printf(TEXT("trigger bonus %s each"), *FormatEnergyFormulaValue(SupercooledEnergyBonus)),
					EffectContext));
			AppendEnergyDeltaFormula(
				EnergyFormulaText,
				*Label,
				EnergyBeforeSupercooled,
				EnergyDelta,
				ResourceInstance.EnergyValue);
			break;
		}
		case ESRResourceProcessTag::Volatile:
		{
			const double EnergyBeforeVolatile = ResourceInstance.EnergyValue;
			const double EnergyDelta = ApplyTagEnergyDelta(
				ResourceInstance,
				static_cast<double>(SafeStackCount) * -VolatileEnergyPenalty,
				EffectContext);
			const FString Label = BuildTagFormulaLabel(
				Tag,
				SafeStackCount,
				AddTagEffectApplicationDetail(
					FString::Printf(TEXT("trigger penalty -%s each"), *FormatEnergyFormulaValue(VolatileEnergyPenalty)),
					EffectContext));
			AppendEnergyDeltaFormula(
				EnergyFormulaText,
				*Label,
				EnergyBeforeVolatile,
				EnergyDelta,
				ResourceInstance.EnergyValue);
			break;
		}
		case ESRResourceProcessTag::HyperReactive:
		{
			const int32 ConsumedProcessLimit = ResolveConsumedProcessLimit(BaselineResource, ResourceInstance);
			const double EnergyBeforeHyperReactive = ResourceInstance.EnergyValue;
			const double EnergyDelta = ApplyTagEnergyDelta(
				ResourceInstance,
				static_cast<double>(ConsumedProcessLimit * SafeStackCount)
					* HyperReactiveEnergyBonusPerProcessLimitLoss,
				EffectContext);
			const FString Label = BuildTagFormulaLabel(
				Tag,
				SafeStackCount,
				AddTagEffectApplicationDetail(
					FString::Printf(
						TEXT("trigger consumed limit %d * %s"),
						ConsumedProcessLimit,
						*FormatEnergyFormulaValue(HyperReactiveEnergyBonusPerProcessLimitLoss)),
					EffectContext));
			AppendEnergyDeltaFormula(
				EnergyFormulaText,
				*Label,
				EnergyBeforeHyperReactive,
				EnergyDelta,
				ResourceInstance.EnergyValue);
			break;
		}
		case ESRResourceProcessTag::HalfLife:
		{
			const double EnergyBeforeHalfLife = ResourceInstance.EnergyValue;
			const double Multiplier = EffectContext.bInvertTagEffects ? 2.0 : 0.5;
			const double AppliedMultiplier = ApplyTagEnergyMultiplier(ResourceInstance, Multiplier, EffectContext);
			const FString Label = BuildTagFormulaLabel(
				Tag,
				SafeStackCount,
				AddTagEffectApplicationDetail(
					FString::Printf(TEXT("trigger multiplier %s"), *FormatEnergyFormulaValue(Multiplier)),
					EffectContext));
			AppendEnergyMultiplyFormula(
				EnergyFormulaText,
				*Label,
				EnergyBeforeHalfLife,
				AppliedMultiplier,
				ResourceInstance.EnergyValue);
			ResetHalfLifeTagCycles(ResourceInstance);
			break;
		}
		case ESRResourceProcessTag::Charge:
		{
			const double EnergyBeforeCharge = ResourceInstance.EnergyValue;
			const double EnergyDelta = ApplyTagEnergyDelta(
				ResourceInstance,
				StarRovers::FacilityResources::ChargeEnergyBonus,
				EffectContext);
			const FString Label = BuildTagFormulaLabel(
				Tag,
				SafeStackCount,
				AddTagEffectApplicationDetail(
					FString::Printf(TEXT("trigger bonus %s"), *FormatEnergyFormulaValue(StarRovers::FacilityResources::ChargeEnergyBonus)),
					EffectContext));
			AppendEnergyDeltaFormula(
				EnergyFormulaText,
				*Label,
				EnergyBeforeCharge,
				EnergyDelta,
				ResourceInstance.EnergyValue);
			break;
		}
		default:
			break;
		}
	}

	void TriggerTagsForEffect(
		FSRResourceInstance& ResourceInstance,
		const FSRFacilityEffectSpec& EffectSpec,
		ESRFacilityTemperatureState TemperatureState,
		const FSRResourceInstance& BaselineResource,
		FSRFacilityEffectContext& EffectContext,
		FString* EnergyFormulaText = nullptr)
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
						EffectContext,
						EnergyFormulaText);
				}
			}
			return;
		}

		ESRResourceProcessTag Tag = ESRResourceProcessTag::Responsive;
		if (ResolveSpecificEffectTag(EffectSpec, EffectContext, Tag))
		{
			const int32 StackCount = FMath::Max(1, CountTagStacks(ResourceInstance, Tag));
			ApplyImmediateTagEffect(
				ResourceInstance,
				Tag,
				StackCount,
				TemperatureState,
				BaselineResource,
				EffectContext,
				EnergyFormulaText);
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

	FString BuildSynthesisEnergyFormulaText(
		const TArray<FSRResourceInstance>& SynthesisResources,
		const TArray<FString>& SynthesisEnergyFormulaTexts,
		const FSRResourceInstance& OutputResource)
	{
		FString FormulaText;
		for (int32 ResourceIndex = 0; ResourceIndex < SynthesisEnergyFormulaTexts.Num(); ++ResourceIndex)
		{
			if (!SynthesisEnergyFormulaTexts[ResourceIndex].IsEmpty())
			{
				AppendEnergyFormulaLine(
					&FormulaText,
					FString::Printf(
						TEXT("Input %d formula:\n%s"),
						ResourceIndex + 1,
						*SynthesisEnergyFormulaTexts[ResourceIndex]));
			}
		}

		FString ProductFormula;
		for (const FSRResourceInstance& ResourceInstance : SynthesisResources)
		{
			if (ResourceInstance.ResourceId.IsNone())
			{
				continue;
			}

			if (!ProductFormula.IsEmpty())
			{
				ProductFormula += TEXT(" * ");
			}
			ProductFormula += FormatEnergyFormulaValue(ResourceInstance.EnergyValue);
		}

		if (!ProductFormula.IsEmpty())
		{
			AppendEnergyFormulaLine(
				&FormulaText,
				FString::Printf(
					TEXT("Synthesize: %s = %s"),
					*ProductFormula,
					*FormatEnergyFormulaValue(OutputResource.EnergyValue)));
		}

		return FormulaText;
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
			else if (EffectSpec.EffectKind == ESRFacilityEffectKind::TransferTagsToWaste && IsValid(EffectSpec.ProducedResource.Get()))
			{
				AdditionalOutputCount += FMath::Max(1, SafeInputResourceCount);
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

		if (FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Mine)
		{
			return FacilityInstance.OutputPortInventories.Num() > 0 ? 1 : 0;
		}

		return FacilityInstance.OutputPortInventories.Num() > 0 ? 1 : 0;
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
	FSRResourceInstance* OutBaselinePrimaryResource,
	TArray<FString>* OutEnergyFormulaTexts)
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
		TArray<FString> ProcessedEnergyFormulaTexts;
		BuildProcessedResourcesBeforeFacilityEffects(
			FacilityInstance,
			InputResources,
			ProcessedResources,
			InitialEnergyChangeCount,
			TagEffectEnergyChangeAmounts,
			&ResourcesBeforeTagEffects,
			&ProcessedEnergyFormulaTexts);

		TArray<FSRResourceInstance> SynthesisResources;
		TArray<FString> SynthesisEnergyFormulaTexts;
		SynthesisResources.Reserve(ProcessedResources.Num());
		SynthesisEnergyFormulaTexts.Reserve(ProcessedResources.Num());
		TArray<FSRResourceInstance> AdditionalOutputs;
		TArray<FString> AdditionalOutputEnergyFormulaTexts;
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
			const FSRResourceInstance* ConditionBaselineResource = ResourcesBeforeTagEffects.IsValidIndex(ResourceIndex)
				? &ResourcesBeforeTagEffects[ResourceIndex]
				: &ProcessedResource;
			FString SynthesisResourceFormula = ProcessedEnergyFormulaTexts.IsValidIndex(ResourceIndex)
				? ProcessedEnergyFormulaTexts[ResourceIndex]
				: FString::Printf(TEXT("Input %d: %s"), ResourceIndex + 1, *FormatEnergyFormulaValue(ProcessedResource.EnergyValue));
			ApplyFacilityEffects(
				FacilityInstance,
				InputResources,
				SynthesisResource,
				bHasSynthesisResource,
				AdditionalOutputs,
				InitialEnergyChangeCount,
				TagEffectEnergyChangeAmounts.IsValidIndex(ResourceIndex) ? TagEffectEnergyChangeAmounts[ResourceIndex] : 0.0,
				true,
				false,
				ConditionBaselineResource,
				&SynthesisResourceFormula,
				&AdditionalOutputEnergyFormulaTexts);
			if (bHasSynthesisResource && !SynthesisResource.ResourceId.IsNone())
			{
				SynthesisResources.Add(SynthesisResource);
				SynthesisEnergyFormulaTexts.Add(MoveTemp(SynthesisResourceFormula));
			}
		}

		FSRResourceInstance BaselineOutputResource = ResourcesBeforeTagEffects.IsEmpty()
			? FSRResourceInstance()
			: BuildSynthesisProductResource(ResourcesBeforeTagEffects);
		FSRResourceInstance OutputResource = SynthesisResources.IsEmpty()
			? FSRResourceInstance()
			: BuildSynthesisProductResource(SynthesisResources);
		FString OutputEnergyFormulaText = BuildSynthesisEnergyFormulaText(
			SynthesisResources,
			SynthesisEnergyFormulaTexts,
			OutputResource);
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
			&BaselineOutputResource,
			&OutputEnergyFormulaText,
			&AdditionalOutputEnergyFormulaTexts);
		if (bHasPrimaryResource)
		{
			IncrementProcessCount(OutputResource, SynthesisResources.Num());
		}

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
			if (OutEnergyFormulaTexts)
			{
				OutEnergyFormulaTexts->Add(OutputEnergyFormulaText);
			}
		}
		OutOutputResources.Append(AdditionalOutputs);
		if (OutEnergyFormulaTexts)
		{
			OutEnergyFormulaTexts->Append(AdditionalOutputEnergyFormulaTexts);
		}
		return;
	}

	int32 InitialEnergyChangeCount = 0;
	double InitialTagEffectEnergyChangeAmount = 0.0;
	FSRResourceInstance ConditionBaselineResource;
	FString OutputEnergyFormulaText;
	FSRResourceInstance OutputResource = BuildBaseOutputResource(
		FacilityInstance,
		InputResources,
		InitialEnergyChangeCount,
		InitialTagEffectEnergyChangeAmount,
		&ConditionBaselineResource,
		&OutputEnergyFormulaText);
	BuildOutputResourcesFromPrimaryResource(
		FacilityInstance,
		InputResources,
		OutputResource,
		OutOutputResources,
		OutPrimaryOutputCount,
		OutBaselinePrimaryResource,
		OutEnergyFormulaTexts,
		InitialEnergyChangeCount,
		InitialTagEffectEnergyChangeAmount,
		&ConditionBaselineResource,
		1,
		&OutputEnergyFormulaText);
}

void FSRFacilityOutputResourceBuilder::BuildOutputResourcesFromPrimaryResource(
	const FSRFacilityInstance& FacilityInstance,
	const TArray<FSRResourceInstance>& InputResources,
	const FSRResourceInstance& PrimaryResource,
	TArray<FSRResourceInstance>& OutOutputResources,
	int32* OutPrimaryOutputCount,
	FSRResourceInstance* OutBaselinePrimaryResource,
	TArray<FString>* OutEnergyFormulaTexts,
	int32 InitialEnergyChangeCount,
	double InitialTagEffectEnergyChangeAmount,
	const FSRResourceInstance* ConditionBaselineResource,
	int32 CompletedProcessCountIncrement,
	const FString* PrimaryEnergyFormulaText)
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
	TArray<FString> AdditionalOutputEnergyFormulaTexts;
	AdditionalOutputs.Reserve(CountAdditionalOutputResourcesForInputCount(
		FacilityInstance.FacilityDataAsset.Get(),
		InputResources.Num()));
	FString OutputEnergyFormulaText = PrimaryEnergyFormulaText
		? *PrimaryEnergyFormulaText
		: FString::Printf(TEXT("Input: %s"), *FormatEnergyFormulaValue(OutputResource.EnergyValue));
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
		ConditionBaselineResource,
		&OutputEnergyFormulaText,
		&AdditionalOutputEnergyFormulaTexts);

	const int32 OutputCount = ResolvePrimaryOutputCountForAdditionalOutputs(
		FacilityInstance,
		bHasPrimaryResource,
		AdditionalOutputs.Num());
	if (OutPrimaryOutputCount)
	{
		*OutPrimaryOutputCount = OutputCount;
	}
	if (bHasPrimaryResource && CompletedProcessCountIncrement > 0)
	{
		IncrementProcessCount(OutputResource, CompletedProcessCountIncrement);
	}
	OutOutputResources.Reserve(OutputCount + AdditionalOutputs.Num());
	for (int32 OutputIndex = 0; OutputIndex < OutputCount; ++OutputIndex)
	{
		OutOutputResources.Add(OutputResource);
		if (OutEnergyFormulaTexts)
		{
			OutEnergyFormulaTexts->Add(OutputEnergyFormulaText);
		}
	}
	OutOutputResources.Append(AdditionalOutputs);
	if (OutEnergyFormulaTexts)
	{
		OutEnergyFormulaTexts->Append(AdditionalOutputEnergyFormulaTexts);
	}
}

void FSRFacilityOutputResourceBuilder::BuildProcessedResourcesBeforeFacilityEffects(
	const FSRFacilityInstance& FacilityInstance,
	const TArray<FSRResourceInstance>& ConsumedResources,
	TArray<FSRResourceInstance>& OutProcessedResources,
	int32& OutEnergyChangeCount,
	TArray<double>& OutTagEffectEnergyChangeAmounts,
	TArray<FSRResourceInstance>* OutResourcesBeforeTagEffects,
	TArray<FString>* OutEnergyFormulaTexts)
{
	OutEnergyChangeCount = 0;
	OutProcessedResources = ConsumedResources;
	OutTagEffectEnergyChangeAmounts.Reset();
	OutTagEffectEnergyChangeAmounts.SetNumZeroed(OutProcessedResources.Num());
	if (OutResourcesBeforeTagEffects)
	{
		OutResourcesBeforeTagEffects->Reset();
	}
	if (OutEnergyFormulaTexts)
	{
		OutEnergyFormulaTexts->Reset();
		OutEnergyFormulaTexts->SetNum(OutProcessedResources.Num());
		for (int32 ResourceIndex = 0; ResourceIndex < OutProcessedResources.Num(); ++ResourceIndex)
		{
			(*OutEnergyFormulaTexts)[ResourceIndex] = FString::Printf(
				TEXT("Input %d: %s"),
				ResourceIndex + 1,
				*FormatEnergyFormulaValue(OutProcessedResources[ResourceIndex].EnergyValue));
		}
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
	ConsumeEnergyInputsForFacilityPass(
		OutProcessedResources,
		EffectiveTemperatureState,
		PreProcessingEffectContext,
		OutEnergyFormulaTexts);
	const int32 ChargeStacksToAdd = ResolveChargeStacksToAddForProcessingPass(
		FacilityInstance,
		EffectiveTemperatureState,
		ConsumedResources);
	if (OutResourcesBeforeTagEffects)
	{
		*OutResourcesBeforeTagEffects = OutProcessedResources;
	}
	for (int32 ResourceIndex = 0; ResourceIndex < OutProcessedResources.Num(); ++ResourceIndex)
	{
		FSRResourceInstance& ProcessedResource = OutProcessedResources[ResourceIndex];
		const double EnergyBeforeTagEffects = ProcessedResource.EnergyValue;
		ApplyTagEffects(
			EffectiveTemperatureState,
			ChargeStacksToAdd,
			PreProcessingEffectContext,
			ProcessedResource,
			OutEnergyFormulaTexts && OutEnergyFormulaTexts->IsValidIndex(ResourceIndex)
				? &(*OutEnergyFormulaTexts)[ResourceIndex]
				: nullptr);
		OutTagEffectEnergyChangeAmounts[ResourceIndex] = FMath::Abs(ProcessedResource.EnergyValue - EnergyBeforeTagEffects);
	}
	OutEnergyChangeCount = PreProcessingEffectContext.EnergyChangeCount;
}

FSRResourceInstance FSRFacilityOutputResourceBuilder::BuildBaseOutputResource(
	const FSRFacilityInstance& FacilityInstance,
	const TArray<FSRResourceInstance>& ConsumedResources,
	int32& OutEnergyChangeCount,
	double& OutTagEffectEnergyChangeAmount,
	FSRResourceInstance* OutConditionBaselineResource,
	FString* OutEnergyFormulaText)
{
	OutEnergyChangeCount = 0;
	OutTagEffectEnergyChangeAmount = 0.0;
	if (OutConditionBaselineResource)
	{
		*OutConditionBaselineResource = FSRResourceInstance();
	}
	if (OutEnergyFormulaText)
	{
		OutEnergyFormulaText->Reset();
	}
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset) || ConsumedResources.IsEmpty())
	{
		return !ConsumedResources.IsEmpty() ? ConsumedResources[0] : FSRResourceInstance();
	}

	TArray<FSRResourceInstance> ProcessedResources;
	TArray<FSRResourceInstance> ResourcesBeforeTagEffects;
	TArray<double> TagEffectEnergyChangeAmounts;
	TArray<FString> EnergyFormulaTexts;
	BuildProcessedResourcesBeforeFacilityEffects(
		FacilityInstance,
		ConsumedResources,
		ProcessedResources,
		OutEnergyChangeCount,
		TagEffectEnergyChangeAmounts,
		&ResourcesBeforeTagEffects,
		&EnergyFormulaTexts);

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
			if (OutEnergyFormulaText)
			{
				*OutEnergyFormulaText = EnergyFormulaTexts.IsValidIndex(ResourceIndex)
					? EnergyFormulaTexts[ResourceIndex]
					: FString::Printf(TEXT("Input %d: %s"), ResourceIndex + 1, *FormatEnergyFormulaValue(ProcessedResources[ResourceIndex].EnergyValue));
			}
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
		if (OutEnergyFormulaText)
		{
			*OutEnergyFormulaText = EnergyFormulaTexts.IsValidIndex(0)
				? EnergyFormulaTexts[0]
				: FString::Printf(TEXT("Input 1: %s"), *FormatEnergyFormulaValue(ProcessedResources[0].EnergyValue));
		}
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
	FSRResourceInstance SynthesisProductResource = BuildSynthesisProductResource(ProcessedResources);
	if (OutEnergyFormulaText)
	{
		*OutEnergyFormulaText = BuildSynthesisEnergyFormulaText(
			ProcessedResources,
			EnergyFormulaTexts,
			SynthesisProductResource);
	}
	return SynthesisProductResource;
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
	const FSRResourceInstance* ConditionBaselineResource,
	FString* EnergyFormulaText,
	TArray<FString>* OutAdditionalOutputEnergyFormulaTexts)
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
		bool bTraceAdjustEnergyFormula = false;
		double AdjustEnergyFormulaValue = 0.0;
		ESRFacilityEnergyAdjustmentMode AdjustEnergyFormulaMode = ESRFacilityEnergyAdjustmentMode::Add;
		FString AdjustEnergyFormulaLabel;
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
				bTraceAdjustEnergyFormula = true;
				AdjustEnergyFormulaValue = EnergyAdjustmentValue;
				AdjustEnergyFormulaMode = EffectSpec.EnergyAdjustmentMode;
				AdjustEnergyFormulaLabel = BuildAdjustEnergyFormulaLabel(
					EffectSpec,
					ResourceInstance,
					EffectContext,
					EnergyAdjustmentValue);
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
				const int32 TargetProcessLimit = ResolveAdjustedProcessLimit(ResourceInstance, EffectSpec);
				SetProcessLimitAndApplyHyperReactive(
					ResourceInstance,
					TargetProcessLimit,
					EffectContext,
					EnergyFormulaText);
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
				if (EffectSpec.AttachTagSource == ESRFacilityAttachTagSource::MissingTags)
				{
					constexpr ESRResourceProcessTag MissingTagCandidates[] =
					{
						ESRResourceProcessTag::Responsive,
						ESRResourceProcessTag::HalfLife,
						ESRResourceProcessTag::Volatile,
						ESRResourceProcessTag::Supercooled,
						ESRResourceProcessTag::HyperReactive,
						ESRResourceProcessTag::Charge,
					};

					for (const ESRResourceProcessTag TagToAttach : MissingTagCandidates)
					{
						if (CountTagStacks(ResourceInstance, TagToAttach) > 0)
						{
							continue;
						}

						AddTagStack(ResourceInstance, TagToAttach, 1);
						EffectContext.LastAttachedTag = TagToAttach;
						EffectContext.bHasLastAttachedTag = true;
					}
					break;
				}
				if (EffectSpec.AttachTagSource == ESRFacilityAttachTagSource::AttachedTags)
				{
					const TArray<FSRResourceTagStack> TagsToAttach = ResourceInstance.Tags;
					for (const FSRResourceTagStack& TagStack : TagsToAttach)
					{
						const int32 SafeStackCount = FMath::Max(0, TagStack.StackCount);
						if (SafeStackCount <= 0)
						{
							continue;
						}

						AddTagStack(ResourceInstance, TagStack.Tag, SafeStackCount);
						EffectContext.LastAttachedTag = TagStack.Tag;
						EffectContext.bHasLastAttachedTag = true;
					}
					break;
				}

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
					if (OutAdditionalOutputEnergyFormulaTexts)
					{
						OutAdditionalOutputEnergyFormulaTexts->Add(FString::Printf(
							TEXT("Waste base: %s"),
							*FormatEnergyFormulaValue(WasteResource.EnergyValue)));
					}
					OutAdditionalOutputs.Add(WasteResource);
				}
			}
			break;
		case ESRFacilityEffectKind::TransferTagsToWaste:
			if (!bApplyResourceEffects)
			{
				break;
			}
			if (bHasPrimaryResource)
			{
				TransferTagsToWasteForEffect(
					ResourceInstance,
					EffectSpec,
					EffectContext,
					OutAdditionalOutputs,
					OutAdditionalOutputEnergyFormulaTexts);
			}
			break;
		case ESRFacilityEffectKind::AdjustCellTemperature:
			break;
		case ESRFacilityEffectKind::InvertHeat:
		case ESRFacilityEffectKind::InvertTagEffects:
		case ESRFacilityEffectKind::DoubleTagEffects:
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
						if (OutAdditionalOutputEnergyFormulaTexts)
						{
							OutAdditionalOutputEnergyFormulaTexts->Add(FString::Printf(
								TEXT("Duplicate input: %s"),
								*FormatEnergyFormulaValue(InputResource.EnergyValue)));
						}
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
					EffectContext,
					EnergyFormulaText);
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
		if (bHasPrimaryResource && !FMath::IsNearlyEqual(EnergyValueBeforeEffect, ResourceInstance.EnergyValue))
		{
			if (bTraceAdjustEnergyFormula)
			{
				if (AdjustEnergyFormulaMode == ESRFacilityEnergyAdjustmentMode::Multiply)
				{
					AppendEnergyMultiplyFormula(
						EnergyFormulaText,
						*AdjustEnergyFormulaLabel,
						EnergyValueBeforeEffect,
						AdjustEnergyFormulaValue,
						ResourceInstance.EnergyValue);
				}
				else
				{
					const double Delta = AdjustEnergyFormulaMode == ESRFacilityEnergyAdjustmentMode::Subtract
						? -AdjustEnergyFormulaValue
						: AdjustEnergyFormulaValue;
					AppendEnergyDeltaFormula(
						EnergyFormulaText,
						*AdjustEnergyFormulaLabel,
						EnergyValueBeforeEffect,
						Delta,
						ResourceInstance.EnergyValue);
				}
			}
			else
			{
				AppendEnergyTransitionFormula(
					EnergyFormulaText,
					GetEnergyFormulaEffectLabel(EffectSpec.EffectKind),
					EnergyValueBeforeEffect,
					ResourceInstance.EnergyValue);
			}
		}
	}
}

void FSRFacilityOutputResourceBuilder::AddTagStack(FSRResourceInstance& ResourceInstance, ESRResourceProcessTag Tag, int32 Count)
{
	const int32 SafeCount = FMath::Max(1, Count);
	FSRResourceTagStack& NewTagStack = ResourceInstance.Tags.AddDefaulted_GetRef();
	NewTagStack.Tag = Tag;
	NewTagStack.StackCount = SafeCount;
	if (Tag == ESRResourceProcessTag::HalfLife)
	{
		NewTagStack.RemainingCycles = StarRovers::FacilityResources::HalfLifeDefaultCycles;
	}
}
