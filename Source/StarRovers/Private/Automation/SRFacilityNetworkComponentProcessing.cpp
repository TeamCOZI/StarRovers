#include "Automation/SRFacilityNetworkComponent.h"

#include "Conveyor/SRConveyorNetworkComponent.h"
#include "SRFacilityNetworkComponentInternal.h"
#include "GameFramework/Actor.h"
#include "Surface/SRPlanetSurfaceGrid.h"

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
					ExistingTag->RemainingCycles = StarRovers::FacilityNetwork::HalfLifeDefaultCycles;
				}
				continue;
			}

			FSRResourceTagStack NewTagStack = TagToMerge;
			if (NewTagStack.Tag == ESRResourceProcessTag::HalfLife && NewTagStack.RemainingCycles <= 0)
			{
				NewTagStack.RemainingCycles = StarRovers::FacilityNetwork::HalfLifeDefaultCycles;
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

	bool DoesInputSetMatchOperation(const USRFacilityDataAsset* FacilityDataAsset, const TArray<FSRResourceInstance>& InputResources)
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

bool USRFacilityNetworkComponent::SetFacilityProcessEnabled(FName OccupantId, bool bEnabled)
{
	FSRFacilityInstance* FacilityInstance = FacilityInstancesByOccupantId.Find(OccupantId);
	if (!FacilityInstance)
	{
		return false;
	}

	FacilityInstance->bProcessEnabled = bEnabled;
	if (bEnabled)
	{
		SetComponentTickEnabled(bAutoProcessFacilities);
	}
	return true;
}

bool USRFacilityNetworkComponent::SetFacilityDeliverEnabled(FName OccupantId, bool bEnabled)
{
	FSRFacilityInstance* FacilityInstance = FacilityInstancesByOccupantId.Find(OccupantId);
	if (!FacilityInstance)
	{
		return false;
	}

	FacilityInstance->bDeliverEnabled = bEnabled;
	if (bEnabled)
	{
		if (AActor* Owner = GetOwner())
		{
			if (USRConveyorNetworkComponent* ConveyorNetwork = Owner->FindComponentByClass<USRConveyorNetworkComponent>())
			{
				ConveyorNetwork->SetComponentTickEnabled(true);
			}
		}
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

		if (FacilityInstance.bProcessing && CanFacilityAdvanceProcessing(FacilityInstance))
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

bool USRFacilityNetworkComponent::CanFacilityAdvanceProcessing(const FSRFacilityInstance& FacilityInstance) const
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset))
	{
		return false;
	}

	if (!FacilityInstance.bProcessEnabled)
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

	return true;
}

bool USRFacilityNetworkComponent::CanFacilityRun(const FSRFacilityInstance& FacilityInstance) const
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!CanFacilityAdvanceProcessing(FacilityInstance) || !IsValid(FacilityDataAsset))
	{
		return false;
	}

	if (FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Mine)
	{
		return CanMiningFacilityRun(FacilityInstance);
	}

	TArray<FSRResourceInstance> InputResources;
	if (!GatherPendingInputResources(FacilityInstance, InputResources))
	{
		return false;
	}

	if (!DoesInputSetMatchOperation(FacilityDataAsset, InputResources))
	{
		return false;
	}

	const int32 RequiredOutputSlots = ResolveRequiredOutputSlots(FacilityInstance);
	return CanStoreOutputResources(FacilityInstance, RequiredOutputSlots);
}

bool USRFacilityNetworkComponent::CanMiningFacilityRun(const FSRFacilityInstance& FacilityInstance) const
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!CanFacilityAdvanceProcessing(FacilityInstance)
		|| !IsValid(FacilityDataAsset)
		|| FacilityDataAsset->OperationKind != ESRFacilityOperationKind::Mine)
	{
		return false;
	}

	FSRResourceDepositInstance ResourceDeposit;
	if (!FindMiningTargetDeposit(FacilityInstance, ResourceDeposit))
	{
		return false;
	}

	return CanStoreOutputResources(FacilityInstance, ResolveRequiredOutputSlots(FacilityInstance));
}

