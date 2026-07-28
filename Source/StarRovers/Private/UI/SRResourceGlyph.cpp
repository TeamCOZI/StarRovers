#include "UI/SRResourceGlyph.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Styling/SlateColor.h"
#include "UI/SRUIComponents.h"

namespace
{
	FText ResolveResourceDisplayName(const FSRResourceInstance& ResourceInstance)
	{
		if (const USRResourceDataAsset* DataAsset = ResourceInstance.ResourceDataAsset.Get())
		{
			if (!DataAsset->DisplayName.IsEmpty())
			{
				return DataAsset->DisplayName;
			}
			if (!DataAsset->ResourceId.IsNone())
			{
				return FText::FromName(DataAsset->ResourceId);
			}
		}
		return ResourceInstance.ResourceId.IsNone()
			? NSLOCTEXT("StarRoversResourceGlyph", "UnknownResource", "Unknown Resource")
			: FText::FromName(ResourceInstance.ResourceId);
	}

	FString FormatExactEnergy(double Energy)
	{
		return FString::Printf(TEXT("%.1f"), FMath::IsFinite(Energy) ? Energy : 0.0);
	}

	FString FormatCompactNumber(double Value)
	{
		const double SafeValue = FMath::IsFinite(Value) ? Value : 0.0;
		const double AbsoluteValue = FMath::Abs(SafeValue);
		if (AbsoluteValue >= 1000000000.0)
		{
			return FString::Printf(TEXT("%.1fB"), SafeValue / 1000000000.0);
		}
		if (AbsoluteValue >= 1000000.0)
		{
			return FString::Printf(TEXT("%.1fM"), SafeValue / 1000000.0);
		}
		if (AbsoluteValue >= 1000.0)
		{
			return FString::Printf(TEXT("%.1fK"), SafeValue / 1000.0);
		}
		return FMath::IsNearlyEqual(
			SafeValue,
			static_cast<double>(FMath::RoundToInt64(SafeValue)),
			0.001)
			? FString::Printf(TEXT("%.0f"), SafeValue)
			: FString::Printf(TEXT("%.1f"), SafeValue);
	}

	FText GetStateShortLabel(ESRResourceFamilyState State)
	{
		switch (State)
		{
		case ESRResourceFamilyState::Tempered: return FText::FromString(TEXT("TMP"));
		case ESRResourceFamilyState::Fatigued: return FText::FromString(TEXT("FAT"));
		case ESRResourceFamilyState::Resonant: return FText::FromString(TEXT("RES"));
		case ESRResourceFamilyState::Fractured: return FText::FromString(TEXT("FRA"));
		case ESRResourceFamilyState::Matured: return FText::FromString(TEXT("MAT"));
		case ESRResourceFamilyState::Depleted: return FText::FromString(TEXT("DEP"));
		case ESRResourceFamilyState::Energized: return FText::FromString(TEXT("ENG"));
		case ESRResourceFamilyState::Overloaded: return FText::FromString(TEXT("OVR"));
		case ESRResourceFamilyState::Echoing: return FText::FromString(TEXT("ECH"));
		case ESRResourceFamilyState::Collapsed: return FText::FromString(TEXT("COL"));
		default: return FText::FromString(TEXT("STATE"));
		}
	}

	FText GetStateFullLabel(ESRResourceFamilyState State)
	{
		const UEnum* StateEnum = StaticEnum<ESRResourceFamilyState>();
		return StateEnum
			? StateEnum->GetDisplayNameTextByValue(static_cast<int64>(State))
			: FText::FromString(TEXT("State"));
	}

	FText GetLifecycleLabel(ESRResourceSlotLifecycle Lifecycle)
	{
		const UEnum* LifecycleEnum = StaticEnum<ESRResourceSlotLifecycle>();
		return LifecycleEnum
			? LifecycleEnum->GetDisplayNameTextByValue(static_cast<int64>(Lifecycle))
			: FText::FromString(TEXT("Unknown"));
	}

