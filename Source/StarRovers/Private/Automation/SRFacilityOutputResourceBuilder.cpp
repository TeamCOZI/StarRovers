#include "SRFacilityOutputResourceBuilder.h"

#include "Automation/SRFacilityDataAsset.h"
#include "SRFacilityResourceOperations.h"

namespace
{
	constexpr double HeatResponsiveBaseEnergyBonus = 1.0;
	constexpr double HeatResponsiveHotEnergyBonus = 1.0;
	constexpr double SupercooledEnergyBonus = 3.0;
	constexpr double VolatileEnergyPenalty = 1.0;
	constexpr double HighActivityEnergyBonusPerProcessLimitLoss = 1.0;
	const FName CompoundResourceId(TEXT("Compound"));

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

	void SetProcessLimitAndApplyHighActivity(FSRResourceInstance& ResourceInstance, int32 NewProcessLimit)
	{
		const int32 PreviousProcessLimit = FMath::Max(0, ResourceInstance.RemainingProcessLimit);
		const int32 ClampedProcessLimit = FMath::Max(0, NewProcessLimit);
		ResourceInstance.RemainingProcessLimit = ClampedProcessLimit;

		const int32 LostProcessLimit = FMath::Max(0, PreviousProcessLimit - ClampedProcessLimit);
		const int32 HighActivityStackCount = CountTagStacks(ResourceInstance, ESRResourceProcessTag::HighActivity);
		if (LostProcessLimit > 0 && HighActivityStackCount > 0)
		{
			ResourceInstance.EnergyValue += static_cast<double>(LostProcessLimit * HighActivityStackCount) * HighActivityEnergyBonusPerProcessLimitLoss;
		}
	}

	void ConsumeEnergyForFacilityPass(FSRResourceInstance& ResourceInstance, ESRFacilityTemperatureState TemperatureState)
	{
		SetProcessLimitAndApplyHighActivity(ResourceInstance, ResourceInstance.RemainingProcessLimit - 1);
		++ResourceInstance.ProcessCount;
		if (TemperatureState == ESRFacilityTemperatureState::Hot)
		{
			SetProcessLimitAndApplyHighActivity(ResourceInstance, ResourceInstance.RemainingProcessLimit - 1);
		}
	}