bool USRFacilityNetworkComponent::FindMiningTargetDeposit(
	const FSRFacilityInstance& FacilityInstance,
	FSRResourceDepositInstance& OutResourceDeposit) const
{
	OutResourceDeposit = FSRResourceDepositInstance();

	const AActor* Owner = GetOwner();
	const USRPlanetSurfaceGrid* SurfaceGrid = IsValid(Owner) ? Owner->FindComponentByClass<USRPlanetSurfaceGrid>() : nullptr;
	const USRStructureInstanceManagerComponent* StructureInstanceManager = IsValid(Owner)
		? Owner->FindComponentByClass<USRStructureInstanceManagerComponent>()
		: nullptr;
	if (!IsValid(SurfaceGrid) || !IsValid(StructureInstanceManager))
	{
		return false;
	}

	if (!FacilityInstance.MiningTargetDepositOccupantId.IsNone()
		&& StructureInstanceManager->GetResourceDepositInstance(FacilityInstance.MiningTargetDepositOccupantId, OutResourceDeposit)
		&& OutResourceDeposit.RemainingAmount > 0
		&& IsValid(OutResourceDeposit.ResourceDataAsset.Get()))
	{
		return true;
	}

	return StructureInstanceManager->FindAdjacentResourceDeposit(
		const_cast<USRPlanetSurfaceGrid*>(SurfaceGrid),
		FacilityInstance.FootprintCellIds,
		OutResourceDeposit);
}

int32 USRFacilityNetworkComponent::CountProducedOutputResources(const USRFacilityDataAsset* FacilityDataAsset) const
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

int32 USRFacilityNetworkComponent::ResolvePrimaryOutputCount(const FSRFacilityInstance& FacilityInstance) const
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

int32 USRFacilityNetworkComponent::ResolveRequiredOutputSlots(const FSRFacilityInstance& FacilityInstance) const
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset))
	{
		return 0;
	}

	return ResolvePrimaryOutputCount(FacilityInstance) + CountProducedOutputResources(FacilityDataAsset);
}

bool USRFacilityNetworkComponent::GatherPendingInputResources(const FSRFacilityInstance& FacilityInstance, TArray<FSRResourceInstance>& OutInputResources) const
{
	OutInputResources.Reset();
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset))
	{
		return false;
	}

	const int32 InputCount = FacilityInstance.InputPortInventories.Num();
	if (InputCount <= 0)
	{
		return false;
	}

	OutInputResources.Reserve(InputCount);
	for (int32 InputIndex = 0; InputIndex < InputCount; ++InputIndex)
	{
		const FSRFacilityPortInventory& InputPortInventory = FacilityInstance.InputPortInventories[InputIndex];
		if (InputPortInventory.Inventory.IsEmpty())
		{
			OutInputResources.Reset();
			return false;
		}

		OutInputResources.Add(InputPortInventory.Inventory[0]);
	}

	return !OutInputResources.IsEmpty();
}

bool USRFacilityNetworkComponent::CanStoreOutputResources(const FSRFacilityInstance& FacilityInstance, int32 OutputResourceCount) const
{
	if (OutputResourceCount <= 0)
	{
		return false;
	}

	const int32 SafeOutputResourceCount = OutputResourceCount;
	if (FacilityInstance.OutputPortInventories.Num() < SafeOutputResourceCount)
	{
		return false;
	}

	for (int32 OutputIndex = 0; OutputIndex < SafeOutputResourceCount; ++OutputIndex)
	{
		const FSRFacilityPortInventory& OutputPortInventory = FacilityInstance.OutputPortInventories[OutputIndex];
		if (OutputPortInventory.Inventory.Num() + 1 > FMath::Max(1, OutputPortInventory.Capacity))
		{
			return false;
		}
	}

	return true;
}

