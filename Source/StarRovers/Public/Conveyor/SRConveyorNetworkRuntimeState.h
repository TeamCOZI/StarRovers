#pragma once

#include "CoreMinimal.h"
#include "Conveyor/SRConveyorTypes.h"
#include "SRConveyorNetworkRuntimeState.generated.h"

class ASRConveyorBeltActor;
class UTextRenderComponent;

struct FSRConveyorActorGroupState
{
    TArray<FSRConveyorBeltPath> BeltPaths;
    ASRConveyorBeltActor* Actor = nullptr;
    bool bDirty = false;
};

struct FSRConveyorActorGroupRuntimeState
{
    TMap<FName, FSRConveyorActorGroupState> GroupsByKey;
    TSet<FName> PendingPlacementDiagnosticKeys;
    TSet<FName> PendingDeletionDiagnosticKeys;

    bool HasDirtyGroups() const;
    void Reset();
};

USTRUCT()
struct STARROVERS_API FSRConveyorTransportRuntimeState
{
    GENERATED_BODY()

    UPROPERTY(Transient)
    TMap<FSRConveyorLaneKey, FSRConveyorItem> ItemsByLane;

    UPROPERTY(Transient)
    TMap<FSRConveyorLaneKey, TObjectPtr<UTextRenderComponent>> ItemLabelsByLane;

    int32 GetItemCount() const;
    bool HasItems() const;
    void ResetItems();
    void ResetLabels();
};
