#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Pattern/SRPatternTypes.h"
#include "SRPatternGridWidget.generated.h"

class UGridPanel;
class UTextBlock;
class UBorder;
class SWidget;

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPatternGridCellPresentation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|UI")
	int32 Row = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|UI")
	int32 Column = 0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|UI")
	ESRGlyphType Glyph = ESRGlyphType::Empty;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|UI")
	bool bMaskActive = true;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|UI")
	FText GlyphLabel;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|UI")
	FLinearColor FillColor = FLinearColor::Black;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|Pattern|UI")
	FLinearColor TextColor = FLinearColor::White;
};

class STARROVERS_API FSRPatternGridPresentation final
{
public:
	static bool BuildCells(
		const FSRPattern& Pattern,
		const FSRPatternMask* OptionalMask,
		TArray<FSRPatternGridCellPresentation>& OutCells,
		FString& OutFailureReason);

	static FText GetGlyphLabel(ESRGlyphType Glyph);
	static FLinearColor GetGlyphColor(ESRGlyphType Glyph);
};

UCLASS(Blueprintable)
class STARROVERS_API USRPatternGridWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativePreConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Pattern|UI")
	void SetPattern(const FSRPattern& NewPattern);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Pattern|UI")
	void SetPatternAndMask(const FSRPattern& NewPattern, const FSRPatternMask& NewMask);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Pattern|UI")
	void ClearPattern();

	UFUNCTION(BlueprintCallable, Category = "StarRovers|Pattern|UI")
	void SetCellSize(float NewCellSize);

	UFUNCTION(BlueprintPure, Category = "StarRovers|Pattern|UI")
	FSRPattern GetPattern() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Pattern|UI")
	FSRPatternMask GetPatternMask() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|Pattern|UI")
	bool IsUsingPatternMask() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Pattern|UI")
	FSRPattern DisplayPattern;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Pattern|UI")
	FSRPatternMask DisplayMask;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Pattern|UI")
	bool bUseMask = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Pattern|UI", meta = (ClampMin = "10.0", ClampMax = "80.0"))
	float CellSize = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Pattern|UI")
	FLinearColor ActiveCellFrameColor = FLinearColor(0.56f, 0.72f, 0.82f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Pattern|UI")
	FLinearColor InactiveCellFrameColor = FLinearColor(0.08f, 0.10f, 0.12f, 0.65f);

private:
	void BuildPatternGrid();
	void RefreshPatternGrid();

	UPROPERTY(Transient)
	TObjectPtr<UGridPanel> GridPanel;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> CellFrames;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> CellFills;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> CellLabels;
};
