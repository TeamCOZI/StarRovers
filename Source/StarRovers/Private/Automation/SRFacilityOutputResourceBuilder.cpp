#include "SRFacilityOutputResourceBuilder.h"

#include "Automation/SRFacilityDataAsset.h"
#include "SRFacilityResourceOperations.h"

namespace
{
	constexpr double HeatResponsiveEnergyBonus = 5.0;
	constexpr double VolatileEnergyPenalty = 5.0;

	bool IsEnergyResource(const FSRResourceInstance& ResourceInstance)
	{
		return ResourceInstance.ResourceKind == ESRResourceKind::Energy;
	}

	bool IsCatalystResource(const FSRResourceInstance& ResourceInstance)
	{
		return ResourceInstance.ResourceKind == ESRResourceKind::Catalyst;
	}

	bool HasTag(const TArray<FSRResourceTagStack>& Tags, ESRResourceProcessTag Tag)
	{
		return Tags.ContainsByPredicate([Tag](const FSRResourceTagStack& TagStack)
		{
			return TagStack.Tag == Tag && TagStack.StackCount > 0;
		});
	}

	bool HasTag(const FSRResourceInstance& ResourceInstance, ESRResourceProcessTag Tag)
	{
		return HasTag(ResourceInstance.Tags, Tag);
	}

	bool CanEnergyResourceEnterFacility(const FSRResourceInstance& ResourceInstance)
	{
		return IsEnergyResource(ResourceInstance)
			&& ResourceInstance.RemainingProcessLimit > 0
			&& !HasTag(ResourceInstance, ESRResourceProcessTag::Singularity);
	}

