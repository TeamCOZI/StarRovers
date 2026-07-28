#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Assembly/SRAssemblyAreaCopy.h"
#include "Assembly/SRAssemblyAreaSelection.h"
#include "Assembly/SRAssemblyModeState.h"
#include "Assembly/SRAssemblyPlacementDragState.h"
#include "Assembly/SRAssemblyPlacementHistory.h"
#include "Assembly/SRAssemblyPlacementQueue.h"
#include "Assembly/SRAssemblyPreviewState.h"
#include "Assembly/SRAssemblySurfaceState.h"
#include "Assembly/SRAssemblyStructurePlacementPreview.h"
#include "Conveyor/SRConveyorTypes.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "SRAssemblyComponent.generated.h"

class ASRConveyorBeltActor;
class ASRPlayerController;
class USRConveyorNetworkComponent;
class USRPlanetSurfaceGrid;
class USRStructureDataAsset;
class USRStructureInstanceManagerComponent;
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
	bool RotateAreaCopyPlacement(int32 StepDelta);
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

	UFUNCTION(BlueprintPure, Category = "StarRovers|Assembly|Placement Preview")
	FSRStructurePlacementPreview GetSelectedStructurePlacementPreview() const;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Surface", meta = (DisplayName = "HoveredSurfaceGrid"))
	TObjectPtr<USRPlanetSurfaceGrid> HoveredSurfaceGrid;

	UPROPERTY(Transient)
	FSRAssemblyModeState ModeState;

	UPROPERTY(Transient)
	FSRAssemblySurfaceState SurfaceState;

	UPROPERTY(Transient)
	FSRAssemblyPlacementDragState PlacementDrag;

private:
	friend class StarRovers::Assembly::FSRAssemblyPlacementHistory;

	using ESRAssemblyPlacementHistoryKind = StarRovers::Assembly::ESRAssemblyPlacementHistoryKind;
	using FSRAssemblyPlacementHistoryEntry = StarRovers::Assembly::FSRAssemblyPlacementHistoryEntry;
	using FSRRestorableNaturalStructure = StarRovers::Assembly::FSRRestorableNaturalStructure;
	using ESRAssemblyAreaCopyPlacementPreviewState = StarRovers::Assembly::ESRAssemblyAreaCopyPlacementPreviewState;
	using FSRAssemblyAreaCopiedStructure = StarRovers::Assembly::FSRAssemblyAreaCopiedStructure;
	using FSRAssemblyAreaCopiedConveyorPath = StarRovers::Assembly::FSRAssemblyAreaCopiedConveyorPath;
	using FSRAssemblyAreaCopyPlacementEvaluation = StarRovers::Assembly::FSRAssemblyAreaCopyPlacementEvaluation;
	using FSRAssemblyAreaCopyCommitResult = StarRovers::Assembly::FSRAssemblyAreaCopyCommitResult;
	using FSRQueuedStructurePlacement = StarRovers::Assembly::FSRQueuedStructurePlacement;

	ASRPlayerController* GetOwnerController() const;
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
	bool UpdateConveyorBulkDeletionPreview();
	bool UpdateConveyorDeletionGhostPreview(
		USRPlanetSurfaceGrid* SurfaceGrid,
		USRConveyorNetworkComponent* ConveyorNetwork,
		const FSRPlanetSurfaceGridCellId& TargetCellId,
		int32 Layer,
		const TArray<FSRConveyorBeltPath>& BeltPaths);
	void UpdateStructureGhostPreview();
	bool UpdateStructurePlacementDragPreview(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& TargetCell);
	bool UpdateConveyorGhostPreview(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& TargetCell, USRStructureDataAsset* ConveyorDataAsset);
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
	void RebuildAreaCopyPreviewActors();
	void UpdateAreaCopyPlacementPreview();
	bool TryCommitAreaCopyPlacement(AActor*& OutSelectedActor);
	bool DeleteAreaCells(USRPlanetSurfaceGrid* SurfaceGrid, const TArray<FSRPlanetSurfaceGridCellId>& CellIds);
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
	void ClearPendingConveyorPathStart();
	UPROPERTY(Transient)
	FSRAssemblyStructurePreviewState StructurePreview;

	UPROPERTY(Transient)
	FSRAssemblyConveyorPreviewState ConveyorPreview;

	StarRovers::Assembly::FSRAssemblyAreaSelection AreaSelection;
	StarRovers::Assembly::FSRAssemblyAreaCopy AreaCopy;
	StarRovers::Assembly::FSRAssemblyPlacementHistory PlacementHistory;
	StarRovers::Assembly::FSRAssemblyPlacementQueue PlacementQueue;
};
