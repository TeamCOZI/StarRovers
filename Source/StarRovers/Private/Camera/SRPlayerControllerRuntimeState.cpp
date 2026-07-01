#include "Camera/SRPlayerControllerRuntimeState.h"

bool FSRPlayerControllerRuntimeState::IsDuplicatePlacementRotationInput(uint64 CurrentFrame, int32 StepDelta) const
{
    return LastPlacementRotationInputFrame == CurrentFrame && LastPlacementRotationInputStepDelta == StepDelta;
}

void FSRPlayerControllerRuntimeState::StorePlacementRotationInput(uint64 CurrentFrame, int32 StepDelta)
{
    LastPlacementRotationInputFrame = CurrentFrame;
    LastPlacementRotationInputStepDelta = StepDelta;
}