	FText GetResourceClassLabel(ESRResourceClass ResourceClass)
	{
		const UEnum* ClassEnum = StaticEnum<ESRResourceClass>();
		return ClassEnum
			? ClassEnum->GetDisplayNameTextByValue(static_cast<int64>(ResourceClass))
			: FText::FromString(TEXT("Unknown"));
	}
}

FSRResourceGlyphPresentation FSRResourceGlyphPresentationBuilder::Build(
	const FSRResourceInstance& ResourceInstance)
{
	FSRResourceGlyphPresentation Result;
	Result.bHasResource = !ResourceInstance.ResourceId.IsNone()
		|| IsValid(ResourceInstance.ResourceDataAsset.Get())
		|| ResourceInstance.ResourceClass != ESRResourceClass::Unknown
		|| ResourceInstance.Family != ESRResourceFamily::None;
	if (!Result.bHasResource)
	{
		Result.DisplayName = NSLOCTEXT("StarRoversResourceGlyph", "EmptyResource", "Empty");
		Result.CompactText = Result.DisplayName;
		Result.ToolTipText = Result.DisplayName;
		Result.StableSignature = TEXT("Empty");
		return Result;
	}

	Result.ResourceId = !ResourceInstance.ResourceId.IsNone()
		? ResourceInstance.ResourceId
		: IsValid(ResourceInstance.ResourceDataAsset.Get())
			? ResourceInstance.ResourceDataAsset->ResourceId
			: NAME_None;
	Result.DisplayName = ResolveResourceDisplayName(ResourceInstance);
	Result.ResourceClass = ResourceInstance.ResourceClass;
	Result.Family = ResourceInstance.Family;
	Result.Spectrum = ResourceInstance.Spectrum;
	Result.Grade = FMath::Clamp(
		ResourceInstance.Grade,
		StarRovers::Resources::MinimumGrade,
		StarRovers::Resources::MaximumGrade);
	const bool bUsesResourceV2 = ResourceInstance.ResourceClass != ESRResourceClass::Unknown
		|| ResourceInstance.Family != ESRResourceFamily::None
		|| (IsValid(ResourceInstance.ResourceDataAsset.Get())
			&& ResourceInstance.ResourceDataAsset->ResourceDefinitionVersion
				>= StarRovers::Resources::CurrentResourceDefinitionVersion);
	Result.Energy = bUsesResourceV2
		? ResourceInstance.CurrentEnergy
		: ResourceInstance.EnergyValue;
	if (!FMath::IsFinite(Result.Energy))
	{
		Result.Energy = 0.0;
	}
	Result.StackCount = FMath::Max(1, ResourceInstance.StackCount);
	Result.FamilyToken = GetFamilyToken(Result.Family);
	Result.SpectrumGradeToken = Result.ResourceClass == ESRResourceClass::Card
		&& Result.Spectrum != ESRResourceSpectrum::None
		? GetSpectrumGradeToken(Result.Spectrum, Result.Grade)
		: FText::GetEmpty();
	Result.EnergyToken = FormatEnergyToken(Result.Energy);
	Result.StackToken = Result.StackCount > 1
		? FText::FromString(FString::Printf(TEXT("x%d"), Result.StackCount))
		: FText::GetEmpty();
	Result.FamilyAccentColor = USRUIThemeLibrary::ResolveFamilyAccentColor(Result.Family);
	Result.SpectrumAccentColor = USRUIThemeLibrary::ResolveSpectrumAccentColor(Result.Spectrum);

	TArray<FString> FullStateLabels;
	for (int32 StateIndex = 0;
		StateIndex <= static_cast<int32>(ESRResourceFamilyState::Collapsed);
		++StateIndex)
	{
		if ((ResourceInstance.ActiveFamilyStateFlags & (1 << StateIndex)) == 0)
		{
			continue;
		}
		const ESRResourceFamilyState State = static_cast<ESRResourceFamilyState>(StateIndex);
		FSRResourceGlyphStateToken& StateToken = Result.StateTokens.AddDefaulted_GetRef();
		StateToken.State = State;
		StateToken.bBeneficial = IsBeneficialState(State);
		StateToken.VisualState = StateToken.bBeneficial
			? ESRUIVisualState::Positive
			: ESRUIVisualState::Danger;
		StateToken.FullLabel = GetStateFullLabel(State);
		StateToken.ShortLabel = FText::FromString(FString::Printf(
			TEXT("%s%s"),
			StateToken.bBeneficial ? TEXT("+") : TEXT("-"),
			*GetStateShortLabel(State).ToString()));
		FullStateLabels.Add(FString::Printf(
			TEXT("%s %s"),
			StateToken.bBeneficial ? TEXT("+") : TEXT("-"),
			*StateToken.FullLabel.ToString()));
	}

	if (ResourceInstance.ProcessTagSlot.Lifecycle != ESRResourceSlotLifecycle::Empty
		&& !ResourceInstance.ProcessTagSlot.TagId.IsNone())
	{
		Result.ProcessTagToken = FText::FromString(FString::Printf(
			TEXT("TAG %s%s"),
			*ResourceInstance.ProcessTagSlot.TagId.ToString(),
			ResourceInstance.ProcessTagSlot.RemainingTriggers > 0
				? *FString::Printf(TEXT(" x%d"), ResourceInstance.ProcessTagSlot.RemainingTriggers)
				: TEXT("")));
	}
	if (!ResourceInstance.FuelImprintSlot.ImprintId.IsNone())
	{
		Result.FuelImprintToken = FText::FromString(FString::Printf(
			TEXT("SEAL %s"),
			*ResourceInstance.FuelImprintSlot.ImprintId.ToString()));
	}

	TArray<FText> CompactParts;
	CompactParts.Add(Result.DisplayName);
	CompactParts.Add(Result.FamilyToken);
	if (!Result.SpectrumGradeToken.IsEmpty()) CompactParts.Add(Result.SpectrumGradeToken);
	CompactParts.Add(Result.EnergyToken);
	for (const FSRResourceGlyphStateToken& StateToken : Result.StateTokens)
	{
		CompactParts.Add(StateToken.ShortLabel);
	}
	if (!Result.StackToken.IsEmpty()) CompactParts.Add(Result.StackToken);
	Result.CompactText = FText::Join(FText::FromString(TEXT("  ")), CompactParts);

	const FString FamilyLabel = GetFamilyLabel(Result.Family).ToString();
	const FString SpectrumLine = Result.SpectrumGradeToken.IsEmpty()
		? FString(TEXT("Spectrum / Grade: -"))
		: FString::Printf(
			TEXT("Spectrum: %s | Grade: %d"),
			*GetSpectrumLabel(Result.Spectrum).ToString(),
			Result.Grade);
	FString ToolTip = FString::Printf(
		TEXT("%s%s\nClass: %s | Family: %s\n%s\nEnergy: %s\nStates: %s"),
		*Result.DisplayName.ToString(),
		Result.StackCount > 1 ? *FString::Printf(TEXT(" x%d"), Result.StackCount) : TEXT(""),
		*GetResourceClassLabel(Result.ResourceClass).ToString(),
		*FamilyLabel,
		*SpectrumLine,
		*FormatExactEnergy(Result.Energy),
		FullStateLabels.IsEmpty() ? TEXT("None") : *FString::Join(FullStateLabels, TEXT(", ")));
	if (!Result.ProcessTagToken.IsEmpty())
	{
		ToolTip += FString::Printf(
			TEXT("\nProcess Tag: %s | %s"),
			*ResourceInstance.ProcessTagSlot.TagId.ToString(),
			*GetLifecycleLabel(ResourceInstance.ProcessTagSlot.Lifecycle).ToString());
		if (ResourceInstance.ProcessTagSlot.RemainingTriggers > 0)
		{
			ToolTip += FString::Printf(
				TEXT(" | %d trigger(s)"),
				ResourceInstance.ProcessTagSlot.RemainingTriggers);
		}
	}
	if (!Result.FuelImprintToken.IsEmpty())
	{
		ToolTip += FString::Printf(
			TEXT("\nFuel Imprint: %s"),
			*ResourceInstance.FuelImprintSlot.ImprintId.ToString());
	}
	Result.ToolTipText = FText::FromString(ToolTip);
	Result.StableSignature = FString::Printf(
		TEXT("%s|%d|%d|%d|%.6f|%d|%d|%s|%d|%d|%s"),
		*Result.ResourceId.ToString(),
		static_cast<int32>(Result.ResourceClass),
		static_cast<int32>(Result.Family),
		static_cast<int32>(Result.Spectrum),
		Result.Energy,
		Result.Grade,
		ResourceInstance.ActiveFamilyStateFlags,
		*ResourceInstance.ProcessTagSlot.TagId.ToString(),
		static_cast<int32>(ResourceInstance.ProcessTagSlot.Lifecycle),
		Result.StackCount,
		*ResourceInstance.FuelImprintSlot.ImprintId.ToString());
	return Result;
}

