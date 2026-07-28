#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Simulation/SRAugmentPackageContent.h"
#include "Simulation/SRAugmentSubsystem.h"
#include "UI/SRAugmentChoicePresentation.h"
#include "UI/SRAugmentChoiceWidget.h"

#if WITH_EDITOR
#include "Engine/Engine.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#endif

namespace StarRovers::AugmentChoiceTests
{
	FSRAugmentChoice MakePackageChoice(FName PackageId, ESRAugmentOfferRoleV2 OfferRole)
	{
		FSRAugmentPackageDefinitionV2 Definition;
		FSRAugmentPackageContentV2::TryGetDefinition(PackageId, Definition);

		FSRAugmentChoice Choice;
		Choice.ChoiceKind = ESRAugmentChoiceKind::ResourceV2Package;
		Choice.PackageId = Definition.PackageId;
		Choice.StrategyId = Definition.StrategyId;
		Choice.PackageRole = Definition.PackageRole;
		Choice.OfferRole = OfferRole;
		Choice.DisplayName = Definition.DisplayName;
		Choice.Description = Definition.Description;
		Choice.Rarity = Definition.Rarity;
		Choice.GrantSummary = FText::FromString(FSRAugmentPackageContentV2::BuildGrantSummary(Definition));
		Choice.ExampleLinePreview = Definition.ExampleLinePreview;
		return Choice;
	}

