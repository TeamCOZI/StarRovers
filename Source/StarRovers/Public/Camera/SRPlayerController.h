#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UI/SRCelestialBodyFocusInfo.h"
#include "SRPlayerController.generated.h"

class UInputAction;
class USRAssemblyComponent;
class USRCelestialBodyFocusInfoWidget;
class USRCelestialBodyOverviewWidget;
class USRFacilityControlWidget;
class USRStructureSelectionWidget;
class USRStructureDataAsset;
class USRTimeControlWidget;
class ASRCameraPawn;
class USRCelestialBodyRegistrySubsystem;

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
    USRFacilityControlWidget* GetFacilityControlWidget() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|UI")
    bool IsPointerOverFacilityControlWidget() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|UI")
    bool IsPointerOverBlockingUi() const;

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

    bool ShouldHandleAssemblyPlacementDrag() const;
    bool ShouldBlockAssemblyCameraDrag() const;
    bool BeginAssemblyPlacementDrag();
    bool ContinueAssemblyPlacementDrag();
    void EndAssemblyPlacementDrag();
    bool RotateStructurePlacement(int32 StepDelta);

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "LeftClickAction"))
    TObjectPtr<UInputAction> LeftClickAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "FocusParentAction"))
    TObjectPtr<UInputAction> FocusParentAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "DeleteStructureAction"))
    TObjectPtr<UInputAction> DeleteStructureAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "RotatePlacementCounterClockwiseAction"))
    TObjectPtr<UInputAction> RotatePlacementCounterClockwiseAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "RotatePlacementClockwiseAction"))
    TObjectPtr<UInputAction> RotatePlacementClockwiseAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "ConveyorWaypointAction"))
    TObjectPtr<UInputAction> ConveyorWaypointAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "BulkDeleteConveyorModifierAction"))
    TObjectPtr<UInputAction> BulkDeleteConveyorModifierAction;

    UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Selection", meta = (DisplayName = "SelectedActor"))
    TObjectPtr<AActor> SelectedActor;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|UI", meta = (DisplayName = "FocusInfoWidgetClass"))
    TSubclassOf<USRCelestialBodyFocusInfoWidget> FocusInfoWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI", meta = (DisplayName = "FocusInfoWidgetZOrder"))
    int32 FocusInfoWidgetZOrder;

    UPROPERTY()
    TObjectPtr<USRCelestialBodyFocusInfoWidget> FocusInfoWidget;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|UI", meta = (DisplayName = "OverviewWidgetClass"))
    TSubclassOf<USRCelestialBodyOverviewWidget> OverviewWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI", meta = (DisplayName = "OverviewWidgetZOrder"))
    int32 OverviewWidgetZOrder;

    UPROPERTY()
    TObjectPtr<USRCelestialBodyOverviewWidget> OverviewWidget;

    UPROPERTY()
    FSRCelestialBodyFocusInfo SelectedActorFocusInfo;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|UI", meta = (DisplayName = "TimeControlWidgetClass"))
    TSubclassOf<USRTimeControlWidget> TimeControlWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI", meta = (DisplayName = "TimeControlWidgetZOrder"))
    int32 TimeControlWidgetZOrder;

    UPROPERTY()
    TObjectPtr<USRTimeControlWidget> TimeControlWidget;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|UI", meta = (DisplayName = "StructureSelectionWidgetClass"))
    TSubclassOf<USRStructureSelectionWidget> StructureSelectionWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI", meta = (DisplayName = "StructureSelectionWidgetZOrder"))
    int32 StructureSelectionWidgetZOrder;

    UPROPERTY()
    TObjectPtr<USRStructureSelectionWidget> StructureSelectionWidget;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|UI", meta = (DisplayName = "FacilityControlWidgetClass"))
    TSubclassOf<USRFacilityControlWidget> FacilityControlWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI", meta = (DisplayName = "FacilityControlWidgetZOrder"))
    int32 FacilityControlWidgetZOrder;

    UPROPERTY()
    TObjectPtr<USRFacilityControlWidget> FacilityControlWidget;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Assembly", meta = (DisplayName = "AvailableStructureDataAssets"))
    TArray<TObjectPtr<USRStructureDataAsset>> AvailableStructureDataAssets;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Assembly|Performance", meta = (DisplayName = "MaxStructurePlacementsPerFrame", ClampMin = "1"))
    int32 MaxStructurePlacementsPerFrame;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StarRovers|Assembly|Performance", meta = (DisplayName = "MaxQueuedStructurePlacements", ClampMin = "1"))
    int32 MaxQueuedStructurePlacements;

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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (DisplayName = "AssemblyComponent"))
    TObjectPtr<USRAssemblyComponent> AssemblyComponent;

    UFUNCTION(BlueprintImplementableEvent, Category = "StarRovers|Selection")
    void OnSelectionChanged(AActor* NewSelectedActor);

private:
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
    void CreateStructureSelectionWidget();
    void RefreshStructureSelectionWidget();
    void CreateFacilityControlWidget();
    void RefreshFacilityControlWidget();
    void HandleStructureBuildOptionSelected(FName StructureId, USRStructureDataAsset* StructureDataAsset);
    void GetAvailableStructureDataAssets(TArray<USRStructureDataAsset*>& OutStructureDataAssets) const;
    void UpdateHitResultTraceDistance();
    void RequestFocusActor(AActor* NewFocusedActor, bool bSnapImmediately = false);
    void TryAutoFocusPrimaryStar();
    void HandleLeftClick();
    void HandleRightClick();
    void HandleFocusParent();
    void HandleConveyorPlacementWaypoint();
    void HandleRotatePlacementCounterClockwise();
    void HandleRotatePlacementClockwise();
    void HandleBulkDeleteConveyorModifierStarted();
    void HandleBulkDeleteConveyorModifierEnded();
    bool TryHandlePlacementRotationInput(int32 StepDelta);
    void UpdateSelection(AActor* NewSelectedActor);

    bool bPendingInitialPrimaryStarFocus;
    uint64 LastPlacementRotationInputFrame;
    int32 LastPlacementRotationInputStepDelta;
    bool bConveyorBulkDeleteModifierActive;
};
