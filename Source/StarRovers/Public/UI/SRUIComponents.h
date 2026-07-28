#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Border.h"
#include "UI/SRUITheme.h"
#include "SRUIComponents.generated.h"

class SWidget;
class UHorizontalBox;
class USizeBox;
class UTextBlock;
class UVerticalBox;

/** A one-child panel that always resolves its surface from the shared theme. */
UCLASS(Blueprintable, meta = (DisplayName = "Star Rovers Themed Card"))
class STARROVERS_API USRThemedCardWidget : public UBorder
{
	GENERATED_BODY()

public:
	USRThemedCardWidget(const FObjectInitializer& ObjectInitializer);

	virtual void SynchronizeProperties() override;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|UI|Theme")
	void SetVisualState(ESRUIVisualState NewVisualState);

	UFUNCTION(BlueprintPure, Category = "StarRovers|UI|Theme")
	ESRUIVisualState GetVisualState() const;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|UI|Theme")
	void SetCardPadding(const FMargin& NewCardPadding);

	UFUNCTION(BlueprintPure, Category = "StarRovers|UI|Theme")
	FSRUIStatePalette GetResolvedPalette() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Theme")
	ESRUIVisualState VisualState = ESRUIVisualState::Neutral;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Theme")
	FMargin CardPadding = FMargin(12.0f, 10.0f);

private:
	void RefreshTheme();
};

/** Compact semantic label used for lock, warning, operational, and selection states. */
UCLASS(Blueprintable, meta = (DisplayName = "Star Rovers Status Badge"))
class STARROVERS_API USRStatusBadgeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|UI|Status")
	void SetBadge(FText NewLabel, ESRUIVisualState NewVisualState);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|UI|Status")
	void SetLabel(FText NewLabel);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|UI|Status")
	void SetVisualState(ESRUIVisualState NewVisualState);

	UFUNCTION(BlueprintPure, Category = "StarRovers|UI|Status")
	FText GetLabel() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|UI|Status")
	ESRUIVisualState GetVisualState() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Status")
	FText Label = NSLOCTEXT("StarRoversUI", "DefaultStatusBadge", "STATUS");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Status")
	ESRUIVisualState VisualState = ESRUIVisualState::Neutral;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Status")
	bool bShowIndicator = true;

private:
	void BuildWidgetTree();
	void CacheWidgetTree();
	void RefreshPresentation();

	UPROPERTY(Transient)
	TObjectPtr<USRThemedCardWidget> RootCard;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> IndicatorSizeBox;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> IndicatorBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LabelTextBlock;
};

/** Reusable title/value/detail card for Capacity, throughput, and inspector summaries. */
UCLASS(Blueprintable, meta = (DisplayName = "Star Rovers Info Card"))
class STARROVERS_API USRInfoCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|UI|Card")
	void SetCardData(
		FText NewTitle,
		FText NewValue,
		FText NewDetail,
		ESRUIVisualState NewVisualState);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|UI|Card")
	void SetStatus(FText NewStatusText, ESRUIVisualState NewStatusState, bool bVisible = true);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|UI|Card")
	void ClearStatus();

	UFUNCTION(BlueprintPure, Category = "StarRovers|UI|Card")
	FText GetTitleText() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|UI|Card")
	FText GetValueText() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|UI|Card")
	FText GetDetailText() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Card")
	FText TitleText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Card")
	FText ValueText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Card")
	FText DetailText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Card")
	ESRUIVisualState VisualState = ESRUIVisualState::Neutral;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Card")
	FText StatusText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Card")
	ESRUIVisualState StatusState = ESRUIVisualState::Info;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Card")
	bool bShowStatus = false;

private:
	void BuildWidgetTree();
	void CacheWidgetTree();
	void RefreshPresentation();

	UPROPERTY(Transient)
	TObjectPtr<USRThemedCardWidget> RootCard;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ValueTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<USRStatusBadgeWidget> StatusBadge;
};
