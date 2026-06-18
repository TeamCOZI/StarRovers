#include "Automation/SRFacilityNetworkComponent.h"

bool USRFacilityNetworkComponent::SetFacilityTemperatureState(FName OccupantId, ESRFacilityTemperatureState TemperatureState)
{
	FSRFacilityInstance* FacilityInstance = FacilityInstancesByOccupantId.Find(OccupantId);
	if (!FacilityInstance)
	{
		if (bLogFacilityNetworkEvents)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[FacilityNetwork] SetTemperature failed: OccupantId=%s Owner=%s Reason=MissingFacility"),
				*OccupantId.ToString(),
				*GetNameSafe(GetOwner()));
		}
		return false;
	}

	FacilityInstance->TemperatureState = TemperatureState;
	if (bLogFacilityNetworkEvents)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[FacilityNetwork] Temperature set: OccupantId=%s TemperatureState=%d Owner=%s"),
			*OccupantId.ToString(),
			static_cast<int32>(TemperatureState),
			*GetNameSafe(GetOwner()));
	}
	return true;
}

int32 USRFacilityNetworkComponent::ProcessFacilities(float DeltaTime)
{
	if (FacilityInstancesByOccupantId.IsEmpty())
	{
		return 0;
	}

	int32 ProcessedCount = 0;
	for (TPair<FName, FSRFacilityInstance>& FacilityPair : FacilityInstancesByOccupantId)
	{
		if (ProcessedCount >= MaxFacilitiesProcessedPerTick)
		{
			break;
		}

		FSRFacilityInstance& FacilityInstance = FacilityPair.Value;
		if (!FacilityInstance.bProcessing)
		{
			TryStartProcessing(FacilityInstance);
		}

		if (FacilityInstance.bProcessing)
		{
			FacilityInstance.ProcessProgressSeconds += FMath::Max(0.0f, DeltaTime);
			if (FacilityInstance.ProcessProgressSeconds >= ResolveProcessSeconds(FacilityInstance))
			{
				TryCompleteProcessing(FacilityInstance);
			}
			++ProcessedCount;
		}
	}

	return ProcessedCount;
}

bool USRFacilityNetworkComponent::CanFacilityRun(const FSRFacilityInstance& FacilityInstance) const
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset))
	{
		return false;
	}

	if (FacilityInstance.TemperatureState == ESRFacilityTemperatureState::Frozen
		|| FacilityInstance.TemperatureState == ESRFacilityTemperatureState::Overheated)
	{
		return false;
	}

	if (FacilityDataAsset->bRequiresColdTemperature && FacilityInstance.TemperatureState != ESRFacilityTemperatureState::Cold)
	{
		return false;
	}

	if (FacilityDataAsset->bRequiresHotTemperature && FacilityInstance.TemperatureState != ESRFacilityTemperatureState::Hot)
	{
		return false;
	}

	const int32 InputCount = FMath::Max(1, FacilityDataAsset->InputResourceCount);
	const int32 OutputCapacity = FMath::Max(1, FacilityDataAsset->OutputCapacity);
	if (FacilityInstance.InputInventory.Num() < InputCount)
	{
		return false;
	}

	int32 ExpiredInputOutputSlots = 0;
	int32 ProcessableInputCount = 0;
	for (int32 InputIndex = 0; InputIndex < InputCount; ++InputIndex)
	{
		if (FacilityInstance.InputInventory[InputIndex].RemainingProcessLimit <= 0)
		{
			++ExpiredInputOutputSlots;
		}
		else
		{
			++ProcessableInputCount;
		}
	}

	const int32 RequiredOutputSlots = ExpiredInputOutputSlots
		+ (ProcessableInputCount > 0 ? ResolveRequiredOutputSlots(FacilityDataAsset) : 0);
	return FacilityInstance.OutputInventory.Num() + RequiredOutputSlots <= OutputCapacity;
}

int32 USRFacilityNetworkComponent::ResolveRequiredOutputSlots(const USRFacilityDataAsset* FacilityDataAsset) const
{
	if (!IsValid(FacilityDataAsset))
	{
		return 1;
	}

	int32 RequiredOutputSlots = FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Split
		? FMath::Max(2, FacilityDataAsset->SplitOutputCount)
		: 1;
	for (const FSRFacilityEffectSpec& EffectSpec : FacilityDataAsset->Effects)
	{
		if (EffectSpec.EffectKind == ESRFacilityEffectKind::ProduceResource && IsValid(EffectSpec.ProducedResource.Get()))
		{
			RequiredOutputSlots += FMath::Max(1, EffectSpec.Count);
		}
	}
	return FMath::Max(1, RequiredOutputSlots);
}

