#pragma once

#include "CoreMinimal.h"
#include "Components/DynamicMeshComponent.h"
#include "Surface/SRPlanetSurfaceGridRuntimeState.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "Surface/SRPlanetTerrainTypes.h"
#include "Rendering/SRScreenSpaceLineThickness.h"
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

    const TArray<FSRPlanetSurfaceGridCell>& GetCellsRef() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    bool GetCellById(const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCell& OutCell) const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    bool GetCellInfoById(const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCellInfo& OutCellInfo) const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    bool GetCellNeighbors(const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCellNeighbors& OutNeighbors) const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    bool GetFootprintCellIds(const FSRPlanetSurfaceGridCellId& OriginCellId, int32 FootprintCellsX, int32 FootprintCellsY, TArray<FSRPlanetSurfaceGridCellId>& OutCellIds) const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    bool GetCellWorldTransform(const FSRPlanetSurfaceGridCellId& CellId, float HeightOffset, FTransform& OutTransform) const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    bool GetCellWorldCorners(const FSRPlanetSurfaceGridCellId& CellId, FVector& OutCorner00, FVector& OutCorner10, FVector& OutCorner11, FVector& OutCorner01) const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    bool ProjectWorldLocationToCell(const FVector& WorldLocation, FSRPlanetSurfaceGridCell& OutCell) const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    bool RaycastCell(const FVector& RayOrigin, const FVector& RayDirection, FSRPlanetSurfaceGridCell& OutCell, FVector& OutHitLocation) const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface|Temperature")
    static ESRFacilityTemperatureState ResolveTemperatureStateFromSurfaceTemperature(float SurfaceTemperature);

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface|Temperature")
    bool GetCellTemperatureState(const FSRPlanetSurfaceGridCellId& CellId, ESRFacilityTemperatureState& OutTemperatureState) const;

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface|Temperature")
    bool SetCellTemperatureState(const FSRPlanetSurfaceGridCellId& CellId, ESRFacilityTemperatureState TemperatureState);

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface|Temperature")
    bool SetCellSurfaceTemperature(const FSRPlanetSurfaceGridCellId& CellId, float SurfaceTemperature);

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface")
    bool SetCellOccupied(const FSRPlanetSurfaceGridCellId& CellId, bool bOccupied, FName OccupantId);

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    bool CanOccupyCells(const TArray<FSRPlanetSurfaceGridCellId>& CellIds) const;

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface")
    bool SetCellsOccupied(const TArray<FSRPlanetSurfaceGridCellId>& CellIds, bool bOccupied, FName OccupantId);

    void BeginInteractionHighlightBatch();
    void EndInteractionHighlightBatch();

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface")
    bool SetHoveredCell(const FSRPlanetSurfaceGridCellId& CellId);

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface")
    void SetHoveredInteractionGridPatchVisible(bool bNewVisible);

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface")
    void ClearHoveredCell();

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    bool HasHoveredCell() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    bool GetHoveredCell(FSRPlanetSurfaceGridCell& OutCell) const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    bool GetHoveredCellInfo(FSRPlanetSurfaceGridCellInfo& OutCellInfo) const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface|Debug")
    bool GetInteractionGridPatchCellIds(const FSRPlanetSurfaceGridCellId& CenterCellId, TArray<FSRPlanetSurfaceGridCellId>& OutCellIds) const;

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface")
    bool SetSelectedCell(const FSRPlanetSurfaceGridCellId& CellId);

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface")
    void ClearSelectedCell();

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface|Selection")
    void SetAreaSelectionCells(const TArray<FSRPlanetSurfaceGridCellId>& CellIds);

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface|Selection")
    void ClearAreaSelectionCells();

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface|Facility")
    void SetFacilityPortPreviewCells(
        const TArray<FSRPlanetSurfaceGridCellId>& InputConnectionCellIds,
        const TArray<FSRPlanetSurfaceGridCellId>& OutputConnectionCellIds);

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface|Facility")
    void ClearFacilityPortPreviewCells();

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface")
    void SetOccupiedPreviewCells(const TArray<FSRPlanetSurfaceGridCellId>& CellIds);

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface")
    void ClearOccupiedPreviewCells();

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface|Deletion")
    void SetDeletionPreviewCells(const TArray<FSRPlanetSurfaceGridCellId>& CellIds);

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface|Deletion")
    void ClearDeletionPreviewCells();

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface|Replacement")
    void SetConstructionReplacementPreviewCells(const TArray<FSRPlanetSurfaceGridCellId>& CellIds);

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface|Replacement")
    void ClearConstructionReplacementPreviewCells();

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface|Invalid")
    void SetInvalidPreviewCells(const TArray<FSRPlanetSurfaceGridCellId>& CellIds);

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface|Invalid")
    void ClearInvalidPreviewCells();

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    bool HasSelectedCell() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    bool GetSelectedCell(FSRPlanetSurfaceGridCell& OutCell) const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface")
    bool GetSelectedCellInfo(FSRPlanetSurfaceGridCellInfo& OutCellInfo) const;

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface|Debug")
    void DrawDebugGrid(float Duration = 0.0f) const;

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface|Debug")
    void SetGridVisible(bool bNewGridVisible);

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface|Debug")
    void PrepareGridForAssembly();

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface|Debug")
    void ConfigureDebugGrid(
        FLinearColor NewGridLineColor,
        float NewGridLineOpacity,
        float NewLineThickness,
        FLinearColor NewHoveredCellColor,
        FLinearColor NewSelectedCellColor,
        FLinearColor NewOccupiedCellColor,
        float NewSurfaceOffset);

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface")
    void SetGridOverlayMaterial(UMaterialInterface* NewGridOverlayMaterial);

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface|Debug")
    bool IsGridVisible() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "StarRovers|Surface|Terrain")
    float GetSurfaceHeightOffsetAtDirection(FVector LocalUnitDirection) const;

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface|Terrain")
    void ConfigureProceduralTerrain(bool bNewDynamicMeshGeneration, int32 NewGenerationSeed, float NewDynamicMeshHeight, float NewDetailFrequency, int32 NewNoiseOctaves, float NewNoisePersistence);

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Surface|Terrain")
    void ConfigureTerrain(const FSRDynamicMeshGeneration& NewDynamicMeshGeneration);

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface|Terrain")
    FSRPlanetTerrainSample GetTerrainSampleAtDirection(FVector LocalUnitDirection) const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Surface|Terrain")
    float GetTerrainHeightStep() const;

    void AppendGeneratedGridCell(UE::Geometry::FDynamicMesh3& GridMesh, const FSRPlanetSurfaceGridCell& Cell, TSet<uint64>& DrawnEdges) const;
    void ApplyGeneratedGridBuild(TArray<FSRPlanetSurfaceGridCell>&& NewCells, UE::Geometry::FDynamicMesh3&& NewGridMesh);
    void ApplyGeneratedGridBuild(TArray<FSRPlanetSurfaceGridCell>&& NewCells, UE::Geometry::FDynamicMesh3&& NewGridMesh, TMap<FSRPlanetSurfaceGridCellId, int32>&& NewCellIndexById);
    void ApplyGeneratedGridBuild(TArray<FSRPlanetSurfaceGridCell>&& NewCells, UE::Geometry::FDynamicMesh3&& NewGridMesh, TArray<int32>&& NewCellIndexByFlatId);

