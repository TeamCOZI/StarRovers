#pragma once

#include "CoreMinimal.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"

struct FSRPlanetSurfaceGridRaycastBucket
{
    ESRCubeSphereFace Face = ESRCubeSphereFace::PositiveX;
    int32 BucketX = 0;
    int32 BucketY = 0;
    FBox LocalBounds = FBox(ForceInit);
    TArray<int32> CellIndices;
};

struct FSRPlanetSurfaceGridCellIndexState
{
    TMap<FSRPlanetSurfaceGridCellId, int32> IndexById;
    TMap<FSRPlanetSurfaceGridCellId, FSRPlanetSurfaceGridCellInfo> InfoById;
    TArray<int32> IndexByFlatId;
    TArray<FSRPlanetSurfaceGridCellInfo> InfoByFlatId;
};

struct FSRPlanetSurfaceGridRaycastState
{
    TArray<FSRPlanetSurfaceGridRaycastBucket> Buckets;
};

struct FSRPlanetSurfaceGridInteractionBatchState
{
    int32 Depth = 0;
    bool bHasBatchedHighlightRefresh = false;

    void Begin();
    bool EndAndShouldRefresh();
    bool IsActive() const;
    void MarkHighlightRefreshPending();
};
