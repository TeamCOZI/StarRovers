#include "Automation/SRFacilityNetworkComponent.h"

#include "SRFacilityNetworkComponentInternal.h"
#include "Structure/SRStructureDataAsset.h"

USRFacilityNetworkComponent::USRFacilityNetworkComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void USRFacilityNetworkComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bAutoProcessFacilities || FacilityInstancesByOccupantId.IsEmpty())
	{
		SetComponentTickEnabled(false);
		return;
	}

	ProcessFacilities(DeltaTime);
}

bool USRFacilityNetworkComponent::RegisterFacility(
	FName OccupantId,
	USRStructureDataAsset* StructureDataAsset,
	const FSRPlanetSurfaceGridCellId& OriginCellId,
	const TArray<FSRPlanetSurfaceGridCellId>& FootprintCellIds)
{
	if (OccupantId.IsNone() || !IsValid(StructureDataAsset))
	{
		if (bLogFacilityNetworkEvents)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[FacilityNetwork] Register failed: OccupantId=%s Structure=%s Owner=%s Reason=InvalidInput"),
				*OccupantId.ToString(),
				*GetNameSafe(StructureDataAsset),
				*GetNameSafe(GetOwner()));
		}
		return false;
	}

	const FSRStructureData StructureData = StructureDataAsset->BuildData();
	if (!IsValid(StructureData.FacilityDataAsset.Get()))
	{
		return false;
	}

	FSRFacilityInstance& FacilityInstance = FacilityInstancesByOccupantId.FindOrAdd(OccupantId);
	FacilityInstance.OccupantId = OccupantId;
	FacilityInstance.StructureDataAsset = StructureDataAsset;
	FacilityInstance.FacilityDataAsset = StructureData.FacilityDataAsset;
	FacilityInstance.OriginCellId = OriginCellId;
	FacilityInstance.FootprintCellIds = FootprintCellIds;
	FacilityInstance.TemperatureState = ESRFacilityTemperatureState::Normal;
	FacilityInstance.ProcessProgressSeconds = 0.0f;
	FacilityInstance.bProcessing = false;
	FacilityInstance.InputInventory.Reset();
	FacilityInstance.OutputInventory.Reset();
	FacilityInstance.ProcessingInventory.Reset();

	SetComponentTickEnabled(bAutoProcessFacilities);
	if (bLogFacilityNetworkEvents)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[FacilityNetwork] Registered: OccupantId=%s Structure=%s Facility=%s Owner=%s Origin=(%s) FootprintCells=%d"),
			*OccupantId.ToString(),
			*GetNameSafe(StructureDataAsset),
			*GetNameSafe(StructureData.FacilityDataAsset.Get()),
			*GetNameSafe(GetOwner()),
			*StarRovers::FacilityNetwork::BuildFacilityCellDebugString(OriginCellId),
			FootprintCellIds.Num());
	}
	return true;
}

bool USRFacilityNetworkComponent::UnregisterFacility(FName OccupantId)
{
	const bool bRemoved = FacilityInstancesByOccupantId.Remove(OccupantId) > 0;
	if (FacilityInstancesByOccupantId.IsEmpty())
	{
		SetComponentTickEnabled(false);
	}
	if (bRemoved && bLogFacilityNetworkEvents)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[FacilityNetwork] Unregistered: OccupantId=%s Owner=%s RemainingFacilities=%d"),
			*OccupantId.ToString(),
			*GetNameSafe(GetOwner()),
			FacilityInstancesByOccupantId.Num());
	}
	return bRemoved;
}

void USRFacilityNetworkComponent::ClearFacilities()
{
	const int32 RemovedFacilityCount = FacilityInstancesByOccupantId.Num();
	FacilityInstancesByOccupantId.Reset();
	SetComponentTickEnabled(false);
	if (bLogFacilityNetworkEvents)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[FacilityNetwork] Cleared: Owner=%s RemovedFacilities=%d"),
			*GetNameSafe(GetOwner()),
			RemovedFacilityCount);
	}
}

bool USRFacilityNetworkComponent::HasFacilityInstance(FName OccupantId) const
{
	return FacilityInstancesByOccupantId.Contains(OccupantId);
}

bool USRFacilityNetworkComponent::GetFacilityInstance(FName OccupantId, FSRFacilityInstance& OutFacilityInstance) const
{
	if (const FSRFacilityInstance* FacilityInstance = FacilityInstancesByOccupantId.Find(OccupantId))
	{
		OutFacilityInstance = *FacilityInstance;
		return true;
	}

	OutFacilityInstance = FSRFacilityInstance();
	return false;
}
