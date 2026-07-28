#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Logistics/SRSpaceLogisticsTypes.h"
#include "SRFacilityControlWidget.generated.h"

class AActor;
class SWidget;
class UBorder;
class UButton;
class UCheckBox;
class UHorizontalBox;
class UProgressBar;
class UScaleBox;
class USizeBox;
class UTextBlock;
class UVerticalBox;
class UWidget;
class USRFacilityNetworkComponent;
class USRFacilityControlWidget;
class USRInfoCardWidget;
class USRStatusBadgeWidget;
class USRSpaceLogisticsSubsystem;
struct FSRFacilityInstance;

UCLASS()
class STARROVERS_API USRFacilityInputSlotDebugAction : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(USRFacilityControlWidget* InOwnerWidget, int32 InInputPortIndex, FName InResourceId, UButton* InButton);

	UFUNCTION()
	void HandleClicked();

	bool TryHandleManualClick(const FVector2D& ScreenPosition);

private:
	UPROPERTY(Transient)
	TObjectPtr<USRFacilityControlWidget> OwnerWidget;

	int32 InputPortIndex = INDEX_NONE;
	FName ResourceId = NAME_None;

	UPROPERTY(Transient)
	TObjectPtr<UButton> Button;
};

UCLASS()
class STARROVERS_API USRHubRouteDestinationAction : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(USRFacilityControlWidget* InOwnerWidget, const FSRSpaceLogisticsHubEndpoint& InDestinationHub, UButton* InButton);

	UFUNCTION()
	void HandleClicked();

	bool TryHandleManualClick(const FVector2D& ScreenPosition);

private:
	UPROPERTY(Transient)
	TObjectPtr<USRFacilityControlWidget> OwnerWidget;

	UPROPERTY(Transient)
	FSRSpaceLogisticsHubEndpoint DestinationHub;

	UPROPERTY(Transient)
	TObjectPtr<UButton> Button;
};

UCLASS()
class STARROVERS_API USRHubRouteLaunchAction : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(USRFacilityControlWidget* InOwnerWidget, const FSRSpaceLogisticsHubEndpoint& InDestinationHub, UButton* InButton);

	UFUNCTION()
	void HandleClicked();

	bool TryHandleManualClick(const FVector2D& ScreenPosition);

private:
	UPROPERTY(Transient)
	TObjectPtr<USRFacilityControlWidget> OwnerWidget;

	UPROPERTY(Transient)
	FSRSpaceLogisticsHubEndpoint DestinationHub;

	UPROPERTY(Transient)
	TObjectPtr<UButton> Button;
};

UCLASS()
class STARROVERS_API USRHubRouteRemovalAction : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(USRFacilityControlWidget* InOwnerWidget, FName InRouteId, UButton* InButton);

	UFUNCTION()
	void HandleClicked();

	bool TryHandleManualClick(const FVector2D& ScreenPosition);

private:
	UPROPERTY(Transient)
	TObjectPtr<USRFacilityControlWidget> OwnerWidget;

	FName RouteId = NAME_None;

	UPROPERTY(Transient)
	TObjectPtr<UButton> Button;
};

UCLASS()
class STARROVERS_API USRHubRouteDebugOrbitAction : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(USRFacilityControlWidget* InOwnerWidget, UButton* InButton);

	UFUNCTION()
	void HandleClicked();

	bool TryHandleManualClick(const FVector2D& ScreenPosition);

private:
	UPROPERTY(Transient)
	TObjectPtr<USRFacilityControlWidget> OwnerWidget;

	UPROPERTY(Transient)
	TObjectPtr<UButton> Button;
};

UCLASS()
class STARROVERS_API USRHubStarFuelMissileLaunchAction : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(USRFacilityControlWidget* InOwnerWidget, UButton* InButton);

	UFUNCTION()
	void HandleClicked();

	bool TryHandleManualClick(const FVector2D& ScreenPosition);

private:
	UPROPERTY(Transient)
	TObjectPtr<USRFacilityControlWidget> OwnerWidget;

	UPROPERTY(Transient)
	TObjectPtr<UButton> Button;
};

UCLASS()
class STARROVERS_API USRHubAutoMissileInventorySlotAction : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(USRFacilityControlWidget* InOwnerWidget, int32 InInputPortIndex, UWidget* InSlotWidget);

	bool TryHandleManualClick(const FVector2D& ScreenPosition);

private:
	UPROPERTY(Transient)
	TObjectPtr<USRFacilityControlWidget> OwnerWidget;

	int32 InputPortIndex = INDEX_NONE;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> SlotWidget;
};