	void MergeTagStacks(TArray<FSRResourceTagStack>& InOutTags, const TArray<FSRResourceTagStack>& TagsToMerge)
	{
		for (const FSRResourceTagStack& TagToMerge : TagsToMerge)
		{
			if (TagToMerge.StackCount <= 0)
			{
				continue;
			}

			FSRResourceTagStack* ExistingTag = InOutTags.FindByPredicate([&TagToMerge](const FSRResourceTagStack& ExistingTagStack)
			{
				return ExistingTagStack.Tag == TagToMerge.Tag;
			});
			if (ExistingTag)
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

	void CountInputResourceKinds(const TArray<FSRResourceInstance>& InputResources, int32& OutEnergyCount, int32& OutCatalystCount)
	{
		OutEnergyCount = 0;
		OutCatalystCount = 0;
		for (const FSRResourceInstance& ResourceInstance : InputResources)
		{
			if (IsEnergyResource(ResourceInstance))
			{
				++OutEnergyCount;
			}
			else if (IsCatalystResource(ResourceInstance))
			{
				++OutCatalystCount;
			}
		}
	}

	void ConsumeEnergyForFacilityPass(FSRResourceInstance& ResourceInstance, ESRFacilityTemperatureState TemperatureState)
	{
		if (!IsEnergyResource(ResourceInstance))
		{
			return;
		}

		ResourceInstance.RemainingProcessLimit = FMath::Max(0, ResourceInstance.RemainingProcessLimit - 1);
		++ResourceInstance.ProcessCount;
		if (TemperatureState == ESRFacilityTemperatureState::Hot)
		{
			ResourceInstance.RemainingProcessLimit = FMath::Max(0, ResourceInstance.RemainingProcessLimit - 1);
		}
	}

	void ConsumeEnergyInputsForFacilityPass(TArray<FSRResourceInstance>& InOutResources, ESRFacilityTemperatureState TemperatureState)
	{
		for (FSRResourceInstance& ResourceInstance : InOutResources)
		{
			ConsumeEnergyForFacilityPass(ResourceInstance, TemperatureState);
		}
	}

	const FSRResourceInstance* FindFirstEnergyResource(const TArray<FSRResourceInstance>& ResourceInstances)
	{
		return ResourceInstances.FindByPredicate([](const FSRResourceInstance& ResourceInstance)
		{
			return IsEnergyResource(ResourceInstance);
		});
	}

	bool FindSynthesisInputs(
		const TArray<FSRResourceInstance>& ResourceInstances,
		const FSRResourceInstance*& OutFirstEnergy,
		const FSRResourceInstance*& OutSecondEnergy,
		const FSRResourceInstance*& OutCatalyst)
	{
		OutFirstEnergy = nullptr;
		OutSecondEnergy = nullptr;
		OutCatalyst = nullptr;

		for (const FSRResourceInstance& ResourceInstance : ResourceInstances)
		{
			if (IsEnergyResource(ResourceInstance))
			{
				if (!OutFirstEnergy)
				{
					OutFirstEnergy = &ResourceInstance;
				}
				else if (!OutSecondEnergy)
				{
					OutSecondEnergy = &ResourceInstance;
				}
			}
			else if (IsCatalystResource(ResourceInstance) && !OutCatalyst)
			{
				OutCatalyst = &ResourceInstance;
			}
		}

		return OutFirstEnergy && OutSecondEnergy && OutCatalyst;
	}

	double ApplyCatalystOperator(double FirstEnergyValue, double SecondEnergyValue, ESRResourceCatalystOperator CatalystOperator)
	{
		switch (CatalystOperator)
		{
		case ESRResourceCatalystOperator::Multiply:
			return FirstEnergyValue * SecondEnergyValue;
		case ESRResourceCatalystOperator::Subtract:
			return FirstEnergyValue - SecondEnergyValue;
		case ESRResourceCatalystOperator::Divide:
			return FMath::Abs(SecondEnergyValue) > UE_DOUBLE_SMALL_NUMBER
				? FirstEnergyValue / SecondEnergyValue
				: FirstEnergyValue;
		case ESRResourceCatalystOperator::Add:
		case ESRResourceCatalystOperator::None:
		default:
			return FirstEnergyValue + SecondEnergyValue;
		}
	}

	FSRResourceInstance BuildDefaultOutputResource(
		const USRFacilityDataAsset* FacilityDataAsset,
		const FSRResourceInstance& FallbackResource)
	{
		USRResourceDataAsset* OutputResourceDataAsset = IsValid(FacilityDataAsset)
			? FacilityDataAsset->DefaultOutputResource.Get()
			: nullptr;
		return IsValid(OutputResourceDataAsset)
			? OutputResourceDataAsset->BuildDefaultInstance()
			: FallbackResource;
	}

	void ApplyExistingTagEffects(
		ESRFacilityTemperatureState TemperatureState,
		const TArray<FSRResourceTagStack>& TagsBeforeFacilityEffects,
		FSRResourceInstance& ResourceInstance)
	{
		if (!IsEnergyResource(ResourceInstance))
		{
			return;
		}

		if (TemperatureState == ESRFacilityTemperatureState::Hot
			&& HasTag(TagsBeforeFacilityEffects, ESRResourceProcessTag::Responsive)
			&& HasTag(ResourceInstance, ESRResourceProcessTag::Responsive))
		{
			ResourceInstance.EnergyValue += HeatResponsiveEnergyBonus;
		}

		if (HasTag(TagsBeforeFacilityEffects, ESRResourceProcessTag::Volatile)
			&& HasTag(ResourceInstance, ESRResourceProcessTag::Volatile))
		{
			ResourceInstance.EnergyValue -= VolatileEnergyPenalty;
		}
	}
}

bool FSRFacilityOutputResourceBuilder::DoesInputSetMatchOperation(
	const USRFacilityDataAsset* FacilityDataAsset,
	const TArray<FSRResourceInstance>& InputResources)
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

	for (const FSRResourceInstance& ResourceInstance : InputResources)
	{
		if (IsEnergyResource(ResourceInstance) && !CanEnergyResourceEnterFacility(ResourceInstance))
		{
			return false;
		}
	}

	int32 EnergyCount = 0;
	int32 CatalystCount = 0;
	CountInputResourceKinds(InputResources, EnergyCount, CatalystCount);

	switch (FacilityDataAsset->OperationKind)
	{
	case ESRFacilityOperationKind::Process:
		return EnergyCount >= 1 && CatalystCount == 0;
	case ESRFacilityOperationKind::Synthesize:
		return EnergyCount == 2 && CatalystCount == 1;
	case ESRFacilityOperationKind::Split:
		return EnergyCount == 1 && CatalystCount == 0;
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
	const TArray<FSRResourceTagStack> TagsBeforeFacilityEffects = OutputResource.Tags;
	ApplyFacilityEffects(FacilityInstance.FacilityDataAsset.Get(), OutputResource, AdditionalOutputs);
	ApplyExistingTagEffects(FacilityInstance.TemperatureState, TagsBeforeFacilityEffects, OutputResource);

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

	if (FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Process)
	{
		if (const FSRResourceInstance* EnergyResource = FindFirstEnergyResource(ProcessedResources))
		{
			return *EnergyResource;
		}
		return ProcessedResources[0];
	}

	if (FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Split)
	{
		const int32 PrimaryOutputCount = FMath::Max(1, ResolvePrimaryOutputCount(FacilityInstance));
		if (const FSRResourceInstance* EnergyResource = FindFirstEnergyResource(ProcessedResources))
		{
			FSRResourceInstance OutputResource = *EnergyResource;
			OutputResource.EnergyValue = EnergyResource->EnergyValue / static_cast<double>(PrimaryOutputCount);
			return OutputResource;
		}
		return ProcessedResources[0];
	}

	const FSRResourceInstance* FirstEnergy = nullptr;
	const FSRResourceInstance* SecondEnergy = nullptr;
	const FSRResourceInstance* Catalyst = nullptr;
	if (!FindSynthesisInputs(ProcessedResources, FirstEnergy, SecondEnergy, Catalyst))
	{
		return ProcessedResources[0];
	}

	FSRResourceInstance OutputResource = BuildDefaultOutputResource(FacilityDataAsset, *FirstEnergy);
	OutputResource.ResourceKind = ESRResourceKind::Energy;
	OutputResource.EnergyValue = ApplyCatalystOperator(
		FirstEnergy->EnergyValue,
		SecondEnergy->EnergyValue,
		Catalyst->CatalystOperator);
	OutputResource.CatalystOperator = ESRResourceCatalystOperator::None;
	OutputResource.RemainingProcessLimit = FMath::Max(0, FirstEnergy->RemainingProcessLimit + SecondEnergy->RemainingProcessLimit);
	OutputResource.ProcessCount = FirstEnergy->ProcessCount + SecondEnergy->ProcessCount;
	OutputResource.Tags.Reset();
	MergeTagStacks(OutputResource.Tags, FirstEnergy->Tags);
	MergeTagStacks(OutputResource.Tags, SecondEnergy->Tags);
	return OutputResource;
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
			ResourceInstance.RemainingProcessLimit = FMath::Max(0, ResourceInstance.RemainingProcessLimit + FMath::RoundToInt(EffectSpec.Value));
			break;
		case ESRFacilityEffectKind::SubtractProcessLimit:
			ResourceInstance.RemainingProcessLimit = FMath::Max(0, ResourceInstance.RemainingProcessLimit - FMath::RoundToInt(EffectSpec.Value));
			break;
		case ESRFacilityEffectKind::MultiplyProcessLimit:
			ResourceInstance.RemainingProcessLimit = FMath::Max(0, FMath::RoundToInt(static_cast<double>(ResourceInstance.RemainingProcessLimit) * EffectSpec.Value));
			break;
		case ESRFacilityEffectKind::DivideProcessLimit:
			if (FMath::Abs(EffectSpec.Value) > UE_DOUBLE_SMALL_NUMBER)
			{
				ResourceInstance.RemainingProcessLimit = FMath::Max(0, FMath::RoundToInt(static_cast<double>(ResourceInstance.RemainingProcessLimit) / EffectSpec.Value));
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
