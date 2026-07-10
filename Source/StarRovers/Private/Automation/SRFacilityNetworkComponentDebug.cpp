#include "Automation/SRFacilityNetworkComponent.h"

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
	ResourceInstance.ResourceKind = ESRResourceKind::Energy;
	ResourceInstance.EnergyValue = EnergyValue;
	ResourceInstance.CatalystOperator = ESRResourceCatalystOperator::None;
	ResourceInstance.RemainingProcessLimit = FMath::Max(0, RemainingProcessLimit);
	ResourceInstance.ProcessCount = 0;
	ResourceInstance.StackCount = FMath::Max(1, StackCount);

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
		TEXT("[FacilityNetwork][Debug] Dump: OccupantId=%s Structure=%s Facility=%s Owner=%s Input=%d Processing=%d Output=%d bProcessing=%s ProcessEnabled=%s DeliverEnabled=%s Progress=%.3f Temperature=%d Origin=(%s)"),
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