FSRResourceGlyphPresentation FSRResourceGlyphPresentationBuilder::Build(
	const USRResourceDataAsset& ResourceDataAsset)
{
	return Build(ResourceDataAsset.BuildDefaultInstance());
}

FSRResourceGlyphPresentation FSRResourceGlyphPresentationBuilder::BuildIdentity(
	const FText& DisplayName,
	FName ResourceId,
	ESRResourceClass ResourceClass,
	ESRResourceFamily Family,
	double Energy,
	ESRResourceSpectrum Spectrum,
	int32 Grade,
	int32 ActiveFamilyStateFlags,
	int32 StackCount)
{
	FSRResourceInstance ResourceInstance;
	ResourceInstance.ResourceId = ResourceId;
	ResourceInstance.ResourceClass = ResourceClass;
	ResourceInstance.Family = Family;
	ResourceInstance.CurrentEnergy = Energy;
	ResourceInstance.Spectrum = Spectrum;
	ResourceInstance.Grade = Grade;
	ResourceInstance.ActiveFamilyStateFlags = ActiveFamilyStateFlags;
	ResourceInstance.StackCount = StackCount;
	FSRResourceGlyphPresentation Result = Build(ResourceInstance);
	if (!DisplayName.IsEmpty())
	{
		Result.DisplayName = DisplayName;
		TArray<FText> CompactParts = {
			Result.DisplayName,
			Result.FamilyToken,
		};
		if (!Result.SpectrumGradeToken.IsEmpty()) CompactParts.Add(Result.SpectrumGradeToken);
		CompactParts.Add(Result.EnergyToken);
		Result.CompactText = FText::Join(FText::FromString(TEXT("  ")), CompactParts);
		const FString ExistingToolTip = Result.ToolTipText.ToString();
		const int32 FirstNewLine = ExistingToolTip.Find(TEXT("\n"));
		Result.ToolTipText = FText::FromString(Result.DisplayName.ToString()
			+ (FirstNewLine == INDEX_NONE ? FString() : ExistingToolTip.Mid(FirstNewLine)));
	}
	return Result;
}

