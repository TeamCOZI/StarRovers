#include "Automation/SRFacilityNetworkComponent.h"

#include "SRFacilityDirectInventoryRouter.h"
#include "SRFacilityHubCargoRouter.h"
#include "SRFacilityResourceOperations.h"
#include "Logistics/SRSpaceLogisticsSubsystem.h"
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
	TryAutoLaunchStarFuelMissilesFromInputPort(*FacilityInstance, TransferResult.PortIndex);
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
	FSRFacilityInventoryTransferResult TransferResult;
	if (!FacilityInstance || !FSRFacilityDirectInventoryRouter::TryAddInputResourceToPort(*FacilityInstance, InputPortIndex, ResourceInstance, &TransferResult))
	{
		return false;
	}
	SetComponentTickEnabled(bAutoProcessFacilities);
	TryAutoLaunchStarFuelMissilesFromInputPort(*FacilityInstance, TransferResult.PortIndex);
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

bool USRFacilityNetworkComponent::TryTakeHubOutboundCargoMatching(
	FName OccupantId,
	int32 MaxStackCount,
	TFunctionRef<bool(const FSRResourceInstance&)> CargoPredicate,
	FSRResourceInstance& OutCargo)
{
	OutCargo = FSRResourceInstance();
	FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	FSRFacilityHubCargoTransferResult TransferResult;
	if (!FacilityInstance
		|| !FSRFacilityHubCargoRouter::TryTakeOutboundCargoMatching(
			*FacilityInstance,
			MaxStackCount,
			CargoPredicate,
			OutCargo,
			&TransferResult))
	{
		return false;
	}

	if (bLogFacilityNetworkEvents)
	{
		SR_LOG(FacilityNetwork,
			LogTemp,
			Display,
			TEXT("[FacilityNetwork][Hub] Matching outbound cargo taken: OccupantId=%s Port=%s ResourceId=%s StackCount=%d RemainingPortInput=%d Owner=%s"),
			*OccupantId.ToString(),
			*TransferResult.PortId.ToString(),
			*OutCargo.ResourceId.ToString(),
			OutCargo.StackCount,
			TransferResult.RemainingPortStackCount,
			*GetNameSafe(GetOwner()));
	}
	return true;
}

