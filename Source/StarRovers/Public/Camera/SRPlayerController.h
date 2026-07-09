#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Camera/SRPlayerControllerRuntimeState.h"
#include "Camera/SRPlayerControllerWidgetLayers.h"
#include "Simulation/SRAugmentSubsystem.h"
#include "UI/SRCelestialBodyFocusInfo.h"
#include "SRPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class USRAssemblyComponent;
class USRAugmentChoiceWidget;
class USRCelestialBodyFocusInfoWidget;
class USRCelestialBodyOverviewWidget;
class USRFacilityControlWidget;
class USRStructureSelectionWidget;
class USRStructureDataAsset;
class USRTimeControlWidget;
class ASRCameraPawn;
class USRCelestialBodyRegistrySubsystem;
class FSRPlayerControllerInputBinder;
class FSRPlayerControllerLifecycle;

UCLASS(Blueprintable)
class STARROVERS_API ASRPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ASRPlayerController();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupInputComponent() override;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Selection")
    AActor* GetSelectedActor() const;

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Focus")
    void RequestActorFocus(AActor* NewFocusedActor, bool bSnapImmediately = false);

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Selection")
    void ClearSelection();

    UFUNCTION(BlueprintPure, Category = "StarRovers|UI")
    USRCelestialBodyFocusInfoWidget* GetFocusInfoWidget() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|UI")
    USRCelestialBodyOverviewWidget* GetOverviewWidget() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|UI")
    bool HasSelectedActorFocusInfo() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|UI")
    FSRCelestialBodyFocusInfo GetSelectedActorFocusInfo() const;

    void SetHoveredSurfaceCellInfo(bool bHasHoveredSurfaceCell, const FSRPlanetSurfaceGridCellInfo& HoveredSurfaceCellInfo);

    void SetSelectedSurfaceStructureInfo(bool bHasSelectedSurfaceStructure, const FSRFocusedSurfaceStructureInfo& SelectedSurfaceStructureInfo);
    void SetSelectedActorSurfaceStructureInfo(AActor* NewSelectedActor, const FSRFocusedSurfaceStructureInfo& SelectedSurfaceStructureInfo);

    UFUNCTION(BlueprintPure, Category = "StarRovers|UI")
    USRTimeControlWidget* GetTimeControlWidget() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|UI")
    USRStructureSelectionWidget* GetStructureSelectionWidget() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|UI")
    USRAugmentChoiceWidget* GetAugmentChoiceWidget() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|UI")
    USRFacilityControlWidget* GetFacilityControlWidget() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|UI")
    bool IsPointerOverFacilityControlWidget() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|UI")
    bool IsPointerOverBlockingUI() const;

    UFUNCTION(BlueprintCallable, Category = "StarRovers|UI")
    void ClearFacilityFocus();

    UFUNCTION(BlueprintPure, Category = "StarRovers|Assembly")
    bool IsAssemblyModeActive() const;

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Assembly")
    void SetAssemblyModeActive(bool bNewAssemblyModeActive);

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Assembly")
    void ToggleAssemblyMode();

    UFUNCTION(BlueprintPure, Category = "StarRovers|Assembly")
    bool HasSelectedStructureBuildId() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Assembly")
    FName GetSelectedStructureBuildId() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Assembly")
    USRStructureDataAsset* GetSelectedStructureDataAsset() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Assembly")
    bool IsConveyorBulkDeleteModifierActive() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Assembly")
    bool IsAssemblyShiftModifierActive() const;

    bool ShouldHandleAssemblyPlacementDrag() const;
    bool ShouldHandleAssemblyAreaSelectionDrag() const;
    bool ShouldHandleAssemblyAreaDeletionDrag() const;
    bool ShouldBlockAssemblyCameraDrag() const;
    bool BeginAssemblyPlacementDrag();
    bool ContinueAssemblyPlacementDrag();
    void EndAssemblyPlacementDrag();
    bool BeginAssemblyAreaSelectionDrag();
    bool ContinueAssemblyAreaSelectionDrag();
    void EndAssemblyAreaSelectionDrag();
    bool BeginAssemblyAreaDeletionDrag();
    bool ContinueAssemblyAreaDeletionDrag();
    void EndAssemblyAreaDeletionDrag();
    bool RotateStructurePlacement(int32 StepDelta);

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "LeftClickAction"))
    TObjectPtr<UInputAction> LeftClickAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "FocusParentAction"))
    TObjectPtr<UInputAction> FocusParentAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "DeleteStructureAction"))
    TObjectPtr<UInputAction> DeleteStructureAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "AssemblyAreaDeletionDragHoldAction"))
    TObjectPtr<UInputAction> AssemblyAreaDeletionDragHoldAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "AssemblyAreaSelectionDeleteAction"))
    TObjectPtr<UInputAction> AssemblyAreaSelectionDeleteAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "AssemblyAreaSelectionCopyAction"))
    TObjectPtr<UInputAction> AssemblyAreaSelectionCopyAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "AssemblyAreaCopyMirrorAction"))
    TObjectPtr<UInputAction> AssemblyAreaCopyMirrorAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "AssemblyPickStructureAction"))
    TObjectPtr<UInputAction> AssemblyPickStructureAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "RotatePlacementCounterClockwiseAction"))
    TObjectPtr<UInputAction> RotatePlacementCounterClockwiseAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "RotatePlacementClockwiseAction"))
    TObjectPtr<UInputAction> RotatePlacementClockwiseAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "RotateAssemblyPlacementAction"))
    TObjectPtr<UInputAction> RotateAssemblyPlacementAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "ConveyorWaypointAction"))
    TObjectPtr<UInputAction> ConveyorWaypointAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "BulkDeleteConveyorModifierAction"))
    TObjectPtr<UInputAction> BulkDeleteConveyorModifierAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "AssemblyShiftModifierAction"))
    TObjectPtr<UInputAction> AssemblyShiftModifierAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "AssemblyUndoRedoAction"))
    TObjectPtr<UInputAction> AssemblyUndoRedoAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> StructureSelectionTabAction;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Selection", meta = (DisplayName = "SelectedActor"))
    TObjectPtr<AActor> SelectedActor;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|UI", meta = (DisplayName = "FocusInfoWidgetClass"))
    TSubclassOf<USRCelestialBodyFocusInfoWidget> FocusInfoWidgetClass;

    UPROPERTY()
    TObjectPtr<USRCelestialBodyFocusInfoWidget> FocusInfoWidget;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|UI", meta = (DisplayName = "OverviewWidgetClass"))
    TSubclassOf<USRCelestialBodyOverviewWidget> OverviewWidgetClass;

    UPROPERTY()
    TObjectPtr<USRCelestialBodyOverviewWidget> OverviewWidget;

    UPROPERTY()
    FSRCelestialBodyFocusInfo SelectedActorFocusInfo;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|UI", meta = (DisplayName = "TimeControlWidgetClass"))
    TSubclassOf<USRTimeControlWidget> TimeControlWidgetClass;

    UPROPERTY()
    TObjectPtr<USRTimeControlWidget> TimeControlWidget;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|UI", meta = (DisplayName = "StructureSelectionWidgetClass"))
    TSubclassOf<USRStructureSelectionWidget> StructureSelectionWidgetClass;

    UPROPERTY()
    TObjectPtr<USRStructureSelectionWidget> StructureSelectionWidget;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|UI", meta = (DisplayName = "AugmentChoiceWidgetClass"))
    TSubclassOf<USRAugmentChoiceWidget> AugmentChoiceWidgetClass;

    UPROPERTY()
    TObjectPtr<USRAugmentChoiceWidget> AugmentChoiceWidget;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|UI", meta = (DisplayName = "FacilityControlWidgetClass"))
    TSubclassOf<USRFacilityControlWidget> FacilityControlWidgetClass;

    UPROPERTY()
    TObjectPtr<USRFacilityControlWidget> FacilityControlWidget;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI", meta = (DisplayName = "WidgetLayerOrder", ToolTip = "Index 0 is the bottom UI layer. Later entries are drawn and hit-tested above earlier entries."))
    TArray<ESRPlayerUILayer> WidgetLayerOrder;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Assembly", meta = (DisplayName = "AvailableStructureDataAssets"))
    TArray<TObjectPtr<USRStructureDataAsset>> AvailableStructureDataAssets;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Assembly|Performance", meta = (DisplayName = "MaxStructurePlacementsPerFrame", ClampMin = "1"))
    int32 MaxStructurePlacementsPerFrame;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Assembly|Performance", meta = (DisplayName = "MaxQueuedStructurePlacements", ClampMin = "1"))
    int32 MaxQueuedStructurePlacements;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Assembly|Auto", meta = (DisplayName = "AssemblyModeScreenSizeThreshold", ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0"))
    float AssemblyModeScreenSizeThreshold;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly", meta = (DisplayName = "SelectedStructureBuildId"))
    FName SelectedStructureBuildId;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly", meta = (DisplayName = "bHasSelectedStructureBuildId"))
    bool bHasSelectedStructureBuildId;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Assembly", meta = (DisplayName = "SelectedStructureDataAsset"))
    TObjectPtr<USRStructureDataAsset> SelectedStructureDataAsset;

    UPROPERTY(Transient)
    TObjectPtr<ASRCameraPawn> BoundCameraPawn;

    UPROPERTY(Transient)
    TObjectPtr<USRCelestialBodyRegistrySubsystem> BoundCelestialBodyRegistry;

    UPROPERTY(Transient)
    TObjectPtr<UInputMappingContext> RuntimeAssemblyInputMappingContext;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "AssemblyComponent"))
    TObjectPtr<USRAssemblyComponent> AssemblyComponent;

    UFUNCTION(BlueprintImplementableEvent, Category = "StarRovers|Selection")
    void OnSelectionChanged(AActor* NewSelectedActor);