FSRResourceGlyphPresentation FSRResourceGlyphPresentationBuilder::BuildFamily(
	ESRResourceFamily Family)
{
	FSRResourceGlyphPresentation Result;
	Result.bHasResource = true;
	Result.Family = Family;
	Result.DisplayName = GetFamilyLabel(Family);
	Result.FamilyToken = GetFamilyToken(Family);
	Result.FamilyAccentColor = USRUIThemeLibrary::ResolveFamilyAccentColor(Family);
	Result.SpectrumAccentColor = USRUIThemeLibrary::ResolveSpectrumAccentColor(
		ESRResourceSpectrum::None);
	Result.CompactText = Result.FamilyToken;
	Result.ToolTipText = FText::Format(
		NSLOCTEXT("StarRoversResourceGlyph", "FamilyWorkflowTooltip", "{0} Family workflow"),
		Result.DisplayName);
	Result.StableSignature = FString::Printf(TEXT("Family|%d"), static_cast<int32>(Family));
	return Result;
}

FText FSRResourceGlyphPresentationBuilder::GetFamilyLabel(ESRResourceFamily Family)
{
	if (Family == ESRResourceFamily::None)
	{
		return NSLOCTEXT("StarRoversResourceGlyph", "SharedFamily", "Shared");
	}
	const UEnum* FamilyEnum = StaticEnum<ESRResourceFamily>();
	return FamilyEnum
		? FamilyEnum->GetDisplayNameTextByValue(static_cast<int64>(Family))
		: NSLOCTEXT("StarRoversResourceGlyph", "UnknownFamily", "Unknown");
}

