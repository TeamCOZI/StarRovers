#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Surface/SRPlanetSurfaceGridTypes.h"
#include "SRFocusedHubShortcutWidget.generated.h"

class AActor;
class SWidget;
class UBorder;
class UButton;
class UTextBlock;
class UVerticalBox;
class UWidget;
class USRFocusedHubShortcutWidget;

USTRUCT(BlueprintType)
struct STARROVERS_API FSRFocusedHubShortcutInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Hub Shortcut", meta = (DisplayName = "BodyActor"))
	TObjectPtr<AActor> BodyActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Hub Shortcut", meta = (DisplayName = "OccupantId"))
	FName OccupantId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Hub Shortcut", meta = (DisplayName = "DisplayName"))
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Hub Shortcut", meta = (DisplayName = "OriginCellId"))
	FSRPlanetSurfaceGridCellId OriginCellId;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Hub Shortcut", meta = (DisplayName = "OriginCellInfo"))
	FSRPlanetSurfaceGridCellInfo OriginCellInfo;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FSRFocusedHubShortcutRequestedSignature, const FSRFocusedHubShortcutInfo&);

UCLASS()
class STARROVERS_API USRFocusedHubShortcutButtonAction : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(USRFocusedHubShortcutWidget* InOwnerWidget, const FSRFocusedHubShortcutInfo& InHubInfo, UButton* InButton);

	UFUNCTION()
	void HandleClicked();

	bool TryHandleManualClick(const FVector2D& ScreenPosition);

private:
	UPROPERTY(Transient)
	TObjectPtr<USRFocusedHubShortcutWidget> OwnerWidget;

	UPROPERTY(Transient)
	FSRFocusedHubShortcutInfo HubInfo;

	UPROPERTY(Transient)
	TObjectPtr<UButton> Button;
};

UCLASS(Blueprintable)
class STARROVERS_API USRFocusedHubShortcutWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Hub Shortcut")
	void SetHubShortcuts(const TArray<FSRFocusedHubShortcutInfo>& NewHubShortcuts);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Hub Shortcut")
	void ClearHubShortcuts();

	UFUNCTION(BlueprintPure, Category = "StarRovers|Hub Shortcut")
	bool HasHubShortcuts() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Hub Shortcut")
	bool IsPointerOverHubShortcutUI() const;

	bool TryHandleHubShortcutPointerClick();
	void RequestHubShortcut(const FSRFocusedHubShortcutInfo& HubInfo);
	FSRFocusedHubShortcutRequestedSignature& OnHubShortcutRequested();

protected:
	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Hub Shortcut", meta = (DisplayName = "HubShortcuts"))
	TArray<FSRFocusedHubShortcutInfo> HubShortcuts;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PanelBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ButtonBox;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USRFocusedHubShortcutButtonAction>> ButtonActions;

private:
	void BuildHubShortcutWidgetTree();
	void RebuildHubButtons();
	bool IsScreenPositionOverHubShortcutUI(const FVector2D& ScreenPosition) const;
	FString BuildHubShortcutSignature() const;

	FString HubShortcutSignature;
	FSRFocusedHubShortcutRequestedSignature HubShortcutRequestedEvent;
};
