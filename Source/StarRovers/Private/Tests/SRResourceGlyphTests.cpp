#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/SRResourceGlyph.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourceGlyphPresentationTest,
	"StarRovers.UI.ResourceGlyph.Presentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourceGlyphPresentationTest::RunTest(const FString& Parameters)
{
	FSRResourceInstance Resource;
	Resource.ResourceId = TEXT("HeliosIron");
	Resource.ResourceClass = ESRResourceClass::Card;
	Resource.Family = ESRResourceFamily::Metal;
	Resource.CurrentEnergy = 1234.0;
	Resource.Spectrum = ESRResourceSpectrum::Red;
	Resource.Grade = 2;
	Resource.StackCount = 3;
	Resource.ActiveFamilyStateFlags =
		(1 << static_cast<int32>(ESRResourceFamilyState::Tempered))
		| (1 << static_cast<int32>(ESRResourceFamilyState::Fatigued));
	Resource.ProcessTagSlot.TagId = TEXT("RedCatalyst");
	Resource.ProcessTagSlot.Lifecycle = ESRResourceSlotLifecycle::Primed;
	Resource.ProcessTagSlot.RemainingTriggers = 2;
	Resource.FuelImprintSlot.ImprintId = TEXT("DeepSpaceSeal");

	const FSRResourceGlyphPresentation Presentation =
		FSRResourceGlyphPresentationBuilder::Build(Resource);
	TestTrue(TEXT("A concrete Card produces a visible resource presentation"),
		Presentation.bHasResource);
	TestTrue(TEXT("Family identity includes a shape and the stable Metal abbreviation"),
		Presentation.FamilyToken.ToString().Contains(TEXT("MTL"))
			&& Presentation.FamilyToken.ToString().Len() > 3);
	TestTrue(TEXT("Spectrum and Grade remain one glanceable token"),
		Presentation.SpectrumGradeToken.ToString().Contains(TEXT("R2"))
			&& Presentation.SpectrumGradeToken.ToString().Len() > 2);
	TestEqual(TEXT("Large Energy values use a compact display token"),
		Presentation.EnergyToken.ToString(),
		FString(TEXT("E 1.2K")));
	TestEqual(TEXT("Both active Family States remain independently visible"),
		Presentation.StateTokens.Num(),
		2);
	if (Presentation.StateTokens.Num() == 2)
	{
		TestEqual(TEXT("The beneficial state carries an explicit plus sign"),
			Presentation.StateTokens[0].ShortLabel.ToString(),
			FString(TEXT("+TMP")));
		TestEqual(TEXT("The harmful state carries an explicit minus sign"),
			Presentation.StateTokens[1].ShortLabel.ToString(),
			FString(TEXT("-FAT")));
		TestEqual(TEXT("The harmful state also uses the danger semantic state"),
			Presentation.StateTokens[1].VisualState,
			ESRUIVisualState::Danger);
	}
	TestTrue(TEXT("Detailed mode can expose the Process Tag slot"),
		Presentation.ProcessTagToken.ToString().Contains(TEXT("TAG RedCatalyst x2")));
	TestTrue(TEXT("Detailed mode can expose the Fuel Imprint slot"),
		Presentation.FuelImprintToken.ToString().Contains(TEXT("SEAL DeepSpaceSeal")));
	TestTrue(TEXT("The tooltip preserves exact Energy while the face remains compact"),
		Presentation.ToolTipText.ToString().Contains(TEXT("Energy: 1234.0")));
	TestFalse(TEXT("Internal processing counters never leak into the player card"),
		Presentation.ToolTipText.ToString().Contains(TEXT("Process Count"))
			|| Presentation.ToolTipText.ToString().Contains(TEXT("Energy Change Count")));

	const TArray<ESRResourceFamily> Families = {
		ESRResourceFamily::Metal,
		ESRResourceFamily::Crystal,
		ESRResourceFamily::Organic,
		ESRResourceFamily::Plasma,
		ESRResourceFamily::Void,
	};
	TSet<FString> FamilyTokens;
	for (const ESRResourceFamily Family : Families)
	{
		FamilyTokens.Add(FSRResourceGlyphPresentationBuilder::GetFamilyToken(Family).ToString());
	}
	TestEqual(TEXT("Every gameplay Family has a distinct non-color token"),
		FamilyTokens.Num(),
		Families.Num());

	const TArray<ESRResourceSpectrum> Spectra = {
		ESRResourceSpectrum::Red,
		ESRResourceSpectrum::Green,
		ESRResourceSpectrum::Blue,
		ESRResourceSpectrum::Yellow,
	};
	TSet<FString> SpectrumTokens;
	for (const ESRResourceSpectrum Spectrum : Spectra)
	{
		SpectrumTokens.Add(
			FSRResourceGlyphPresentationBuilder::GetSpectrumGradeToken(Spectrum, 2).ToString());
	}
	TestEqual(TEXT("Every Spectrum has a distinct shape and letter token"),
		SpectrumTokens.Num(),
		Spectra.Num());

	FSRResourceInstance ChangedResource = Resource;
	ChangedResource.Grade = 3;
	const FSRResourceGlyphPresentation ChangedPresentation =
		FSRResourceGlyphPresentationBuilder::Build(ChangedResource);
	TestNotEqual(TEXT("Visible identity changes invalidate UI presentation caches"),
		ChangedPresentation.StableSignature,
		Presentation.StableSignature);
	return true;
}

#endif
