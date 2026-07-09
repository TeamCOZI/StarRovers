#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Simulation/SRAugmentSubsystem.h"
#include "SRAugmentChoiceWidget.generated.h"

class SWidget;
class UBorder;
class UButton;
class UHorizontalBox;
class UTextBlock;
class UVerticalBox;
class USRAugmentChoiceWidget;

UCLASS()
class STARROVERS_API USRAugmentChoiceButtonAction : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(USRAugmentChoiceWidget* InOwnerWidget, int32 InChoiceIndex);

	UFUNCTION()
	void HandleClicked();

private:
	UPROPERTY(Transient)
	TObjectPtr<USRAugmentChoiceWidget> OwnerWidget;

	UPROPERTY(Transient)
	int32 ChoiceIndex = INDEX_NONE;
};

UCLASS(Blueprintable)
class STARROVERS_API USRAugmentChoiceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Augment")
	void SetAugmentChoices(const TArray<FSRAugmentChoice>& NewChoices, int32 NewCycleIndex);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Augment")
	void ClearAugmentChoices();

	UFUNCTION(BlueprintPure, Category = "StarRovers|Augment")
	TArray<FSRAugmentChoice> GetAugmentChoices() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Augment")
	int32 GetCycleIndex() const;

	void DispatchChoiceSelected(int32 ChoiceIndex);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment", meta = (DisplayName = "TitleText"))
	FText TitleText = NSLOCTEXT("StarRoversAugmentChoice", "TitleText", "Choose Augment");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment", meta = (DisplayName = "SubtitleText"))
	FText SubtitleText = NSLOCTEXT("StarRoversAugmentChoice", "SubtitleText", "Unlock one facility for construction.");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment", meta = (DisplayName = "PanelColor"))
	FLinearColor PanelColor = FLinearColor(0.018f, 0.022f, 0.028f, 0.96f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment", meta = (DisplayName = "ChoiceButtonColor"))
	FLinearColor ChoiceButtonColor = FLinearColor(0.09f, 0.13f, 0.15f, 0.98f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment", meta = (DisplayName = "ChoiceButtonHoverColor"))
	FLinearColor ChoiceButtonHoverColor = FLinearColor(0.13f, 0.22f, 0.23f, 0.98f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment", meta = (DisplayName = "PanelWidth", ClampMin = "240.0"))
	float PanelWidth = 760.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment", meta = (DisplayName = "PanelHeight", ClampMin = "240.0"))
	float PanelHeight = 320.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|Augment", meta = (DisplayName = "ChoiceButtonHeight", ClampMin = "56.0"))
	float ChoiceButtonHeight = 180.0f;

private:
	void BuildAugmentChoiceWidgetTree();
	void CacheAugmentChoiceWidgetTree();
	void RebuildChoiceButtons();
	FText FormatCycleText() const;
	FText FormatRarityText(ESRFacilityRarity Rarity) const;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PanelBorder;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> PanelVerticalBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SubtitleTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CycleTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> ChoicesHorizontalBox;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USRAugmentChoiceButtonAction>> ChoiceActions;

	UPROPERTY(Transient)
	TArray<FSRAugmentChoice> Choices;

	UPROPERTY(Transient)
	int32 CycleIndex = 0;
};
