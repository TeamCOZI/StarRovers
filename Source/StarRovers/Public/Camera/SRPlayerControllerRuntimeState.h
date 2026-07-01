#pragma once

#include "CoreMinimal.h"

struct FSRPlayerControllerRuntimeState
{
    bool bPendingInitialPrimaryStarFocus = true;
    uint64 LastPlacementRotationInputFrame = MAX_uint64;
    int32 LastPlacementRotationInputStepDelta = 0;
    bool bConveyorBulkDeleteModifierActive = false;
    bool bAssemblyShiftModifierActive = false;
    bool bAssemblyAreaDeletionDragHoldActive = false;
    bool bRuntimeAssemblyInputMappingApplied = false;

    bool IsDuplicatePlacementRotationInput(uint64 CurrentFrame, int32 StepDelta) const;
    void StorePlacementRotationInput(uint64 CurrentFrame, int32 StepDelta);
};
