#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Conveyor/SRConveyorTypes.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "SRAssemblyComponent.generated.h"

class ASRConveyorBeltActor;
class ASRPlayerController;
class USRConveyorNetworkComponent;
class USRPlanetSurfaceGrid;
class USRStructureDataAsset;
struct FSRStructureData;

UCLASS(ClassGroup = (StarRovers), Blueprintable, meta = (BlueprintSpawnableComponent))
class STARROVERS_API USRAssemblyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USRAssemblyComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Assembly")
	bool IsAssemblyModeActive() const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Assembly")
	void SetAssemblyModeActive(bool bNewAssemblyModeActive);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Assembly")
	void ToggleAssemblyMode();

	void ConfigurePlacementPerformance(int32 NewMaxStructurePlacementsPerFrame, int32 NewMaxQueuedStructurePlacements);

	bool TryHandleAssemblyClick(AActor*& OutSelectedActor);
	bool TryHandleAssemblyDelete(AActor*& OutSelectedActor);
	bool ShouldHandleStructurePlacementDrag() const;
	bool BeginStructurePlacementDrag(AActor*& OutSelectedActor);
	bool ContinueStructurePlacementDrag(AActor*& OutSelectedActor);
	void EndStructurePlacementDrag(bool bCommitConveyorDrag = false);
	void ClearSurfaceGridInteraction(AActor* SurfaceActor);
	void ClearSurfaceHover();
	void ClearSelectedStructureFocus();
	bool RotateStructurePlacement(int32 StepDelta);
	bool TryAddConveyorPlacementDragWaypoint();
	int32 GetStructurePlacementRotationSteps() const;
	float GetStructurePlacementAdditionalYawDegrees() const;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Surface", meta = (DisplayName = "HoveredSurfaceGrid"))
	TObjectPtr<USRPlanetSurfaceGrid> HoveredSurfaceGrid;

	bool bAssemblyModeActive;
	int32 MaxStructurePlacementsPerFrame;
	int32 MaxQueuedStructurePlacements;

	UPROPERTY(Transient)
	TObjectPtr<USRPlanetSurfaceGrid> ActiveAssemblySurfaceGrid;

	UPROPERTY(Transient)
	TObjectPtr<USRPlanetSurfaceGrid> LastHoveredSampleSurfaceGrid;

	UPROPERTY(Transient)
	TObjectPtr<USRPlanetSurfaceGrid> LastPublishedHoveredSurfaceGrid;

	UPROPERTY(Transient)
	TObjectPtr<AActor> StructureGhostActor;

	UPROPERTY(Transient)
	TObjectPtr<USRStructureDataAsset> StructureGhostDataAsset;

	UPROPERTY(Transient)
	TObjectPtr<USRPlanetSurfaceGrid> StructureGhostPortPreviewSurfaceGrid;

	UPROPERTY(Transient)
	TObjectPtr<USRPlanetSurfaceGrid> ConveyorPortPreviewSurfaceGrid;

	UPROPERTY(Transient)
	TObjectPtr<ASRConveyorBeltActor> ConveyorGhostActor;

	UPROPERTY(Transient)
	TObjectPtr<ASRConveyorBeltActor> ConveyorDeletionGhostActor;

	UPROPERTY(Transient)
	TObjectPtr<USRStructureDataAsset> ConveyorGhostDataAsset;

	UPROPERTY(Transient)
	TObjectPtr<USRStructureDataAsset> ConveyorDeletionGhostDataAsset;

	UPROPERTY(Transient)
	TObjectPtr<USRPlanetSurfaceGrid> ConveyorGhostSurfaceGrid;

	UPROPERTY(Transient)
	TObjectPtr<USRPlanetSurfaceGrid> ConveyorDeletionGhostSurfaceGrid;

	UPROPERTY(Transient)
	TObjectPtr<USRPlanetSurfaceGrid> ConveyorBulkDeletionPreviewSurfaceGrid;

	UPROPERTY(Transient)
	TObjectPtr<USRStructureDataAsset> LastLoggedInvalidGhostDataAsset;

	FVector2D LastHoveredSampleMousePosition;
	bool bHasLastHoveredSampleMousePosition;
	bool bHasLastPublishedHoveredCellInfo;
	FSRPlanetSurfaceGridCellId LastPublishedHoveredCellId;
	FSRPlanetSurfaceGridCellId StructureGhostCellId;
	bool bHasStructureGhostCellId;
	bool bHasStructureGhostPortPreview;
	bool bHasConveyorPortPreview;
	bool bIsStructurePlacementDragActive;
	bool bIsConveyorPlacementDragActive;
	int32 StructurePlacementRotationSteps;

	UPROPERTY(Transient)
	TObjectPtr<USRPlanetSurfaceGrid> LastStructurePlacementDragSurfaceGrid;

	FSRPlanetSurfaceGridCellId LastStructurePlacementDragCellId;
	bool bHasLastStructurePlacementDragCellId;

	UPROPERTY(Transient)
	TObjectPtr<USRPlanetSurfaceGrid> PendingConveyorStartSurfaceGrid;

	FSRPlanetSurfaceGridCellId PendingConveyorStartCellId;
	bool bHasPendingConveyorStartCell;

	UPROPERTY(Transient)
	TObjectPtr<USRPlanetSurfaceGrid> ConveyorDragStartSurfaceGrid;

	TArray<FSRPlanetSurfaceGridCellId> ConveyorDragWaypointCellIds;
	FSRPlanetSurfaceGridCellId ConveyorDragStartCellId;
	FSRPlanetSurfaceGridCellId ConveyorGhostTargetCellId;
	FSRPlanetSurfaceGridCellId ConveyorDeletionGhostTargetCellId;
	bool bHasConveyorDragStartCell;
	bool bHasConveyorGhostTargetCell;
	bool bHasConveyorDeletionGhostTargetCell;
	bool bHasConveyorBulkDeletionPreview;
	int32 ConveyorDeletionGhostLayer;

