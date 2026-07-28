#include "Automation/SRFacilityNetworkComponent.h"

#include "Automation/SRResourceSystemContent.h"
#include "SRFacilityResourceOperations.h"
#include "Structure/SRStructureDataAsset.h"
#include "Utility/SRLog.h"

void USRFacilityNetworkComponent::SetFacilityDebugLoggingEnabled(bool bEnabled)
{
	bLogFacilityNetworkEvents = bEnabled;
}

bool USRFacilityNetworkComponent::IsFacilityDebugLoggingEnabled() const
{
	return bLogFacilityNetworkEvents;
}

void USRFacilityNetworkComponent::GetRegisteredFacilityOccupantIds(TArray<FName>& OutOccupantIds) const
{
	OutOccupantIds.Reset();
	RuntimeState.FacilityInstancesByOccupantId.GetKeys(OutOccupantIds);
	OutOccupantIds.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});
}

bool USRFacilityNetworkComponent::DebugAddInputResourceFromDataAsset(
	FName OccupantId,
	USRResourceDataAsset* ResourceDataAsset,
	int32 StackCount)
{
	if (!IsValid(ResourceDataAsset))
	{
		SR_LOG(FacilityNetwork,
			LogTemp,
			Warning,
			TEXT("[FacilityNetwork][Debug] AddInputFromDataAsset failed: OccupantId=%s Owner=%s Reason=InvalidResourceDataAsset"),
			*OccupantId.ToString(),
			*GetNameSafe(GetOwner()));
		return false;
	}

	FSRResourceInstance ResourceInstance = ResourceDataAsset->BuildDefaultInstance();
	ResourceInstance.StackCount = FMath::Max(1, StackCount);
	const bool bAdded = AddInputResource(OccupantId, ResourceInstance);
	if (bAdded)
	{
		SR_LOG(FacilityNetwork,
			LogTemp,
			Display,
			TEXT("[FacilityNetwork][Debug] Added input from DA: OccupantId=%s %s Owner=%s"),
			*OccupantId.ToString(),
			*StarRovers::FacilityResources::BuildResourceDebugString(ResourceInstance),
			*GetNameSafe(GetOwner()));
	}
	return bAdded;
}

bool USRFacilityNetworkComponent::DebugAddRawEnergyInputResource(
	FName OccupantId,
	FName ResourceId,
	double EnergyValue,
	int32 RemainingProcessLimit,
	int32 StackCount)
{
	if (ResourceId.IsNone())
	{
		SR_LOG(FacilityNetwork,
			LogTemp,
			Warning,
			TEXT("[FacilityNetwork][Debug] AddRawEnergyInput failed: OccupantId=%s Owner=%s Reason=InvalidResourceId"),
			*OccupantId.ToString(),
			*GetNameSafe(GetOwner()));
		return false;
	}

	FSRResourceInstance ResourceInstance;
	ResourceInstance.ResourceInstanceId = FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits));
	ResourceInstance.ResourceId = ResourceId;
	ResourceInstance.EnergyValue = EnergyValue;
	ResourceInstance.RemainingProcessLimit = FMath::Max(0, RemainingProcessLimit);
	ResourceInstance.ProcessCount = 0;
	ResourceInstance.EnergyChangeCount = 0;
	ResourceInstance.StackCount = FMath::Max(1, StackCount);
	StarRovers::Resources::SynchronizeLegacyRuntimeStateToResourceV2(ResourceInstance);

	const bool bAdded = AddInputResource(OccupantId, ResourceInstance);
	if (bAdded)
	{
		SR_LOG(FacilityNetwork,
			LogTemp,
			Display,
			TEXT("[FacilityNetwork][Debug] Added raw energy input: OccupantId=%s %s Owner=%s"),
			*OccupantId.ToString(),
			*StarRovers::FacilityResources::BuildResourceDebugString(ResourceInstance),
			*GetNameSafe(GetOwner()));
	}
	return bAdded;
}