void USRFacilityNetworkComponent::StoreOutputResources(FSRFacilityInstance& FacilityInstance, const TArray<FSRResourceInstance>& OutputResources)
{
	for (int32 OutputIndex = 0; OutputIndex < OutputResources.Num() && FacilityInstance.OutputPortInventories.IsValidIndex(OutputIndex); ++OutputIndex)
	{
		FacilityInstance.OutputPortInventories[OutputIndex].Inventory.Add(OutputResources[OutputIndex]);
	}

	RefreshFacilityAggregateInventories(FacilityInstance);
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

	if (const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
		IsValid(FacilityDataAsset) && FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Mine)
	{
		FSRResourceDepositInstance ResourceDeposit;
		if (!FindMiningTargetDeposit(FacilityInstance, ResourceDeposit))
		{
			return false;
		}

		FacilityInstance.MiningTargetDepositOccupantId = ResourceDeposit.OccupantId;
		FacilityInstance.ProcessingInventory.Reset();
		FacilityInstance.bProcessing = true;
		FacilityInstance.ProcessProgressSeconds = 0.0f;
		if (bLogFacilityNetworkEvents)
		{
			UE_LOG(
				LogTemp,
				Display,
				TEXT("[FacilityNetwork] Mining started: OccupantId=%s Facility=%s Deposit=%s ResourceId=%s Remaining=%d/%d Owner=%s"),
				*FacilityInstance.OccupantId.ToString(),
				*GetNameSafe(FacilityInstance.FacilityDataAsset.Get()),
				*ResourceDeposit.OccupantId.ToString(),
				*ResourceDeposit.ResourceId.ToString(),
				ResourceDeposit.RemainingAmount,
				ResourceDeposit.TotalAmount,
				*GetNameSafe(GetOwner()));
		}
		return true;
	}

	const int32 InputCount = FacilityInstance.InputPortInventories.Num();
	if (InputCount <= 0)
	{
		return false;
	}

	FacilityInstance.ProcessingInventory.Reset();
	for (int32 InputIndex = 0; InputIndex < InputCount; ++InputIndex)
	{
		if (!FacilityInstance.InputPortInventories.IsValidIndex(InputIndex)
			|| FacilityInstance.InputPortInventories[InputIndex].Inventory.IsEmpty())
		{
			FacilityInstance.ProcessingInventory.Reset();
			FacilityInstance.bProcessing = false;
			FacilityInstance.ProcessProgressSeconds = 0.0f;
			return false;
		}

		FSRResourceInstance ResourceInstance = FacilityInstance.InputPortInventories[InputIndex].Inventory[0];
		FacilityInstance.InputPortInventories[InputIndex].Inventory.RemoveAt(0);
		FacilityInstance.ProcessingInventory.Add(ResourceInstance);
	}
	RefreshFacilityAggregateInventories(FacilityInstance);

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
	if (const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
		IsValid(FacilityDataAsset) && FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Mine)
	{
		return TryCompleteMining(FacilityInstance);
	}

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
	const TArray<FSRResourceTagStack> TagsBeforeFacilityEffects = OutputResource.Tags;
	ApplyFacilityEffects(FacilityInstance.FacilityDataAsset.Get(), OutputResource, AdditionalOutputs);
	ApplyExistingTagEffects(FacilityInstance.TemperatureState, TagsBeforeFacilityEffects, OutputResource);

	const int32 OutputCount = ResolvePrimaryOutputCount(FacilityInstance);
	TArray<FSRResourceInstance> OutputResources;
	OutputResources.Reserve(OutputCount + AdditionalOutputs.Num());
	for (int32 OutputIndex = 0; OutputIndex < OutputCount; ++OutputIndex)
	{
		OutputResources.Add(OutputResource);
	}
	OutputResources.Append(AdditionalOutputs);
	if (!CanStoreOutputResources(FacilityInstance, OutputResources.Num()))
	{
		FacilityInstance.ProcessProgressSeconds = ResolveProcessSeconds(FacilityInstance);
		return false;
	}

	StoreOutputResources(FacilityInstance, OutputResources);
	const int32 CellTemperatureEffects = ApplyFacilityCellTemperatureEffects(FacilityInstance);
	if (CellTemperatureEffects > 0)
	{
		RefreshFacilityTemperatureFromSurface(FacilityInstance.OccupantId);
	}
	FacilityInstance.ProcessingInventory.Reset();
	FacilityInstance.bProcessing = false;
	FacilityInstance.ProcessProgressSeconds = 0.0f;
	if (bLogFacilityNetworkEvents)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[FacilityNetwork] Processing completed: OccupantId=%s Facility=%s OutputResourceId=%s OutputCount=%d AdditionalOutputs=%d CellTemperatureEffects=%d Owner=%s"),
			*FacilityInstance.OccupantId.ToString(),
			*GetNameSafe(FacilityInstance.FacilityDataAsset.Get()),
			*OutputResource.ResourceId.ToString(),
			OutputCount,
			AdditionalOutputs.Num(),
			CellTemperatureEffects,
			*GetNameSafe(GetOwner()));
	}
	return true;
}

