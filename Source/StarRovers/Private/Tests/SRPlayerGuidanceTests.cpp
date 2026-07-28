#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/SRPlayerController.h"
#include "Automation/SRResourceDataAsset.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "UI/SRPlayerGuidancePresentation.h"
#include "UI/SRPlayerGuidanceWidget.h"

#if WITH_EDITOR
#include "Engine/Engine.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRPlayerGuidancePriorityTest,
	"StarRovers.UI.Guidance.PriorityAndSuppression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRPlayerGuidancePriorityTest::RunTest(const FString& Parameters)
{
	FSRPlayerGuidanceSnapshot Snapshot;
	Snapshot.bHasFocusedActor = true;
	Snapshot.bCanConstructOnFocusedActor = true;
	Snapshot.bOperationsAvailable = true;
	Snapshot.BlockedRouteCount = 2;
	Snapshot.OperationalPressure = ESRCelestialBodyOperationsPressure::OverCapacity;
	Snapshot.OperationalLoad = 42;
	Snapshot.OperationalCapacity = 30;

	FSRPlayerGuidanceMessage Message = FSRPlayerGuidancePresentationBuilder::Evaluate(Snapshot);
	TestEqual(TEXT("A blocked inter-body route outranks local Capacity pressure"),
		Message.MessageId,
		FName(TEXT("BlockedRoutes")));
	TestEqual(TEXT("Blocked routes use the danger semantic state"),
		Message.VisualState,
		ESRUIVisualState::Danger);
	TestTrue(TEXT("The route alert contains a direct next action"),
		Message.ActionText.ToString().Contains(TEXT("Select a Hub")));

	Snapshot.bBlockingChoiceVisible = true;
	Message = FSRPlayerGuidancePresentationBuilder::Evaluate(Snapshot);
	TestFalse(TEXT("A modal Augment choice suppresses the non-modal banner"), Message.IsVisible());
	TestEqual(TEXT("Suppression is explicit so transient notices cannot leak over the modal"),
		Message.MessageId,
		FName(TEXT("BlockingChoice")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRPlayerGuidanceCapacityTest,
	"StarRovers.UI.Guidance.OperationalAlerts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRPlayerGuidanceCapacityTest::RunTest(const FString& Parameters)
{
	FSRPlayerGuidanceSnapshot Snapshot;
	Snapshot.bHasFocusedActor = true;
	Snapshot.bCanConstructOnFocusedActor = true;
	Snapshot.bOperationsAvailable = true;
	Snapshot.FacilityCount = 8;
	Snapshot.ProcessingFacilityCount = 6;
	Snapshot.OperationalPressure = ESRCelestialBodyOperationsPressure::OverCapacity;
	Snapshot.OperationalLoad = 38;
	Snapshot.OperationalCapacity = 30;

	FSRPlayerGuidanceMessage Message = FSRPlayerGuidancePresentationBuilder::Evaluate(Snapshot);
	TestEqual(TEXT("Over-capacity state has a stable identity"),
		Message.MessageId,
		FName(TEXT("OverOperationalCapacity")));
	TestTrue(TEXT("The player sees the exact Load/Capacity pair"),
		Message.DetailText.ToString().Contains(TEXT("38 / 30")));
	TestTrue(TEXT("The alert offers multiple valid remedies"),
		Message.ActionText.ToString().Contains(TEXT("Service Core")));

	Snapshot.OperationalPressure = ESRCelestialBodyOperationsPressure::Nominal;
	Snapshot.FleetQueuedDepartureCount = 3;
	Snapshot.FleetAvailableCapacity = 0;
	Message = FSRPlayerGuidancePresentationBuilder::Evaluate(Snapshot);
	TestEqual(TEXT("A saturated Fleet queue becomes the next situational alert"),
		Message.MessageId,
		FName(TEXT("FleetQueueFull")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRPlayerGuidanceOnboardingTest,
	"StarRovers.UI.Guidance.AdaptiveFirstLine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRPlayerGuidanceOnboardingTest::RunTest(const FString& Parameters)
{
	FSRPlayerGuidanceSnapshot Snapshot;
	FSRPlayerGuidanceMessage Message = FSRPlayerGuidancePresentationBuilder::Evaluate(Snapshot);
	TestEqual(TEXT("Without focus, the first action is choosing a world"),
		Message.MessageId,
		FName(TEXT("ChooseWorld")));

	Snapshot.bHasFocusedActor = true;
	Message = FSRPlayerGuidancePresentationBuilder::Evaluate(Snapshot);
	TestEqual(TEXT("A focused Star redirects construction to a planet or moon"),
		Message.MessageId,
		FName(TEXT("ChooseConstructibleWorld")));

	Snapshot.bCanConstructOnFocusedActor = true;
	Snapshot.bOperationsAvailable = true;
	Message = FSRPlayerGuidancePresentationBuilder::Evaluate(Snapshot);
	TestEqual(TEXT("An empty world teaches the Miner-to-Family-process opening"),
		Message.MessageId,
		FName(TEXT("BuildFirstLine")));
	TestTrue(TEXT("The first Line action starts from extraction"),
		Message.ActionText.ToString().Contains(TEXT("EXTRACTION")));

	Snapshot.FacilityCount = 2;
	Message = FSRPlayerGuidancePresentationBuilder::Evaluate(Snapshot);
	TestEqual(TEXT("An installed but idle Line directs the player to the Inspector"),
		Message.MessageId,
		FName(TEXT("InspectIdleLine")));
	TestTrue(TEXT("The Inspector hint preserves the stage vocabulary"),
		Message.ActionText.ToString().Contains(TEXT("INPUT")));

	Snapshot.ProcessingFacilityCount = 1;
	Message = FSRPlayerGuidancePresentationBuilder::Evaluate(Snapshot);
	TestFalse(TEXT("A healthy local Line does not receive permanent tutorial noise"), Message.IsVisible());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRPlayerGuidanceFirstFuelMilestoneTest,
	"StarRovers.UI.RunCommand.FirstFuelMilestone.Presentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRPlayerGuidanceFirstFuelMilestoneTest::RunTest(const FString& Parameters)
{
	FSRPlayerGuidanceSnapshot Snapshot;
	Snapshot.FirstFuelMilestone.bIsTracking = true;
	Snapshot.FirstFuelMilestone.TotalMilestoneCount = 9;
	Snapshot.FirstFuelMilestone.CurrentMilestone = ESRFirstFuelMilestone::PlaceExtractor;
	Snapshot.FirstFuelMilestone.InitialSystemScan.bScanComplete = true;
	Snapshot.FirstFuelMilestone.InitialSystemScan.ScannedConstructibleBodyCount = 6;
	Snapshot.FirstFuelMilestone.InitialSystemScan.ScannedCardDepositCount = 30;
	FSRSystemScanCandidate& RecommendedCandidate =
		Snapshot.FirstFuelMilestone.InitialSystemScan.RankedCandidates.AddDefaulted_GetRef();
	RecommendedCandidate.BodyActor = AActor::StaticClass()->GetDefaultObject<AActor>();
	RecommendedCandidate.BodyDisplayName = FText::FromString(TEXT("Aurelia"));
	RecommendedCandidate.DepositOccupantId = TEXT("Deposit.AuroraPlasma");
	RecommendedCandidate.ResourceDataAsset =
		NewObject<USRResourceDataAsset>(GetTransientPackage());
	RecommendedCandidate.ResourceDisplayName = FText::FromString(TEXT("Aurora Plasma"));
	RecommendedCandidate.ResourceId = TEXT("AuroraPlasma");
	RecommendedCandidate.Family = ESRResourceFamily::Plasma;
	RecommendedCandidate.Spectrum = ESRResourceSpectrum::Yellow;
	RecommendedCandidate.Grade = 4;
	RecommendedCandidate.SeedEnergy = 6.0;
	RecommendedCandidate.DepositTotalAmount = 120;
	RecommendedCandidate.DepositRemainingAmount = 120;
	RecommendedCandidate.OperationalHeadroom = 30;
	RecommendedCandidate.StarProximityNormalized = 0.9f;
	RecommendedCandidate.bHasFamilyProcessorAccess = true;
	RecommendedCandidate.bHasAdjacentBuildAccess = true;
	RecommendedCandidate.Score = FSRSystemScanModel::ScoreCandidate(
		1.0f, 0.9f, 1.0f, true, true);

	FSRPlayerGuidanceMessage Message = FSRPlayerGuidancePresentationBuilder::Evaluate(Snapshot);
	TestEqual(TEXT("The first action is presented as an automatic System Scan recommendation"),
		Message.MessageId,
		FName(TEXT("FirstFuel.PlaceExtractor")));
	TestEqual(TEXT("The scan recommendation remains a direct Extractor action"),
		Message.ActionKind,
		ESRPlayerGuidanceActionKind::BuildExtractor);
	TestTrue(TEXT("The recommendation names both the body and concrete Card resource"),
		Message.TitleText.ToString().Contains(TEXT("Aurelia"))
			&& Message.TitleText.ToString().Contains(TEXT("Aurora Plasma")));
	TestTrue(TEXT("The recommendation exposes the shared Resource Glyph contract"),
		Message.bShowResourceGlyph && Message.ResourceGlyph.bHasResource);
	TestTrue(TEXT("The glyph carries the candidate Family without relying on color"),
		Message.ResourceGlyph.FamilyToken.ToString().Contains(TEXT("PLS")));
	TestTrue(TEXT("The glyph combines Spectrum and Grade into one glance token"),
		Message.ResourceGlyph.SpectrumGradeToken.ToString().Contains(TEXT("Y4")));
	TestEqual(TEXT("The glyph carries the seed Energy independently from score text"),
		Message.ResourceGlyph.EnergyToken.ToString(),
		FString(TEXT("E 6")));
	TestTrue(TEXT("The remaining glance line is reserved for recommendation context"),
		Message.DetailText.ToString().Contains(TEXT("Capacity"))
			&& Message.DetailText.ToString().Contains(TEXT("/100")));
	TestTrue(TEXT("The detailed score explicitly excludes Grade and Spectrum from scalar quality"),
		Message.ToolTipText.ToString().Contains(TEXT("Grade"))
			&& Message.ToolTipText.ToString().Contains(TEXT("추천 점수")));

	FSRPlayerGuidanceSnapshot RecoverySnapshot;
	RecoverySnapshot.FirstFuelMilestone.bIsTracking = true;
	RecoverySnapshot.FirstFuelMilestone.TotalMilestoneCount = 9;
	RecoverySnapshot.FirstFuelMilestone.CurrentMilestone =
		ESRFirstFuelMilestone::PlaceExtractor;
	RecoverySnapshot.FirstFuelMilestone.InitialSystemScan.bScanComplete = true;
	RecoverySnapshot.FirstFuelMilestone.InitialSystemScan.ScannedCardDepositCount = 5;
	RecoverySnapshot.FirstFuelMilestone.InitialSystemScan.DepletedCardDepositCount = 5;
	RecoverySnapshot.FirstFuelMilestone.InitialProgressRecovery.bEnabled = true;
	RecoverySnapshot.FirstFuelMilestone.InitialProgressRecovery.bAvailable = true;
	const FSRPlayerGuidanceMessage RecoveryMessage =
		FSRPlayerGuidancePresentationBuilder::Evaluate(RecoverySnapshot);
	TestEqual(TEXT("A failed opening scan offers the bounded recovery action"),
		RecoveryMessage.MessageId,
		FName(TEXT("FirstFuel.EmergencyProspecting")));
	TestEqual(TEXT("Emergency prospecting is executable from the command lane"),
		RecoveryMessage.ActionKind,
		ESRPlayerGuidanceActionKind::ActivateEmergencyProspecting);

	Snapshot.FirstFuelMilestone.CurrentMilestone = ESRFirstFuelMilestone::PlaceFamilyProcessor;
	Snapshot.FirstFuelMilestone.CompletedMilestoneCount = 2;
	Snapshot.FirstFuelMilestone.FirstResourceFamily = ESRResourceFamily::Metal;

	Message = FSRPlayerGuidancePresentationBuilder::Evaluate(Snapshot);
	TestEqual(TEXT("The onboarding rail exposes the first unmet fuel milestone"),
		Message.MessageId,
		FName(TEXT("FirstFuel.PlaceFamilyProcessor")));
	TestEqual(TEXT("A construction milestone owns an executable Build Dock action"),
		Message.ActionKind,
		ESRPlayerGuidanceActionKind::BuildFamilyProcessor);
	TestTrue(TEXT("The category carries glanceable progress instead of tutorial prose"),
		Message.CategoryText.ToString().Contains(TEXT("3/9")));
	TestTrue(TEXT("The detected resource Family is named in the next action"),
		Message.TitleText.ToString().Contains(TEXT("Metal")));

	Snapshot.FirstFuelMilestone.CurrentMilestone = ESRFirstFuelMilestone::ExtractFirstCard;
	Snapshot.FirstFuelMilestone.CompletedMilestoneCount = 1;
	Snapshot.FirstFuelMilestone.TargetFacilityBodyActor =
		AActor::StaticClass()->GetDefaultObject<AActor>();
	Snapshot.FirstFuelMilestone.TargetFacilityOccupantId = TEXT("Test.Extractor");
	Snapshot.bSimulationPaused = true;
	Message = FSRPlayerGuidancePresentationBuilder::Evaluate(Snapshot);
	TestEqual(TEXT("A paused production milestone changes the direct action to Play"),
		Message.ActionKind,
		ESRPlayerGuidanceActionKind::ResumeSimulation);
	Snapshot.bSimulationPaused = false;
	Snapshot.FirstFuelMilestone.TargetFacilityBodyActor = nullptr;
	Snapshot.FirstFuelMilestone.TargetFacilityOccupantId = NAME_None;
	Message = FSRPlayerGuidancePresentationBuilder::Evaluate(Snapshot);
	TestEqual(TEXT("A removed Extractor returns a direct rebuild action instead of a dead Inspect action"),
		Message.MessageId,
		FName(TEXT("FirstFuel.RebuildExtractor")));
	TestEqual(TEXT("The broken extraction route reopens the Build Dock"),
		Message.ActionKind,
		ESRPlayerGuidanceActionKind::BuildExtractor);

	Snapshot.FirstFuelMilestone.CurrentMilestone = ESRFirstFuelMilestone::Complete;
	Snapshot.FirstFuelMilestone.CompletedMilestoneCount = 9;
	Snapshot.bHasFocusedActor = true;
	Snapshot.bCanConstructOnFocusedActor = true;
	Snapshot.bOperationsAvailable = true;
	Snapshot.FacilityCount = 3;
	Snapshot.ProcessingFacilityCount = 1;
	Message = FSRPlayerGuidancePresentationBuilder::Evaluate(Snapshot);
	TestFalse(TEXT("The first-fuel tutorial does not become permanent HUD noise after delivery"),
		Message.IsVisible());
	return true;
}

#if WITH_EDITOR

namespace StarRovers::PlayerGuidanceTests
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

	class FWaitForPlayerGuidanceCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FWaitForPlayerGuidanceCommand(FAutomationTestBase& InTest)
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
				Test.AddError(TEXT("A PIE world was not available for Player Guidance validation."));
				return true;
			}

			USRPlayerGuidanceWidget* Widget = CreateWidget<USRPlayerGuidanceWidget>(
				PIEWorld,
				USRPlayerGuidanceWidget::StaticClass());
			Test.TestNotNull(TEXT("The native Player Guidance widget is constructible in PIE"), Widget);
			if (IsValid(Widget))
			{
				Widget->SetAutomaticContextEvaluationEnabled(false);
				FSRPlayerGuidanceMessage Preview;
				Preview.MessageId = TEXT("PIEPreview");
				Preview.CategoryText = FText::FromString(TEXT("CAPACITY ALERT"));
				Preview.TitleText = FText::FromString(TEXT("Operational Load exceeds Capacity"));
				Preview.DetailText = FText::FromString(TEXT("Load 38 / 30"));
				Preview.ActionText = FText::FromString(TEXT("NEXT · Supply a Service Core"));
				Preview.VisualState = ESRUIVisualState::Danger;
				Widget->SetGuidanceMessage(Preview);
				const TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();
				Test.TestTrue(TEXT("The Guidance widget builds a cached Slate hierarchy"),
					Widget->GetCachedWidget().IsValid());
				const TArray<FName> RequiredWidgets = {
					TEXT("PlayerGuidanceBannerScaleBox"),
					TEXT("PlayerGuidanceBannerDesignSizeBox"),
					TEXT("PlayerGuidanceBannerCard"),
					TEXT("PlayerGuidanceCategoryBadge"),
					TEXT("PlayerGuidanceTitleTextBlock"),
					TEXT("PlayerGuidanceDetailTextBlock"),
					TEXT("PlayerGuidanceActionTextBlock"),
					TEXT("PlayerGuidanceActionButton"),
					TEXT("PlayerGuidanceActionButtonTextBlock"),
				};
				for (const FName WidgetName : RequiredWidgets)
				{
					Test.TestNotNull(
						*FString::Printf(TEXT("Player Guidance contains %s"), *WidgetName.ToString()),
						Widget->GetWidgetFromName(WidgetName));
				}
				Test.TestEqual(TEXT("The explicit preview survives native construction"),
					Widget->GetDisplayedGuidanceMessage().MessageId,
					FName(TEXT("PIEPreview")));
			}

			ASRPlayerController* ProjectController = Cast<ASRPlayerController>(PIEWorld->GetFirstPlayerController());
			Test.TestNotNull(TEXT("SolarSystem PIE uses the project Player Controller"), ProjectController);
			if (IsValid(ProjectController))
			{
				Test.TestNotNull(TEXT("The project lifecycle creates the global Guidance layer"),
					ProjectController->GetPlayerGuidanceWidget());
			}
			return true;
		}

	private:
		FAutomationTestBase& Test;
		double StartTimeSeconds = 0.0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRPlayerGuidancePIETest,
	"StarRovers.UI.Guidance.PIE.NativeAndControllerFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRPlayerGuidancePIETest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Levels/SolarSystem")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(
		StarRovers::PlayerGuidanceTests::FWaitForPlayerGuidanceCommand(*this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif

#endif