FText FSRResourceGlyphPresentationBuilder::GetFamilyToken(ESRResourceFamily Family)
{
	switch (Family)
	{
	case ESRResourceFamily::Metal: return FText::FromString(TEXT("■ MTL"));
	case ESRResourceFamily::Crystal: return FText::FromString(TEXT("◆ CRY"));
	case ESRResourceFamily::Organic: return FText::FromString(TEXT("● ORG"));
	case ESRResourceFamily::Plasma: return FText::FromString(TEXT("▲ PLS"));
	case ESRResourceFamily::Void: return FText::FromString(TEXT("✦ VOI"));
	case ESRResourceFamily::None:
	default: return FText::FromString(TEXT("◇ SHR"));
	}
}

FText FSRResourceGlyphPresentationBuilder::GetSpectrumLabel(ESRResourceSpectrum Spectrum)
{
	if (Spectrum == ESRResourceSpectrum::None)
	{
		return NSLOCTEXT("StarRoversResourceGlyph", "NoSpectrum", "None");
	}
	const UEnum* SpectrumEnum = StaticEnum<ESRResourceSpectrum>();
	return SpectrumEnum
		? SpectrumEnum->GetDisplayNameTextByValue(static_cast<int64>(Spectrum))
		: NSLOCTEXT("StarRoversResourceGlyph", "UnknownSpectrum", "Unknown");
}

FText FSRResourceGlyphPresentationBuilder::GetSpectrumGradeToken(
	ESRResourceSpectrum Spectrum,
	int32 Grade)
{
	const int32 SafeGrade = FMath::Clamp(
		Grade,
		StarRovers::Resources::MinimumGrade,
		StarRovers::Resources::MaximumGrade);
	switch (Spectrum)
	{
	case ESRResourceSpectrum::Red:
		return FText::FromString(FString::Printf(TEXT("◆ R%d"), SafeGrade));
	case ESRResourceSpectrum::Green:
		return FText::FromString(FString::Printf(TEXT("▲ G%d"), SafeGrade));
	case ESRResourceSpectrum::Blue:
		return FText::FromString(FString::Printf(TEXT("● B%d"), SafeGrade));
	case ESRResourceSpectrum::Yellow:
		return FText::FromString(FString::Printf(TEXT("■ Y%d"), SafeGrade));
	case ESRResourceSpectrum::None:
	default:
		return FText::GetEmpty();
	}
}

FText FSRResourceGlyphPresentationBuilder::FormatEnergyToken(double Energy)
{
	return FText::FromString(FString::Printf(TEXT("E %s"), *FormatCompactNumber(Energy)));
}

bool FSRResourceGlyphPresentationBuilder::IsBeneficialState(
	ESRResourceFamilyState State)
{
	switch (State)
	{
	case ESRResourceFamilyState::Tempered:
	case ESRResourceFamilyState::Resonant:
	case ESRResourceFamilyState::Matured:
	case ESRResourceFamilyState::Energized:
	case ESRResourceFamilyState::Echoing:
		return true;
	case ESRResourceFamilyState::Fatigued:
	case ESRResourceFamilyState::Fractured:
	case ESRResourceFamilyState::Depleted:
	case ESRResourceFamilyState::Overloaded:
	case ESRResourceFamilyState::Collapsed:
	default:
		return false;
	}
}