UCLASS()
class STARROVERS_API USRHubRouteSettingAction : public UObject
{
	GENERATED_BODY()

public:
	void InitializeMaxCargoStackCount(USRFacilityControlWidget* InOwnerWidget, FName InRouteId, int32 InMaxCargoStackCount, UButton* InButton);
	void InitializeReturnEmptyWhenNoCargo(USRFacilityControlWidget* InOwnerWidget, FName InRouteId, bool bInReturnEmptyWhenNoCargo, UButton* InButton);
	void InitializeCargoResourceId(USRFacilityControlWidget* InOwnerWidget, FName InRouteId, FName InCargoResourceId, UButton* InButton);
	void InitializeRouteProfile(USRFacilityControlWidget* InOwnerWidget, FName InRouteId, ESRSpaceLogisticsRouteProfileV2 InRouteProfile, UButton* InButton);
	void InitializeConditionedTransitModule(USRFacilityControlWidget* InOwnerWidget, FName InRouteId, ESRConditionedTransitModuleV2 InModule, UButton* InButton);

	UFUNCTION()
	void HandleClicked();

	bool TryHandleManualClick(const FVector2D& ScreenPosition);

private:
	UPROPERTY(Transient)
	TObjectPtr<USRFacilityControlWidget> OwnerWidget;

	FName RouteId = NAME_None;
	int32 MaxCargoStackCount = 1;
	bool bReturnEmptyWhenNoCargo = true;
	FName CargoResourceId = NAME_None;
	ESRSpaceLogisticsRouteProfileV2 RouteProfile = ESRSpaceLogisticsRouteProfileV2::NeutralShuttle;
	ESRConditionedTransitModuleV2 ConditionedTransitModule = ESRConditionedTransitModuleV2::None;
	bool bSetMaxCargoStackCount = false;
	bool bSetReturnEmptyWhenNoCargo = false;
	bool bSetCargoResourceId = false;
	bool bSetRouteProfile = false;
	bool bSetConditionedTransitModule = false;

	UPROPERTY(Transient)
	TObjectPtr<UButton> Button;
};

UCLASS(Blueprintable)
class STARROVERS_API USRFacilityControlWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility")
	void SetFocusedFacility(AActor* NewFocusedActor, FName NewOccupantId);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility")
	void ClearFocusedFacility();

	UFUNCTION(BlueprintPure, Category = "StarRovers|Facility")
	bool HasFocusedFacility() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Facility")
	bool IsPointerOverControlPanel() const;

	bool TryHandleFacilityControlPointerClick();

	bool AddDebugInputResourceToPort(int32 InputPortIndex, FName ResourceId);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Hub")
	bool CreateRouteToHubEndpoint(const FSRSpaceLogisticsHubEndpoint& DestinationHub);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Hub")
	bool SelectRouteDestinationHubEndpoint(const FSRSpaceLogisticsHubEndpoint& DestinationHub);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Hub")
	bool LaunchDebugLocalOrbitRoute();

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Hub")
	bool LaunchStarFuelMissileFromFocusedHub();

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Hub")
	bool BeginSelectStarFuelMissileAutoLaunchSlot();

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Hub")
	bool SelectStarFuelMissileAutoLaunchInputPort(int32 InputPortIndex);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Hub")
	bool RemoveHubRoute(FName RouteId);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Hub")
	bool SetHubRouteMaxCargoStackCount(FName RouteId, int32 MaxCargoStackCount);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Hub")
	bool SetHubRouteReturnEmptyWhenNoCargo(FName RouteId, bool bReturnEmptyWhenNoCargo);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Hub")
	bool SetHubRouteCargoResourceId(FName RouteId, FName CargoResourceId);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Hub")
	bool SetHubRouteProfile(FName RouteId, ESRSpaceLogisticsRouteProfileV2 RouteProfile);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Hub")
	bool SetHubRouteConditionedTransitModule(FName RouteId, ESRConditionedTransitModuleV2 ConditionedTransitModule);