bool USRFacilityNetworkComponent::TryCompleteMining(FSRFacilityInstance& FacilityInstance)
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset) || FacilityDataAsset->OperationKind != ESRFacilityOperationKind::Mine)
	{
		FacilityInstance.MiningTargetDepositOccupantId = NAME_None;
		FacilityInstance.bProcessing = false;
		FacilityInstance.ProcessProgressSeconds = 0.0f;
		return false;
	}

	FSRResourceDepositInstance ResourceDeposit;
	if (!FindMiningTargetDeposit(FacilityInstance, ResourceDeposit))
	{
		FacilityInstance.MiningTargetDepositOccupantId = NAME_None;
		FacilityInstance.bProcessing = false;
		FacilityInstance.ProcessProgressSeconds = 0.0f;
		return false;
	}

	const int32 OutputCount = ResolvePrimaryOutputCount(FacilityInstance);
	if (!CanStoreOutputResources(FacilityInstance, OutputCount))
	{
		FacilityInstance.ProcessProgressSeconds = ResolveProcessSeconds(FacilityInstance);
		return false;
	}

	AActor* Owner = GetOwner();
	USRPlanetSurfaceGrid* SurfaceGrid = IsValid(Owner) ? Owner->FindComponentByClass<USRPlanetSurfaceGrid>() : nullptr;
	USRStructureInstanceManagerComponent* StructureInstanceManager = IsValid(Owner)
		? Owner->FindComponentByClass<USRStructureInstanceManagerComponent>()
		: nullptr;
	if (!IsValid(SurfaceGrid) || !IsValid(StructureInstanceManager))
	{
		FacilityInstance.MiningTargetDepositOccupantId = NAME_None;
		FacilityInstance.bProcessing = false;
		FacilityInstance.ProcessProgressSeconds = 0.0f;
		return false;
	}

	FSRResourceInstance MinedResource;
	FSRResourceDepositInstance UpdatedResourceDeposit;
	if (!StructureInstanceManager->TryHarvestResourceDeposit(
		SurfaceGrid,
		ResourceDeposit.OccupantId,
		MinedResource,
		UpdatedResourceDeposit))
	{
		FacilityInstance.MiningTargetDepositOccupantId = NAME_None;
		FacilityInstance.bProcessing = false;
		FacilityInstance.ProcessProgressSeconds = 0.0f;
		return false;
	}

	TArray<FSRResourceInstance> OutputResources;
	OutputResources.Reserve(OutputCount);
	for (int32 OutputIndex = 0; OutputIndex < OutputCount; ++OutputIndex)
	{
		OutputResources.Add(MinedResource);
	}

	StoreOutputResources(FacilityInstance, OutputResources);
	FacilityInstance.MiningTargetDepositOccupantId = UpdatedResourceDeposit.RemainingAmount > 0
		? UpdatedResourceDeposit.OccupantId
		: NAME_None;
	FacilityInstance.ProcessingInventory.Reset();
	FacilityInstance.bProcessing = false;
	FacilityInstance.ProcessProgressSeconds = 0.0f;
	if (bLogFacilityNetworkEvents)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[FacilityNetwork] Mining completed: OccupantId=%s Facility=%s Deposit=%s ResourceId=%s Remaining=%d/%d Owner=%s"),
			*FacilityInstance.OccupantId.ToString(),
			*GetNameSafe(FacilityDataAsset),
			*ResourceDeposit.OccupantId.ToString(),
			*MinedResource.ResourceId.ToString(),
			UpdatedResourceDeposit.RemainingAmount,
			UpdatedResourceDeposit.TotalAmount,
			*GetNameSafe(GetOwner()));
	}
	return true;
}

bool USRFacilityNetworkComponent::GetFacilityOutputPreview(
	FName OccupantId,
	FSRResourceInstance& OutPrimaryOutput,
	TArray<FSRResourceInstance>& OutAdditionalOutputs,
	int32& OutOutputCount) const
{
	OutPrimaryOutput = FSRResourceInstance();
	OutAdditionalOutputs.Reset();
	OutOutputCount = 0;

	const FSRFacilityInstance* FacilityInstance = FacilityInstancesByOccupantId.Find(OccupantId);
	if (!FacilityInstance || !IsValid(FacilityInstance->FacilityDataAsset.Get()))
	{
		return false;
	}

	if (FacilityInstance->FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Mine)
	{
		FSRResourceDepositInstance ResourceDeposit;
		if (!FindMiningTargetDeposit(*FacilityInstance, ResourceDeposit)
			|| !IsValid(ResourceDeposit.ResourceDataAsset.Get()))
		{
			return false;
		}

		OutPrimaryOutput = ResourceDeposit.ResourceDataAsset->BuildDefaultInstance();
		OutAdditionalOutputs.Reset();
		OutOutputCount = ResolvePrimaryOutputCount(*FacilityInstance);
		return OutOutputCount > 0 && !OutPrimaryOutput.ResourceId.IsNone();
	}

	TArray<FSRResourceInstance> PreviewInputs;
	if (!FacilityInstance->ProcessingInventory.IsEmpty())
	{
		PreviewInputs = FacilityInstance->ProcessingInventory;
	}
	else
	{
		if (!GatherPendingInputResources(*FacilityInstance, PreviewInputs))
		{
			return false;
		}
	}

	if (!DoesInputSetMatchOperation(FacilityInstance->FacilityDataAsset.Get(), PreviewInputs))
	{
		return false;
	}

	OutPrimaryOutput = BuildBaseOutputResource(*FacilityInstance, PreviewInputs);
	const TArray<FSRResourceTagStack> TagsBeforeFacilityEffects = OutPrimaryOutput.Tags;
	ApplyFacilityEffects(FacilityInstance->FacilityDataAsset.Get(), OutPrimaryOutput, OutAdditionalOutputs);
	ApplyExistingTagEffects(FacilityInstance->TemperatureState, TagsBeforeFacilityEffects, OutPrimaryOutput);
	OutOutputCount = ResolvePrimaryOutputCount(*FacilityInstance);
	return true;
}

