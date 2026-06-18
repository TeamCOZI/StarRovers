#include "Automation/SRFacilityNetworkComponent.h"

bool USRFacilityNetworkComponent::AddInputResource(FName OccupantId, const FSRResourceInstance& ResourceInstance)
{
	FSRFacilityInstance* FacilityInstance = FacilityInstancesByOccupantId.Find(OccupantId);
	if (!FacilityInstance || ResourceInstance.ResourceId.IsNone() || ResourceInstance.StackCount <= 0)
	{
		if (bLogFacilityNetworkEvents)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[FacilityNetwork] AddInputResource failed: OccupantId=%s ResourceId=%s StackCount=%d Owner=%s Reason=InvalidInputOrFacility"),
				*OccupantId.ToString(),
				*ResourceInstance.ResourceId.ToString(),
				ResourceInstance.StackCount,
				*GetNameSafe(GetOwner()));
		}
		return false;
	}

	FacilityInstance->InputInventory.Add(ResourceInstance);
	SetComponentTickEnabled(bAutoProcessFacilities);
	if (bLogFacilityNetworkEvents)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[FacilityNetwork] Input added: OccupantId=%s ResourceId=%s StackCount=%d InputCount=%d Owner=%s"),
			*OccupantId.ToString(),
			*ResourceInstance.ResourceId.ToString(),
			ResourceInstance.StackCount,
			FacilityInstance->InputInventory.Num(),
			*GetNameSafe(GetOwner()));
	}
	return true;
}

bool USRFacilityNetworkComponent::ExtractOutputResource(FName OccupantId, FSRResourceInstance& OutResourceInstance)
{
	FSRFacilityInstance* FacilityInstance = FacilityInstancesByOccupantId.Find(OccupantId);
	if (!FacilityInstance || FacilityInstance->OutputInventory.IsEmpty())
	{
		OutResourceInstance = FSRResourceInstance();
		if (bLogFacilityNetworkEvents)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[FacilityNetwork] ExtractOutputResource failed: OccupantId=%s Owner=%s Reason=MissingFacilityOrEmptyOutput"),
				*OccupantId.ToString(),
				*GetNameSafe(GetOwner()));
		}
		return false;
	}

	OutResourceInstance = FacilityInstance->OutputInventory[0];
	FacilityInstance->OutputInventory.RemoveAt(0);
	if (bLogFacilityNetworkEvents)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[FacilityNetwork] Output extracted: OccupantId=%s ResourceId=%s RemainingOutputCount=%d Owner=%s"),
			*OccupantId.ToString(),
			*OutResourceInstance.ResourceId.ToString(),
			FacilityInstance->OutputInventory.Num(),
			*GetNameSafe(GetOwner()));
	}
	return true;
}