	FSRAugmentBuildContextV2 MakeReadyContext()
	{
		FSRAugmentBuildContextV2 Context;
		Context.AccessibleFamilies = {
			ESRResourceFamily::Metal,
			ESRResourceFamily::Crystal,
			ESRResourceFamily::Organic,
			ESRResourceFamily::Plasma,
			ESRResourceFamily::Void,
		};
		Context.AccessibleSpectra = {
			ESRResourceSpectrum::Red,
			ESRResourceSpectrum::Green,
			ESRResourceSpectrum::Blue,
			ESRResourceSpectrum::Yellow,
		};
		Context.AccessibleGrades = { 1, 2, 3, 4, 5 };
		Context.HubEndpointCount = 4;
		FSRAugmentPackageContentV2::GetTechnologyFacilityContentIds(Context.AvailableFacilityContentIds);
		return Context;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRAugmentChoiceTransitImpactTest,
	"StarRovers.UI.AugmentChoice.Presentation.TransitImpact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRAugmentChoiceTransitImpactTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::AugmentChoiceTests;
	const FSRAugmentChoice Choice = MakePackageChoice(TEXT("DeepSpaceTempering"), ESRAugmentOfferRoleV2::Immediate);
	const FSRAugmentChoicePresentation Presentation =
		FSRAugmentChoicePresentationBuilder::Build(Choice, MakeReadyContext());

	TestTrue(TEXT("The transit package is eligible in a ready multi-Hub context"), Presentation.bEligibleInContext);
	TestEqual(TEXT("The transit Package grants a hull Profile and one concrete Module"), Presentation.NewUnlockCount, 2);
	TestTrue(TEXT("The unlock section resolves the player-facing Module name"),
		Presentation.UnlockDetailText.ToString().Contains(TEXT("Cryogenic Hold")));
	TestTrue(TEXT("The unlock section names the selectable Conditioned Hold Profile separately"),
		Presentation.UnlockDetailText.ToString().Contains(TEXT("ROUTE PROFILE"))
			&& Presentation.UnlockDetailText.ToString().Contains(TEXT("Conditioned Hold")));
	TestTrue(TEXT("Fit preview exposes the actual Hub threshold"),
		Presentation.FitDetailText.ToString().Contains(TEXT("Hubs 4/2")));
	TestEqual(TEXT("Run fit counts the Family and Hub prerequisite groups"),
		Presentation.RunFitBadgeText.ToString(),
		FString(TEXT("READY 2/2")));
	TestEqual(TEXT("A transit Package exposes separate hull and Module rules"),
		Presentation.ConditionEffectRows.Num(),
		2);
	const FSRAugmentConditionEffectPresentation* ModuleFlow =
		Presentation.ConditionEffectRows.FindByPredicate(
			[](const FSRAugmentConditionEffectPresentation& Flow)
			{
				return Flow.EffectText.ToString().Contains(TEXT("E +3"));
			});
	TestNotNull(TEXT("The conditioned Module keeps its concrete transit processing row"), ModuleFlow);
	if (ModuleFlow)
	{
		TestTrue(TEXT("The flow names compatible Metal cargo"),
			ModuleFlow->ConditionText.ToString().Contains(TEXT("METAL")));
		TestTrue(TEXT("The flow exposes the actual transit Energy delta"),
			ModuleFlow->EffectText.ToString().Contains(TEXT("E +3")));
	}
	const FSRAugmentConditionEffectPresentation* ProfileFlow =
		Presentation.ConditionEffectRows.FindByPredicate(
			[](const FSRAugmentConditionEffectPresentation& Flow)
			{
				return Flow.EffectText.ToString().Contains(TEXT("CARGO/LOAD"));
			});
	TestNotNull(TEXT("The conditioned hull exposes its payload/Fleet Load tradeoff"), ProfileFlow);
	TestTrue(TEXT("Line shape names the conditioned Hold"),
		Presentation.ImpactDetailText.ToString().Contains(TEXT("Cryogenic Hold")));
	TestTrue(TEXT("The preview warns that transit processing is not passive"),
		Presentation.RiskDetailText.ToString().Contains(TEXT("Not passive")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRAugmentChoiceDoctrineCommitmentTest,
	"StarRovers.UI.AugmentChoice.Presentation.DoctrineCommitment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRAugmentChoiceDoctrineCommitmentTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::AugmentChoiceTests;
	const FSRAugmentChoice Choice = MakePackageChoice(TEXT("CentralConvergence"), ESRAugmentOfferRoleV2::Pivot);
	const FSRAugmentChoicePresentation Presentation =
		FSRAugmentChoicePresentationBuilder::Build(Choice, MakeReadyContext());

	TestTrue(TEXT("Macro Doctrine identity is preserved in presentation"), Presentation.bIsMacroDoctrine);
	TestEqual(TEXT("The Doctrine exposes two recipes plus its Bulk Raw hull"), Presentation.NewUnlockCount, 3);
	TestEqual(TEXT("Both Doctrine recipes and its hull expose their own rule row"),
		Presentation.ConditionEffectRows.Num(),
		3);
	TestTrue(TEXT("Central Convergence visibly grants Bulk Raw Hold"),
		Presentation.UnlockDetailText.ToString().Contains(TEXT("ROUTE PROFILE"))
			&& Presentation.UnlockDetailText.ToString().Contains(TEXT("Bulk Raw Hold")));
	TestTrue(TEXT("Doctrine selection exposes the single-slot commitment before click"),
		Presentation.RiskDetailText.ToString().Contains(TEXT("only Macro Doctrine slot")));
	TestTrue(TEXT("The surface Watch label keeps the commitment scannable"),
		Presentation.WatchSummaryText.ToString().Contains(TEXT("DOCTRINE SLOT")));
	TestEqual(TEXT("A Pivot offer uses the warning semantic state"),
		Presentation.OfferState,
		ESRUIVisualState::Warning);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRAugmentChoiceStaleOfferTest,
	"StarRovers.UI.AugmentChoice.Presentation.StaleOfferGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRAugmentChoiceStaleOfferTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::AugmentChoiceTests;
	FSRAugmentBuildContextV2 Context = MakeReadyContext();
	Context.SelectedPackageIds.Add(TEXT("StateResonator"));
	const FSRAugmentChoice Choice = MakePackageChoice(TEXT("StateResonator"), ESRAugmentOfferRoleV2::Synergy);
	const FSRAugmentChoicePresentation Presentation =
		FSRAugmentChoicePresentationBuilder::Build(Choice, Context);

	TestEqual(TEXT("An already granted recipe is not advertised as a new unlock"), Presentation.NewUnlockCount, 0);
	TestEqual(TEXT("The duplicate grant is counted explicitly"), Presentation.AlreadyAvailableCount, 1);
	TestFalse(TEXT("A stale already-selected offer is marked context-changed"), Presentation.bEligibleInContext);
	TestTrue(TEXT("The stale fit badge changes instead of promising READY"),
		Presentation.RunFitBadgeText.ToString().Contains(TEXT("CHANGED")));
	TestEqual(TEXT("A stale card uses the disabled visual contract"),
		Presentation.CardState,
		ESRUIVisualState::Disabled);
	TestTrue(TEXT("The stale offer tells the player why it is no longer new"),
		Presentation.RiskDetailText.ToString().Contains(TEXT("already available")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRAugmentChoiceTagFlowTest,
	"StarRovers.UI.AugmentChoice.Presentation.TagConditionEffect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRAugmentChoiceTagFlowTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::AugmentChoiceTests;
	const FSRAugmentChoice Choice = MakePackageChoice(TEXT("StateResonator"), ESRAugmentOfferRoleV2::Immediate);
	const FSRAugmentChoicePresentation Presentation =
		FSRAugmentChoicePresentationBuilder::Build(Choice, MakeReadyContext());

	TestTrue(TEXT("A Tag Package explicitly labels its grant as a recipe"),
		Presentation.UnlockDetailText.ToString().Contains(TEXT("TAG RECIPE")));
	TestEqual(TEXT("State Resonator has one readable rule row"),
		Presentation.ConditionEffectRows.Num(),
		1);
	if (!Presentation.ConditionEffectRows.IsEmpty())
	{
		const FSRAugmentConditionEffectPresentation& Flow = Presentation.ConditionEffectRows[0];
		TestTrue(TEXT("The condition names positive-State activation"),
			Flow.ConditionText.ToString().Contains(TEXT("POSITIVE STATE")));
		TestTrue(TEXT("The result names Overtone"),
			Flow.EffectText.ToString().Contains(TEXT("OVERTONE")));
		TestTrue(TEXT("The result exposes the actual +5 additive Energy"),
			Flow.EffectText.ToString().Contains(TEXT("E +5")));
	}
	TestTrue(TEXT("Full explanation is retained outside the scan surface"),
		Presentation.FullDetailText.ToString().Contains(TEXT("not passive Energy")));
	return true;
}

#if WITH_EDITOR

namespace StarRovers::AugmentChoiceTests
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

	class FWaitForAugmentChoiceWidgetCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FWaitForAugmentChoiceWidgetCommand(FAutomationTestBase& InTest)
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
				Test.AddError(TEXT("A PIE world was not available for Augment Choice validation."));
				return true;
			}
			const USRAugmentSubsystem* AugmentSubsystem =
				PIEWorld->GetSubsystem<USRAugmentSubsystem>();
			const bool bResourceContextReady = IsValid(AugmentSubsystem)
				&& !AugmentSubsystem->BuildResourceV2OfferContext().AccessibleFamilies.IsEmpty();
			if (!bResourceContextReady)
			{
				if (FPlatformTime::Seconds() - StartTimeSeconds < 60.0)
				{
					return false;
				}
				Test.AddError(TEXT("The PIE Run did not generate an accessible Card Family for Augment validation."));
				return true;
			}

			USRAugmentChoiceWidget* ChoiceWidget = CreateWidget<USRAugmentChoiceWidget>(
				PIEWorld,
				USRAugmentChoiceWidget::StaticClass());
			Test.TestNotNull(TEXT("The native Augment Choice widget is constructible in PIE"), ChoiceWidget);
			if (!IsValid(ChoiceWidget))
			{
				return true;
			}

			const TArray<FSRAugmentChoice> Choices = {
				MakePackageChoice(TEXT("StateResonator"), ESRAugmentOfferRoleV2::Immediate),
				MakePackageChoice(TEXT("RecoveryDividend"), ESRAugmentOfferRoleV2::Pivot),
				MakePackageChoice(TEXT("DeepSpaceTempering"), ESRAugmentOfferRoleV2::Synergy),
			};
			ChoiceWidget->SetAugmentChoices(Choices, 6);
			const TSharedRef<SWidget> SlateWidget = ChoiceWidget->TakeWidget();
			Test.TestTrue(TEXT("The Augment Choice widget builds a cached Slate hierarchy"),
				ChoiceWidget->GetCachedWidget().IsValid());

			const TArray<FName> RequiredWidgets = {
				TEXT("AugmentChoicePanelScaleBox"),
				TEXT("AugmentChoicePanelDesignSizeBox"),
				TEXT("AugmentChoicePanelBorder"),
				TEXT("AugmentChoiceHeaderStatusBox"),
				TEXT("AugmentChoiceCycleStatusBadge"),
				TEXT("AugmentChoiceGuaranteeStatusBadge"),
				TEXT("AugmentChoiceDecisionStatusBadge"),
				TEXT("AugmentChoiceChoicesScrollBox"),
				TEXT("AugmentChoiceCard1"),
				TEXT("AugmentChoiceOfferBadge1"),
				TEXT("AugmentChoiceFitBadge1"),
				TEXT("AugmentChoiceStrategyBadge1"),
				TEXT("AugmentChoiceGrantTextBlock1"),
				TEXT("AugmentChoiceFitTextBlock1"),
				TEXT("AugmentChoiceConditionEffectBox1"),
				TEXT("AugmentChoiceConditionCard1_1"),
				TEXT("AugmentChoiceConditionTextBlock1_1"),
				TEXT("AugmentChoiceFlowArrow1_1"),
				TEXT("AugmentChoiceEffectCard1_1"),
				TEXT("AugmentChoiceEffectTextBlock1_1"),
				TEXT("AugmentChoiceImpactTextBlock1"),
				TEXT("AugmentChoiceRiskTextBlock1"),
				TEXT("AugmentChoiceSelectTextBlock1"),
				TEXT("AugmentChoiceCard2"),
				TEXT("AugmentChoiceCard3"),
				TEXT("AugmentChoiceConditionEffectBox3"),
				TEXT("AugmentChoiceConditionCard3_1"),
				TEXT("AugmentChoiceEffectCard3_1"),
				TEXT("AugmentChoiceConditionCard3_2"),
				TEXT("AugmentChoiceEffectCard3_2"),
			};
			for (const FName WidgetName : RequiredWidgets)
			{
				Test.TestNotNull(
					*FString::Printf(TEXT("Augment Choice contains %s"), *WidgetName.ToString()),
					ChoiceWidget->GetWidgetFromName(WidgetName));
			}
			Test.TestTrue(TEXT("The first Augment choice accepts default keyboard or gamepad focus"),
				ChoiceWidget->FocusDefaultChoice());
			Test.TestEqual(TEXT("Default Augment focus starts on the first choice"),
				ChoiceWidget->GetFocusedChoiceIndex(),
				0);
			Test.TestTrue(TEXT("Right navigation moves Augment focus"), ChoiceWidget->NavigateChoiceFocus(1));
			Test.TestEqual(TEXT("Augment focus moves to the second choice"),
				ChoiceWidget->GetFocusedChoiceIndex(),
				1);
			Test.TestTrue(TEXT("Left navigation moves Augment focus back"), ChoiceWidget->NavigateChoiceFocus(-1));
			Test.TestEqual(TEXT("Augment focus returns to the first choice"),
				ChoiceWidget->GetFocusedChoiceIndex(),
				0);
			return true;
		}

	private:
		FAutomationTestBase& Test;
		double StartTimeSeconds = 0.0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRAugmentChoicePIETest,
	"StarRovers.UI.AugmentChoice.PIE.NativeImpactPreview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRAugmentChoicePIETest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Levels/SolarSystem")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(
		StarRovers::AugmentChoiceTests::FWaitForAugmentChoiceWidgetCommand(*this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif

#endif