float USRFacilityNetworkComponent::ResolveProcessSeconds(const FSRFacilityInstance& FacilityInstance) const
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	float ProcessSeconds = IsValid(FacilityDataAsset) ? FMath::Max(0.01f, FacilityDataAsset->BaseProcessSeconds) : 1.0f;
	if (FacilityInstance.TemperatureState == ESRFacilityTemperatureState::Cold)
	{
		ProcessSeconds *= 2.0f;
	}
	return ProcessSeconds;
}

bool USRFacilityNetworkComponent::TryStartProcessing(FSRFacilityInstance& FacilityInstance)
{
	if (!CanFacilityRun(FacilityInstance))
	{
		return false;
	}

	const int32 InputCount = FMath::Max(1, FacilityInstance.FacilityDataAsset->InputResourceCount);
	FacilityInstance.ProcessingInventory.Reset();
	for (int32 InputIndex = 0; InputIndex < InputCount; ++InputIndex)
	{
		FSRResourceInstance ResourceInstance = FacilityInstance.InputInventory[0];
		FacilityInstance.InputInventory.RemoveAt(0);

		if (ResourceInstance.RemainingProcessLimit <= 0)
		{
			FacilityInstance.OutputInventory.Add(ResourceInstance);
			continue;
		}

		--ResourceInstance.RemainingProcessLimit;
		++ResourceInstance.ProcessCount;
		if (FacilityInstance.TemperatureState == ESRFacilityTemperatureState::Hot)
		{
			ResourceInstance.RemainingProcessLimit = FMath::Max(0, ResourceInstance.RemainingProcessLimit - 1);
		}
		FacilityInstance.ProcessingInventory.Add(ResourceInstance);
	}

	if (FacilityInstance.ProcessingInventory.IsEmpty())
	{
		FacilityInstance.bProcessing = false;
		FacilityInstance.ProcessProgressSeconds = 0.0f;
		return false;
	}

	FacilityInstance.bProcessing = true;
	FacilityInstance.ProcessProgressSeconds = 0.0f;
	if (bLogFacilityNetworkEvents)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[FacilityNetwork] Processing started: OccupantId=%s Facility=%s ProcessingInputs=%d RemainingInputs=%d Owner=%s"),
			*FacilityInstance.OccupantId.ToString(),
			*GetNameSafe(FacilityInstance.FacilityDataAsset.Get()),
			FacilityInstance.ProcessingInventory.Num(),
			FacilityInstance.InputInventory.Num(),
			*GetNameSafe(GetOwner()));
	}
	return true;
}

bool USRFacilityNetworkComponent::TryCompleteProcessing(FSRFacilityInstance& FacilityInstance)
{
	if (!IsValid(FacilityInstance.FacilityDataAsset.Get()) || FacilityInstance.ProcessingInventory.IsEmpty())
	{
		FacilityInstance.ProcessingInventory.Reset();
		FacilityInstance.bProcessing = false;
		FacilityInstance.ProcessProgressSeconds = 0.0f;
		return false;
	}

	TArray<FSRResourceInstance> ConsumedResources = FacilityInstance.ProcessingInventory;
	FSRResourceInstance OutputResource = BuildBaseOutputResource(FacilityInstance, ConsumedResources);
	TArray<FSRResourceInstance> AdditionalOutputs;
	ApplyFacilityEffects(FacilityInstance.FacilityDataAsset.Get(), OutputResource, AdditionalOutputs);

	const int32 OutputCount = FacilityInstance.FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Split
		? FMath::Max(2, FacilityInstance.FacilityDataAsset->SplitOutputCount)
		: 1;
	const int32 OutputCapacity = FMath::Max(1, FacilityInstance.FacilityDataAsset->OutputCapacity);
	if (FacilityInstance.OutputInventory.Num() + OutputCount + AdditionalOutputs.Num() > OutputCapacity)
	{
		FacilityInstance.ProcessProgressSeconds = ResolveProcessSeconds(FacilityInstance);
		return false;
	}

	for (int32 OutputIndex = 0; OutputIndex < OutputCount; ++OutputIndex)
	{
		FacilityInstance.OutputInventory.Add(OutputResource);
	}
	FacilityInstance.OutputInventory.Append(AdditionalOutputs);
	FacilityInstance.ProcessingInventory.Reset();
	FacilityInstance.bProcessing = false;
	FacilityInstance.ProcessProgressSeconds = 0.0f;
	if (bLogFacilityNetworkEvents)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[FacilityNetwork] Processing completed: OccupantId=%s Facility=%s OutputResourceId=%s OutputCount=%d AdditionalOutputs=%d Owner=%s"),
			*FacilityInstance.OccupantId.ToString(),
			*GetNameSafe(FacilityInstance.FacilityDataAsset.Get()),
			*OutputResource.ResourceId.ToString(),
			OutputCount,
			AdditionalOutputs.Num(),
			*GetNameSafe(GetOwner()));
	}
	return true;
}