bool USRFacilityNetworkComponent::TryTakeHubOutboundCargoMatchingFromInputPort(
	FName OccupantId,
	int32 InputPortIndex,
	int32 MaxStackCount,
	TFunctionRef<bool(const FSRResourceInstance&)> CargoPredicate,
	FSRResourceInstance& OutCargo)
{
	OutCargo = FSRResourceInstance();
	FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	FSRFacilityHubCargoTransferResult TransferResult;
	if (!FacilityInstance
		|| !FSRFacilityHubCargoRouter::TryTakeOutboundCargoMatchingFromInputPort(
			*FacilityInstance,
			InputPortIndex,
			MaxStackCount,
			CargoPredicate,
			OutCargo,
			&TransferResult))
	{
		return false;
	}

	if (bLogFacilityNetworkEvents)
	{
		SR_LOG(FacilityNetwork,
			LogTemp,
			Display,
			TEXT("[FacilityNetwork][Hub] Matching outbound cargo taken from input port: OccupantId=%s Port=%s PortIndex=%d ResourceId=%s StackCount=%d RemainingPortInput=%d Owner=%s"),
			*OccupantId.ToString(),
			*TransferResult.PortId.ToString(),
			InputPortIndex,
			*OutCargo.ResourceId.ToString(),
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

bool USRFacilityNetworkComponent::SetHubStarFuelMissileAutoLaunchInputPort(FName OccupantId, int32 InputPortIndex, bool bEnabled)
{
	FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	if (!FacilityInstance
		|| !FSRFacilityHubCargoRouter::IsHubFacility(*FacilityInstance)
		|| !FacilityInstance->InputPortInventories.IsValidIndex(InputPortIndex))
	{
		return false;
	}

	FacilityInstance->StarFuelMissileAutoLaunchInputPortIndices.RemoveAll([](int32 StoredInputPortIndex)
	{
		return StoredInputPortIndex == INDEX_NONE;
	});

	if (bEnabled)
	{
		FacilityInstance->StarFuelMissileAutoLaunchInputPortIndices.AddUnique(InputPortIndex);
		TryAutoLaunchStarFuelMissilesFromInputPort(*FacilityInstance, InputPortIndex);
	}
	else
	{
		FacilityInstance->StarFuelMissileAutoLaunchInputPortIndices.Remove(InputPortIndex);
	}
	return true;
}

bool USRFacilityNetworkComponent::IsHubStarFuelMissileAutoLaunchInputPort(FName OccupantId, int32 InputPortIndex) const
{
	const FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	return FacilityInstance
		&& FSRFacilityHubCargoRouter::IsHubFacility(*FacilityInstance)
		&& FacilityInstance->StarFuelMissileAutoLaunchInputPortIndices.Contains(InputPortIndex);
}

void USRFacilityNetworkComponent::GetHubStarFuelMissileAutoLaunchInputPorts(FName OccupantId, TArray<int32>& OutInputPortIndices) const
{
	OutInputPortIndices.Reset();
	const FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	if (!FacilityInstance || !FSRFacilityHubCargoRouter::IsHubFacility(*FacilityInstance))
	{
		return;
	}

	OutInputPortIndices = FacilityInstance->StarFuelMissileAutoLaunchInputPortIndices;
	OutInputPortIndices.RemoveAll([FacilityInstance](int32 InputPortIndex)
	{
		return !FacilityInstance->InputPortInventories.IsValidIndex(InputPortIndex);
	});
	OutInputPortIndices.Sort();
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

void USRFacilityNetworkComponent::TryAutoLaunchStarFuelMissilesFromInputPort(FSRFacilityInstance& FacilityInstance, int32 InputPortIndex)
{
	if (!FacilityInstance.InputPortInventories.IsValidIndex(InputPortIndex)
		|| !FacilityInstance.StarFuelMissileAutoLaunchInputPortIndices.Contains(InputPortIndex)
		|| !FSRFacilityHubCargoRouter::IsHubFacility(FacilityInstance))
	{
		return;
	}

	UWorld* World = GetWorld();
	USRSpaceLogisticsSubsystem* SpaceLogisticsSubsystem = IsValid(World)
		? World->GetSubsystem<USRSpaceLogisticsSubsystem>()
		: nullptr;
	if (!IsValid(SpaceLogisticsSubsystem))
	{
		return;
	}

	FSRSpaceLogisticsHubEndpoint SourceHub;
	if (!SpaceLogisticsSubsystem->GetHubEndpoint(GetOwner(), FacilityInstance.OccupantId, SourceHub))
	{
		return;
	}

	const auto HasMissileFuelCargo = [&FacilityInstance, InputPortIndex]()
	{
		if (!FacilityInstance.InputPortInventories.IsValidIndex(InputPortIndex))
		{
			return false;
		}

		const FSRFacilityPortInventory& InputPortInventory = FacilityInstance.InputPortInventories[InputPortIndex];
		for (const FSRResourceInstance& ResourceInstance : InputPortInventory.Inventory)
		{
			if (!ResourceInstance.ResourceId.IsNone()
				&& ResourceInstance.StackCount > 0
				&& FMath::Max(0.0, ResourceInstance.EnergyValue) * static_cast<double>(FMath::Max(1, ResourceInstance.StackCount)) > UE_DOUBLE_SMALL_NUMBER)
			{
				return true;
			}
		}
		return false;
	};

	if (!HasMissileFuelCargo())
	{
		return;
	}

	const int32 MaxLaunchAttempts = FMath::Max(
		1,
		StarRovers::FacilityResources::GetInventorySlotStackCount(FacilityInstance.InputPortInventories[InputPortIndex]));
	for (int32 LaunchAttempt = 0; LaunchAttempt < MaxLaunchAttempts; ++LaunchAttempt)
	{
		if (!HasMissileFuelCargo())
		{
			break;
		}

		FName MissileId = NAME_None;
		if (!SpaceLogisticsSubsystem->LaunchStarFuelMissileFromHubInputPort(SourceHub, InputPortIndex, MissileId))
		{
			break;
		}
	}
}
