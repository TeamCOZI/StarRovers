#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/SRStrategicOverlayPresentation.h"
#include "SRCelestialBodyOverviewWidget.generated.h"

class AActor;
class SWidget;
class UBorder;
class UButton;
class UCanvasPanel;
class UScrollBox;
class UTextBlock;
class UVerticalBox;
class USRCelestialBodyOverviewWidget;
class USRStatusBadgeWidget;

DECLARE_MULTICAST_DELEGATE_OneParam(FSRStarRoversCelestialBodyRequestedSignature, AActor*);

struct FSRNameplateButtonLayout
{
	TWeakObjectPtr<AActor> CelestialBodyActor;
	FVector2D Center = FVector2D::ZeroVector;
	FVector2D LabelPosition = FVector2D::ZeroVector;
	FVector2D LabelAlignment = FVector2D(0.5f, 1.0f);
	FVector2D LeaderStart = FVector2D::ZeroVector;
	FVector2D LeaderEnd = FVector2D::ZeroVector;
	float OutlineRadius = 0.0f;
	bool bIsVisible = false;
};

struct FSRStrategicRouteLineLayout
{
	FName RouteId = NAME_None;
	TWeakObjectPtr<AActor> SourceBodyActor;
	TWeakObjectPtr<AActor> DestinationBodyActor;
	FVector2D Start = FVector2D::ZeroVector;
	FVector2D End = FVector2D::ZeroVector;
	ESRUIVisualState VisualState = ESRUIVisualState::Neutral;
	bool bEnabled = true;
	bool bIsVisible = false;
};

UCLASS()
class STARROVERS_API USRCelestialBodyOverviewEntryAction : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(USRCelestialBodyOverviewWidget* InOwnerWidget, AActor* InCelestialBodyActor);

	UFUNCTION()
	void HandleClicked();

private:
	UPROPERTY(Transient)
	TObjectPtr<USRCelestialBodyOverviewWidget> OwnerWidget;

	UPROPERTY(Transient)
	TObjectPtr<AActor> CelestialBodyActor;
};