	void ConsumeEnergyInputsForFacilityPass(TArray<FSRResourceInstance>& InOutResources, ESRFacilityTemperatureState TemperatureState)
	{
		for (FSRResourceInstance& ResourceInstance : InOutResources)
		{
			ConsumeEnergyForFacilityPass(ResourceInstance, TemperatureState);
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

	float ResolveFacilityProcessSeconds(const FSRFacilityInstance& FacilityInstance)
	{
		const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
		float ProcessSeconds = IsValid(FacilityDataAsset)
			? FMath::Max(0.01f, FacilityDataAsset->BaseProcessSeconds)
			: 1.0f;
		if (FacilityInstance.TemperatureState == ESRFacilityTemperatureState::Cold)
		{
			ProcessSeconds *= 2.0f;
		}
		return ProcessSeconds;
	}

	int32 ResolveChargeStacksToAddForProcessingPass(const FSRFacilityInstance& FacilityInstance)
	{
		return FMath::Max(0, FMath::FloorToInt(ResolveFacilityProcessSeconds(FacilityInstance)))
			* StarRovers::FacilityResources::ChargeStacksPerProcessingSecond;
	}

	void ApplyHalfLifeProcessingEffect(FSRResourceInstance& ResourceInstance)
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

			ResourceInstance.EnergyValue *= 0.5;
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

	void ApplyChargeProcessingEffect(FSRResourceInstance& ResourceInstance, int32 ChargeStacksToAdd)
	{
		if (ConsumeChargeStacks(ResourceInstance, StarRovers::FacilityResources::ChargeRequiredStacks))
		{
			ResourceInstance.EnergyValue += StarRovers::FacilityResources::ChargeEnergyBonus;
		}
		AddChargeStacks(ResourceInstance, ChargeStacksToAdd);
	}

	void ApplyTagEffects(
		ESRFacilityTemperatureState TemperatureState,
		int32 ChargeStacksToAdd,
		FSRResourceInstance& ResourceInstance)
	{
		const int32 ResponsiveStackCount = CountTagStacks(ResourceInstance, ESRResourceProcessTag::Responsive);
		if (ResponsiveStackCount > 0)
		{
			ResourceInstance.EnergyValue += static_cast<double>(ResponsiveStackCount) * HeatResponsiveBaseEnergyBonus;
			if (TemperatureState == ESRFacilityTemperatureState::Hot)
			{
				ResourceInstance.EnergyValue += static_cast<double>(ResponsiveStackCount) * HeatResponsiveHotEnergyBonus;
			}
		}

		const int32 SupercooledStackCount = CountTagStacks(ResourceInstance, ESRResourceProcessTag::Supercooled);
		if (SupercooledStackCount > 0 && TemperatureState == ESRFacilityTemperatureState::Cold)
		{
			ResourceInstance.EnergyValue += static_cast<double>(SupercooledStackCount) * SupercooledEnergyBonus;
		}

		const int32 VolatileStackCount = CountTagStacks(ResourceInstance, ESRResourceProcessTag::Volatile);
		if (VolatileStackCount > 0)
		{
			ResourceInstance.EnergyValue -= static_cast<double>(VolatileStackCount) * VolatileEnergyPenalty;
		}

		ApplyChargeProcessingEffect(ResourceInstance, ChargeStacksToAdd);
		ApplyHalfLifeProcessingEffect(ResourceInstance);
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

	int32 EnergyCount = 0;
	if (!TryCountInputEnergyResources(InputResources, TemperatureState, EnergyCount))
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
	if (!IsValid(FacilityDataAsset))
	{
		return 0;
	}

	int32 ProducedOutputCount = 0;
	for (const FSRFacilityEffectSpec& EffectSpec : FacilityDataAsset->Effects)
	{
		if (EffectSpec.EffectKind == ESRFacilityEffectKind::ProduceResource && IsValid(EffectSpec.ProducedResource.Get()))
		{
			ProducedOutputCount += FMath::Max(1, EffectSpec.Count);
		}
	}
	return ProducedOutputCount;
}

int32 FSRFacilityOutputResourceBuilder::ResolvePrimaryOutputCount(const FSRFacilityInstance& FacilityInstance)
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset))
	{
		return 0;
	}

	const int32 OutputPortCount = FacilityInstance.OutputPortInventories.Num();
	const int32 ProducedOutputCount = CountProducedOutputResources(FacilityDataAsset);
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

	return ResolvePrimaryOutputCount(FacilityInstance) + CountProducedOutputResources(FacilityDataAsset);
}

void FSRFacilityOutputResourceBuilder::BuildOutputResources(
	const FSRFacilityInstance& FacilityInstance,
	const TArray<FSRResourceInstance>& InputResources,
	TArray<FSRResourceInstance>& OutOutputResources)
{
	OutOutputResources.Reset();
	if (!IsValid(FacilityInstance.FacilityDataAsset.Get()) || InputResources.IsEmpty())
	{
		return;
	}

	FSRResourceInstance OutputResource = BuildBaseOutputResource(FacilityInstance, InputResources);
	TArray<FSRResourceInstance> AdditionalOutputs;
	AdditionalOutputs.Reserve(CountProducedOutputResources(FacilityInstance.FacilityDataAsset.Get()));
	ApplyFacilityEffects(FacilityInstance.FacilityDataAsset.Get(), OutputResource, AdditionalOutputs);

	const int32 OutputCount = ResolvePrimaryOutputCount(FacilityInstance);
	OutOutputResources.Reserve(OutputCount + AdditionalOutputs.Num());
	for (int32 OutputIndex = 0; OutputIndex < OutputCount; ++OutputIndex)
	{
		OutOutputResources.Add(OutputResource);
	}
	OutOutputResources.Append(AdditionalOutputs);
}

