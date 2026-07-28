#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/SRFacilityControlWidget.h"
#include "UI/SRFacilityInspectorPresentation.h"

#if WITH_EDITOR
#include "Engine/Engine.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRFacilityInspectorActivityTest,
	"StarRovers.UI.FacilityInspector.ActivityStates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRFacilityInspectorActivityTest::RunTest(const FString& Parameters)
{
	FSRFacilityInspectorPresentationInput Input;
	Input.bProcessEnabled = true;
	Input.bCanOperate = true;
	Input.InputResourceCount = 0;
	Input.bPreviewResolved = false;

	FSRFacilityInspectorPresentation Presentation =
		FSRFacilityInspectorPresentationBuilder::Build(Input);
	TestEqual(TEXT("An empty required input is a visible waiting state"),
		Presentation.Activity,
		ESRFacilityInspectorActivity::WaitingForInput);
	TestEqual(TEXT("Waiting for input is a warning, not a fatal error"),
		Presentation.StatusVisualState,
		ESRUIVisualState::Warning);

	Input.InputResourceCount = 1;
	Presentation = FSRFacilityInspectorPresentationBuilder::Build(Input);
	TestEqual(TEXT("Queued input without a valid preview exposes the Recipe mismatch"),
		Presentation.Activity,
		ESRFacilityInspectorActivity::Blocked);
	TestEqual(TEXT("A Recipe mismatch is shown as danger"),
		Presentation.StatusVisualState,
		ESRUIVisualState::Danger);

	Input.bPreviewResolved = true;
	Input.bProcessing = true;
	Input.ProgressRatio = 0.42f;
	Presentation = FSRFacilityInspectorPresentationBuilder::Build(Input);
	TestEqual(TEXT("An active cycle takes precedence over ready state"),
		Presentation.Activity,
		ESRFacilityInspectorActivity::Processing);
	TestTrue(TEXT("Processing exposes exact progress in its detail"),
		Presentation.StatusDetail.ToString().Contains(TEXT("42%")));

	Input.bOutputBlocked = true;
	Presentation = FSRFacilityInspectorPresentationBuilder::Build(Input);
	TestEqual(TEXT("A completed cycle with no output space exposes its bottleneck"),
		Presentation.Activity,
		ESRFacilityInspectorActivity::WaitingForOutput);

	Input.bProcessing = false;
	Input.bOutputBlocked = false;
	Input.OperationalSpeedFactor = 0.65f;
	Presentation = FSRFacilityInspectorPresentationBuilder::Build(Input);
	TestEqual(TEXT("Operational Capacity slowdown has a distinct state"),
		Presentation.Activity,
		ESRFacilityInspectorActivity::Throttled);
	TestTrue(TEXT("Throttle detail exposes effective speed"),
		Presentation.StatusDetail.ToString().Contains(TEXT("65%")));

	Input.bProcessEnabled = false;
	Presentation = FSRFacilityInspectorPresentationBuilder::Build(Input);
	TestEqual(TEXT("Player-disabled processing is always shown as offline"),
		Presentation.Activity,
		ESRFacilityInspectorActivity::Disabled);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRFacilityInspectorEnergyScopeTest,
	"StarRovers.UI.FacilityInspector.EnergyScope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRFacilityInspectorEnergyScopeTest::RunTest(const FString& Parameters)
{
	FSRFacilityInspectorPresentationInput Input;
	Input.bProcessEnabled = true;
	Input.bCanOperate = true;
	Input.bPreviewResolved = true;
	Input.InputResourceCount = 1;
	Input.OutputResourceCount = 1;
	Input.bHasEnergyTransition = true;
	Input.InputEnergy = 12.0;
	Input.OutputEnergy = 17.0;

	FSRFacilityInspectorPresentation Presentation =
		FSRFacilityInspectorPresentationBuilder::Build(Input);
	const FString AdditiveEnergyText = Presentation.EnergyTransition.ToString();
	TestTrue(TEXT("Ordinary processing names its additive rule"),
		AdditiveEnergyText.Contains(TEXT("additive")));
	TestTrue(TEXT("Ordinary processing exposes the signed Energy delta"),
		AdditiveEnergyText.Contains(TEXT("+5.0")));
	TestFalse(TEXT("Ordinary processing never presents the final multiplier formula"),
		AdditiveEnergyText.Contains(TEXT("A + B x C")));

	Input.bUsesFinalFuelFormula = true;
	Input.InputResourceCount = 5;
	Input.InputEnergy = 220.0;
	Input.OutputEnergy = 1000.0;
	Presentation = FSRFacilityInspectorPresentationBuilder::Build(Input);
	const FString FinalEnergyText = Presentation.EnergyTransition.ToString();
	TestTrue(TEXT("The Stellar Fuel Fabricator explicitly owns A + B x C"),
		FinalEnergyText.Contains(TEXT("A + B x C")));
	TestFalse(TEXT("The final fabrication formula is not mislabeled as additive"),
		FinalEnergyText.Contains(TEXT("additive")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRFacilityInspectorStellarFuelBatchTest,
	"StarRovers.UI.FacilityInspector.StellarFuelBatchStates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRFacilityInspectorStellarFuelBatchTest::RunTest(const FString& Parameters)
{
	FSRFacilityInspectorPresentationInput Input;
	Input.bProcessEnabled = true;
	Input.bCanOperate = true;
	Input.bUsesFinalFuelFormula = true;
	Input.bUsesStellarFuelBatch = true;
	Input.StellarFuelBatchState = ESRStellarFuelBatchStateV2::Collecting;
	Input.StellarFuelValidCardCount = 3;
	Input.StellarFuelBatchSummary = TEXT("BATCH 3/5 | Current: One Pair | Empty lanes: 4, 5");
	Input.StellarFuelBatchDetail = TEXT("CARD LANES | 1:R2  2:B2  3:G4  4:EMPTY  5:EMPTY");
	Input.InputResourceCount = 3;

	FSRFacilityInspectorPresentation Presentation =
		FSRFacilityInspectorPresentationBuilder::Build(Input);
	TestEqual(TEXT("A partial valid hand is assembling instead of a Recipe mismatch"),
		Presentation.Activity,
		ESRFacilityInspectorActivity::WaitingForInput);
	TestEqual(TEXT("The glanceable status exposes the exact fill count"),
		Presentation.StatusLabel.ToString(),
		FString(TEXT("ASSEMBLING 3/5")));
	TestTrue(TEXT("The process rule exposes the current hand pattern"),
		Presentation.ProcessRule.ToString().Contains(TEXT("One Pair")));
	TestTrue(TEXT("The lane diagram is visible in the state row"),
		Presentation.StateTransition.ToString().Contains(TEXT("4:EMPTY")));

	Input.StellarFuelBatchState = ESRStellarFuelBatchStateV2::Contaminated;
	Input.StellarFuelBatchDetail = TEXT("BLOCKED LANES | 2\nResource is not a valid Family Card.");
	Presentation = FSRFacilityInspectorPresentationBuilder::Build(Input);
	TestEqual(TEXT("Contamination becomes a blocking danger state"),
		Presentation.StatusVisualState,
		ESRUIVisualState::Danger);
	TestEqual(TEXT("Contamination has a unique status label"),
		Presentation.StatusLabel.ToString(),
		FString(TEXT("BATCH CONTAMINATED")));
	TestTrue(TEXT("Output guidance tells the player to clear the blocked lane"),
		Presentation.OutputCaption.ToString().Contains(TEXT("Clear blocked")));

	Input.StellarFuelBatchState = ESRStellarFuelBatchStateV2::Reserved;
	Input.StellarFuelValidCardCount = 5;
	Input.InputResourceCount = 5;
	Input.bPreviewResolved = true;
	Input.bHasEnergyTransition = true;
	Input.OutputResourceCount = 1;
	Input.OutputEnergy = 1180.0;
	Input.bProcessing = true;
	Input.ProgressRatio = 0.35f;
	Presentation = FSRFacilityInspectorPresentationBuilder::Build(Input);
	TestEqual(TEXT("A committed five-Card batch is visibly reserved"),
		Presentation.StatusLabel.ToString(),
		FString(TEXT("BATCH RESERVED")));
	TestTrue(TEXT("The reserved state still exposes cycle progress"),
		Presentation.StatusDetail.ToString().Contains(TEXT("35%")));
	return true;
}

#if WITH_EDITOR

namespace StarRovers::FacilityInspectorTests
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

	class FWaitForFacilityInspectorCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FWaitForFacilityInspectorCommand(FAutomationTestBase& InTest)
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
				Test.AddError(TEXT("A PIE world was not available for Facility Inspector validation."));
				return true;
			}

			USRFacilityControlWidget* Inspector = CreateWidget<USRFacilityControlWidget>(
				PIEWorld,
				USRFacilityControlWidget::StaticClass());
			Test.TestNotNull(TEXT("The Facility Inspector is constructible in PIE"), Inspector);
			if (!IsValid(Inspector))
			{
				return true;
			}

			const TSharedRef<SWidget> InspectorSlateWidget = Inspector->TakeWidget();
			Test.TestTrue(TEXT("The Facility Inspector builds a cached Slate hierarchy"),
				Inspector->GetCachedWidget().IsValid());
			const TArray<FName> RequiredFlowWidgets = {
				TEXT("FacilityControlPanelScaleBox"),
				TEXT("FacilityControlPanelDesignSizeBox"),
				TEXT("FacilityControlPanelBorder"),
				TEXT("FacilityControlStatusBadge"),
				TEXT("FacilityControlInputStageBadge"),
				TEXT("FacilityControlProcessStageBadge"),
				TEXT("FacilityControlOutputStageBadge"),
				TEXT("FacilityControlInputToProcessArrowTextBlock"),
				TEXT("FacilityControlProcessToOutputArrowTextBlock"),
				TEXT("FacilityControlEnergyTransitionTextBlock"),
				TEXT("FacilityControlStateTransitionTextBlock"),
				TEXT("FacilityControlProcessProgressBar"),
				TEXT("FacilityControlHubRouteBorder"),
				TEXT("FacilityControlHubNetworkStatusBadge"),
				TEXT("FacilityControlHubFleetInfoCard"),
				TEXT("FacilityControlHubQueueInfoCard"),
				TEXT("FacilityControlHubMissileInfoCard"),
				TEXT("FacilityControlHubUtilityButtonBox"),
				TEXT("FacilityControlHubDestinationScrollBox"),
			};
			for (const FName WidgetName : RequiredFlowWidgets)
			{
				Test.TestNotNull(
					*FString::Printf(TEXT("Facility Inspector contains %s"), *WidgetName.ToString()),
					Inspector->GetWidgetFromName(WidgetName));
			}
			return true;
		}

	private:
		FAutomationTestBase& Test;
		double StartTimeSeconds = 0.0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRFacilityInspectorPIETest,
	"StarRovers.UI.FacilityInspector.PIE.NativeFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRFacilityInspectorPIETest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Levels/SolarSystem")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(
		StarRovers::FacilityInspectorTests::FWaitForFacilityInspectorCommand(*this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif

#endif
