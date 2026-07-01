#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Assembly/SRAssemblyAreaCopy.h"
#include "Assembly/SRAssemblyAreaSelection.h"
#include "Assembly/SRAssemblyPlacementHistory.h"
#include "Assembly/SRAssemblyPlacementQueue.h"
#include "Conveyor/SRConveyorTypes.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "SRAssemblyComponent.generated.h"

class ASRConveyorBeltActor;
class ASRPlayerController;
class USRConveyorNetworkComponent;
class USRPlanetSurfaceGrid;
class USRStructureDataAsset;
class USRStructureInstanceManagerComponent;
class UMaterialInterface;
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
	bool ShouldHandleAreaSelectionDrag() const;
	bool ShouldHandleAreaDeletionDrag() const;
	bool IsAreaSelectionDragActive() const;
	bool IsAreaDeletionDragActive() const;
	bool BeginStructurePlacementDrag(AActor*& OutSelectedActor);
	bool ContinueStructurePlacementDrag(AActor*& OutSelectedActor);
	void EndStructurePlacementDrag(bool bCommitConveyorDrag = false);
	bool BeginAreaSelectionDrag(AActor*& OutSelectedActor);
	bool ContinueAreaSelectionDrag(AActor*& OutSelectedActor);
	void EndAreaSelectionDrag();
	void ClearAreaSelection();
	bool TryDeleteAreaSelection();
	bool IsAreaCopyPlacementActive() const;
	bool TryBeginAreaSelectionCopyPlacement();
	bool MirrorAreaCopyPlacement(bool bMirrorLeftRight);
	void CancelAreaCopyPlacement();
	bool BeginAreaDeletionDrag(AActor*& OutSelectedActor);
	bool ContinueAreaDeletionDrag(AActor*& OutSelectedActor);
	void EndAreaDeletionDrag();
	void ClearAreaDeletion();
	void ClearSurfaceGridInteraction(AActor* SurfaceActor);
	void ClearSurfaceHover();
	void ClearSelectedStructureFocus();
	void CancelSelectedStructurePlacement();
	bool RotateStructurePlacement(int32 StepDelta);
	bool TryUndoAssemblyPlacement();
	bool TryRedoAssemblyPlacement();
	bool TryAddConveyorPlacementDragWaypoint();
	int32 GetStructurePlacementRotationSteps() const;
	float GetStructurePlacementAdditionalYawDegrees() const;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Surface", meta = (DisplayName = "HoveredSurfaceGrid"))
	TObjectPtr<USRPlanetSurfaceGrid> HoveredSurfaceGrid;

	bool bAssemblyModeActive;

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
	TObjectPtr<USRPlanetSurfaceGrid> ConveyorInvalidPlacementPreviewSurfaceGrid;

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
	TObjectPtr<USRPlanetSurfaceGrid> StructurePlacementDragSurfaceGrid;

	FSRPlanetSurfaceGridCellId StructurePlacementDragStartCellId;
	int32 StructurePlacementDragRotationSteps;
	TArray<FSRPlanetSurfaceGridCellId> StructurePlacementDragCellIds;
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
	bool bHasConveyorInvalidPlacementPreview;
	int32 ConveyorDeletionGhostLayer;