private:
    friend class FSRPlayerControllerInputBinder;
    friend class FSRPlayerControllerLifecycle;

    void TryBindCameraPawnFocusEvents();
    void TryBindCelestialBodyRegistryEvents();
    USRCelestialBodyRegistrySubsystem* GetCelestialBodyRegistry() const;
    void HandleFocusedActorChanged(AActor* NewFocusedActor);
    void HandleCelestialBodiesChanged();
    void HandlePrimaryStarActorChanged(AActor* NewPrimaryStarActor);
    void CreateFocusInfoWidget();
    void RefreshFocusInfoWidget();
    void CreateOverviewWidget();
    void RefreshOverviewWidget();
    void HandleOverviewCelestialBodyRequested(AActor* RequestedActor);
    void CreateTimeControlWidget();
    void CreateAugmentChoiceWidget();
    void BindAugmentSubsystem();
    void RegisterAvailableStructuresForAugments();
    UFUNCTION()
    void HandleAugmentChoicesReady(const TArray<FSRAugmentChoice>& Choices, int32 CycleIndex);
    UFUNCTION()
    void HandleAugmentChoiceSelected(const FSRAugmentChoice& Choice);
    UFUNCTION()
    void HandleUnlockedStructuresChanged();
    void CreateStructureSelectionWidget();
    void RefreshStructureSelectionWidget();
    void CreateFacilityControlWidget();
    void RefreshFacilityControlWidget();
    int32 ResolveWidgetLayerZOrder(ESRPlayerUILayer WidgetLayer) const;
    void HandleStructureBuildOptionSelected(FName StructureId, USRStructureDataAsset* StructureDataAsset);
    void GetAvailableStructureDataAssets(TArray<USRStructureDataAsset*>& OutStructureDataAssets) const;
    void UpdateAssemblyModeFromFocusedActorScreenSize();
    bool ShouldActivateAssemblyModeForFocusedActorScreenSize() const;
    void UpdateHitResultTraceDistance();
    void RequestFocusActor(AActor* NewFocusedActor, bool bSnapImmediately = false);
    void TryAutoFocusPrimaryStar();
    void HandleLeftClick();
    void HandleRightClick();
    void HandleAssemblyAreaDeletionDragStarted();
    void HandleAssemblyAreaDeletionDragCompleted();
    void HandleAssemblyAreaSelectionDelete();
    void HandleAssemblyAreaSelectionCopy();
    void HandleAssemblyAreaCopyMirror();
    void EnsureAssemblyAreaCopyMirrorInputAction();
    void EnsureAssemblyPickStructureInputAction();
    void EnsureRotatePlacementInputActions();
    void EnsureRotateAssemblyPlacementInputAction();
    void EnsureStructureSelectionTabInputAction();
    void ApplyRuntimeAssemblyInputMapping();
    void HandleStructureSelectionTab();
    void HandleAssemblyPickStructure();
    void HandleAssemblyUndoRedoAction();
    void HandleFocusParent();
    void HandleConveyorPlacementWaypoint();
    void HandleRotatePlacementCounterClockwise();
    void HandleRotatePlacementClockwise();
    void HandleRotateAssemblyPlacement();
    void HandleBulkDeleteConveyorModifierStarted();
    void HandleBulkDeleteConveyorModifierEnded();
    void HandleAssemblyShiftModifierStarted();
    void HandleAssemblyShiftModifierEnded();
    bool ClearSelectedStructureBuildOption();
    bool TrySelectBuildOptionFromHoveredCell();
    bool TryHandlePlacementRotationInput(int32 StepDelta);
    bool TryHandleSurfaceViewRotationInput(int32 StepDelta);
    void UpdateSelection(AActor* NewSelectedActor);

    FSRPlayerControllerRuntimeState RuntimeState;
};
