#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SRLoadingScreenWidget.generated.h"

class SWidget;
class UBorder;
class UProgressBar;
class UTextBlock;

UCLASS(Blueprintable)
class STARROVERS_API USRLoadingScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Loading")
	void SetLoadingProgress(float InProgress, const FText& InStatusText);

protected:
	UPROPERTY(Transient)
	TObjectPtr<UBorder> BackgroundBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> ProgressBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ProgressTextBlock;

private:
	void BuildLoadingScreenWidgetTree();
	void RefreshLoadingScreenText();

	float LoadingProgress = 0.0f;
	FText LoadingStatusText;
};