FSRResourceInstance USRFacilityNetworkComponent::BuildBaseOutputResource(const FSRFacilityInstance& FacilityInstance, const TArray<FSRResourceInstance>& ConsumedResources) const
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	USRResourceDataAsset* OutputResourceDataAsset = IsValid(FacilityDataAsset) ? FacilityDataAsset->DefaultOutputResource.Get() : nullptr;
	if (!IsValid(OutputResourceDataAsset) && !ConsumedResources.IsEmpty())
	{
		OutputResourceDataAsset = ConsumedResources[0].ResourceDataAsset.Get();
	}

	FSRResourceInstance OutputResource = IsValid(OutputResourceDataAsset)
		? OutputResourceDataAsset->BuildDefaultInstance()
		: (!ConsumedResources.IsEmpty() ? ConsumedResources[0] : FSRResourceInstance());

	if (!IsValid(FacilityDataAsset) || ConsumedResources.IsEmpty())
	{
		return OutputResource;
	}

	if (FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Synthesize)
	{
		double EnergySum = 0.0;
		int32 RemainingProcessLimitSum = 0;
		TArray<FSRResourceTagStack> MergedTags;
		for (const FSRResourceInstance& ResourceInstance : ConsumedResources)
		{
			EnergySum += ResourceInstance.EnergyValue;
			RemainingProcessLimitSum += ResourceInstance.RemainingProcessLimit;
			MergedTags.Append(ResourceInstance.Tags);
		}
		OutputResource.EnergyValue = EnergySum;
		OutputResource.RemainingProcessLimit = RemainingProcessLimitSum;
		OutputResource.Tags = MergedTags;
	}
	else if (FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Split)
	{
		const int32 SplitOutputCount = FMath::Max(2, FacilityDataAsset->SplitOutputCount);
		OutputResource.EnergyValue = ConsumedResources[0].EnergyValue / static_cast<double>(SplitOutputCount);
		OutputResource.Tags = ConsumedResources[0].Tags;
	}
	else
	{
		OutputResource = ConsumedResources[0];
	}

	return OutputResource;
}

void USRFacilityNetworkComponent::ApplyFacilityEffects(const USRFacilityDataAsset* FacilityDataAsset, FSRResourceInstance& ResourceInstance, TArray<FSRResourceInstance>& OutAdditionalOutputs) const
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
		case ESRFacilityEffectKind::AddProcessLimit:
			ResourceInstance.RemainingProcessLimit = FMath::Max(0, ResourceInstance.RemainingProcessLimit + FMath::RoundToInt(EffectSpec.Value));
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

void USRFacilityNetworkComponent::AddTagStack(FSRResourceInstance& ResourceInstance, ESRResourceProcessTag Tag, int32 Count) const
{
	const int32 SafeCount = FMath::Max(1, Count);
	for (FSRResourceTagStack& TagStack : ResourceInstance.Tags)
	{
		if (TagStack.Tag == Tag)
		{
			TagStack.StackCount += SafeCount;
			return;
		}
	}

	FSRResourceTagStack& NewTagStack = ResourceInstance.Tags.AddDefaulted_GetRef();
	NewTagStack.Tag = Tag;
	NewTagStack.StackCount = SafeCount;
}

void USRFacilityNetworkComponent::RemoveTagStack(FSRResourceInstance& ResourceInstance, ESRResourceProcessTag Tag, int32 Count) const
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
