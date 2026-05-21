#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UI/SRCelestialBodyFocusInfo.h"
#include "SRPlayerController.generated.h"

class UInputAction;
class USRAssemblyComponent;
class USRCelestialBodyFocusInfoWidget;
class USRCelestialBodyOverviewWidget;
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

    UFUNCTION(BlueprintPure, Category = "StarRovers|UI")
    USRTimeControlWidget* GetTimeControlWidget() const;

    UFUNCTION(BlueprintPure, Category = "StarRovers|Assembly")
    bool IsAssemblyModeActive() const;

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Assembly")
    void SetAssemblyModeActive(bool bNewAssemblyModeActive);

    UFUNCTION(BlueprintCallable, Category = "StarRovers|Assembly")
    void ToggleAssemblyMode();

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "LeftClickAction"))
    TObjectPtr<UInputAction> LeftClickAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (DisplayName = "FocusParentAction"))
    TObjectPtr<UInputAction> FocusParentAction;

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

    UPROPERTY(Transient)
    TObjectPtr<ASRCameraPawn> BoundCameraPawn;

    UPROPERTY(Transient)
    TObjectPtr<USRCelestialBodyRegistrySubsystem> BoundCelestialBodyRegistry;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Assembly", meta = (DisplayName = "AssemblyComponent"))
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
    void UpdateHitResultTraceDistance();
    void RequestFocusActor(AActor* NewFocusedActor, bool bSnapImmediately = false);
    void TryAutoFocusPrimaryStar();
    void HandleLeftClick();
    void HandleFocusParent();
    void UpdateSelection(AActor* NewSelectedActor);

    bool bPendingInitialPrimaryStarFocus;
};
