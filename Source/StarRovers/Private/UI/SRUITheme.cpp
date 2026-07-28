#include "UI/SRUITheme.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateColor.h"

FName USRUIThemeSettings::GetCategoryName() const
{
	return FName(TEXT("Game"));
}

FName USRUIThemeSettings::GetSectionName() const
{
	return FName(TEXT("Star Rovers UI Theme"));
}

const USRUIThemeSettings* USRUIThemeLibrary::GetThemeSettings()
{
	return GetDefault<USRUIThemeSettings>();
}

FSRUIStatePalette USRUIThemeLibrary::ResolveStatePalette(ESRUIVisualState VisualState)
{
	const USRUIThemeSettings* Theme = GetThemeSettings();
	if (!IsValid(Theme))
	{
		return FSRUIStatePalette();
	}

	switch (VisualState)
	{
	case ESRUIVisualState::Hovered:
		return Theme->HoveredPalette;
	case ESRUIVisualState::Selected:
		return Theme->SelectedPalette;
	case ESRUIVisualState::Info:
		return Theme->InfoPalette;
	case ESRUIVisualState::Positive:
		return Theme->PositivePalette;
	case ESRUIVisualState::Warning:
		return Theme->WarningPalette;
	case ESRUIVisualState::Danger:
		return Theme->DangerPalette;
	case ESRUIVisualState::Locked:
		return Theme->LockedPalette;
	case ESRUIVisualState::Disabled:
		return Theme->DisabledPalette;
	case ESRUIVisualState::Neutral:
	default:
		return Theme->NeutralPalette;
	}
}

FLinearColor USRUIThemeLibrary::ResolveFamilyAccentColor(ESRResourceFamily Family)
{
	const USRUIThemeSettings* Theme = GetThemeSettings();
	if (!IsValid(Theme))
	{
		return FLinearColor::White;
	}

	switch (Family)
	{
	case ESRResourceFamily::Metal:
		return Theme->MetalFamilyColor;
	case ESRResourceFamily::Crystal:
		return Theme->CrystalFamilyColor;
	case ESRResourceFamily::Organic:
		return Theme->OrganicFamilyColor;
	case ESRResourceFamily::Plasma:
		return Theme->PlasmaFamilyColor;
	case ESRResourceFamily::Void:
		return Theme->VoidFamilyColor;
	case ESRResourceFamily::None:
	default:
		return Theme->NeutralFamilyColor;
	}
}

FLinearColor USRUIThemeLibrary::ResolveSpectrumAccentColor(ESRResourceSpectrum Spectrum)
{
	const USRUIThemeSettings* Theme = GetThemeSettings();
	if (!IsValid(Theme))
	{
		return FLinearColor::White;
	}

	switch (Spectrum)
	{
	case ESRResourceSpectrum::Red:
		return Theme->RedSpectrumColor;
	case ESRResourceSpectrum::Green:
		return Theme->GreenSpectrumColor;
	case ESRResourceSpectrum::Blue:
		return Theme->BlueSpectrumColor;
	case ESRResourceSpectrum::Yellow:
		return Theme->YellowSpectrumColor;
	case ESRResourceSpectrum::None:
	default:
		return Theme->NeutralSpectrumColor;
	}
}

ESRUIVisualState USRUIThemeLibrary::ResolveBuildAvailabilityVisualState(
	ESRStructureBuildAvailability Availability)
{
	switch (Availability)
	{
	case ESRStructureBuildAvailability::Available:
		return ESRUIVisualState::Positive;
	case ESRStructureBuildAvailability::LockedByAugment:
		return ESRUIVisualState::Locked;
	case ESRStructureBuildAvailability::ConstructionDisabled:
	default:
		return ESRUIVisualState::Disabled;
	}
}

FText USRUIThemeLibrary::ResolveBuildAvailabilityLabel(ESRStructureBuildAvailability Availability)
{
	switch (Availability)
	{
	case ESRStructureBuildAvailability::Available:
		return NSLOCTEXT("StarRoversUITheme", "BuildAvailabilityAvailable", "AVAILABLE");
	case ESRStructureBuildAvailability::LockedByAugment:
		return NSLOCTEXT("StarRoversUITheme", "BuildAvailabilityLocked", "AUGMENT LOCK");
	case ESRStructureBuildAvailability::ConstructionDisabled:
	default:
		return NSLOCTEXT("StarRoversUITheme", "BuildAvailabilityDisabled", "UNAVAILABLE");
	}
}

int32 USRUIThemeLibrary::ResolveFontSize(ESRUITextStyle TextStyle)
{
	const USRUIThemeSettings* Theme = GetThemeSettings();
	if (!IsValid(Theme))
	{
		return 12;
	}

	switch (TextStyle)
	{
	case ESRUITextStyle::Display:
		return Theme->DisplayFontSize;
	case ESRUITextStyle::Title:
		return Theme->TitleFontSize;
	case ESRUITextStyle::Heading:
		return Theme->HeadingFontSize;
	case ESRUITextStyle::Caption:
		return Theme->CaptionFontSize;
	case ESRUITextStyle::Metric:
		return Theme->MetricFontSize;
	case ESRUITextStyle::Body:
	default:
		return Theme->BodyFontSize;
	}
}

float USRUIThemeLibrary::ResolveSpacing(int32 UnitCount)
{
	const USRUIThemeSettings* Theme = GetThemeSettings();
	return IsValid(Theme)
		? Theme->SpacingUnit * static_cast<float>(FMath::Max(0, UnitCount))
		: 4.0f * static_cast<float>(FMath::Max(0, UnitCount));
}

void USRUIThemeLibrary::ApplyCardStyle(
	UBorder* Border,
	ESRUIVisualState VisualState,
	const FMargin& Padding)
{
	if (!IsValid(Border))
	{
		return;
	}

	Border->SetBrushColor(ResolveStatePalette(VisualState).SurfaceColor);
	Border->SetPadding(Padding);
}

void USRUIThemeLibrary::ApplyTextStyle(
	UTextBlock* TextBlock,
	ESRUITextStyle TextStyle,
	ESRUIVisualState VisualState,
	bool bUseAccentColor)
{
	if (!IsValid(TextBlock))
	{
		return;
	}

	const FSRUIStatePalette Palette = ResolveStatePalette(VisualState);
	FSlateFontInfo Font = TextBlock->GetFont();
	Font.Size = ResolveFontSize(TextStyle);
	TextBlock->SetFont(Font);
	TextBlock->SetColorAndOpacity(FSlateColor(
		bUseAccentColor
			? Palette.AccentColor
			: TextStyle == ESRUITextStyle::Body || TextStyle == ESRUITextStyle::Caption
				? Palette.SecondaryTextColor
				: Palette.PrimaryTextColor));
}