FSRResourceInstance FSRFacilityOutputResourceBuilder::BuildBaseOutputResource(
	const FSRFacilityInstance& FacilityInstance,
	const TArray<FSRResourceInstance>& ConsumedResources)
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset) || ConsumedResources.IsEmpty())
	{
		return !ConsumedResources.IsEmpty() ? ConsumedResources[0] : FSRResourceInstance();
	}

	TArray<FSRResourceInstance> ProcessedResources = ConsumedResources;
	ConsumeEnergyInputsForFacilityPass(ProcessedResources, FacilityInstance.TemperatureState);
	const int32 ChargeStacksToAdd = ResolveChargeStacksToAddForProcessingPass(FacilityInstance);
	for (FSRResourceInstance& ProcessedResource : ProcessedResources)
	{
		ApplyTagEffects(FacilityInstance.TemperatureState, ChargeStacksToAdd, ProcessedResource);
	}

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
	const USRFacilityDataAsset* FacilityDataAsset,
	FSRResourceInstance& ResourceInstance,
	TArray<FSRResourceInstance>& OutAdditionalOutputs)
{
	if (!IsValid(FacilityDataAsset))
	{
		return;
	}

	for (const FSRFacilityEffectSpec& EffectSpec : FacilityDataAsset->Effects)
	{
		const int32 Count = FMath::Max(1, EffectSpec.Count);
		switch (EffectSpec.EffectKind)
		{
		case ESRFacilityEffectKind::AddEnergy:
			ResourceInstance.EnergyValue += EffectSpec.Value;
			break;
		case ESRFacilityEffectKind::MultiplyEnergy:
			ResourceInstance.EnergyValue *= EffectSpec.Value;
			break;
		case ESRFacilityEffectKind::SubtractEnergy:
			ResourceInstance.EnergyValue -= EffectSpec.Value;
			break;
		case ESRFacilityEffectKind::DivideEnergy:
			if (FMath::Abs(EffectSpec.Value) > UE_DOUBLE_SMALL_NUMBER)
			{
				ResourceInstance.EnergyValue /= EffectSpec.Value;
			}
			break;
		case ESRFacilityEffectKind::AddProcessLimit:
			SetProcessLimitAndApplyHighActivity(ResourceInstance, ResourceInstance.RemainingProcessLimit + FMath::RoundToInt(EffectSpec.Value));
			break;
		case ESRFacilityEffectKind::SubtractProcessLimit:
			SetProcessLimitAndApplyHighActivity(ResourceInstance, ResourceInstance.RemainingProcessLimit - FMath::RoundToInt(EffectSpec.Value));
			break;
		case ESRFacilityEffectKind::MultiplyProcessLimit:
			SetProcessLimitAndApplyHighActivity(
				ResourceInstance,
				FMath::RoundToInt(static_cast<double>(ResourceInstance.RemainingProcessLimit) * EffectSpec.Value));
			break;
		case ESRFacilityEffectKind::DivideProcessLimit:
			if (FMath::Abs(EffectSpec.Value) > UE_DOUBLE_SMALL_NUMBER)
			{
				SetProcessLimitAndApplyHighActivity(
					ResourceInstance,
					FMath::RoundToInt(static_cast<double>(ResourceInstance.RemainingProcessLimit) / EffectSpec.Value));
			}
			break;
		case ESRFacilityEffectKind::AddTag:
			AddTagStack(ResourceInstance, EffectSpec.ResourceTag, Count);
			break;
		case ESRFacilityEffectKind::RemoveTag:
			RemoveTagStack(ResourceInstance, EffectSpec.ResourceTag, Count);
			break;
		case ESRFacilityEffectKind::ProduceResource:
			if (IsValid(EffectSpec.ProducedResource.Get()))
			{
				for (int32 OutputIndex = 0; OutputIndex < Count; ++OutputIndex)
				{
					OutAdditionalOutputs.Add(EffectSpec.ProducedResource->BuildDefaultInstance());
				}
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

void FSRFacilityOutputResourceBuilder::RemoveTagStack(FSRResourceInstance& ResourceInstance, ESRResourceProcessTag Tag, int32 Count)
{
	int32 RemainingCount = FMath::Max(1, Count);
	for (int32 TagIndex = ResourceInstance.Tags.Num() - 1; TagIndex >= 0 && RemainingCount > 0; --TagIndex)
	{
		FSRResourceTagStack& TagStack = ResourceInstance.Tags[TagIndex];
		if (TagStack.Tag != Tag)
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