bool USRFacilityNetworkComponent::DebugAddResourceV2Card(
	FName OccupantId,
	FName ResourceId,
	ESRResourceFamily Family,
	double CurrentEnergy,
	ESRResourceSpectrum Spectrum,
	int32 Grade,
	int32 StackCount)
{
	if (ResourceId.IsNone()
		|| Family == ESRResourceFamily::None
		|| Spectrum == ESRResourceSpectrum::None
		|| !FMath::IsFinite(CurrentEnergy)
		|| CurrentEnergy < 0.0)
	{
		SR_LOG(FacilityNetwork,
			LogTemp,
			Warning,
			TEXT("[FacilityNetwork][Debug] AddResourceV2Card failed: OccupantId=%s Owner=%s Reason=InvalidCardDefinition"),
			*OccupantId.ToString(),
			*GetNameSafe(GetOwner()));
		return false;
	}

	FSRResourceInstance ResourceInstance;
	ResourceInstance.ResourceSchemaVersion = StarRovers::Resources::CurrentResourceSchemaVersion;
	ResourceInstance.ResourceInstanceId = FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits));
	ResourceInstance.ResourceId = ResourceId;
	ResourceInstance.ResourceClass = ESRResourceClass::Card;
	ResourceInstance.Family = Family;
	ResourceInstance.CurrentEnergy = CurrentEnergy;
	ResourceInstance.Spectrum = Spectrum;
	ResourceInstance.Grade = FMath::Clamp(
		Grade,
		StarRovers::Resources::MinimumGrade,
		StarRovers::Resources::MaximumGrade);
	ResourceInstance.StackCount = FMath::Max(1, StackCount);
	StarRovers::Resources::EnsureResourceSeedEnergySnapshot(ResourceInstance);
	StarRovers::Resources::SynchronizeResourceV2RuntimeStateToLegacy(ResourceInstance);

	const bool bAdded = AddInputResource(OccupantId, ResourceInstance);
	if (bAdded)
	{
		SR_LOG(FacilityNetwork,
			LogTemp,
			Display,
			TEXT("[FacilityNetwork][Debug] Added Resource V2 Card: OccupantId=%s %s Owner=%s"),
			*OccupantId.ToString(),
			*StarRovers::FacilityResources::BuildResourceDebugString(ResourceInstance),
			*GetNameSafe(GetOwner()));
	}
	return bAdded;
}

bool USRFacilityNetworkComponent::DebugAddReferenceResourceV2Card(
	FName OccupantId,
	ESRResourceContentPresetV2 ResourcePreset,
	FName OriginBodyId,
	int32 StackCount)
{
	FSRReferenceResourceDefinitionV2 CardDefinition;
	if (!FSRResourceSystemContent::TryGetReferenceResourceDefinition(ResourcePreset, CardDefinition))
	{
		SR_LOG(FacilityNetwork,
			LogTemp,
			Warning,
			TEXT("[FacilityNetwork][Debug] AddReferenceResourceV2Card failed: OccupantId=%s Owner=%s Reason=PresetIsNotCard"),
			*OccupantId.ToString(),
			*GetNameSafe(GetOwner()));
		return false;
	}

	FSRResourceInstance ResourceInstance;
	if (!FSRResourceSystemContent::MakeReferenceResourceInstance(
		ResourcePreset,
		OriginBodyId,
		ResourceInstance))
	{
		SR_LOG(FacilityNetwork,
			LogTemp,
			Warning,
			TEXT("[FacilityNetwork][Debug] AddReferenceResourceV2Card failed: OccupantId=%s Owner=%s Reason=InvalidPreset"),
			*OccupantId.ToString(),
			*GetNameSafe(GetOwner()));
		return false;
	}

	ResourceInstance.ResourceInstanceId = FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits));
	ResourceInstance.StackCount = FMath::Max(1, StackCount);
	const bool bAdded = AddInputResource(OccupantId, ResourceInstance);
	if (bAdded)
	{
		SR_LOG(FacilityNetwork,
			LogTemp,
			Display,
			TEXT("[FacilityNetwork][Debug] Added reference Resource V2 Card: Preset=%s OccupantId=%s %s Owner=%s"),
			*StaticEnum<ESRResourceContentPresetV2>()->GetNameStringByValue(static_cast<int64>(ResourcePreset)),
			*OccupantId.ToString(),
			*StarRovers::FacilityResources::BuildResourceDebugString(ResourceInstance),
			*GetNameSafe(GetOwner()));
	}
	return bAdded;
}

