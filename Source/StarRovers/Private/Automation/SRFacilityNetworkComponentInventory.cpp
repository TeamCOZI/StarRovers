#include "Automation/SRFacilityNetworkComponent.h"

#include "SRFacilityDirectInventoryRouter.h"
#include "SRFacilityHubCargoRouter.h"
#include "SRFacilityResourceOperations.h"
#include "Utility/SRLog.h"

bool USRFacilityNetworkComponent::AddInputResource(FName OccupantId, const FSRResourceInstance& ResourceInstance)
{
	FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	FSRFacilityInventoryTransferResult TransferResult;
	if (!FacilityInstance || !FSRFacilityDirectInventoryRouter::TryAddInputResource(*FacilityInstance, ResourceInstance, &TransferResult))
	{
		if (bLogFacilityNetworkEvents)
		{
			SR_LOG(FacilityNetwork,
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

	SetComponentTickEnabled(bAutoProcessFacilities);
	if (bLogFacilityNetworkEvents)
	{
		SR_LOG(FacilityNetwork,
			LogTemp,
			Display,
			TEXT("[FacilityNetwork] Input added: OccupantId=%s Port=%s ResourceId=%s StackCount=%d PortInputCount=%d InputCount=%d Owner=%s"),
			*OccupantId.ToString(),
			*TransferResult.PortId.ToString(),
			*ResourceInstance.ResourceId.ToString(),
			ResourceInstance.StackCount,
			TransferResult.PortStackCount,
			TransferResult.AggregateStackCount,
			*GetNameSafe(GetOwner()));
	}
	return true;
}

bool USRFacilityNetworkComponent::AddInputResourceToPort(FName OccupantId, int32 InputPortIndex, const FSRResourceInstance& ResourceInstance)
{
	FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	if (!FacilityInstance || !FSRFacilityDirectInventoryRouter::TryAddInputResourceToPort(*FacilityInstance, InputPortIndex, ResourceInstance))
	{
		return false;
	}
	SetComponentTickEnabled(bAutoProcessFacilities);
	return true;
}

bool USRFacilityNetworkComponent::ExtractOutputResource(FName OccupantId, FSRResourceInstance& OutResourceInstance)
{
	FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	FSRFacilityInventoryTransferResult TransferResult;
	if (!FacilityInstance || !FSRFacilityDirectInventoryRouter::TryExtractOutputResource(*FacilityInstance, OutResourceInstance, &TransferResult))
	{
		OutResourceInstance = FSRResourceInstance();
		if (bLogFacilityNetworkEvents)
		{
			SR_LOG(FacilityNetwork,
				LogTemp,
				Warning,
				TEXT("[FacilityNetwork] ExtractOutputResource failed: OccupantId=%s Owner=%s Reason=MissingFacilityOrEmptyOutput"),
				*OccupantId.ToString(),
				*GetNameSafe(GetOwner()));
		}
		return false;
	}

	if (bLogFacilityNetworkEvents)
	{
		SR_LOG(FacilityNetwork,
			LogTemp,
			Display,
			TEXT("[FacilityNetwork] Output extracted: OccupantId=%s Port=%s ResourceId=%s RemainingPortOutput=%d RemainingOutputCount=%d Owner=%s"),
			*OccupantId.ToString(),
			*TransferResult.PortId.ToString(),
			*OutResourceInstance.ResourceId.ToString(),
			TransferResult.PortStackCount,
			TransferResult.AggregateStackCount,
			*GetNameSafe(GetOwner()));
	}
	return true;
}

bool USRFacilityNetworkComponent::IsHubFacility(FName OccupantId) const
{
	const FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	return FacilityInstance && FSRFacilityHubCargoRouter::IsHubFacility(*FacilityInstance);
}

bool USRFacilityNetworkComponent::TryTakeHubOutboundCargo(FName OccupantId, int32 MaxStackCount, FSRResourceInstance& OutCargo)
{
	return TryTakeHubOutboundCargoByResource(OccupantId, NAME_None, MaxStackCount, OutCargo);
}

bool USRFacilityNetworkComponent::TryTakeHubOutboundCargoByResource(FName OccupantId, FName ResourceId, int32 MaxStackCount, FSRResourceInstance& OutCargo)
{
	OutCargo = FSRResourceInstance();
	FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	FSRFacilityHubCargoTransferResult TransferResult;
	if (!FacilityInstance
		|| !FSRFacilityHubCargoRouter::TryTakeOutboundCargo(*FacilityInstance, MaxStackCount, ResourceId, OutCargo, &TransferResult))
	{
		return false;
	}

	if (bLogFacilityNetworkEvents)
	{
		SR_LOG(FacilityNetwork,
			LogTemp,
			Display,
			TEXT("[FacilityNetwork][Hub] Outbound cargo taken: OccupantId=%s Port=%s ResourceId=%s RequestedResourceId=%s StackCount=%d RemainingPortInput=%d Owner=%s"),
			*OccupantId.ToString(),
			*TransferResult.PortId.ToString(),
			*OutCargo.ResourceId.ToString(),
			ResourceId.IsNone() ? TEXT("Any") : *ResourceId.ToString(),
			OutCargo.StackCount,
			TransferResult.RemainingPortStackCount,
			*GetNameSafe(GetOwner()));
	}
	return true;
}

void USRFacilityNetworkComponent::GetHubOutboundCargoResourceIds(FName OccupantId, TArray<FName>& OutResourceIds) const
{
	OutResourceIds.Reset();
	const FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	if (!FacilityInstance)
	{
		return;
	}

	FSRFacilityHubCargoRouter::GetOutboundCargoResourceIds(*FacilityInstance, OutResourceIds);
}

bool USRFacilityNetworkComponent::CanStoreHubInboundCargo(FName OccupantId, const FSRResourceInstance& Cargo) const
{
	const FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	return FacilityInstance && FSRFacilityHubCargoRouter::CanStoreInboundCargo(*FacilityInstance, Cargo);
}

bool USRFacilityNetworkComponent::TryStoreHubInboundCargo(FName OccupantId, const FSRResourceInstance& Cargo)
{
	FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	if (!FacilityInstance || !FSRFacilityHubCargoRouter::TryStoreInboundCargo(*FacilityInstance, Cargo))
	{
		return false;
	}

	if (bLogFacilityNetworkEvents)
	{
		SR_LOG(FacilityNetwork,
			LogTemp,
			Display,
			TEXT("[FacilityNetwork][Hub] Inbound cargo stored: OccupantId=%s ResourceId=%s StackCount=%d OutputCount=%d Owner=%s"),
			*OccupantId.ToString(),
			*Cargo.ResourceId.ToString(),
			Cargo.StackCount,
			FacilityInstance->OutputInventory.Num(),
			*GetNameSafe(GetOwner()));
	}
	return true;
}
