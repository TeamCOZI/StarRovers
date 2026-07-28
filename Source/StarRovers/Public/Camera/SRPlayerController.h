#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Camera/SRPlayerControllerRuntimeState.h"
#include "Camera/SRPlayerControllerWidgetLayers.h"
#include "Assembly/SRAssemblyStructurePlacementPreview.h"
#include "Simulation/SRAugmentSubsystem.h"
#include "UI/SRCelestialBodyFocusInfo.h"
#include "UI/SRFocusedHubShortcutWidget.h"
#include "SRPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class USRAssemblyComponent;
class USRAugmentChoiceWidget;
class USRCelestialBodyFocusInfoWidget;
class USRCelestialBodyOverviewWidget;
class USRFacilityControlWidget;
class USRGameOverWidget;
class USRPlayerGuidanceWidget;
class USRStructureSelectionWidget;
class USRStructureDataAsset;
class USRTimeControlWidget;
class ASRStar;
class ASRCameraPawn;
class USRCelestialBodyRegistrySubsystem;
class FSRPlayerControllerInputBinder;
class FSRPlayerControllerLifecycle;
struct FSRStructureBuildCatalog;

USTRUCT(BlueprintType)
struct STARROVERS_API FSRAvailableStructureDataAssetOperationCategory
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Assembly", meta = (DisplayName = "Processor"))
    TArray<TObjectPtr<USRStructureDataAsset>> Processor;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Assembly", meta = (DisplayName = "Synthesizer"))
    TArray<TObjectPtr<USRStructureDataAsset>> Synthesizer;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Assembly", meta = (DisplayName = "Miner"))
    TArray<TObjectPtr<USRStructureDataAsset>> Miner;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Assembly", meta = (DisplayName = "Conveyor"))
    TArray<TObjectPtr<USRStructureDataAsset>> Conveyor;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Assembly", meta = (DisplayName = "Hub"))
    TArray<TObjectPtr<USRStructureDataAsset>> Hub;
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRAvailableStructureDataAssetCategories
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Assembly", meta = (DisplayName = "Starting"))
    FSRAvailableStructureDataAssetOperationCategory Starting;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Assembly", meta = (DisplayName = "Basic"))
    FSRAvailableStructureDataAssetOperationCategory Basic;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Assembly", meta = (DisplayName = "Advance"))
    FSRAvailableStructureDataAssetOperationCategory Advance;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Assembly", meta = (DisplayName = "Expert"))
    FSRAvailableStructureDataAssetOperationCategory Expert;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Assembly", meta = (DisplayName = "Innovation"))
    FSRAvailableStructureDataAssetOperationCategory Innovation;
};

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

    /** Focuses one placed Facility and opens its existing Inspector path. */
    UFUNCTION(BlueprintCallable, Category = "StarRovers|Focus")
    bool RequestFacilityFocus(AActor* BodyActor, FName OccupantId, bool bCenterSurface = true);

    /** Focuses any placed surface structure, including a natural resource deposit. */
    bool RequestSurfaceStructureFocus(AActor* BodyActor, FName OccupantId, bool bCenterSurface = true);

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
    USRPlayerGuidanceWidget* GetPlayerGuidanceWidget() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|UI")
    USRStructureSelectionWidget* GetStructureSelectionWidget() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|UI")
    USRAugmentChoiceWidget* GetAugmentChoiceWidget() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|UI")
    USRFacilityControlWidget* GetFacilityControlWidget() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|UI")
    USRFocusedHubShortcutWidget* GetFocusedHubShortcutWidget() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|UI")
    USRGameOverWidget* GetGameOverWidget() const;

    // Returns the same ruleset- and Augment-filtered catalog consumed by the
    // structure selection widget. Exposed for PIE validation and tooling.
    void GetBuildableStructureDataAssets(TArray<USRStructureDataAsset*>& OutStructureDataAssets) const;

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

    UFUNCTION(BlueprintPure, Category = "StarRovers|Assembly|Placement Preview")
    FSRStructurePlacementPreview GetSelectedStructurePlacementPreview() const;

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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "StructureSelectionCategory1Action"))
    TObjectPtr<UInputAction> StructureSelectionCategory1Action;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "StructureSelectionCategory2Action"))
    TObjectPtr<UInputAction> StructureSelectionCategory2Action;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "StructureSelectionCategory3Action"))
    TObjectPtr<UInputAction> StructureSelectionCategory3Action;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "StructureSelectionCategory4Action"))
    TObjectPtr<UInputAction> StructureSelectionCategory4Action;

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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|UI", meta = (DisplayName = "PlayerGuidanceWidgetClass"))
    TSubclassOf<USRPlayerGuidanceWidget> PlayerGuidanceWidgetClass;

    UPROPERTY()
    TObjectPtr<USRPlayerGuidanceWidget> PlayerGuidanceWidget;

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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|UI", meta = (DisplayName = "FocusedHubShortcutWidgetClass"))
    TSubclassOf<USRFocusedHubShortcutWidget> FocusedHubShortcutWidgetClass;

    UPROPERTY()
    TObjectPtr<USRFocusedHubShortcutWidget> FocusedHubShortcutWidget;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Hub Shortcut", meta = (DisplayName = "FocusedHubShortcutRefreshInterval", ClampMin = "0.0"))
    float FocusedHubShortcutRefreshInterval;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|UI", meta = (DisplayName = "GameOverWidgetClass"))
    TSubclassOf<USRGameOverWidget> GameOverWidgetClass;

    UPROPERTY()
    TObjectPtr<USRGameOverWidget> GameOverWidget;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI", meta = (DisplayName = "WidgetLayerOrder", ToolTip = "Index 0 is the bottom UI layer. Later entries are above earlier entries. Augment Choice and Game Over always retain modal priority."))
    TArray<ESRPlayerUILayer> WidgetLayerOrder;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Assembly", meta = (DisplayName = "AvailableStructureDataAssets"))
    FSRAvailableStructureDataAssetCategories AvailableStructureDataAssets;

    // Native soft references keep the generated Resource V2 build catalog out of
    // the legacy Blueprint category arrays while remaining visible to the cooker.
    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Assembly|Resource V2", meta = (DisplayName = "Authored Resource V2 Structures"))
    TArray<TSoftObjectPtr<USRStructureDataAsset>> AuthoredResourceV2StructureDataAssets;

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
    TObjectPtr<ASRStar> BoundGameOverStar;

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
    void CreatePlayerGuidanceWidget();
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
    void CreateFocusedHubShortcutWidget();
    void RefreshFocusedHubShortcutWidget(bool bForceRefresh = false);
    void BuildFocusedHubShortcutInfos(TArray<FSRFocusedHubShortcutInfo>& OutHubInfos) const;
    void HandleFocusedHubShortcutRequested(const FSRFocusedHubShortcutInfo& HubInfo);
    void CreateGameOverWidget();
    void BindPrimaryStarGameOver(AActor* PrimaryStarActor);
    void ShowGameOverScreen(ASRStar* Star);
    UFUNCTION()
    void HandlePrimaryStarGameOver(ASRStar* Star);
    int32 ResolveWidgetLayerZOrder(ESRPlayerUILayer WidgetLayer) const;
    void HandleStructureBuildOptionSelected(FName StructureId, USRStructureDataAsset* StructureDataAsset);
    void GetConfiguredStructureDataAssets(TArray<USRStructureDataAsset*>& OutStructureDataAssets) const;
    void GetAvailableStructureDataAssets(TArray<USRStructureDataAsset*>& OutStructureDataAssets) const;
    void BuildStructureBuildCatalog(FSRStructureBuildCatalog& OutBuildCatalog) const;
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
    void HandleStructureSelectionCategory1();
    void HandleStructureSelectionCategory2();
    void HandleStructureSelectionCategory3();
    void HandleStructureSelectionCategory4();
    void HandleStructureSelectionCategoryShortcut(int32 CategoryIndex);
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
    void RefreshMiningResourceDepositHighlights();

    FSRPlayerControllerRuntimeState RuntimeState;
    double NextFocusedHubShortcutRefreshTime = 0.0;
};