bool USRFacilityNetworkComponent::GetFacilityMiningTarget(
	FName OccupantId,
	FSRResourceDepositInstance& OutResourceDeposit) const
{
	OutResourceDeposit = FSRResourceDepositInstance();
	const FSRFacilityInstance* FacilityInstance = FacilityInstancesByOccupantId.Find(OccupantId);
	if (!FacilityInstance
		|| !IsValid(FacilityInstance->FacilityDataAsset.Get())
		|| FacilityInstance->FacilityDataAsset->OperationKind != ESRFacilityOperationKind::Mine)
	{
		return false;
	}

	return FindMiningTargetDeposit(*FacilityInstance, OutResourceDeposit);
}

FSRResourceInstance USRFacilityNetworkComponent::BuildBaseOutputResource(const FSRFacilityInstance& FacilityInstance, const TArray<FSRResourceInstance>& ConsumedResources) const
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

int32 USRFacilityNetworkComponent::ApplyFacilityCellTemperatureEffects(const FSRFacilityInstance& FacilityInstance)
{
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	if (!IsValid(FacilityDataAsset))
	{
		return 0;
	}

	AActor* Owner = GetOwner();
	USRPlanetSurfaceGrid* SurfaceGrid = IsValid(Owner) ? Owner->FindComponentByClass<USRPlanetSurfaceGrid>() : nullptr;
	if (!IsValid(SurfaceGrid))
	{
		return 0;
	}

	int32 AppliedEffectCount = 0;
	for (const FSRFacilityEffectSpec& EffectSpec : FacilityDataAsset->Effects)
	{
		double TemperatureDelta = 0.0;
		switch (EffectSpec.EffectKind)
		{
		case ESRFacilityEffectKind::AddCellTemperature:
			TemperatureDelta = EffectSpec.Value;
			break;
		case ESRFacilityEffectKind::SubtractCellTemperature:
			TemperatureDelta = -EffectSpec.Value;
			break;
		default:
			continue;
		}

		FSRPlanetSurfaceGridCellInfo OriginCellInfo;
		if (!SurfaceGrid->GetCellInfoById(FacilityInstance.OriginCellId, OriginCellInfo))
		{
			continue;
		}

		const float NewSurfaceTemperature = FMath::Clamp(
			OriginCellInfo.SurfaceTemperature + static_cast<float>(TemperatureDelta),
			0.0f,
			1.0f);
		if (SurfaceGrid->SetCellSurfaceTemperature(FacilityInstance.OriginCellId, NewSurfaceTemperature))
		{
			++AppliedEffectCount;
		}
	}

	return AppliedEffectCount;
}

void USRFacilityNetworkComponent::AddTagStack(FSRResourceInstance& ResourceInstance, ESRResourceProcessTag Tag, int32 Count) const
{
	const int32 SafeCount = FMath::Max(1, Count);
	for (FSRResourceTagStack& TagStack : ResourceInstance.Tags)
	{
		if (TagStack.Tag == Tag)
		{
			TagStack.StackCount += SafeCount;
			if (Tag == ESRResourceProcessTag::HalfLife && TagStack.RemainingCycles <= 0)
			{
				TagStack.RemainingCycles = StarRovers::FacilityNetwork::HalfLifeDefaultCycles;
			}
			return;
		}
	}

	FSRResourceTagStack& NewTagStack = ResourceInstance.Tags.AddDefaulted_GetRef();
	NewTagStack.Tag = Tag;
	NewTagStack.StackCount = SafeCount;
	if (Tag == ESRResourceProcessTag::HalfLife)
	{
		NewTagStack.RemainingCycles = StarRovers::FacilityNetwork::HalfLifeDefaultCycles;
	}
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
