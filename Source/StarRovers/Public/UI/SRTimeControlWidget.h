#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "SRTimeControlWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UImage;
class UHorizontalBox;
class UProgressBar;
class UScaleBox;
class USizeBox;
class USRStatusBadgeWidget;
class UTextBlock;
class UTexture2D;
class SWidget;
class AActor;

UCLASS(Blueprintable)
class STARROVERS_API USRTimeControlWidget : public UUserWidget
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

	UFUNCTION(BlueprintPure, Category = "StarRovers|Input")
	bool IsPointerOverTimeControlPanel() const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|UI|Top Bar")
	void SetPauseButtonIcon(const FSlateBrush& NewIconBrush);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|UI|Top Bar")
	void SetPlayButtonIcon(const FSlateBrush& NewIconBrush);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|UI|Top Bar")
	void SetFastForwardButtonIcon(const FSlateBrush& NewIconBrush);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|UI|Top Bar")
	void SetFlaskButtonIcon(const FSlateBrush& NewIconBrush);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|UI|Top Bar")
	void SetRouteButtonIcon(const FSlateBrush& NewIconBrush);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|UI|Top Bar")
	void SetStatsButtonIcon(const FSlateBrush& NewIconBrush);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|UI|Top Bar")
	void SetHelpButtonIcon(const FSlateBrush& NewIconBrush);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|UI|Top Bar")
	void SetCodexButtonIcon(const FSlateBrush& NewIconBrush);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|UI|Top Bar")
	void SetSettingsButtonIcon(const FSlateBrush& NewIconBrush);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|UI|Bottom Bar")
	void SetMiniMapImage(const FSlateBrush& NewImageBrush);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Top Bar", meta = (DisplayName = "Top Bar Height Viewport Ratio", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "0.25"))
	float TopBarHeightViewportRatio = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Top Bar", meta = (DisplayName = "Cycle Count Height Viewport Ratio", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "0.25"))
	float CycleCountHeightViewportRatio = 0.032f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Bottom Bar", meta = (DisplayName = "Bottom Focus Name Height Viewport Ratio", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "0.25"))
	float BottomFocusNameHeightViewportRatio = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Bottom Bar", meta = (DisplayName = "Bottom Focus Width Viewport Ratio", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float BottomFocusWidthViewportRatio = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Bottom Bar", meta = (DisplayName = "Mini Map Enabled"))
	bool bMiniMapEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Bottom Bar", meta = (DisplayName = "Mini Map Inner Size Ratio", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float MiniMapInnerSizeRatio = 0.90f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Bottom Bar", meta = (DisplayName = "Mini Map Mask Height To Width Ratio", ClampMin = "0.1", ClampMax = "1.0", UIMin = "0.1", UIMax = "1.0"))
	float MiniMapMaskHeightToWidthRatio = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Bottom Bar", meta = (DisplayName = "Mini Map Outline Thickness Pixels", ClampMin = "0", UIMin = "0", UIMax = "8"))
	int32 MiniMapOutlineThicknessPixels = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Bottom Bar", meta = (DisplayName = "Mini Map Refresh Interval", ClampMin = "0.0", UIMin = "0.0"))
	float MiniMapRefreshInterval = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Top Bar", meta = (DisplayName = "Top Center Width Viewport Ratio", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float TopCenterWidthViewportRatio = 0.56f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Top Bar", meta = (DisplayName = "Progress Bar Height Ratio", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float ProgressBarHeightRatio = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Top Bar", meta = (DisplayName = "Cycle Count Width Viewport Ratio", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float CycleCountWidthViewportRatio = 0.34f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Top Bar", meta = (DisplayName = "Button Gap To Width Ratio", ClampMin = "0.0", UIMin = "0.0", UIMax = "1.0"))
	float ButtonGapToWidthRatio = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Top Bar", meta = (DisplayName = "Fast Forward Time Scale", ClampMin = "0.0", UIMin = "0.0"))
	float FastForwardTimeScale = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Top Bar", meta = (DisplayName = "Pause Button Icon"))
	FSlateBrush PauseButtonIconBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Top Bar", meta = (DisplayName = "Play Button Icon"))
	FSlateBrush PlayButtonIconBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Top Bar", meta = (DisplayName = "Fast Forward Button Icon"))
	FSlateBrush FastForwardButtonIconBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Top Bar", meta = (DisplayName = "Flask Button Icon"))
	FSlateBrush FlaskButtonIconBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Top Bar", meta = (DisplayName = "Route Button Icon"))
	FSlateBrush RouteButtonIconBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Top Bar", meta = (DisplayName = "Stats Button Icon"))
	FSlateBrush StatsButtonIconBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Top Bar", meta = (DisplayName = "Help Button Icon"))
	FSlateBrush HelpButtonIconBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Top Bar", meta = (DisplayName = "Codex Button Icon"))
	FSlateBrush CodexButtonIconBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Top Bar", meta = (DisplayName = "Settings Button Icon"))
	FSlateBrush SettingsButtonIconBrush;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Bottom Bar", meta = (DisplayName = "Mini Map Image"))
	FSlateBrush MiniMapImageBrush;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> TopBarContainerBorder;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> TopBarCanvasPanel;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> TopLeftControlsCanvasPanel;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> FuelSupplyProgressContainer;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> FuelSupplyProgressBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FuelSupplyProgressTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UScaleBox> StellarSurvivalRailScaleBox;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> StellarSurvivalRailHorizontalBox;

	UPROPERTY(Transient)
	TObjectPtr<USRStatusBadgeWidget> StellarSurvivalTimeBadge;

	UPROPERTY(Transient)
	TObjectPtr<USRStatusBadgeWidget> StellarObjectiveBadge;

	UPROPERTY(Transient)
	TObjectPtr<USRStatusBadgeWidget> StellarIncomeBadge;

	UPROPERTY(Transient)
	TObjectPtr<USRStatusBadgeWidget> StellarConsumptionBadge;

	UPROPERTY(Transient)
	TObjectPtr<USRStatusBadgeWidget> StellarNetBadge;

	UPROPERTY(Transient)
	TObjectPtr<USRStatusBadgeWidget> StellarInboundBadge;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> CycleProgressContainer;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> CycleProgressBar;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> TopRightControlsCanvasPanel;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> CycleCountContainerBorder;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> CycleCountCanvasPanel;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> CycleCountTextSizeBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CycleCountTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> BottomFocusNameContainerBorder;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> BottomFocusNameCanvasPanel;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> BottomFocusNameTextSizeBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BottomFocusNameTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> MiniMapContainerBorder;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> MiniMapCanvasPanel;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> MiniMapImageSizeBox;

	UPROPERTY(Transient)
	TObjectPtr<UImage> MiniMapImage;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> PauseButtonSizeBox;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PauseButton;

	UPROPERTY(Transient)
	TObjectPtr<UImage> PauseButtonImage;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> PlayButtonSizeBox;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PlayButton;

	UPROPERTY(Transient)
	TObjectPtr<UImage> PlayButtonImage;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> FastForwardButtonSizeBox;

	UPROPERTY(Transient)
	TObjectPtr<UButton> FastForwardButton;

	UPROPERTY(Transient)
	TObjectPtr<UImage> FastForwardButtonImage;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> FlaskButtonSizeBox;

	UPROPERTY(Transient)
	TObjectPtr<UButton> FlaskButton;

	UPROPERTY(Transient)
	TObjectPtr<UImage> FlaskButtonImage;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> RouteButtonSizeBox;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RouteButton;

	UPROPERTY(Transient)
	TObjectPtr<UImage> RouteButtonImage;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> StatsButtonSizeBox;

	UPROPERTY(Transient)
	TObjectPtr<UButton> StatsButton;

	UPROPERTY(Transient)
	TObjectPtr<UImage> StatsButtonImage;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> HelpButtonSizeBox;

	UPROPERTY(Transient)
	TObjectPtr<UButton> HelpButton;

	UPROPERTY(Transient)
	TObjectPtr<UImage> HelpButtonImage;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> CodexButtonSizeBox;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CodexButton;

	UPROPERTY(Transient)
	TObjectPtr<UImage> CodexButtonImage;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> SettingsButtonSizeBox;

	UPROPERTY(Transient)
	TObjectPtr<UButton> SettingsButton;

	UPROPERTY(Transient)
	TObjectPtr<UImage> SettingsButtonImage;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> DefaultPauseIconTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> DefaultPlayIconTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> DefaultFastForwardIconTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> DefaultFlaskIconTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> DefaultRouteIconTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> DefaultStatsIconTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> DefaultHelpIconTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> DefaultCodexIconTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> DefaultSettingsIconTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> DefaultMiniMapTexture;

	UPROPERTY(Transient)
	TObjectPtr<AActor> LastMiniMapSourceActor;

	UPROPERTY(Transient)
	float MiniMapRefreshAccumulator = 0.0f;

	UPROPERTY(Transient)
	float LayoutRefreshAccumulator = 0.0f;

	UPROPERTY(Transient)
	float StateRefreshAccumulator = 0.0f;

	UPROPERTY(Transient)
	FVector2D LastLayoutWidgetSize = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	FVector2D LastLayoutTopBarSize = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	FString LastFocusedBodyNameString;

	UPROPERTY(Transient)
	FString LastFuelSupplyTextString;

	UPROPERTY(Transient)
	float LastFuelSupplyProgressRatio = -1.0f;

	UPROPERTY(Transient)
	float LastCycleProgressRatio = -1.0f;

	UPROPERTY(Transient)
	int32 LastCycleIndex = INDEX_NONE;

	UPROPERTY(Transient)
	FString LastCycleSummaryTextString;

	UPROPERTY(Transient)
	bool bHasLayoutState = false;

	UFUNCTION(BlueprintImplementableEvent, Category = "StarRovers|Simulation")
	void OnTimeControlChanged(bool bIsPaused, float CurrentTimeScale);

private:
	void BuildTimeControlWidgetTree();
	void BindTimeControlButtonHandlers();
	void RefreshTimeControlState();
	void RefreshProgressState();
	void RefreshFocusedBodyState();
	void RefreshMiniMapTextureFromFocusedBody(bool bForceRefresh);
	void RefreshButtonIconBrushes();
	void SynchronizeTopBarLayout();
	class USRTimeControlSubsystem* GetTimeControlSubsystem() const;
	void UpdateButtonStyle(UButton* Button, bool bIsActive) const;
	bool IsScreenPositionOverTimeControlPanel(const FVector2D& ScreenPosition) const;

	UFUNCTION()
	void HandlePauseClicked();

	UFUNCTION()
	void HandlePlayClicked();

	UFUNCTION()
	void HandleFastForwardClicked();
};