protected:
    UPROPERTY()
    int32 FaceResolution;

    UPROPERTY()
    float PlanetRadius;

    UPROPERTY()
    bool bRebuildGridOnRegister;

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

    UPROPERTY()
    TObjectPtr<UMaterialInterface> GridOverlayMaterial;

    UPROPERTY()
    FSRDynamicMeshGeneration DynamicMeshGeneration;

    UPROPERTY()
    TArray<FSRPlanetSurfaceGridCell> Cells;

    UPROPERTY()
    bool bHasHoveredCell;

    UPROPERTY()
    FSRPlanetSurfaceGridCellId HoveredCellId;

    UPROPERTY()
    bool bHoveredInteractionGridPatchVisible;

    UPROPERTY()
    bool bHasSelectedCell;

    UPROPERTY()
    FSRPlanetSurfaceGridCellId SelectedCellId;

    UPROPERTY()
    TArray<FSRPlanetSurfaceGridCellId> AreaSelectionCellIds;

    UPROPERTY()
    FLinearColor AreaSelectionCellColor;

    UPROPERTY()
    TArray<FSRPlanetSurfaceGridCellId> InputPortPreviewCellIds;

    UPROPERTY()
    TArray<FSRPlanetSurfaceGridCellId> OutputPortPreviewCellIds;

    UPROPERTY()
    TArray<FSRPlanetSurfaceGridCellId> OccupiedPreviewCellIds;

    UPROPERTY()
    FLinearColor InputPortPreviewCellColor;

    UPROPERTY()
    FLinearColor OutputPortPreviewCellColor;

    UPROPERTY()
    TArray<FSRPlanetSurfaceGridCellId> DeletionPreviewCellIds;

    UPROPERTY()
    FLinearColor DeletionPreviewCellColor;

    UPROPERTY()
    TArray<FSRPlanetSurfaceGridCellId> ConstructionReplacementPreviewCellIds;

    UPROPERTY()
    TArray<FSRPlanetSurfaceGridCellId> InvalidPreviewCellIds;

    UPROPERTY()
    FLinearColor InvalidPreviewCellColor;

    UPROPERTY()
    bool bUsingGeneratedGridCells;

    UPROPERTY()
    bool bGridMeshDirty;

    UPROPERTY()
    bool bCellsDirty;

    FSRPlanetSurfaceGridInteractionBatchState InteractionBatch;