private:
	friend class StarRovers::Assembly::FSRAssemblyPlacementHistory;

	using ESRAssemblyPlacementHistoryKind = StarRovers::Assembly::ESRAssemblyPlacementHistoryKind;
	using FSRAssemblyPlacementHistoryEntry = StarRovers::Assembly::FSRAssemblyPlacementHistoryEntry;
	using FSRRestorableNaturalStructure = StarRovers::Assembly::FSRRestorableNaturalStructure;
	using ESRAreaCopyPlacementPreviewState = StarRovers::Assembly::ESRAreaCopyPlacementPreviewState;
	using FSRAreaCopiedStructure = StarRovers::Assembly::FSRAreaCopiedStructure;
	using FSRAreaCopiedConveyorPath = StarRovers::Assembly::FSRAreaCopiedConveyorPath;
	using FSRAreaCopyPlacementEvaluation = StarRovers::Assembly::FSRAreaCopyPlacementEvaluation;
	using FSRQueuedStructurePlacement = StarRovers::Assembly::FSRQueuedStructurePlacement;

	struct FSRStructurePlacementDragPreviewActor
	{
		FSRPlanetSurfaceGridCellId CellId;
		TWeakObjectPtr<AActor> PreviewActor;
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
	void SetConveyorInvalidPlacementPreview(USRPlanetSurfaceGrid* SurfaceGrid, const TArray<FSRPlanetSurfaceGridCellId>& CellIds);
	void ClearConveyorInvalidPlacementPreview();
	bool UpdateConveyorDeletionGhostPreview(
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRConveyorNetworkComponent* ConveyorNetwork,
		const FSRPlanetSurfaceGridCellId& TargetCellId,
		int32 Layer,
		const TArray<FSRConveyorVisualPath>& VisualPaths);
	void DestroyConveyorDeletionGhostPreview();
	void UpdateStructureGhostPreview();
	void DestroyStructureGhostPreview();
	bool UpdateStructurePlacementDragPreview(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& TargetCell);
	void DestroyStructurePlacementDragPreviewActors();
	AActor* SpawnStructurePlacementDragPreviewActor(USRPlanetSurfaceGrid* SurfaceGrid, USRStructureDataAsset* StructureDataAsset);
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
	bool BuildAreaSelectionCellIds(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& StartCellId,
		const FSRPlanetSurfaceGridCellId& EndCellId,
		TArray<FSRPlanetSurfaceGridCellId>& OutCellIds) const;
	bool ResolveAreaSelectionCenterCellId(FSRPlanetSurfaceGridCellId& OutCenterCellId) const;
	bool UpdateAreaSelection(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& TargetCell);
	void ApplyAreaSelectionGhosts(USRPlanetSurfaceGrid* SurfaceGrid, const TArray<FSRPlanetSurfaceGridCellId>& CellIds);
	bool HasAreaCopyPayload() const;
	void DestroyAreaCopyPreviewActors();
	void RebuildAreaCopyPreviewActors();
	void UpdateAreaCopyPlacementPreview();
	void UpdateAreaCopyStructurePreviewActors(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const TArray<FSRPlanetSurfaceGridCellId>& TargetOriginCellIds,
		ESRAreaCopyPlacementPreviewState PreviewState);
	bool BuildAreaCopyPlacementEvaluation(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCellId& HoverCellId,
		FSRAreaCopyPlacementEvaluation& OutEvaluation) const;
	bool TryCommitAreaCopyPlacement(AActor*& OutSelectedActor);
	void ApplyAreaCopyPreviewState(ESRAreaCopyPlacementPreviewState PreviewState);
	UMaterialInterface* ResolveAreaCopyPreviewMaterial(USRStructureDataAsset* StructureDataAsset, ESRAreaCopyPlacementPreviewState PreviewState) const;
	void ApplyAreaCopyPreviewMaterial(AActor* PreviewActor, UMaterialInterface* Material) const;
	void CollectReplaceableOccupiedCellIds(USRStructureInstanceManagerComponent* StructureInstanceManager, const TSet<FName>& OccupantIds, TSet<FSRPlanetSurfaceGridCellId>& OutCellIds) const;
	bool TryBuildConnectedAreaCopyConveyorPath(
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRConveyorNetworkComponent* ConveyorNetwork,
		const FSRConveyorVisualPath& TargetVisualPath,
		const TSet<FSRPlanetSurfaceGridCellId>& IgnoredOccupiedCellIds,
		FSRConveyorVisualPath& OutVisualPath) const;
	bool UpdateAreaDeletion(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& TargetCell);
	void ApplyAreaDeletionPreview(USRPlanetSurfaceGrid* SurfaceGrid, const TArray<FSRPlanetSurfaceGridCellId>& CellIds);
	bool CommitAreaDeletion();
	bool DeleteAreaCells(USRPlanetSurfaceGrid* SurfaceGrid, const TArray<FSRPlanetSurfaceGridCellId>& CellIds);
	void CollectAreaDeletionTargetOccupantIds(USRPlanetSurfaceGrid* SurfaceGrid, const TArray<FSRPlanetSurfaceGridCellId>& CellIds, TSet<FName>& OutOccupantIds) const;
	bool CommitStructurePlacementDrag();
	bool CommitConveyorPlacementDrag();
	bool TryPlaceSelectedStructure(
		USRPlanetSurfaceGrid* SurfaceGrid,
		const FSRPlanetSurfaceGridCell& TargetCell,
		bool bRefreshPreviewAndUI = true,
		int32 PlacementRotationStepsOverride = INDEX_NONE,
		TArray<FSRAssemblyPlacementHistoryEntry>* OutHistoryEntries = nullptr);
	bool TryPlaceSelectedConveyor(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& TargetCell, USRStructureDataAsset* ConveyorDataAsset, bool bRefreshPreviewAndUI = true);
	bool TryPlaceSelectedConveyorPath(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& StartCellId, const FSRPlanetSurfaceGridCell& TargetCell, USRStructureDataAsset* ConveyorDataAsset, bool bRefreshPreviewAndUI = true);
	bool TryDeleteStructureAtCell(AActor* FocusedActor, USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& TargetCellId);
	bool TryDeleteConnectedConveyorsAtCell(AActor* FocusedActor, USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& TargetCellId);
	bool TryDestroyAttachedOccupantActor(AActor* SurfaceOwner, FName OccupantId) const;
	void ClearPendingConveyorPathStart();
	void BuildCandidateConveyorLayers(TArray<int32>& OutLayers) const;
	void LogInvalidGhostDataAssetOnce(USRStructureDataAsset* StructureDataAsset, const TCHAR* Reason);
	void ClearAssemblyPlacementHistory();
	void PushAssemblyPlacementHistoryEntry(USRPlanetSurfaceGrid* SurfaceGrid, const FSRAssemblyPlacementHistoryEntry& Entry);
	void RecordAssemblyPlacementHistoryBatch(USRPlanetSurfaceGrid* SurfaceGrid, const TArray<FSRAssemblyPlacementHistoryEntry>& Entries);
	void RecordStructurePlacementHistory(
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRStructureInstanceManagerComponent* StructureInstanceManager,
		USRStructureDataAsset* StructureDataAsset,
		const FSRPlanetSurfaceGridCellId& OriginCellId,
		int32 PlacementRotationSteps,
		FName OccupantId);
	void RecordConveyorPlacementHistory(
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRConveyorNetworkComponent* ConveyorNetwork,
		const FSRConveyorVisualPath& VisualPath,
		const TArray<FSRPlanetSurfaceGridCellId>& PlacedCellIds,
		const TArray<FSRRestorableNaturalStructure>& RemovedNaturalStructures);
	void BuildConveyorPlacementHistoryPayload(
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRConveyorNetworkComponent* ConveyorNetwork,
		USRStructureDataAsset* StructureDataAsset,
		const TArray<FSRPlanetSurfaceGridCellId>& PathCellIds,
		int32 Layer,
		float LayerHeight,
		FName NetworkId,
		FSRConveyorVisualPath& OutVisualPath,
		TArray<FSRPlanetSurfaceGridCellId>& OutPlacedCellIds,
		TArray<FSRRestorableNaturalStructure>& OutRemovedNaturalStructures) const;
	TArray<FSRStructurePlacementDragPreviewActor> StructurePlacementDragPreviewActors;
	StarRovers::Assembly::FSRAssemblyAreaSelection AreaSelection;
	StarRovers::Assembly::FSRAssemblyAreaCopy AreaCopy;
	StarRovers::Assembly::FSRAssemblyPlacementHistory PlacementHistory;
	StarRovers::Assembly::FSRAssemblyPlacementQueue PlacementQueue;
};