TSharedRef<SWidget> USRResourceGlyphWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void USRResourceGlyphWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildWidgetTree();
	RefreshPresentation();
}

void USRResourceGlyphWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	BuildWidgetTree();
	RefreshPresentation();
}

void USRResourceGlyphWidget::SetResource(const FSRResourceInstance& ResourceInstance)
{
	SetPresentation(FSRResourceGlyphPresentationBuilder::Build(ResourceInstance));
}

void USRResourceGlyphWidget::SetResourceDataAsset(USRResourceDataAsset* ResourceDataAsset)
{
	SetPresentation(IsValid(ResourceDataAsset)
		? FSRResourceGlyphPresentationBuilder::Build(*ResourceDataAsset)
		: FSRResourceGlyphPresentation());
}

void USRResourceGlyphWidget::SetFamily(ESRResourceFamily Family)
{
	SetPresentation(FSRResourceGlyphPresentationBuilder::BuildFamily(Family));
}

void USRResourceGlyphWidget::SetPresentation(
	const FSRResourceGlyphPresentation& NewPresentation)
{
	Presentation = NewPresentation;
	RefreshPresentation();
}

void USRResourceGlyphWidget::SetGlyphMode(ESRResourceGlyphMode NewMode)
{
	if (GlyphMode == NewMode)
	{
		return;
	}
	GlyphMode = NewMode;
	RefreshPresentation();
}

FSRResourceGlyphPresentation USRResourceGlyphWidget::GetPresentation() const
{
	return Presentation;
}

ESRResourceGlyphMode USRResourceGlyphWidget::GetGlyphMode() const
{
	return GlyphMode;
}

void USRResourceGlyphWidget::BuildWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}
	if (WidgetTree->RootWidget)
	{
		CacheWidgetTree();
		if (RootCard && TokenWrapBox)
		{
			RefreshPresentation();
			return;
		}
		WidgetTree->RootWidget = nullptr;
	}

	RootCard = WidgetTree->ConstructWidget<USRThemedCardWidget>(
		USRThemedCardWidget::StaticClass(),
		TEXT("ResourceGlyphRootCard"));
	WidgetTree->RootWidget = RootCard;
	RootCard->SetVisualState(ESRUIVisualState::Neutral);

	TokenWrapBox = WidgetTree->ConstructWidget<UWrapBox>(
		UWrapBox::StaticClass(),
		TEXT("ResourceGlyphTokenWrapBox"));
	TokenWrapBox->SetInnerSlotPadding(FVector2D(4.0f, 3.0f));
	RootCard->SetContent(TokenWrapBox);
	RefreshPresentation();
}

void USRResourceGlyphWidget::CacheWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}
	RootCard = Cast<USRThemedCardWidget>(
		WidgetTree->FindWidget(TEXT("ResourceGlyphRootCard")));
	TokenWrapBox = Cast<UWrapBox>(
		WidgetTree->FindWidget(TEXT("ResourceGlyphTokenWrapBox")));
}

