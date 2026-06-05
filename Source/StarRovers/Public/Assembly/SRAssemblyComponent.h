#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "SRAssemblyComponent.generated.h"

class ASRPlayerController;
class USRConveyorNetworkComponent;
class USRPlanetSurfaceGrid;
class USRStructureDataAsset;

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
	bool ShouldHandleStructurePlacementDrag() const;
	bool BeginStructurePlacementDrag(AActor*& OutSelectedActor);
	bool ContinueStructurePlacementDrag(AActor*& OutSelectedActor);
	void EndStructurePlacementDrag();
	void ClearSurfaceGridInteraction(AActor* SurfaceActor);
	void ClearSurfaceHover();

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
	TObjectPtr<USRStructureDataAsset> LastLoggedInvalidGhostDataAsset;

	FVector2D LastHoveredSampleMousePosition;
	bool bHasLastHoveredSampleMousePosition;
	bool bHasLastPublishedHoveredCellInfo;
	FSRPlanetSurfaceGridCellId LastPublishedHoveredCellId;
	FSRPlanetSurfaceGridCellId StructureGhostCellId;
	bool bHasStructureGhostCellId;
	bool bIsStructurePlacementDragActive;

	UPROPERTY(Transient)
	TObjectPtr<USRPlanetSurfaceGrid> LastStructurePlacementDragSurfaceGrid;

	FSRPlanetSurfaceGridCellId LastStructurePlacementDragCellId;
	bool bHasLastStructurePlacementDragCellId;

	UPROPERTY(Transient)
	TObjectPtr<USRPlanetSurfaceGrid> PendingConveyorStartSurfaceGrid;

	FSRPlanetSurfaceGridCellId PendingConveyorStartCellId;
	bool bHasPendingConveyorStartCell;

private:
	struct FSRQueuedStructurePlacement
	{
		TWeakObjectPtr<USRPlanetSurfaceGrid> SurfaceGrid;
		FSRPlanetSurfaceGridCellId CellId;
	};

	ASRPlayerController* GetOwnerController() const;
	bool GetCursorRay(FVector& OutRayOrigin, FVector& OutRayDirection) const;
	bool TryGetFocusedSurfaceGrid(AActor*& OutFocusedActor, USRPlanetSurfaceGrid*& OutSurfaceGrid) const;
	bool TryProjectCursorToSurfaceCell(USRPlanetSurfaceGrid* SurfaceGrid, FSRPlanetSurfaceGridCell& OutCell, FVector& OutHitLocation) const;
	void UpdateSurfaceHover();
	void ProcessQueuedStructurePlacements();
	void ApplyAssemblyModeToFocusedSurfaceGrid();
	void ResetHoverSampleCache();
	void PublishHoveredCellInfo(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& HoveredCell);
	void ClearPublishedHoveredCellInfo();
	void UpdateStructureGhostPreview();
	void DestroyStructureGhostPreview();
	bool BuildStructureGhostTransform(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& CellId, USRStructureDataAsset* StructureDataAsset, FTransform& OutTransform) const;
	void PublishStructureGhostPlacementDebug(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& HoveredCell, const FTransform& GhostTransform, float StructureHeightOffset, bool bLogDebug) const;
	bool TryResolveStructurePlacementDragTarget(AActor*& OutFocusedActor, USRPlanetSurfaceGrid*& OutSurfaceGrid, FSRPlanetSurfaceGridCell& OutTargetCell) const;
	bool TryGetFocusedConveyorNetwork(AActor*& OutFocusedActor, USRConveyorNetworkComponent*& OutConveyorNetwork) const;
	void EnqueueStructurePlacement(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& CellId);
	bool TryPlaceStructureDragPath(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& TargetCell);
	bool TryPlaceConveyorDragPath(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& TargetCell, USRStructureDataAsset* ConveyorDataAsset);
	bool TryPlaceSelectedStructure(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& TargetCell, bool bRefreshPreviewAndUI = true);
	bool TryPlaceSelectedConveyor(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCell& TargetCell, USRStructureDataAsset* ConveyorDataAsset, bool bRefreshPreviewAndUI = true);
	bool TryPlaceSelectedConveyorPath(USRPlanetSurfaceGrid* SurfaceGrid, const FSRPlanetSurfaceGridCellId& StartCellId, const FSRPlanetSurfaceGridCell& TargetCell, USRStructureDataAsset* ConveyorDataAsset, bool bRefreshPreviewAndUI = true);
	void ClearPendingConveyorPathStart();
	void LogInvalidGhostDataAssetOnce(USRStructureDataAsset* StructureDataAsset, const TCHAR* Reason);

	TArray<FSRQueuedStructurePlacement> PendingStructurePlacementQueue;
};