bool USRFacilityNetworkComponent::DebugAddReferenceResourceV2(
	FName OccupantId,
	ESRResourceContentPresetV2 ResourcePreset,
	FName OriginBodyId,
	int32 StackCount)
{
	FSRResourceInstance ResourceInstance;
	if (!FSRResourceSystemContent::MakeReferenceResourceInstance(
		ResourcePreset,
		OriginBodyId,
		ResourceInstance))
	{
		SR_LOG(FacilityNetwork,
			LogTemp,
			Warning,
			TEXT("[FacilityNetwork][Debug] AddReferenceResourceV2 failed: OccupantId=%s Owner=%s Reason=InvalidPreset"),
			*OccupantId.ToString(),
			*GetNameSafe(GetOwner()));
		return false;
	}

	ResourceInstance.ResourceInstanceId = FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits));
	ResourceInstance.StackCount = FMath::Max(1, StackCount);
	const bool bAdded = AddInputResource(OccupantId, ResourceInstance);
	if (bAdded)
	{
		SR_LOG(FacilityNetwork,
			LogTemp,
			Display,
			TEXT("[FacilityNetwork][Debug] Added reference Resource V2: Preset=%s OccupantId=%s %s Owner=%s"),
			*StaticEnum<ESRResourceContentPresetV2>()->GetNameStringByValue(static_cast<int64>(ResourcePreset)),
			*OccupantId.ToString(),
			*StarRovers::FacilityResources::BuildResourceDebugString(ResourceInstance),
			*GetNameSafe(GetOwner()));
	}
	return bAdded;
}

bool USRFacilityNetworkComponent::DebugAddReferenceStellarFuelBatchV2(
	FName OccupantId,
	ESRStellarFuelReferenceTopologyV2 Topology,
	FName FabricatorBodyId)
{
	if (FabricatorBodyId.IsNone())
	{
		FabricatorBodyId = FName(TEXT("Concord"));
	}

	FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	if (!FacilityInstance
		|| FacilityInstance->InputPortInventories.Num() != StarRovers::StellarFuel::RequiredCardCount)
	{
		SR_LOG(FacilityNetwork,
			LogTemp,
			Warning,
			TEXT("[FacilityNetwork][Debug] AddReferenceStellarFuelBatchV2 failed: OccupantId=%s Owner=%s Reason=FabricatorRequiresFiveInputPorts"),
			*OccupantId.ToString(),
			*GetNameSafe(GetOwner()));
		return false;
	}

	for (const FSRFacilityPortInventory& InputPort : FacilityInstance->InputPortInventories)
	{
		if (!InputPort.Inventory.IsEmpty() || InputPort.Capacity < 1)
		{
			SR_LOG(FacilityNetwork,
				LogTemp,
				Warning,
				TEXT("[FacilityNetwork][Debug] AddReferenceStellarFuelBatchV2 failed: OccupantId=%s Owner=%s Reason=InputPortsMustBeEmpty"),
				*OccupantId.ToString(),
				*GetNameSafe(GetOwner()));
			return false;
		}
	}

	TArray<FSRResourceInstance> Cards;
	if (!FSRResourceSystemContent::MakeReferenceStellarFuelBatch(Topology, FabricatorBodyId, Cards))
	{
		return false;
	}

	for (int32 CardIndex = 0; CardIndex < Cards.Num(); ++CardIndex)
	{
		if (!AddInputResourceToPort(OccupantId, CardIndex, Cards[CardIndex]))
		{
			SR_LOG(FacilityNetwork,
				LogTemp,
				Error,
				TEXT("[FacilityNetwork][Debug] AddReferenceStellarFuelBatchV2 encountered an unexpected insertion failure: OccupantId=%s CardIndex=%d Owner=%s"),
				*OccupantId.ToString(),
				CardIndex,
				*GetNameSafe(GetOwner()));
			return false;
		}
	}

	SR_LOG(FacilityNetwork,
		LogTemp,
		Display,
		TEXT("[FacilityNetwork][Debug] Added reference Stellar Fuel batch: OccupantId=%s Topology=%s FabricatorBody=%s Owner=%s"),
		*OccupantId.ToString(),
		*StaticEnum<ESRStellarFuelReferenceTopologyV2>()->GetNameStringByValue(static_cast<int64>(Topology)),
		*FabricatorBodyId.ToString(),
		*GetNameSafe(GetOwner()));
	return true;
}

bool USRFacilityNetworkComponent::DebugStepFacilities(float DeltaTime, int32 StepCount)
{
	const int32 SafeStepCount = FMath::Max(1, StepCount);
	const float SafeDeltaTime = FMath::Max(0.0f, DeltaTime);
	int32 TotalProcessedCount = 0;
	for (int32 StepIndex = 0; StepIndex < SafeStepCount; ++StepIndex)
	{
		TotalProcessedCount += ProcessFacilities(SafeDeltaTime);
	}

	SR_LOG(FacilityNetwork,
		LogTemp,
		Display,
		TEXT("[FacilityNetwork][Debug] StepFacilities: Owner=%s DeltaTime=%.3f StepCount=%d ProcessedCount=%d RegisteredFacilities=%d"),
		*GetNameSafe(GetOwner()),
		SafeDeltaTime,
		SafeStepCount,
		TotalProcessedCount,
		RuntimeState.FacilityInstancesByOccupantId.Num());
	return TotalProcessedCount > 0;
}