protected:
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "FocusedActor"))
	TWeakObjectPtr<AActor> FocusedActor;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "FocusedOccupantId"))
	FName FocusedOccupantId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "bHasFocusedFacility"))
	bool bHasFocusedFacility = false;

	UPROPERTY(Transient)
	TObjectPtr<UScaleBox> PanelScaleBox;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> PanelDesignSizeBox;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PanelBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<USRStatusBadgeWidget> FacilityStatusBadge;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> ProcessCheckBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ProcessStatusTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UButton> OperationalPriorityButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OperationalPriorityTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ResourceV2RecipeButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ResourceV2RecipeTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InputResourceTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<USRStatusBadgeWidget> InputStageBadge;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> InputResourceSlotBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EffectsTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<USRStatusBadgeWidget> ProcessStageBadge;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EnergyTransitionTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StateTransitionTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> ProcessProgressBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ProcessTimeTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OutputPreviewTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<USRStatusBadgeWidget> OutputStageBadge;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InputToProcessArrowTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ProcessToOutputArrowTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> OutputResourceSlotBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InputInventoryTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> InputInventorySlotBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OutputInventoryTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> OutputInventorySlotBox;

	UPROPERTY(Transient)
	TObjectPtr<UButton> DebugAddTerriteButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> DebugAddAquidButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> DebugAddNitainButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> DebugAddWasteButton;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> DeliverCheckBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DeliverStatusTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HubRouteTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> HubRoutePanelBorder;

	UPROPERTY(Transient)
	TObjectPtr<USRStatusBadgeWidget> HubNetworkStatusBadge;

	UPROPERTY(Transient)
	TObjectPtr<USRInfoCardWidget> HubFleetInfoCard;

	UPROPERTY(Transient)
	TObjectPtr<USRInfoCardWidget> HubQueueInfoCard;

	UPROPERTY(Transient)
	TObjectPtr<USRInfoCardWidget> HubMissileInfoCard;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> HubUtilityButtonBox;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> HubDestinationButtonBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HubRouteStatusTextBlock;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USRFacilityInputSlotDebugAction>> InputSlotDebugActions;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USRHubRouteDestinationAction>> HubRouteDestinationActions;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USRHubRouteLaunchAction>> HubRouteLaunchActions;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USRHubRouteRemovalAction>> HubRouteRemovalActions;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USRHubRouteDebugOrbitAction>> HubRouteDebugOrbitActions;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USRHubStarFuelMissileLaunchAction>> HubStarFuelMissileLaunchActions;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USRHubAutoMissileInventorySlotAction>> HubAutoMissileInventorySlotActions;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USRHubRouteSettingAction>> HubRouteSettingActions;

private:
	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleProcessCheckStateChanged(bool bIsChecked);

	UFUNCTION()
	void HandleCycleResourceV2RecipeClicked();

	UFUNCTION()
	void HandleCycleOperationalPriorityClicked();

	UFUNCTION()
	void HandleDeliverCheckStateChanged(bool bIsChecked);

	UFUNCTION()
	void HandleDebugAddTerriteClicked();

	UFUNCTION()
	void HandleDebugAddAquidClicked();

	UFUNCTION()
	void HandleDebugAddNitainClicked();

	UFUNCTION()
	void HandleDebugAddWasteClicked();

	void BuildFacilityControlWidgetTree();
	void BindControlHandlers();
	void RefreshControlText();
	void RefreshInputResourceSlots(USRFacilityNetworkComponent* FacilityNetwork, const FSRFacilityInstance& FacilityInstance);
	void RefreshOutputResourceSlots(USRFacilityNetworkComponent* FacilityNetwork, const FSRFacilityInstance& FacilityInstance);
	void RefreshInputInventorySlots(USRFacilityNetworkComponent* FacilityNetwork, const FSRFacilityInstance& FacilityInstance, bool bIsMiningFacility);
	void RefreshOutputInventorySlots(const FSRFacilityInstance& FacilityInstance);
	void RefreshHubRouteSection(USRFacilityNetworkComponent* FacilityNetwork, const FSRFacilityInstance& FacilityInstance);
	USRFacilityNetworkComponent* GetFocusedFacilityNetwork() const;
	USRSpaceLogisticsSubsystem* GetSpaceLogisticsSubsystem() const;
	bool IsScreenPositionOverControlPanel(const FVector2D& ScreenPosition) const;

	bool bUpdatingControls = false;
	FString InputResourcePanelSignature;
	FString OutputResourcePanelSignature;
	FString InputInventoryPanelSignature;
	FString OutputInventoryPanelSignature;
	FString HubRoutePanelSignature;
	FString LastHubRouteStatus;

	UPROPERTY(Transient)
	FSRSpaceLogisticsHubEndpoint SelectedHubRouteDestination;

	bool bHasSelectedHubRouteDestination = false;
	bool bSelectingHubStarFuelMissileAutoLaunchSlot = false;
};
