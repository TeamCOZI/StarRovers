#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "SRPlanetSurfaceGridLibrary.generated.h"

UCLASS()
class STARROVERS_API USRPlanetSurfaceGridLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    static bool IsValidCubeSphereCellId(const FSRPlanetSurfaceGridCellId& CellId, int32 Resolution);

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    static FText GetCubeSphereFaceText(ESRCubeSphereFace Face);

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    static FVector GetSpherifiedCubeDirection(ESRCubeSphereFace Face, float FaceU, float FaceV);

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    static bool BuildCubeSphereCell(int32 Resolution, float Radius, const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCell& OutCell);

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    static bool ProjectDirectionToCubeSphereCellId(const FVector& UnitDirection, int32 Resolution, FSRPlanetSurfaceGridCellId& OutCellId, FVector2D& OutFaceCoordinates);

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    static FSRPlanetSurfaceGridCellNeighbors GetCubeSphereNeighborIds(const FSRPlanetSurfaceGridCellId& CellId, int32 Resolution);

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    static TArray<FSRPlanetSurfaceGridCell> GenerateCubeSphereCells(int32 Resolution, float Radius);
};
