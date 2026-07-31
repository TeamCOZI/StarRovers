#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SRStellarContractHUDWidget.generated.h"

class ASRStar;
class SWidget;
class UBorder;
class UProgressBar;
class UTextBlock;
class USRPatternGridWidget;

UCLASS(Blueprintable)
class STARROVERS_API USRStellarContractHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Stellar Contract HUD", meta = (ClampMin = "0.0"))
	float RefreshInterval = 0.10f;

private:
	void BuildWidgetTree();
	void RefreshFromPrimaryStar();
	ASRStar* ResolvePrimaryStar() const;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PanelBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ContractTitleTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<USRPatternGridWidget> TargetPatternGridWidget;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ScoreTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> StellarHealthProgressBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StellarHealthTextBlock;

	float TimeUntilRefresh = 0.0f;
};