void USRResourceGlyphWidget::RefreshPresentation()
{
	if (!RootCard || !TokenWrapBox || !WidgetTree)
	{
		return;
	}

	RootCard->SetCardPadding(GlyphMode == ESRResourceGlyphMode::FamilyOnly
		? FMargin(2.0f, 1.0f)
		: FMargin(5.0f, 3.0f));
	RootCard->SetToolTipText(Presentation.ToolTipText);
	TokenWrapBox->ClearChildren();

	if (!Presentation.bHasResource)
	{
		AddToken(
			NSLOCTEXT("StarRoversResourceGlyph", "EmptyToken", "EMPTY"),
			USRUIThemeLibrary::ResolveStatePalette(ESRUIVisualState::Disabled).AccentColor,
			TEXT("ResourceGlyphEmpty"));
		return;
	}

	if (GlyphMode == ESRResourceGlyphMode::Compact
		|| GlyphMode == ESRResourceGlyphMode::Detailed)
	{
		UTextBlock* NameTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("ResourceGlyphName"));
		NameTextBlock->SetText(Presentation.DisplayName);
		NameTextBlock->SetAutoWrapText(false);
		USRUIThemeLibrary::ApplyTextStyle(NameTextBlock, ESRUITextStyle::Caption);
		TokenWrapBox->AddChildToWrapBox(NameTextBlock);
	}

	AddToken(
		Presentation.FamilyToken,
		Presentation.FamilyAccentColor,
		TEXT("ResourceGlyphFamily"));
	if (GlyphMode != ESRResourceGlyphMode::FamilyOnly)
	{
		if (!Presentation.SpectrumGradeToken.IsEmpty())
		{
			AddToken(
				Presentation.SpectrumGradeToken,
				Presentation.SpectrumAccentColor,
				TEXT("ResourceGlyphSpectrumGrade"));
		}
		AddToken(
			Presentation.EnergyToken,
			USRUIThemeLibrary::ResolveStatePalette(ESRUIVisualState::Info).AccentColor,
			TEXT("ResourceGlyphEnergy"));
		for (const FSRResourceGlyphStateToken& StateToken : Presentation.StateTokens)
		{
			AddStateToken(StateToken);
		}
		if (GlyphMode == ESRResourceGlyphMode::Detailed)
		{
			if (!Presentation.ProcessTagToken.IsEmpty())
			{
				AddToken(
					Presentation.ProcessTagToken,
					USRUIThemeLibrary::ResolveStatePalette(ESRUIVisualState::Info).AccentColor,
					TEXT("ResourceGlyphProcessTag"));
			}
			if (!Presentation.FuelImprintToken.IsEmpty())
			{
				AddToken(
					Presentation.FuelImprintToken,
					USRUIThemeLibrary::ResolveStatePalette(ESRUIVisualState::Selected).AccentColor,
					TEXT("ResourceGlyphFuelImprint"));
			}
		}
		if (!Presentation.StackToken.IsEmpty())
		{
			AddToken(
				Presentation.StackToken,
				USRUIThemeLibrary::ResolveStatePalette(ESRUIVisualState::Neutral).AccentColor,
				TEXT("ResourceGlyphStack"));
		}
	}
}

void USRResourceGlyphWidget::AddToken(
	const FText& Text,
	const FLinearColor& AccentColor,
	FName WidgetName)
{
	if (!WidgetTree || !TokenWrapBox || Text.IsEmpty())
	{
		return;
	}
	const FSRUIStatePalette NeutralPalette =
		USRUIThemeLibrary::ResolveStatePalette(ESRUIVisualState::Neutral);
	UBorder* TokenBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		WidgetName);
	TokenBorder->SetPadding(FMargin(5.0f, 2.0f));
	FLinearColor TokenSurface = FLinearColor::LerpUsingHSV(
		NeutralPalette.SurfaceColor,
		AccentColor,
		0.24f);
	TokenSurface.A = 0.98f;
	TokenBorder->SetBrushColor(TokenSurface);

	UTextBlock* TokenTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		NAME_None);
	TokenTextBlock->SetText(Text);
	TokenTextBlock->SetAutoWrapText(false);
	USRUIThemeLibrary::ApplyTextStyle(TokenTextBlock, ESRUITextStyle::Caption);
	TokenTextBlock->SetColorAndOpacity(FSlateColor(AccentColor));
	TokenBorder->SetContent(TokenTextBlock);
	TokenWrapBox->AddChildToWrapBox(TokenBorder);
}

void USRResourceGlyphWidget::AddStateToken(
	const FSRResourceGlyphStateToken& StateToken)
{
	AddToken(
		StateToken.ShortLabel,
		USRUIThemeLibrary::ResolveStatePalette(StateToken.VisualState).AccentColor,
		FName(*FString::Printf(
			TEXT("ResourceGlyphState%d"),
			static_cast<int32>(StateToken.State))));
}