UCLASS(Blueprintable)
class STARROVERS_API USRCelestialBodyOverviewWidget : public UUserWidget
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
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Celestial")
	void SetCelestialBodies(const TArray<AActor*>& NewCelestialBodies);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Celestial")
	void SetSelectedActor(AActor* NewSelectedActor);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Input")
	bool IsPointerOverOverviewUI() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|System Scan")
	bool IsBodyInitialSystemScanRecommendation(const AActor* CelestialBodyActor) const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Strategy")
	bool IsBodyStrategicBottleneck(const AActor* CelestialBodyActor) const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Strategy")
	AActor* GetRecommendedStrategicBody() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Strategy")
	FText GetStrategicSummaryLabel() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Strategy")
	bool IsStrategyOverlayVisible() const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Strategy")
	bool FocusRecommendedStrategicBody();

	void DispatchEntryClicked(AActor* CelestialBodyActor);
	FSRStarRoversCelestialBodyRequestedSignature& OnCelestialBodyRequested();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Celestial", meta = (DisplayName = "StarSystemText"))
	FText StarSystemText = NSLOCTEXT("StarRoversOverview", "StarSystemText", "System Objects");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Celestial", meta = (DisplayName = "StarSystemNameplateIndentPixels", ClampMin = "0.0"))
	float StarSystemNameplateIndentPixels = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Celestial", meta = (DisplayName = "OverviewBorderColor"))
	FLinearColor OverviewBorderColor = FLinearColor(0.015f, 0.025f, 0.04f, 0.88f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Celestial", meta = (DisplayName = "StarSystemScrollBoxButtonColor"))
	FLinearColor StarSystemScrollBoxButtonColor = FLinearColor(0.08f, 0.11f, 0.15f, 0.82f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Celestial", meta = (DisplayName = "SelectedStarSystemScrollBoxButtonColor"))
	FLinearColor SelectedStarSystemScrollBoxButtonColor = FLinearColor(0.18f, 0.36f, 0.42f, 0.95f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Celestial|System Scan", meta = (DisplayName = "RecommendedSystemScanColor"))
	FLinearColor RecommendedSystemScanColor = FLinearColor(0.95f, 0.68f, 0.18f, 0.98f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Celestial", meta = (DisplayName = "StarSystemNameplateTextColor"))
	FLinearColor StarSystemNameplateTextColor = FLinearColor(0.86f, 0.92f, 0.97f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Celestial|Nameplates", meta = (DisplayName = "bShowNameplateButtons"))
	bool bShowNameplateButtons = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Celestial|Strategy", meta = (DisplayName = "bShowStrategyOverlay"))
	bool bShowStrategyOverlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Celestial|Nameplates", meta = (DisplayName = "NameplateButtonColor"))
	FLinearColor NameplateButtonColor = FLinearColor(0.02f, 0.04f, 0.06f, 0.78f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Celestial|Nameplates", meta = (DisplayName = "SelectedNameplateButtonColor"))
	FLinearColor SelectedNameplateButtonColor = FLinearColor(0.20f, 0.48f, 0.56f, 0.92f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Celestial|Nameplates", meta = (DisplayName = "NameplateOutlineMinRadiusPixels", ClampMin = "1.0"))
	float NameplateOutlineMinRadiusPixels = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Celestial|Nameplates", meta = (DisplayName = "NameplateOutlineMaxRadiusPixels", ClampMin = "1.0"))
	float NameplateOutlineMaxRadiusPixels = 72.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Celestial|Nameplates", meta = (DisplayName = "NameplateOutlinePaddingPixels", ClampMin = "0.0"))
	float NameplateOutlinePaddingPixels = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Celestial|Nameplates", meta = (DisplayName = "NameplateLeaderLengthPixels", ClampMin = "0.0"))
	float NameplateLeaderLengthPixels = 72.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Celestial|Nameplates", meta = (DisplayName = "NameplateOutlineLineThickness", ClampMin = "0.0"))
	float NameplateOutlineLineThickness = 2.0f;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> OverviewCanvasPanel;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> OverviewBorder;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> OverviewVerticalBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StarSystemTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> StarSystemScrollBox;

	UPROPERTY(Transient)
	TObjectPtr<UButton> NameplateToggleButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameplateToggleButtonTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<USRStatusBadgeWidget> StrategicStatusBadge;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StrategicDetailTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UButton> StrategicFocusButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StrategicFocusButtonTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UButton> StrategyOverlayToggleButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StrategyOverlayToggleButtonTextBlock;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> CelestialBodies;

	UPROPERTY(Transient)
	TObjectPtr<AActor> SelectedActor;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USRCelestialBodyOverviewEntryAction>> EntryActions;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> NameplateActors;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> NameplateButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> NameplateTextBlocks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> OperationsBadgeActors;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> OperationsBadgeTextBlocks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> StarSystemRowActors;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> StarSystemRowButtons;

	UPROPERTY(Transient)
	TObjectPtr<AActor> RecommendedSystemScanBody;

	FText RecommendedSystemScanBadgeText;
	FText RecommendedSystemScanToolTipText;

	TArray<FSRNameplateButtonLayout> NameplateButtonLayouts;
	TArray<FSRStrategicRouteLineLayout> StrategicRouteLineLayouts;
	FSRStrategicOverlayPresentation StrategicPresentation;

	UPROPERTY(Transient)
	FVector LastNameplateCameraLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	FRotator LastNameplateCameraRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient)
	FVector2D LastNameplateViewportSize = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	float NameplateLayoutRefreshAccumulator = 0.0f;

	UPROPERTY(Transient)
	float OperationsBadgeRefreshAccumulator = 0.0f;

	UPROPERTY(Transient)
	bool bHasNameplateLayoutState = false;

private:
	UFUNCTION()
	void HandleNameplateToggleClicked();

	UFUNCTION()
	void HandleStrategyOverlayToggleClicked();

	UFUNCTION()
	void HandleStrategicFocusClicked();

	void BuildOverviewWidgetTree();
	void RebuildStarSystemScrollBox();
	void RefreshOperationsBadges();
	void RefreshStrategicOverlay();
	void RefreshStrategicHeader();
	void RefreshNameplateStrategicVisuals();
	bool RefreshInitialSystemScanRecommendation();
	void RebuildNameplateButtons();
	void RefreshNameplateButtonLayout();
	void RefreshStrategicRouteLineLayouts();
	bool BuildNameplateButtonLayoutForActor(AActor* CelestialBodyActor, int32 NameplateButtonIndex, FSRNameplateButtonLayout& OutLayout) const;
	void AddStarSystemScrollBoxButton(AActor* CelestialBodyActor, int32 Depth, const TMap<AActor*, TArray<AActor*>>& ChildrenByParent);
	FText GetStarSystemNameplateText(const AActor* CelestialBodyActor) const;
	FText GetWorldNameplateText(const AActor* CelestialBodyActor) const;
	FText GetStarSystemNameplatePrefixText(AActor* CelestialBodyActor) const;
	FText GetStarSystemTreePrefixText(AActor* CelestialBodyActor) const;
	int32 GetStarSystemSiblingSortIndex(AActor* CelestialBodyActor) const;
	void SortStarSystemBodies(TArray<TObjectPtr<AActor>>& StarSystemBodiesToSort) const;
	bool CompareStarSystemBodies(const AActor& Left, const AActor& Right) const;
	bool IsScreenPositionOverOverviewUI(const FVector2D& ScreenPosition) const;

	FSRStarRoversCelestialBodyRequestedSignature CelestialBodyRequestedEvent;
};