private:
    void InitializeSurfaceGridDefaults();
    void ConfigureSurfaceGridComponentDefaults();
    void ApplyDefaultGridOverlayMaterial();
    void ApplyRegisteredGridOverlayMaterial();
    void RebuildGridOnRegisterIfNeeded();
    void RebuildGridIfVisible();
    void MarkGridMeshDirtyAndRefreshIfVisible();
    void RefreshInteractionIfGridVisible();
    bool RebuildCellsFromOwnerGeneratedGrid();
    void RebuildDefaultSurfaceCells();
    void ResetSurfaceInteractionState();
    void FinalizeGridRebuild();
    void ApplyGridVisibilityState();
    bool ShouldShowInteractionOverlayForCurrentState() const;
    void ClearInteractionStateForHiddenGrid();
    void PrepareGridForVisibleState();
    void EnsureAssemblyGridCellsReady();
    void ApplyEmptyPrimaryGridMeshIfNeeded();
    void ClearGeneratedGridBuildHighlights();
    void AssignGeneratedGridBuildCells(TArray<FSRPlanetSurfaceGridCell>&& NewCells);
    void ApplyGeneratedGridCellIndex(TArray<int32>&& NewCellIndexByFlatId);
    void RebuildGeneratedGridCellInfoIndex();
    void RebuildGeneratedGridRaycastIndex();
    void FinalizeGeneratedGridBuildMesh();
    void EnsureInteractionOverlay();
    void RequestInteractionHighlightRefresh();
    void RefreshInteractionHighlight();
    void RebuildInteractionOverlayMesh(bool bIncludeCellHighlightOverlay);
    void SetInteractionOverlayVisible(bool bNewVisible);
    void AppendInteractionGridPatch(UE::Geometry::FDynamicMesh3& OverlayMesh, const FSRPlanetSurfaceGridCellId& CenterCellId, const FLinearColor& BaseLineColor, float LineThickness, TSet<uint64>& DrawnEdges) const;
    bool GetCellIndex(const FSRPlanetSurfaceGridCellId& CellId, int32& OutIndex) const;
    int32 GetFlatCellIndex(const FSRPlanetSurfaceGridCellId& CellId) const;
    void RebuildCellIndex();
    void RebuildRaycastIndex();
    bool IsInteractionCellIdValid(const FSRPlanetSurfaceGridCellId& CellId) const;
    bool SetFocusedInteractionCell(const FSRPlanetSurfaceGridCellId& CellId, bool& bHasCell, FSRPlanetSurfaceGridCellId& StoredCellId);
    bool ClearFocusedInteractionCell(bool& bHasCell, FSRPlanetSurfaceGridCellId& StoredCellId);
    bool SetInteractionPreviewCellIds(const TArray<FSRPlanetSurfaceGridCellId>& CellIds, TArray<FSRPlanetSurfaceGridCellId>& StoredCellIds);
    bool ClearInteractionPreviewCellIds(TArray<FSRPlanetSurfaceGridCellId>& StoredCellIds);
    bool SetInteractionPortPreviewCellIds(
        const TArray<FSRPlanetSurfaceGridCellId>& InputConnectionCellIds,
        const TArray<FSRPlanetSurfaceGridCellId>& OutputConnectionCellIds);
    bool ClearInteractionPortPreviewCellIds();
    void NotifyInteractionStateChanged();
    void UpdateDebugTickState();
    void AppendInteractionCell(UE::Geometry::FDynamicMesh3& OverlayMesh, const FSRPlanetSurfaceGridCell& Cell, const FLinearColor& LineColor, float LineThickness) const;
    void AppendInteractionCellRegionBoundary(UE::Geometry::FDynamicMesh3& OverlayMesh, const TArray<FSRPlanetSurfaceGridCellId>& CellIds, const FLinearColor& LineColor, float LineThickness, bool bIncludeFill, TSet<uint64>* SharedDrawnEdges = nullptr) const;
    void AppendInteractionCellRegion(UE::Geometry::FDynamicMesh3& OverlayMesh, const TArray<FSRPlanetSurfaceGridCellId>& CellIds, const FLinearColor& LineColor, float LineThickness, bool bPreferCompactRectangles) const;
    bool TryAppendRectangularInteractionCellRegion(UE::Geometry::FDynamicMesh3& OverlayMesh, const TArray<FSRPlanetSurfaceGridCellId>& CellIds, const FLinearColor& LineColor, float LineThickness) const;
    void RebuildGridMesh();
    FSRPlanetSurfaceGridCellInfo BuildCellInfo(const FSRPlanetSurfaceGridCell& Cell) const;
    FSRPlanetSurfaceGridCellInfo ResolveRuntimeCellInfo(const FSRPlanetSurfaceGridCellInfo& CellInfo) const;
    void RebuildCellInfoIndex();
    bool GetStoredCellInfoById(const FSRPlanetSurfaceGridCellId& CellId, FSRPlanetSurfaceGridCellInfo& OutCellInfo) const;
    void StoreCellInfo(const FSRPlanetSurfaceGridCellInfo& CellInfo);
    bool AppendOwnerDynamicMeshWire(UE::Geometry::FDynamicMesh3& GridMesh, const FLinearColor& LineColor, float LineThickness) const;
    void AppendGridWireCell(UE::Geometry::FDynamicMesh3& GridMesh, const FSRPlanetSurfaceGridCell& Cell, const FLinearColor& LineColor, float LineThickness, bool bIncludeInEdgeSet, TSet<uint64>* DrawnEdges) const;
    void AppendGridWireEdge(UE::Geometry::FDynamicMesh3& GridMesh, const FVector& LocalDirectionA, const FVector& LocalDirectionB, const FLinearColor& LineColor, float LineThickness) const;
    void AppendGridWireSegment(UE::Geometry::FDynamicMesh3& GridMesh, const FVector& LocalPointA, const FVector& LocalPointB, const FLinearColor& LineColor, float LineThickness) const;
    float GetEffectiveWorldRadius() const;
    void DrawDebugSurfaceLine(const FVector& LocalDirectionA, const FVector& LocalDirectionB, const FColor& LineColor, float Duration, float LineThickness, const FSRScreenSpaceLineViewInfo& CameraInfo, float ReferenceViewDepth, float ReferenceTanHalfFieldOfView) const;
    FVector ResolveLocalSurfacePoint(const FVector& LocalUnitDirection, float HeightOffset = 0.0f) const;
    FVector ResolveWorldSurfacePoint(const FVector& LocalUnitDirection, float HeightOffset = 0.0f) const;
    float ComputeProceduralDynamicMeshHeight(FVector LocalUnitDirection) const;
    FVector ComputeProceduralSurfaceNormal(FVector LocalUnitDirection) const;
    bool IntersectRayWithSurfaceSphere(const FVector& RayOrigin, const FVector& RayDirection, FVector& OutHitLocation) const;

    FSRPlanetSurfaceGridCellIndexState CellIndexState;
    FSRPlanetSurfaceGridRaycastState RaycastState;

    UPROPERTY(Transient)
    TObjectPtr<UDynamicMeshComponent> InteractionOverlayMesh;
};
