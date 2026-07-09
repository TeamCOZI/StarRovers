#include "Conveyor/SRConveyorNetworkComponent.h"

#include "Automation/SRFacilityNetworkComponent.h"
#include "Conveyor/SRConveyorTransportProcessor.h"
#include "GameFramework/Actor.h"
#include "Surface/SRPlanetSurfaceGrid.h"

int32 USRConveyorNetworkComponent::GetConveyorItemCount() const
{
	return TransportState.GetItemCount();
}

void USRConveyorNetworkComponent::ProcessConveyorTransport(USRPlanetSurfaceGrid* SurfaceGrid, float DeltaTime)
{
	const float ClampedDeltaTime = FMath::Max(0.0f, DeltaTime);
	if (!IsValid(SurfaceGrid) || ClampedDeltaTime <= 0.0f)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	USRFacilityNetworkComponent* FacilityNetwork = IsValid(OwnerActor)
		? OwnerActor->FindComponentByClass<USRFacilityNetworkComponent>()
		: nullptr;
	if (!IsValid(FacilityNetwork))
	{
		return;
	}

	StarRovers::Conveyor::FSRConveyorTransportSettings Settings;
	Settings.ItemSpeedCellsPerSecond = ItemSpeedCellsPerSecond;
	Settings.MaxItemTransfersPerTick = MaxItemTransfersPerTick;
	StarRovers::Conveyor::FSRConveyorTransportProcessor::Process(
		SurfaceGrid,
		FacilityNetwork,
		Segments,
		TransportState,
		ClampedDeltaTime,
		Settings);
}

bool USRConveyorNetworkComponent::ShouldKeepTransportTickEnabled() const
{
	return (bAutoTransportItems && (!Segments.IsEmpty() || TransportState.HasItems()))
		|| (bShowTransportItemLabels && TransportState.HasItems());
}
