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
class UTextBlock;
class UVerticalBox;
class USRFacilityNetworkComponent;
class USRFacilityControlWidget;
struct FSRFacilityInstance;

UCLASS()
class STARROVERS_API USRFacilityInputSlotDebugAction : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(USRFacilityControlWidget* InOwnerWidget, int32 InInputPortIndex, FName InResourceId);

	UFUNCTION()
	void HandleClicked();

private:
	UPROPERTY(Transient)
	TObjectPtr<USRFacilityControlWidget> OwnerWidget;

	int32 InputPortIndex = INDEX_NONE;
	FName ResourceId = NAME_None;
};

UCLASS()
class STARROVERS_API USRHubRouteDestinationAction : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(USRFacilityControlWidget* InOwnerWidget, const FSRHubEndpoint& InDestinationHub, UButton* InButton);

	UFUNCTION()
	void HandleClicked();

	bool TryHandleManualClick(const FVector2D& ScreenPosition);

private:
	UPROPERTY(Transient)
	TObjectPtr<USRFacilityControlWidget> OwnerWidget;

	UPROPERTY(Transient)
	FSRHubEndpoint DestinationHub;

	UPROPERTY(Transient)
	TObjectPtr<UButton> Button;
};

UCLASS()
class STARROVERS_API USRHubRouteLaunchAction : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(USRFacilityControlWidget* InOwnerWidget, const FSRHubEndpoint& InDestinationHub, UButton* InButton);

	UFUNCTION()
	void HandleClicked();

	bool TryHandleManualClick(const FVector2D& ScreenPosition);

private:
	UPROPERTY(Transient)
	TObjectPtr<USRFacilityControlWidget> OwnerWidget;

	UPROPERTY(Transient)
	FSRHubEndpoint DestinationHub;

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
class STARROVERS_API USRHubRouteSettingAction : public UObject
{
	GENERATED_BODY()

public:
	void InitializeMaxCargoStackCount(USRFacilityControlWidget* InOwnerWidget, FName InRouteId, int32 InMaxCargoStackCount, UButton* InButton);
	void InitializeReturnEmptyWhenNoCargo(USRFacilityControlWidget* InOwnerWidget, FName InRouteId, bool bInReturnEmptyWhenNoCargo, UButton* InButton);
	void InitializeCargoResourceId(USRFacilityControlWidget* InOwnerWidget, FName InRouteId, FName InCargoResourceId, UButton* InButton);

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
	bool bSetMaxCargoStackCount = false;
	bool bSetReturnEmptyWhenNoCargo = false;
	bool bSetCargoResourceId = false;

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
	bool CreateRouteToHubEndpoint(const FSRHubEndpoint& DestinationHub);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Hub")
	bool SelectRouteDestinationHubEndpoint(const FSRHubEndpoint& DestinationHub);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Hub")
	bool LaunchDebugLocalOrbitRoute();

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Hub")
	bool RemoveHubRoute(FName RouteId);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Hub")
	bool SetHubRouteMaxCargoStackCount(FName RouteId, int32 MaxCargoStackCount);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Hub")
	bool SetHubRouteReturnEmptyWhenNoCargo(FName RouteId, bool bReturnEmptyWhenNoCargo);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Facility|Hub")
	bool SetHubRouteCargoResourceId(FName RouteId, FName CargoResourceId);

protected:
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "FocusedActor"))
	TWeakObjectPtr<AActor> FocusedActor;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "FocusedOccupantId"))
	FName FocusedOccupantId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Facility", meta = (DisplayName = "bHasFocusedFacility"))
	bool bHasFocusedFacility = false;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PanelBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> ProcessCheckBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ProcessStatusTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InputResourceTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> InputResourceSlotBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EffectsTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> ProcessProgressBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ProcessTimeTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OutputPreviewTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> OutputResourceSlotBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InputInventoryTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> InputInventorySlotBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OutputInventoryTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> OutputInventorySlotBox;

	UPROPERTY(Transient)
	TObjectPtr<UButton> DebugAddTerriteButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> DebugAddAquidButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> DebugAddNitainButton;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> DeliverCheckBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DeliverStatusTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HubRouteTextBlock;

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
	TArray<TObjectPtr<USRHubRouteSettingAction>> HubRouteSettingActions;

private:
	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleProcessCheckStateChanged(bool bIsChecked);

	UFUNCTION()
	void HandleDeliverCheckStateChanged(bool bIsChecked);

	UFUNCTION()
	void HandleDebugAddTerriteClicked();

	UFUNCTION()
	void HandleDebugAddAquidClicked();

	UFUNCTION()
	void HandleDebugAddNitainClicked();

	void BuildFacilityControlWidgetTree();
	void BindControlHandlers();
	void RefreshControlText();
	void RefreshInputResourceSlots(USRFacilityNetworkComponent* FacilityNetwork, const FSRFacilityInstance& FacilityInstance);
	void RefreshOutputResourceSlots(USRFacilityNetworkComponent* FacilityNetwork, const FSRFacilityInstance& FacilityInstance);
	void RefreshInputInventorySlots(USRFacilityNetworkComponent* FacilityNetwork, const FSRFacilityInstance& FacilityInstance, bool bIsMiningFacility);
	void RefreshOutputInventorySlots(const FSRFacilityInstance& FacilityInstance);
	void RefreshHubRouteSection(USRFacilityNetworkComponent* FacilityNetwork, const FSRFacilityInstance& FacilityInstance);
	USRFacilityNetworkComponent* GetFocusedFacilityNetwork() const;
	bool IsScreenPositionOverControlPanel(const FVector2D& ScreenPosition) const;

	bool bUpdatingControls = false;
	FString InputResourcePanelSignature;
	FString OutputResourcePanelSignature;
	FString InputInventoryPanelSignature;
	FString OutputInventoryPanelSignature;
	FString HubRoutePanelSignature;
	FString LastHubRouteStatus;

	UPROPERTY(Transient)
	FSRHubEndpoint SelectedHubRouteDestination;

	bool bHasSelectedHubRouteDestination = false;
};
