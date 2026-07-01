#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
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
	TObjectPtr<UVerticalBox> InputResourceSlotBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EffectsTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> ProcessProgressBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ProcessTimeTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OutputPreviewTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> OutputResourceSlotBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InputInventoryTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OutputInventoryTextBlock;

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
	TArray<TObjectPtr<USRFacilityInputSlotDebugAction>> InputSlotDebugActions;

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
	USRFacilityNetworkComponent* GetFocusedFacilityNetwork() const;
	bool IsScreenPositionOverControlPanel(const FVector2D& ScreenPosition) const;

	bool bUpdatingControls = false;
	FString InputResourcePanelSignature;
	FString OutputResourcePanelSignature;
};
