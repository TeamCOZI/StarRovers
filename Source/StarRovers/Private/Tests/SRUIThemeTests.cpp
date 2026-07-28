#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/SRResourceGlyph.h"
#include "UI/SRUIComponents.h"
#include "UI/SRUITheme.h"

#if WITH_EDITOR
#include "Engine/Engine.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRUIThemeSemanticMappingTest,
	"StarRovers.UI.Theme.SemanticMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRUIThemeSemanticMappingTest::RunTest(const FString& Parameters)
{
	const USRUIThemeSettings* Theme = USRUIThemeLibrary::GetThemeSettings();
	TestNotNull(TEXT("The shared UI theme is available from Developer Settings"), Theme);
	if (!Theme)
	{
		return false;
	}

	TestEqual(TEXT("Available construction uses the positive semantic state"),
		USRUIThemeLibrary::ResolveBuildAvailabilityVisualState(ESRStructureBuildAvailability::Available),
		ESRUIVisualState::Positive);
	TestEqual(TEXT("Augment locks use the locked semantic state"),
		USRUIThemeLibrary::ResolveBuildAvailabilityVisualState(ESRStructureBuildAvailability::LockedByAugment),
		ESRUIVisualState::Locked);
	TestEqual(TEXT("Disabled construction uses the disabled semantic state"),
		USRUIThemeLibrary::ResolveBuildAvailabilityVisualState(ESRStructureBuildAvailability::ConstructionDisabled),
		ESRUIVisualState::Disabled);
	TestFalse(TEXT("Every construction state has a player-facing label"),
		USRUIThemeLibrary::ResolveBuildAvailabilityLabel(ESRStructureBuildAvailability::LockedByAugment).IsEmpty());

	const TArray<ESRResourceFamily> Families = {
		ESRResourceFamily::Metal,
		ESRResourceFamily::Crystal,
		ESRResourceFamily::Organic,
		ESRResourceFamily::Plasma,
		ESRResourceFamily::Void,
	};
	TArray<FLinearColor> FamilyColors;
	for (const ESRResourceFamily Family : Families)
	{
		const FLinearColor Color = USRUIThemeLibrary::ResolveFamilyAccentColor(Family);
		TestTrue(TEXT("Family accents are visible"), Color.A > 0.9f);
		TestFalse(TEXT("Every Family receives a distinct accent"), FamilyColors.ContainsByPredicate(
			[Color](const FLinearColor& ExistingColor)
			{
				return ExistingColor.Equals(Color, KINDA_SMALL_NUMBER);
			}));
		FamilyColors.Add(Color);
	}

	const TArray<ESRResourceSpectrum> Spectra = {
		ESRResourceSpectrum::Red,
		ESRResourceSpectrum::Green,
		ESRResourceSpectrum::Blue,
		ESRResourceSpectrum::Yellow,
	};
	TArray<FLinearColor> SpectrumColors;
	for (const ESRResourceSpectrum Spectrum : Spectra)
	{
		const FLinearColor Color = USRUIThemeLibrary::ResolveSpectrumAccentColor(Spectrum);
		TestTrue(TEXT("Spectrum accents are visible"), Color.A > 0.9f);
		TestFalse(TEXT("Every Spectrum receives a distinct accent"),
			SpectrumColors.ContainsByPredicate(
				[Color](const FLinearColor& ExistingColor)
				{
					return ExistingColor.Equals(Color, KINDA_SMALL_NUMBER);
				}));
		SpectrumColors.Add(Color);
	}

	TestTrue(TEXT("Display text is larger than headings"),
		USRUIThemeLibrary::ResolveFontSize(ESRUITextStyle::Display)
			> USRUIThemeLibrary::ResolveFontSize(ESRUITextStyle::Heading));
	TestTrue(TEXT("Metrics are larger than body text"),
		USRUIThemeLibrary::ResolveFontSize(ESRUITextStyle::Metric)
			> USRUIThemeLibrary::ResolveFontSize(ESRUITextStyle::Body));
	TestTrue(TEXT("Spacing is based on one configurable unit"),
		FMath::IsNearlyEqual(
			USRUIThemeLibrary::ResolveSpacing(3),
			Theme->SpacingUnit * 3.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRUIThemeComponentStateTest,
	"StarRovers.UI.Theme.ReusableComponents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRUIThemeComponentStateTest::RunTest(const FString& Parameters)
{
	USRThemedCardWidget* Card = NewObject<USRThemedCardWidget>(GetTransientPackage());
	TestNotNull(TEXT("A native themed card can be created without a Blueprint"), Card);
	if (!Card)
	{
		return false;
	}
	Card->SetVisualState(ESRUIVisualState::Warning);
	TestEqual(TEXT("The card stores its semantic state"), Card->GetVisualState(), ESRUIVisualState::Warning);
	TestTrue(TEXT("The card resolves its surface from the shared palette"),
		Card->GetBrushColor().Equals(
			USRUIThemeLibrary::ResolveStatePalette(ESRUIVisualState::Warning).SurfaceColor,
			KINDA_SMALL_NUMBER));

	USRStatusBadgeWidget* Badge = NewObject<USRStatusBadgeWidget>(GetTransientPackage());
	TestNotNull(TEXT("A native status badge can be created without a Blueprint"), Badge);
	if (Badge)
	{
		Badge->SetBadge(FText::FromString(TEXT("Blocked")), ESRUIVisualState::Danger);
		TestEqual(TEXT("The badge stores its semantic state"), Badge->GetVisualState(), ESRUIVisualState::Danger);
		TestEqual(TEXT("The badge stores its label"), Badge->GetLabel().ToString(), FString(TEXT("Blocked")));
	}

	USRInfoCardWidget* InfoCard = NewObject<USRInfoCardWidget>(GetTransientPackage());
	TestNotNull(TEXT("A native info card can be created without a Blueprint"), InfoCard);
	if (InfoCard)
	{
		InfoCard->SetCardData(
			FText::FromString(TEXT("Capacity")),
			FText::FromString(TEXT("24 / 30")),
			FText::FromString(TEXT("Normal load")),
			ESRUIVisualState::Info);
		TestEqual(TEXT("Info card title is data-driven"), InfoCard->GetTitleText().ToString(), FString(TEXT("Capacity")));
		TestEqual(TEXT("Info card value is data-driven"), InfoCard->GetValueText().ToString(), FString(TEXT("24 / 30")));
		TestEqual(TEXT("Info card detail is data-driven"), InfoCard->GetDetailText().ToString(), FString(TEXT("Normal load")));
	}
	return true;
}

#if WITH_EDITOR

namespace StarRovers::UIThemeTests
{
	UWorld* FindPIEWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
			{
				return Context.World();
			}
		}
		return nullptr;
	}

	class FWaitForNativeUIThemeComponentsCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FWaitForNativeUIThemeComponentsCommand(FAutomationTestBase& InTest)
			: Test(InTest)
			, StartTimeSeconds(FPlatformTime::Seconds())
		{
		}

		virtual bool Update() override
		{
			UWorld* PIEWorld = FindPIEWorld();
			if (!IsValid(PIEWorld))
			{
				if (FPlatformTime::Seconds() - StartTimeSeconds < 30.0)
				{
					return false;
				}
				Test.AddError(TEXT("A PIE world was not available for native UI component validation."));
				return true;
			}

			USRStatusBadgeWidget* Badge = CreateWidget<USRStatusBadgeWidget>(PIEWorld, USRStatusBadgeWidget::StaticClass());
			Test.TestNotNull(TEXT("Status Badge is constructible through the runtime CreateWidget path"), Badge);
			if (IsValid(Badge))
			{
				Badge->SetBadge(FText::FromString(TEXT("PIE Warning")), ESRUIVisualState::Warning);
				const TSharedRef<SWidget> BadgeSlateWidget = Badge->TakeWidget();
				Test.TestTrue(TEXT("Status Badge builds a cached Slate hierarchy"), Badge->GetCachedWidget().IsValid());
				Test.TestNotNull(TEXT("Status Badge contains its native themed root"),
					Badge->GetWidgetFromName(FName(TEXT("StatusBadgeRootCard"))));
			}

			USRInfoCardWidget* InfoCard = CreateWidget<USRInfoCardWidget>(PIEWorld, USRInfoCardWidget::StaticClass());
			Test.TestNotNull(TEXT("Info Card is constructible through the runtime CreateWidget path"), InfoCard);
			if (IsValid(InfoCard))
			{
				InfoCard->SetCardData(
					FText::FromString(TEXT("Operational Capacity")),
					FText::FromString(TEXT("18 / 30")),
					FText::FromString(TEXT("PIE component check")),
					ESRUIVisualState::Info);
				InfoCard->SetStatus(FText::FromString(TEXT("Stable")), ESRUIVisualState::Positive);
				const TSharedRef<SWidget> InfoCardSlateWidget = InfoCard->TakeWidget();
				Test.TestTrue(TEXT("Info Card builds a cached Slate hierarchy"), InfoCard->GetCachedWidget().IsValid());
				Test.TestNotNull(TEXT("Info Card contains its native status badge"),
					InfoCard->GetWidgetFromName(FName(TEXT("InfoCardStatusBadge"))));
			}

			FSRResourceInstance Resource;
			Resource.ResourceId = TEXT("HeliosIron");
			Resource.ResourceClass = ESRResourceClass::Card;
			Resource.Family = ESRResourceFamily::Metal;
			Resource.CurrentEnergy = 55.0;
			Resource.Spectrum = ESRResourceSpectrum::Red;
			Resource.Grade = 2;
			Resource.ActiveFamilyStateFlags =
				1 << static_cast<int32>(ESRResourceFamilyState::Tempered);
			Resource.ProcessTagSlot.TagId = TEXT("RedCatalyst");
			Resource.ProcessTagSlot.Lifecycle = ESRResourceSlotLifecycle::Primed;
			Resource.ProcessTagSlot.RemainingTriggers = 1;
			Resource.FuelImprintSlot.ImprintId = TEXT("DeepSpaceSeal");
			USRResourceGlyphWidget* ResourceGlyph =
				CreateWidget<USRResourceGlyphWidget>(
					PIEWorld,
					USRResourceGlyphWidget::StaticClass());
			Test.TestNotNull(
				TEXT("Resource Glyph is constructible through the runtime CreateWidget path"),
				ResourceGlyph);
			if (IsValid(ResourceGlyph))
			{
				ResourceGlyph->SetGlyphMode(ESRResourceGlyphMode::Detailed);
				ResourceGlyph->SetResource(Resource);
				const TSharedRef<SWidget> ResourceGlyphSlateWidget = ResourceGlyph->TakeWidget();
				Test.TestTrue(TEXT("Resource Glyph builds a cached Slate hierarchy"),
					ResourceGlyph->GetCachedWidget().IsValid());
				const TArray<FName> RequiredGlyphTokens = {
					TEXT("ResourceGlyphFamily"),
					TEXT("ResourceGlyphSpectrumGrade"),
					TEXT("ResourceGlyphEnergy"),
					TEXT("ResourceGlyphProcessTag"),
					TEXT("ResourceGlyphFuelImprint"),
				};
				for (const FName TokenName : RequiredGlyphTokens)
				{
					Test.TestNotNull(
						*FString::Printf(TEXT("Resource Glyph creates %s"), *TokenName.ToString()),
						ResourceGlyph->GetWidgetFromName(TokenName));
				}
			}
			return true;
		}

	private:
		FAutomationTestBase& Test;
		double StartTimeSeconds = 0.0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRUIThemeNativeComponentsPIETest,
	"StarRovers.UI.Theme.PIE.NativeComponents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRUIThemeNativeComponentsPIETest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Levels/SolarSystem")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(
		StarRovers::UIThemeTests::FWaitForNativeUIThemeComponentsCommand(*this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif

#endif
