#include "Automation/SRFacilityNetworkComponent.h"

#include "Utility/SRLog.h"
#include "Conveyor/SRConveyorNetworkComponent.h"
#include "GameFramework/Actor.h"
#include "Pattern/SRPatternRoutingFilter.h"
#include "SRFacilityConveyorPortConnector.h"
#include "SRFacilityPortInventoryBuilder.h"
#include "SRFacilityResourceOperations.h"
#include "Surface/SRPlanetSurfaceGrid.h"

bool USRFacilityNetworkComponent::TryAcceptInputResourceFromConveyorCell(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& ConveyorCellId,
	const FSRResourceInstance& ResourceInstance,
	FName SourceFacilityOccupantId)
{
	if (!IsValid(SurfaceGrid)
		|| !StarRovers::PatternRouting::IsValidPatternPayload(ResourceInstance))
	{
		return false;
	}

	for (TPair<FName, FSRFacilityInstance>& FacilityPair : RuntimeState.FacilityInstancesByOccupantId)
	{
		FSRFacilityInstance& FacilityInstance = FacilityPair.Value;
		const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
		FSRFacilityPortInventory* InputPortInventory = FSRFacilityConveyorPortConnector::FindConnectedInputPortInventory(
			SurfaceGrid,
			FacilityInstance,
			ConveyorCellId,
			ResourceInstance);
		if (!IsValid(FacilityDataAsset)
			|| !InputPortInventory
			|| (!SourceFacilityOccupantId.IsNone() && FacilityInstance.OccupantId == SourceFacilityOccupantId))
		{
			continue;
		}

		FSRFacilityPortInventory SimulatedInputPortInventory = *InputPortInventory;
		const int32 RequiredStackCount = StarRovers::FacilityResources::GetResourceStackCount(ResourceInstance);
		if (StarRovers::FacilityResources::TryAddResourceToInventorySlot(SimulatedInputPortInventory, ResourceInstance) != RequiredStackCount
			|| StarRovers::FacilityResources::TryAddResourceToInventorySlot(*InputPortInventory, ResourceInstance) != RequiredStackCount)
		{
			continue;
		}
		FSRFacilityPortInventoryBuilder::RefreshAggregateInventories(FacilityInstance);
		SetComponentTickEnabled(bAutoProcessFacilities);
		TryAutoLaunchStarFuelMissilesFromInputPort(FacilityInstance, InputPortInventory->PortIndex);
		if (bLogFacilityNetworkEvents)
		{
			SR_LOG(FacilityNetwork, LogTemp,
				Display,
				TEXT("[FacilityNetwork] Transfer accepted input: OccupantId=%s Port=%s ResourceId=%s PortInputCount=%d InputCount=%d Owner=%s"),
				*FacilityInstance.OccupantId.ToString(),
				*InputPortInventory->PortId.ToString(),
				*ResourceInstance.ResourceId.ToString(),
				StarRovers::FacilityResources::GetInventorySlotStackCount(*InputPortInventory),
				FacilityInstance.InputInventory.Num(),
				*GetNameSafe(GetOwner()));
		}
		return true;
	}

	return false;
}

bool USRFacilityNetworkComponent::TryPullOutputResourceToConveyorCell(
	USRPlanetSurfaceGrid* SurfaceGrid,
	const FSRPlanetSurfaceGridCellId& ConveyorCellId,
	FSRResourceInstance& OutResourceInstance,
	FName& OutSourceFacilityOccupantId)
{
	if (!IsValid(SurfaceGrid))
	{
		OutResourceInstance = FSRResourceInstance();
		OutSourceFacilityOccupantId = NAME_None;
		return false;
	}

	for (TPair<FName, FSRFacilityInstance>& FacilityPair : RuntimeState.FacilityInstancesByOccupantId)
	{
		FSRFacilityInstance& FacilityInstance = FacilityPair.Value;
		FSRFacilityPortInventory* OutputPortInventory = FSRFacilityConveyorPortConnector::FindConnectedOutputPortInventory(
			SurfaceGrid,
			FacilityInstance,
			ConveyorCellId);
		if (!OutputPortInventory
			|| StarRovers::FacilityResources::GetInventorySlotStackCount(*OutputPortInventory) <= 0
			|| !FacilityInstance.bDeliverEnabled
			|| !FSRFacilityConveyorPortConnector::IsConveyorCellConnectedToPortInventory(SurfaceGrid, FacilityInstance, *OutputPortInventory, ConveyorCellId))
		{
			continue;
		}

		if (!StarRovers::FacilityResources::TryTakeSingleResourceFromInventorySlot(*OutputPortInventory, OutResourceInstance))
		{
			continue;
		}
		OutSourceFacilityOccupantId = FacilityInstance.OccupantId;
		FSRFacilityPortInventoryBuilder::RefreshAggregateInventories(FacilityInstance);
		if (bLogFacilityNetworkEvents)
		{
			SR_LOG(FacilityNetwork, LogTemp,
				Display,
				TEXT("[FacilityNetwork] Transfer provided output: OccupantId=%s Port=%s ResourceId=%s RemainingPortOutput=%d RemainingOutput=%d Owner=%s"),
				*FacilityInstance.OccupantId.ToString(),
				*OutputPortInventory->PortId.ToString(),
				*OutResourceInstance.ResourceId.ToString(),
				StarRovers::FacilityResources::GetInventorySlotStackCount(*OutputPortInventory),
				FacilityInstance.OutputInventory.Num(),
				*GetNameSafe(GetOwner()));
		}
		return true;
	}

	OutResourceInstance = FSRResourceInstance();
	OutSourceFacilityOccupantId = NAME_None;
	return false;
}

bool USRFacilityNetworkComponent::HasConnectedConveyorForFacilityPort(FName OccupantId, ESRFacilityPortKind PortKind) const
{
	const FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	if (!FacilityInstance)
	{
		return false;
	}

	AActor* Owner = GetOwner();
	USRPlanetSurfaceGrid* SurfaceGrid = IsValid(Owner) ? Owner->FindComponentByClass<USRPlanetSurfaceGrid>() : nullptr;
	const USRConveyorNetworkComponent* ConveyorNetwork = IsValid(Owner) ? Owner->FindComponentByClass<USRConveyorNetworkComponent>() : nullptr;
	if (!IsValid(SurfaceGrid) || !IsValid(ConveyorNetwork))
	{
		return false;
	}

	for (const FSRPlanetSurfaceGridCellId& FootprintCellId : FacilityInstance->FootprintCellIds)
	{
		TArray<FSRPlanetSurfaceGridCellId> NeighborCellIds;
		if (!FSRFacilityConveyorPortConnector::GetNeighborCellIds(SurfaceGrid, FootprintCellId, NeighborCellIds))
		{
			continue;
		}

		for (const FSRPlanetSurfaceGridCellId& NeighborCellId : NeighborCellIds)
		{
			if (ConveyorNetwork->HasConveyorSegmentAtCell(NeighborCellId)
				&& FSRFacilityConveyorPortConnector::IsConveyorCellConnectedToFacilityPort(SurfaceGrid, *FacilityInstance, NeighborCellId, PortKind))
			{
				return true;
			}
		}
	}

	return false;
}
