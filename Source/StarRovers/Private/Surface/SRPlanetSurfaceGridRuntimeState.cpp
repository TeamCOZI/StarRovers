#include "Surface/SRPlanetSurfaceGridRuntimeState.h"

void FSRPlanetSurfaceGridInteractionBatchState::Begin()
{
    ++Depth;
}

bool FSRPlanetSurfaceGridInteractionBatchState::EndAndShouldRefresh()
{
    Depth = FMath::Max(0, Depth - 1);
    if (Depth == 0 && bHasBatchedHighlightRefresh)
    {
        bHasBatchedHighlightRefresh = false;
        return true;
    }

    return false;
}

bool FSRPlanetSurfaceGridInteractionBatchState::IsActive() const
{
    return Depth > 0;
}

void FSRPlanetSurfaceGridInteractionBatchState::MarkHighlightRefreshPending()
{
    bHasBatchedHighlightRefresh = true;
}
