#pragma once

#include "Automation/SRResourceDataAsset.h"
#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "UI/SRUITheme.h"
#include "SRResourceGlyph.generated.h"

class SWidget;
class USRThemedCardWidget;
class UWrapBox;

/** Controls how much of the shared resource language is visible in one context. */
UENUM(BlueprintType)
enum class ESRResourceGlyphMode : uint8
{
	FamilyOnly UMETA(DisplayName = "Family Only"),
	Identity UMETA(DisplayName = "Identity"),
	Compact UMETA(DisplayName = "Compact"),
	Detailed UMETA(DisplayName = "Detailed"),
};

/** One signed Family State token. Text and sign remain meaningful without color. */
USTRUCT(BlueprintType)
struct STARROVERS_API FSRResourceGlyphStateToken
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Resource Glyph")
	ESRResourceFamilyState State = ESRResourceFamilyState::Tempered;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Resource Glyph")
	FText ShortLabel;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Resource Glyph")
	FText FullLabel;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Resource Glyph")
	ESRUIVisualState VisualState = ESRUIVisualState::Neutral;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Resource Glyph")
	bool bBeneficial = false;
};

/** Screen-independent resource identity consumed by every compact UI representation. */
USTRUCT(BlueprintType)
struct STARROVERS_API FSRResourceGlyphPresentation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Resource Glyph")
	bool bHasResource = false;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Resource Glyph")
	FName ResourceId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Resource Glyph")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Resource Glyph")
	ESRResourceClass ResourceClass = ESRResourceClass::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Resource Glyph")
	ESRResourceFamily Family = ESRResourceFamily::None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Resource Glyph")
	ESRResourceSpectrum Spectrum = ESRResourceSpectrum::None;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Resource Glyph")
	int32 Grade = StarRovers::Resources::MinimumGrade;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Resource Glyph")
	double Energy = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Resource Glyph")
	int32 StackCount = 1;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Resource Glyph")
	FText FamilyToken;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Resource Glyph")
	FText SpectrumGradeToken;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Resource Glyph")
	FText EnergyToken;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Resource Glyph")
	FText StackToken;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Resource Glyph")
	FText ProcessTagToken;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Resource Glyph")
	FText FuelImprintToken;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Resource Glyph")
	TArray<FSRResourceGlyphStateToken> StateTokens;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Resource Glyph")
	FText CompactText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Resource Glyph")
	FText ToolTipText;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Resource Glyph")
	FLinearColor FamilyAccentColor = FLinearColor::White;

	UPROPERTY(BlueprintReadOnly, Category = "StarRovers|UI|Resource Glyph")
	FLinearColor SpectrumAccentColor = FLinearColor::White;

	/** Contains every visible identity field and is safe for UI refresh caches. */
	FString StableSignature;
};

/** Pure formatter that owns the resource visual language and accessibility text. */
class STARROVERS_API FSRResourceGlyphPresentationBuilder final
{
public:
	static FSRResourceGlyphPresentation Build(const FSRResourceInstance& ResourceInstance);
	static FSRResourceGlyphPresentation Build(const USRResourceDataAsset& ResourceDataAsset);
	static FSRResourceGlyphPresentation BuildIdentity(
		const FText& DisplayName,
		FName ResourceId,
		ESRResourceClass ResourceClass,
		ESRResourceFamily Family,
		double Energy,
		ESRResourceSpectrum Spectrum,
		int32 Grade,
		int32 ActiveFamilyStateFlags = 0,
		int32 StackCount = 1);
	static FSRResourceGlyphPresentation BuildFamily(ESRResourceFamily Family);

	static FText GetFamilyLabel(ESRResourceFamily Family);
	static FText GetFamilyToken(ESRResourceFamily Family);
	static FText GetSpectrumLabel(ESRResourceSpectrum Spectrum);
	static FText GetSpectrumGradeToken(ESRResourceSpectrum Spectrum, int32 Grade);
	static FText FormatEnergyToken(double Energy);
	static bool IsBeneficialState(ESRResourceFamilyState State);
};

/** Reusable native glyph: Family + Spectrum/Grade + Energy + signed State + Tag/Imprint. */
UCLASS(Blueprintable, meta = (DisplayName = "Star Rovers Resource Glyph"))
class STARROVERS_API USRResourceGlyphWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "StarRovers|UI|Resource Glyph")
	void SetResource(const FSRResourceInstance& ResourceInstance);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|UI|Resource Glyph")
	void SetResourceDataAsset(USRResourceDataAsset* ResourceDataAsset);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|UI|Resource Glyph")
	void SetFamily(ESRResourceFamily Family);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|UI|Resource Glyph")
	void SetPresentation(const FSRResourceGlyphPresentation& NewPresentation);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|UI|Resource Glyph")
	void SetGlyphMode(ESRResourceGlyphMode NewMode);

	UFUNCTION(BlueprintPure, Category = "StarRovers|UI|Resource Glyph")
	FSRResourceGlyphPresentation GetPresentation() const;

	UFUNCTION(BlueprintPure, Category = "StarRovers|UI|Resource Glyph")
	ESRResourceGlyphMode GetGlyphMode() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Resource Glyph")
	ESRResourceGlyphMode GlyphMode = ESRResourceGlyphMode::Compact;

private:
	void BuildWidgetTree();
	void CacheWidgetTree();
	void RefreshPresentation();
	void AddToken(const FText& Text, const FLinearColor& AccentColor, FName WidgetName = NAME_None);
	void AddStateToken(const FSRResourceGlyphStateToken& StateToken);

	UPROPERTY(Transient)
	TObjectPtr<USRThemedCardWidget> RootCard;

	UPROPERTY(Transient)
	TObjectPtr<UWrapBox> TokenWrapBox;

	FSRResourceGlyphPresentation Presentation;
};
