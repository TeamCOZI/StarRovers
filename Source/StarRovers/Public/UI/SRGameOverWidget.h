#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SRGameOverWidget.generated.h"

class ASRStar;
class SWidget;
class UBorder;
class UTextBlock;

UCLASS(Blueprintable)
class STARROVERS_API USRGameOverWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Game Over")
	void SetGameOverStar(ASRStar* Star);

protected:
	UPROPERTY(Transient)
	TObjectPtr<UBorder> BackgroundBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SubtitleTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailTextBlock;

private:
	void BuildGameOverWidgetTree();
	void RefreshGameOverText();

	UPROPERTY(Transient)
	TWeakObjectPtr<ASRStar> GameOverStar;
};
