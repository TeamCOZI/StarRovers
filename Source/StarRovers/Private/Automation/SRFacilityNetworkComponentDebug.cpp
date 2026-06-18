#include "Automation/SRFacilityNetworkComponent.h"

#include "Structure/SRStructureDataAsset.h"

namespace
{
	FString BuildFacilityCellDebugString(const FSRPlanetSurfaceGridCellId& CellId)
	{
		return FString::Printf(
			TEXT("Face=%d X=%d Y=%d"),
			static_cast<int32>(CellId.Face),
			CellId.CellX,
			CellId.CellY);
	}

	FString BuildResourceDebugString(const FSRResourceInstance& ResourceInstance)
	{
		return FString::Printf(
			TEXT("ResourceId=%s Energy=%.3f RemainingProcessLimit=%d ProcessCount=%d StackCount=%d Tags=%d"),
			*ResourceInstance.ResourceId.ToString(),
			ResourceInstance.EnergyValue,
			ResourceInstance.RemainingProcessLimit,
			ResourceInstance.ProcessCount,
			ResourceInstance.StackCount,
			ResourceInstance.Tags.Num());
	}
}

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
	FacilityInstancesByOccupantId.GetKeys(OutOccupantIds);
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
		UE_LOG(
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
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[FacilityNetwork][Debug] Added input from DA: OccupantId=%s %s Owner=%s"),
			*OccupantId.ToString(),
			*BuildResourceDebugString(ResourceInstance),
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
		UE_LOG(
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
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[FacilityNetwork][Debug] Added raw energy input: OccupantId=%s %s Owner=%s"),
			*OccupantId.ToString(),
			*BuildResourceDebugString(ResourceInstance),
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

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[FacilityNetwork][Debug] StepFacilities: Owner=%s DeltaTime=%.3f StepCount=%d ProcessedCount=%d RegisteredFacilities=%d"),
		*GetNameSafe(GetOwner()),
		SafeDeltaTime,
		SafeStepCount,
		TotalProcessedCount,
		FacilityInstancesByOccupantId.Num());
	return TotalProcessedCount > 0;
}

bool USRFacilityNetworkComponent::DebugDumpFacilityState(FName OccupantId) const
{
	const FSRFacilityInstance* FacilityInstance = FacilityInstancesByOccupantId.Find(OccupantId);
	if (!FacilityInstance)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[FacilityNetwork][Debug] Dump failed: OccupantId=%s Owner=%s Reason=MissingFacility"),
			*OccupantId.ToString(),
			*GetNameSafe(GetOwner()));
		return false;
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[FacilityNetwork][Debug] Dump: OccupantId=%s Structure=%s Facility=%s Owner=%s Input=%d Processing=%d Output=%d bProcessing=%s Progress=%.3f Temperature=%d Origin=(%s)"),
		*FacilityInstance->OccupantId.ToString(),
		*GetNameSafe(FacilityInstance->StructureDataAsset.Get()),
		*GetNameSafe(FacilityInstance->FacilityDataAsset.Get()),
		*GetNameSafe(GetOwner()),
		FacilityInstance->InputInventory.Num(),
		FacilityInstance->ProcessingInventory.Num(),
		FacilityInstance->OutputInventory.Num(),
		FacilityInstance->bProcessing ? TEXT("true") : TEXT("false"),
		FacilityInstance->ProcessProgressSeconds,
		static_cast<int32>(FacilityInstance->TemperatureState),
		*BuildFacilityCellDebugString(FacilityInstance->OriginCellId));

	if (!FacilityInstance->InputInventory.IsEmpty())
	{
		UE_LOG(LogTemp, Display, TEXT("[FacilityNetwork][Debug]   FirstInput: %s"), *BuildResourceDebugString(FacilityInstance->InputInventory[0]));
	}
	if (!FacilityInstance->ProcessingInventory.IsEmpty())
	{
		UE_LOG(LogTemp, Display, TEXT("[FacilityNetwork][Debug]   FirstProcessing: %s"), *BuildResourceDebugString(FacilityInstance->ProcessingInventory[0]));
	}
	if (!FacilityInstance->OutputInventory.IsEmpty())
	{
		UE_LOG(LogTemp, Display, TEXT("[FacilityNetwork][Debug]   FirstOutput: %s"), *BuildResourceDebugString(FacilityInstance->OutputInventory[0]));
	}
	return true;
}

bool USRFacilityNetworkComponent::DebugExtractAndLogOutputResource(FName OccupantId, FSRResourceInstance& OutResourceInstance)
{
	const bool bExtracted = ExtractOutputResource(OccupantId, OutResourceInstance);
	if (bExtracted)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[FacilityNetwork][Debug] Extracted output: OccupantId=%s %s Owner=%s"),
			*OccupantId.ToString(),
			*BuildResourceDebugString(OutResourceInstance),
			*GetNameSafe(GetOwner()));
	}
	return bExtracted;
}
