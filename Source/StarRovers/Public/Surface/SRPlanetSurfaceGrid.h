#pragma once

#include "CoreMinimal.h"
#include "Components/DynamicMeshComponent.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "Surface/SRPlanetTerrainTypes.h"
#include "Visual/SRLineThicknessUtils.h"
#include "SRPlanetSurfaceGrid.generated.h"

namespace UE::Geometry
{
    class FDynamicMesh3;
}

class UMaterialInterface;

UCLASS(ClassGroup = (StarRovers), Blueprintable, meta = (BlueprintSpawnableComponent))
class STARROVERS_API USRPlanetSurfaceGrid : public UDynamicMeshComponent
{
    GENERATED_BODY()

public:
    USRPlanetSurfaceGrid();

    virtual void OnRegister() override;
    virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface")
    void RebuildGrid();

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface")
    void SetPlanetRadius(float NewPlanetRadius);

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface")
    void SetFaceResolution(int32 NewFaceResolution);

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface")
    void ClearOccupancy();

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    int32 GetFaceResolution() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    float GetPlanetRadius() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    int32 GetCellCount() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    TArray<FSRPlanetSurfaceGridCell> GetCells() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    bool GetCellById(const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCell& OutCell) const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    bool GetCellNeighbors(const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCellNeighbors& OutNeighbors) const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    bool GetCellWorldTransform(const FSRPlanetSurfaceGridCellId& CellId, float HeightOffset, FTransform& OutTransform) const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    bool GetCellWorldCorners(const FSRPlanetSurfaceGridCellId& CellId, FVector& OutCorner00, FVector& OutCorner10, FVector& OutCorner11, FVector& OutCorner01) const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    bool ProjectWorldLocationToCell(const FVector& WorldLocation, FSRPlanetSurfaceGridCell& OutCell) const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    bool RaycastCell(const FVector& RayOrigin, const FVector& RayDirection, FSRPlanetSurfaceGridCell& OutCell, FVector& OutHitLocation) const;

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface")
    bool SetCellOccupied(const FSRPlanetSurfaceGridCellId& CellId, bool bOccupied, FName OccupantId);

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface")
    bool SetHoveredCell(const FSRPlanetSurfaceGridCellId& CellId);

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface")
    void ClearHoveredCell();

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    bool HasHoveredCell() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    bool GetHoveredCell(FSRPlanetSurfaceGridCell& OutCell) const;

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface")
    bool SetSelectedCell(const FSRPlanetSurfaceGridCellId& CellId);

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface")
    void ClearSelectedCell();

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    bool HasSelectedCell() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    bool GetSelectedCell(FSRPlanetSurfaceGridCell& OutCell) const;

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface|Debug")
    void DrawDebugGrid(float Duration = 0.0f) const;

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface|Debug")
    void SetGridVisible(bool bNewGridVisible);

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface|Debug")
    void ConfigureDebugGrid(
        FLinearColor NewGridLineColor,
        float NewGridLineOpacity,
        float NewLineThickness,
        FLinearColor NewHoveredCellColor,
        FLinearColor NewSelectedCellColor,
        FLinearColor NewOccupiedCellColor,
        float NewSurfaceOffset);

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface|Debug")
    void SetGridMaterial(UMaterialInterface* NewGridMaterial);

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface")
    void ConfigureConstructionHeightOffset(float NewConstructionHeightOffset);

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface|Debug")
    bool IsGridVisible() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "StarRovers|Surface|Terrain")
    float GetSurfaceHeightOffsetAtDirection(FVector LocalUnitDirection) const;

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface|Terrain")
    void ConfigureProceduralTerrain(bool bNewUseProceduralTerrain, int32 NewTerrainSeed, float NewTerrainHeight, float NewTerrainFrequency, int32 NewTerrainOctaves, float NewTerrainPersistence);

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface|Terrain")
    void ConfigureTerrain(const FSRPlanetTerrainSettings& NewTerrainSettings);

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface|Terrain")
    FSRPlanetTerrainSample GetTerrainSampleAtDirection(FVector LocalUnitDirection) const;

protected:
    UPROPERTY()
    int32 FaceResolution;

    UPROPERTY()
    float PlanetRadius;

    UPROPERTY()
    bool bRebuildGridOnRegister;

    UPROPERTY()
    float ConstructionHeightOffset;

    UPROPERTY()
    bool bDrawDebugGrid;

    UPROPERTY()
    bool bGridVisible;

    UPROPERTY()
    FLinearColor DebugLineColor;

    UPROPERTY()
    float DebugLineOpacity;

    UPROPERTY()
    FLinearColor HoveredCellColor;

    UPROPERTY()
    FLinearColor SelectedCellColor;

    UPROPERTY()
    FLinearColor OccupiedCellColor;

    UPROPERTY()
    float DebugLineThickness;

    UPROPERTY()
    float GridSurfaceOffset;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Surface|Debug", meta = (DisplayName = "GridMaterial"))
    TObjectPtr<UMaterialInterface> GridMaterial;

    UPROPERTY()
    FSRPlanetTerrainSettings TerrainSettings;

    UPROPERTY()
    TArray<FSRPlanetSurfaceGridCell> Cells;

    UPROPERTY()
    bool bHasHoveredCell;

    UPROPERTY()
    FSRPlanetSurfaceGridCellId HoveredCellId;

    UPROPERTY()
    bool bHasSelectedCell;

    UPROPERTY()
    FSRPlanetSurfaceGridCellId SelectedCellId;

private:
    bool GetCellIndex(const FSRPlanetSurfaceGridCellId& CellId, int32& OutIndex) const;
    void RebuildCellIndex();
    void UpdateDebugTickState();
    void RebuildGridMesh();
    bool AppendOwnerDynamicMeshWire(UE::Geometry::FDynamicMesh3& GridMesh, const FLinearColor& LineColor, float LineThickness) const;
    void AppendGridWireCell(UE::Geometry::FDynamicMesh3& GridMesh, const FSRPlanetSurfaceGridCell& Cell, const FLinearColor& LineColor, float LineThickness, bool bIncludeInEdgeSet, TSet<uint64>* DrawnEdges) const;
    void AppendGridWireEdge(UE::Geometry::FDynamicMesh3& GridMesh, const FVector& LocalDirectionA, const FVector& LocalDirectionB, const FLinearColor& LineColor, float LineThickness) const;
    void AppendGridWireSegment(UE::Geometry::FDynamicMesh3& GridMesh, const FVector& LocalPointA, const FVector& LocalPointB, const FLinearColor& LineColor, float LineThickness) const;
    void ApplyGridMaterial();
    float GetEffectiveWorldRadius() const;
    void DrawDebugSurfaceLine(const FVector& LocalDirectionA, const FVector& LocalDirectionB, const FColor& LineColor, float Duration, float LineThickness, const FSRCameraInfo& CameraInfo, float ReferenceViewDepth, float ReferenceFieldOfViewDegrees) const;
    FVector ResolveLocalSurfacePoint(const FVector& LocalUnitDirection, float HeightOffset = 0.0f) const;
    FVector ResolveWorldSurfacePoint(const FVector& LocalUnitDirection, float HeightOffset = 0.0f) const;
    float ComputeProceduralTerrainHeight(FVector LocalUnitDirection) const;
    FVector ComputeProceduralSurfaceNormal(FVector LocalUnitDirection) const;
    bool IntersectRayWithSurfaceSphere(const FVector& RayOrigin, const FVector& RayDirection, FVector& OutHitLocation) const;

    TMap<FSRPlanetSurfaceGridCellId, int32> CellIndexById;
};
