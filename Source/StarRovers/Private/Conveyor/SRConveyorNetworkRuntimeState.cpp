#include "Conveyor/SRConveyorNetworkRuntimeState.h"

bool FSRConveyorActorGroupRuntimeState::HasDirtyGroups() const
{
    for (const TPair<FName, FSRConveyorActorGroupState>& ActorGroupPair : GroupsByKey)
    {
        if (ActorGroupPair.Value.bDirty)
        {
            return true;
        }
    }

    return false;
}

void FSRConveyorActorGroupRuntimeState::Reset()
{
    GroupsByKey.Reset();
    PendingPlacementDiagnosticKeys.Reset();
    PendingDeletionDiagnosticKeys.Reset();
}

int32 FSRConveyorTransportRuntimeState::GetItemCount() const
{
    return ItemsByLane.Num();
}

bool FSRConveyorTransportRuntimeState::HasItems() const
{
    return !ItemsByLane.IsEmpty();
}

void FSRConveyorTransportRuntimeState::ResetItems()
{
    ItemsByLane.Reset();
}

void FSRConveyorTransportRuntimeState::ResetLabels()
{
    ItemLabelsByLane.Reset();
}
