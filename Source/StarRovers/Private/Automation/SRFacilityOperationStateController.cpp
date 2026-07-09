#include "SRFacilityOperationStateController.h"

#include "Components/ActorComponent.h"
#include "Conveyor/SRConveyorNetworkComponent.h"
#include "GameFramework/Actor.h"

bool FSRFacilityOperationStateController::SetTemperatureState(
	FSRFacilityNetworkRuntimeState& RuntimeState,
	FName OccupantId,
	ESRFacilityTemperatureState TemperatureState)
{
	FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	if (!FacilityInstance)
	{
		return false;
	}

	FacilityInstance->TemperatureState = TemperatureState;
	return true;
}

bool FSRFacilityOperationStateController::SetProcessEnabled(
	UActorComponent* OwnerComponent,
	FSRFacilityNetworkRuntimeState& RuntimeState,
	FName OccupantId,
	bool bEnabled,
	bool bAutoProcessFacilities)
{
	FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	if (!FacilityInstance)
	{
		return false;
	}

	FacilityInstance->bProcessEnabled = bEnabled;
	if (bEnabled && IsValid(OwnerComponent))
	{
		OwnerComponent->SetComponentTickEnabled(bAutoProcessFacilities);
	}
	return true;
}

bool FSRFacilityOperationStateController::SetDeliverEnabled(
	UActorComponent* OwnerComponent,
	FSRFacilityNetworkRuntimeState& RuntimeState,
	FName OccupantId,
	bool bEnabled)
{
	FSRFacilityInstance* FacilityInstance = RuntimeState.FacilityInstancesByOccupantId.Find(OccupantId);
	if (!FacilityInstance)
	{
		return false;
	}

	FacilityInstance->bDeliverEnabled = bEnabled;
	if (bEnabled && IsValid(OwnerComponent))
	{
		if (AActor* Owner = OwnerComponent->GetOwner())
		{
			if (USRConveyorNetworkComponent* ConveyorNetwork = Owner->FindComponentByClass<USRConveyorNetworkComponent>())
			{
				ConveyorNetwork->SetComponentTickEnabled(true);
			}
		}
	}
	return true;
}