bool USRFacilityNetworkComponent::DebugDumpFacilityState(FName OccupantId) const
{
	const FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	if (!FacilityInstance)
	{
		SR_LOG(FacilityNetwork,
			LogTemp,
			Warning,
			TEXT("[FacilityNetwork][Debug] Dump failed: OccupantId=%s Owner=%s Reason=MissingFacility"),
			*OccupantId.ToString(),
			*GetNameSafe(GetOwner()));
		return false;
	}

	SR_LOG(FacilityNetwork,
		LogTemp,
		Display,
		TEXT("[FacilityNetwork][Debug] Dump: OccupantId=%s Structure=%s Facility=%s Owner=%s Input=%d Processing=%d Output=%d bProcessing=%s ProcessEnabled=%s DeliverEnabled=%s Progress=%.3f Temperature=%d Priority=%s Load=%d Speed=%.3f Capacity=%d/%d Cores=%d Origin=(%s)"),
		*FacilityInstance->OccupantId.ToString(),
		*GetNameSafe(FacilityInstance->StructureDataAsset.Get()),
		*GetNameSafe(FacilityInstance->FacilityDataAsset.Get()),
		*GetNameSafe(GetOwner()),
		FacilityInstance->InputInventory.Num(),
		FacilityInstance->ProcessingInventory.Num(),
		FacilityInstance->OutputInventory.Num(),
		FacilityInstance->bProcessing ? TEXT("true") : TEXT("false"),
		FacilityInstance->bProcessEnabled ? TEXT("true") : TEXT("false"),
		FacilityInstance->bDeliverEnabled ? TEXT("true") : TEXT("false"),
		FacilityInstance->ProcessProgressSeconds,
		static_cast<int32>(FacilityInstance->TemperatureState),
		*StaticEnum<ESROperationalPriorityV2>()->GetNameStringByValue(
			static_cast<int64>(FacilityInstance->OperationalPriority)),
		IsValid(FacilityInstance->FacilityDataAsset.Get())
			? FacilityInstance->FacilityDataAsset->OperationalLoad
			: 0,
		FacilityInstance->OperationalSpeedFactor,
		RuntimeState.OperationalCapacityReport.TotalDemand,
		RuntimeState.OperationalCapacityReport.TotalCapacity,
		RuntimeState.OperationalCapacityReport.ActiveServiceCoreCount,
		*StarRovers::FacilityResources::BuildFacilityCellDebugString(FacilityInstance->OriginCellId));

	if (!FacilityInstance->InputInventory.IsEmpty())
	{
		SR_LOG(FacilityNetwork, LogTemp, Display, TEXT("[FacilityNetwork][Debug]   FirstInput: %s"), *StarRovers::FacilityResources::BuildResourceDebugString(FacilityInstance->InputInventory[0]));
	}
	if (!FacilityInstance->ProcessingInventory.IsEmpty())
	{
		SR_LOG(FacilityNetwork, LogTemp, Display, TEXT("[FacilityNetwork][Debug]   FirstProcessing: %s"), *StarRovers::FacilityResources::BuildResourceDebugString(FacilityInstance->ProcessingInventory[0]));
	}
	if (!FacilityInstance->OutputInventory.IsEmpty())
	{
		SR_LOG(FacilityNetwork, LogTemp, Display, TEXT("[FacilityNetwork][Debug]   FirstOutput: %s"), *StarRovers::FacilityResources::BuildResourceDebugString(FacilityInstance->OutputInventory[0]));
	}
	return true;
}

bool USRFacilityNetworkComponent::DebugExtractAndLogOutputResource(FName OccupantId, FSRResourceInstance& OutResourceInstance)
{
	const bool bExtracted = ExtractOutputResource(OccupantId, OutResourceInstance);
	if (bExtracted)
	{
		SR_LOG(FacilityNetwork,
			LogTemp,
			Display,
			TEXT("[FacilityNetwork][Debug] Extracted output: OccupantId=%s %s Owner=%s"),
			*OccupantId.ToString(),
			*StarRovers::FacilityResources::BuildResourceDebugString(OutResourceInstance),
			*GetNameSafe(GetOwner()));
	}
	return bExtracted;
}