private:
	struct FSRQueuedStructurePlacement
	{
		TWeakObjectPtr<USRPlanetSurfaceGrid> SurfaceGrid;
		FSRPlanetSurfaceGridCellId CellId;
		int32 PlacementRotationSteps = 0;
	};

	ASRPlayerController* GetOwnerController() const;
	bool GetCursorRay(FVector& OutRayOrigin, FVector& OutRayDirection) const;
	bool TryGetFocusedSurfaceGrid(AActor*& OutFocusedActor, USRPlanetSurfaceGrid*& OutSurfaceGrid) const;
	bool TryProjectCursorToSurfaceCell(USRPlanetSurfaceGrid* SurfaceGrid, FSRPlanetSurfaceGridCell& OutCell, FVector& OutHitLocation) const;
	void UpdateSurfaceHover();
	void ClearSurfaceHoverPreview();
	void ProcessQueuedStructurePlacements();
	void ApplyAssemblyModeToFocusedSurfaceGrid();
	void ResetHoverSampleCache();
	void PublishHoveredCellInfo(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& HoveredCell);
	void ClearPublishedHoveredCellInfo();
	bool TryPublishSelectedStructureInfo(AActor* FocusedActor, USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& ClickedCell);
	void ClearSelectedStructureInfo();
	void UpdateConveyorPlacementPortPreview();
	void ClearConveyorPlacementPortPreview();
	bool UpdateConveyorBulkDeletionPreview();
	void ClearConveyorBulkDeletionPreview();
	bool UpdateConveyorDeletionGhostPreview(
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRConveyorNetworkComponent* ConveyorNetwork,
		const FSRPlanetSurfaceGridCellId& TargetCellId,
		int32 Layer,
		const TArray<FSRConveyorVisualPath>& VisualPaths);
	void DestroyConveyorDeletionGhostPreview();
	void UpdateStructureGhostPreview();
	void DestroyStructureGhostPreview();
	bool UpdateConveyorGhostPreview(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& TargetCell, USRStructureDataAsset* ConveyorDataAsset);
	void DestroyConveyorGhostPreview();
	void UpdateStructureGhostPortPreview(USRPlanetSurfaceGrid* SurfaceGrid, const FSRStructureData& StructureData, const TArray<FSRPlanetSurfaceGridCellId>& FootprintCellIds, int32 PlacementRotationSteps);
	void ClearStructureGhostPortPreview();
	bool BuildStructureGhostTransform(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& CellId, USRStructureDataAsset* StructureDataAsset, FTransform& OutTransform) const;
	void PublishStructureGhostPlacementDebug(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& HoveredCell, const FTransform& GhostTransform, float StructureHeightOffset, bool bLogDebug) const;
	bool TryResolveStructurePlacementDragTarget(AActor*& OutFocusedActor, USRPlanetSurfaceGrid*& OutSurfaceGrid, FSRPlanetSurfaceGridCell& OutTargetCell) const;
	bool TryGetFocusedConveyorNetwork(AActor*& OutFocusedActor, USRConveyorNetworkComponent*& OutConveyorNetwork) const;
	void EnqueueStructurePlacement(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& CellId);
	bool TryPlaceStructureDragPath(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& TargetCell);
	bool TryPlaceConveyorDragPath(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& TargetCell, USRStructureDataAsset* ConveyorDataAsset);
	bool BuildConveyorPlacementDragPath(
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRConveyorNetworkComponent* ConveyorNetwork,
		const FSRStructureData& ConveyorData,
		const FSRPlanetSurfaceGridCellId& TargetCellId,
		TArray<FSRPlanetSurfaceGridCellId>& OutPathCellIds) const;
	bool CommitConveyorPlacementDrag();
	bool TryPlaceSelectedStructure(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& TargetCell, bool bRefreshPreviewAndUI = true, int32 PlacementRotationStepsOverride = INDEX_NONE);
	bool TryPlaceSelectedConveyor(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& TargetCell, USRStructureDataAsset* ConveyorDataAsset, bool bRefreshPreviewAndUI = true);
	bool TryPlaceSelectedConveyorPath(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& StartCellId, const FSRPlanetSurfaceGridCell& TargetCell, USRStructureDataAsset* ConveyorDataAsset, bool bRefreshPreviewAndUI = true);
	bool TryDeleteStructureAtCell(AActor* FocusedActor, USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& TargetCellId);
	bool TryDeleteConnectedConveyorsAtCell(AActor* FocusedActor, USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& TargetCellId);
	bool TryDestroyAttachedOccupantActor(AActor* SurfaceOwner, FName OccupantId) const;
	void ClearPendingConveyorPathStart();
	void BuildCandidateConveyorLayers(TArray<int32>& OutLayers) const;
	void LogInvalidGhostDataAssetOnce(USRStructureDataAsset* StructureDataAsset, const TCHAR* Reason);

	TArray<FSRQueuedStructurePlacement> PendingStructurePlacementQueue;
};
