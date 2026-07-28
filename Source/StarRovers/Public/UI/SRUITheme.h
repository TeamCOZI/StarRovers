#pragma once

#include "CoreMinimal.h"
#include "Assembly/SRStructureBuildCatalog.h"
#include "Engine/DeveloperSettings.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SRUITheme.generated.h"

class UBorder;
class UTextBlock;

UENUM(BlueprintType)
enum class ESRUIVisualState : uint8
{
	Neutral UMETA(DisplayName = "Neutral"),
	Hovered UMETA(DisplayName = "Hovered"),
	Selected UMETA(DisplayName = "Selected"),
	Info UMETA(DisplayName = "Info"),
	Positive UMETA(DisplayName = "Positive"),
	Warning UMETA(DisplayName = "Warning"),
	Danger UMETA(DisplayName = "Danger"),
	Locked UMETA(DisplayName = "Locked"),
	Disabled UMETA(DisplayName = "Disabled"),
};

UENUM(BlueprintType)
enum class ESRUITextStyle : uint8
{
	Display UMETA(DisplayName = "Display"),
	Title UMETA(DisplayName = "Title"),
	Heading UMETA(DisplayName = "Heading"),
	Body UMETA(DisplayName = "Body"),
	Caption UMETA(DisplayName = "Caption"),
	Metric UMETA(DisplayName = "Metric"),
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRUIStatePalette
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Theme")
	FLinearColor SurfaceColor = FLinearColor(0.055f, 0.075f, 0.10f, 0.96f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Theme")
	FLinearColor AccentColor = FLinearColor(0.28f, 0.46f, 0.58f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Theme")
	FLinearColor PrimaryTextColor = FLinearColor(0.92f, 0.96f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StarRovers|UI|Theme")
	FLinearColor SecondaryTextColor = FLinearColor(0.68f, 0.76f, 0.82f, 1.0f);
};

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Star Rovers UI Theme"))
class STARROVERS_API USRUIThemeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Surfaces")
	FLinearColor BackdropColor = FLinearColor(0.008f, 0.014f, 0.024f, 0.88f);

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Surfaces")
	FLinearColor PanelColor = FLinearColor(0.015f, 0.025f, 0.040f, 0.94f);

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Semantic Palettes")
	FSRUIStatePalette NeutralPalette;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Semantic Palettes")
	FSRUIStatePalette HoveredPalette = {
		FLinearColor(0.080f, 0.130f, 0.170f, 0.98f),
		FLinearColor(0.35f, 0.74f, 0.84f, 1.0f),
		FLinearColor(0.94f, 0.98f, 1.0f, 1.0f),
		FLinearColor(0.74f, 0.84f, 0.90f, 1.0f),
	};

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Semantic Palettes")
	FSRUIStatePalette SelectedPalette = {
		FLinearColor(0.060f, 0.240f, 0.300f, 0.98f),
		FLinearColor(0.26f, 0.86f, 0.94f, 1.0f),
		FLinearColor(0.96f, 0.99f, 1.0f, 1.0f),
		FLinearColor(0.78f, 0.91f, 0.94f, 1.0f),
	};

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Semantic Palettes")
	FSRUIStatePalette InfoPalette = {
		FLinearColor(0.060f, 0.130f, 0.190f, 0.98f),
		FLinearColor(0.34f, 0.70f, 1.0f, 1.0f),
		FLinearColor(0.92f, 0.97f, 1.0f, 1.0f),
		FLinearColor(0.68f, 0.82f, 0.92f, 1.0f),
	};

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Semantic Palettes")
	FSRUIStatePalette PositivePalette = {
		FLinearColor(0.055f, 0.160f, 0.115f, 0.98f),
		FLinearColor(0.31f, 0.88f, 0.58f, 1.0f),
		FLinearColor(0.91f, 1.0f, 0.95f, 1.0f),
		FLinearColor(0.67f, 0.88f, 0.75f, 1.0f),
	};

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Semantic Palettes")
	FSRUIStatePalette WarningPalette = {
		FLinearColor(0.190f, 0.125f, 0.045f, 0.98f),
		FLinearColor(1.0f, 0.70f, 0.24f, 1.0f),
		FLinearColor(1.0f, 0.96f, 0.88f, 1.0f),
		FLinearColor(0.93f, 0.79f, 0.56f, 1.0f),
	};

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Semantic Palettes")
	FSRUIStatePalette DangerPalette = {
		FLinearColor(0.200f, 0.060f, 0.060f, 0.98f),
		FLinearColor(1.0f, 0.34f, 0.30f, 1.0f),
		FLinearColor(1.0f, 0.93f, 0.92f, 1.0f),
		FLinearColor(0.94f, 0.68f, 0.66f, 1.0f),
	};

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Semantic Palettes")
	FSRUIStatePalette LockedPalette = {
		FLinearColor(0.120f, 0.080f, 0.180f, 0.98f),
		FLinearColor(0.72f, 0.50f, 0.95f, 1.0f),
		FLinearColor(0.96f, 0.92f, 1.0f, 1.0f),
		FLinearColor(0.78f, 0.69f, 0.88f, 1.0f),
	};

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Semantic Palettes")
	FSRUIStatePalette DisabledPalette = {
		FLinearColor(0.040f, 0.050f, 0.060f, 0.72f),
		FLinearColor(0.28f, 0.30f, 0.34f, 1.0f),
		FLinearColor(0.52f, 0.55f, 0.58f, 1.0f),
		FLinearColor(0.38f, 0.41f, 0.44f, 1.0f),
	};

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Family Accents")
	FLinearColor NeutralFamilyColor = FLinearColor(0.55f, 0.65f, 0.72f, 1.0f);

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Family Accents")
	FLinearColor MetalFamilyColor = FLinearColor(0.92f, 0.52f, 0.28f, 1.0f);

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Family Accents")
	FLinearColor CrystalFamilyColor = FLinearColor(0.35f, 0.72f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Family Accents")
	FLinearColor OrganicFamilyColor = FLinearColor(0.40f, 0.86f, 0.46f, 1.0f);

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Family Accents")
	FLinearColor PlasmaFamilyColor = FLinearColor(0.98f, 0.82f, 0.30f, 1.0f);

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Family Accents")
	FLinearColor VoidFamilyColor = FLinearColor(0.68f, 0.48f, 0.96f, 1.0f);

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Spectrum Accents")
	FLinearColor NeutralSpectrumColor = FLinearColor(0.62f, 0.69f, 0.75f, 1.0f);

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Spectrum Accents")
	FLinearColor RedSpectrumColor = FLinearColor(1.0f, 0.34f, 0.30f, 1.0f);

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Spectrum Accents")
	FLinearColor GreenSpectrumColor = FLinearColor(0.31f, 0.88f, 0.58f, 1.0f);

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Spectrum Accents")
	FLinearColor BlueSpectrumColor = FLinearColor(0.34f, 0.70f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Spectrum Accents")
	FLinearColor YellowSpectrumColor = FLinearColor(1.0f, 0.78f, 0.24f, 1.0f);

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Typography", meta = (ClampMin = "8"))
	int32 DisplayFontSize = 32;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Typography", meta = (ClampMin = "8"))
	int32 TitleFontSize = 20;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Typography", meta = (ClampMin = "8"))
	int32 HeadingFontSize = 15;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Typography", meta = (ClampMin = "8"))
	int32 BodyFontSize = 12;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Typography", meta = (ClampMin = "8"))
	int32 CaptionFontSize = 10;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Typography", meta = (ClampMin = "8"))
	int32 MetricFontSize = 24;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Spacing", meta = (ClampMin = "1.0"))
	float SpacingUnit = 4.0f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Spacing", meta = (ClampMin = "0.0"))
	FMargin CardPadding = FMargin(12.0f, 10.0f);

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Spacing", meta = (ClampMin = "0.0"))
	FMargin BadgePadding = FMargin(7.0f, 3.0f);
};

UCLASS()
class STARROVERS_API USRUIThemeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "StarRovers|UI|Theme")
	static const USRUIThemeSettings* GetThemeSettings();

	UFUNCTION(BlueprintPure, Category = "StarRovers|UI|Theme")
	static FSRUIStatePalette ResolveStatePalette(ESRUIVisualState VisualState);

	UFUNCTION(BlueprintPure, Category = "StarRovers|UI|Theme")
	static FLinearColor ResolveFamilyAccentColor(ESRResourceFamily Family);

	UFUNCTION(BlueprintPure, Category = "StarRovers|UI|Theme")
	static FLinearColor ResolveSpectrumAccentColor(ESRResourceSpectrum Spectrum);

	UFUNCTION(BlueprintPure, Category = "StarRovers|UI|Theme")
	static ESRUIVisualState ResolveBuildAvailabilityVisualState(ESRStructureBuildAvailability Availability);

	UFUNCTION(BlueprintPure, Category = "StarRovers|UI|Theme")
	static FText ResolveBuildAvailabilityLabel(ESRStructureBuildAvailability Availability);

	UFUNCTION(BlueprintPure, Category = "StarRovers|UI|Theme")
	static int32 ResolveFontSize(ESRUITextStyle TextStyle);

	UFUNCTION(BlueprintPure, Category = "StarRovers|UI|Theme")
	static float ResolveSpacing(int32 UnitCount = 1);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|UI|Theme")
	static void ApplyCardStyle(UBorder* Border, ESRUIVisualState VisualState, const FMargin& Padding);

	UFUNCTION(BlueprintCallable, Category = "StarRovers|UI|Theme")
	static void ApplyTextStyle(
		UTextBlock* TextBlock,
		ESRUITextStyle TextStyle,
		ESRUIVisualState VisualState = ESRUIVisualState::Neutral,
		bool bUseAccentColor = false);
};
