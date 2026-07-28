#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Automation/SRResourceDataAsset.h"
#include "Camera/SRPlayerController.h"
#include "Camera/SRPlayerControllerPointerUIRouter.h"
#include "Camera/SRPlayerControllerWidgetLayers.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Celestial/SRStar.h"
#include "Components/TextRenderComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "Simulation/SRRunTelemetrySubsystem.h"
#include "Simulation/SRRunMilestoneSubsystem.h"
#include "Simulation/SRTimeControlSubsystem.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetSurfaceGrid.h"
#include "UI/SRAugmentChoiceWidget.h"
#include "UI/SRCelestialBodyFocusInfoWidget.h"
#include "UI/SRCelestialBodyOverviewWidget.h"
#include "UI/SRFacilityControlWidget.h"
#include "UI/SRFocusedHubShortcutWidget.h"
#include "UI/SRGameOverWidget.h"
#include "UI/SRPlayerGuidanceWidget.h"
#include "UI/SRStructureSelectionWidget.h"
#include "UI/SRTimeControlWidget.h"
#include "UI/SRUIComponents.h"

#if WITH_EDITOR
#include "Engine/Engine.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRPlayerUILayerAndModalContractTest,
	"StarRovers.UI.Integration.LayerAndModalContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRPlayerUILayerAndModalContractTest::RunTest(const FString& Parameters)
{
	const TArray<ESRPlayerUILayer> DefaultOrder =
		StarRovers::PlayerControllerUI::MakeDefaultWidgetLayerOrder();
	TestEqual(TEXT("The default HUD contains all eight semantic layers"), DefaultOrder.Num(), 8);

	TSet<ESRPlayerUILayer> UniqueLayers;
	for (const ESRPlayerUILayer Layer : DefaultOrder)
	{
		UniqueLayers.Add(Layer);
	}
	TestEqual(TEXT("Every default HUD layer occurs exactly once"), UniqueLayers.Num(), DefaultOrder.Num());

	const TArray<ESRPlayerUILayer> LegacyConfiguredOrder = {
		ESRPlayerUILayer::AugmentChoice,
		ESRPlayerUILayer::GameOver,
		ESRPlayerUILayer::FacilityControl,
		ESRPlayerUILayer::FocusInfo,
		ESRPlayerUILayer::StructureSelection,
		ESRPlayerUILayer::AugmentChoice,
	};
	const int32 FacilityZ = StarRovers::PlayerControllerUI::ResolveWidgetLayerZOrder(
		LegacyConfiguredOrder,
		ESRPlayerUILayer::FacilityControl);
	const int32 AugmentZ = StarRovers::PlayerControllerUI::ResolveWidgetLayerZOrder(
		LegacyConfiguredOrder,
		ESRPlayerUILayer::AugmentChoice);
	const int32 GameOverZ = StarRovers::PlayerControllerUI::ResolveWidgetLayerZOrder(
		LegacyConfiguredOrder,
		ESRPlayerUILayer::GameOver);
	TestTrue(TEXT("Augment Choice remains above gameplay panels even with a legacy Blueprint order"),
		AugmentZ > FacilityZ);
	TestTrue(TEXT("Game Over remains above every other modal"), GameOverZ > AugmentZ);

	return true;
}

#if WITH_EDITOR

namespace StarRovers::PlayerUIIntegrationTests
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

	class FWaitForProjectHUDContractCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FWaitForProjectHUDContractCommand(FAutomationTestBase& InTest)
			: Test(InTest)
			, StartTimeSeconds(FPlatformTime::Seconds())
		{
		}

		virtual bool Update() override
		{
			UWorld* PIEWorld = FindPIEWorld();
			ASRPlayerController* PlayerController = IsValid(PIEWorld)
				? Cast<ASRPlayerController>(PIEWorld->GetFirstPlayerController())
				: nullptr;
			if (!IsValid(PlayerController))
			{
				if (FPlatformTime::Seconds() - StartTimeSeconds < 30.0)
				{
					return false;
				}
				Test.AddError(TEXT("The project PlayerController was not ready for integrated HUD validation."));
				return true;
			}

			USRCelestialBodyFocusInfoWidget* FocusInfo = PlayerController->GetFocusInfoWidget();
			USRCelestialBodyOverviewWidget* Overview = PlayerController->GetOverviewWidget();
			USRTimeControlWidget* TimeControl = PlayerController->GetTimeControlWidget();
			USRPlayerGuidanceWidget* Guidance = PlayerController->GetPlayerGuidanceWidget();
			USRStructureSelectionWidget* BuildDock = PlayerController->GetStructureSelectionWidget();
			USRAugmentChoiceWidget* AugmentChoice = PlayerController->GetAugmentChoiceWidget();
			USRFacilityControlWidget* FacilityInspector = PlayerController->GetFacilityControlWidget();
			USRFocusedHubShortcutWidget* HubShortcut = PlayerController->GetFocusedHubShortcutWidget();
			USRGameOverWidget* GameOver = PlayerController->GetGameOverWidget();
			USRCelestialBodyRegistrySubsystem* RegistrySubsystem =
				PIEWorld->GetSubsystem<USRCelestialBodyRegistrySubsystem>();
			ASRStar* PrimaryStar = IsValid(RegistrySubsystem)
				? Cast<ASRStar>(RegistrySubsystem->GetPrimaryStarActor())
				: nullptr;
			const FSRPlayerGuidanceSnapshot InitialGuidanceSnapshot = IsValid(Guidance)
				? Guidance->BuildCurrentSnapshot()
				: FSRPlayerGuidanceSnapshot();

			const bool bAllWidgetsReady = IsValid(FocusInfo)
				&& IsValid(Overview)
				&& IsValid(TimeControl)
				&& IsValid(Guidance)
				&& IsValid(BuildDock)
				&& IsValid(AugmentChoice)
				&& IsValid(FacilityInspector)
				&& IsValid(HubShortcut)
				&& IsValid(GameOver);
			const bool bFirstFuelTargetReady =
				InitialGuidanceSnapshot.FirstFuelMilestone.bIsTracking
				&& InitialGuidanceSnapshot.FirstFuelMilestone.InitialSystemScan.bScanComplete
				&& InitialGuidanceSnapshot.FirstFuelMilestone.InitialSystemScan.HasRecommendation()
				&& IsValid(InitialGuidanceSnapshot.FirstFuelMilestone.RecommendedBodyActor.Get())
				&& USRCelestialBodyRuntimeLibrary::GetCelestialCanConstruct(
					InitialGuidanceSnapshot.FirstFuelMilestone.RecommendedBodyActor.Get())
				&& (!IsValid(Overview)
					|| Overview->IsBodyInitialSystemScanRecommendation(
						InitialGuidanceSnapshot.FirstFuelMilestone.RecommendedBodyActor.Get()));
			const bool bRuntimeStateReady =
				bAllWidgetsReady && IsValid(PrimaryStar) && bFirstFuelTargetReady;
			if (!bRuntimeStateReady && FPlatformTime::Seconds() - StartTimeSeconds < 30.0)
			{
				return false;
			}

			auto ValidateRuntimeWidget = [this](const TCHAR* WidgetLabel, UUserWidget* Widget)
			{
				Test.TestNotNull(*FString::Printf(TEXT("Project HUD creates %s"), WidgetLabel), Widget);
				if (IsValid(Widget))
				{
					Test.TestTrue(
						*FString::Printf(TEXT("%s owns a live Slate hierarchy"), WidgetLabel),
						Widget->GetCachedWidget().IsValid());
				}
			};

			ValidateRuntimeWidget(TEXT("Focus Info"), FocusInfo);
			ValidateRuntimeWidget(TEXT("Celestial Overview"), Overview);
			ValidateRuntimeWidget(TEXT("Time Control"), TimeControl);
			ValidateRuntimeWidget(TEXT("Player Guidance"), Guidance);
			ValidateRuntimeWidget(TEXT("Build Dock"), BuildDock);
			ValidateRuntimeWidget(TEXT("Augment Choice"), AugmentChoice);
			ValidateRuntimeWidget(TEXT("Facility Inspector"), FacilityInspector);
			ValidateRuntimeWidget(TEXT("Hub Shortcut"), HubShortcut);
			ValidateRuntimeWidget(TEXT("Game Over"), GameOver);
			if (!bAllWidgetsReady)
			{
				return true;
			}

			Test.TestNotNull(TEXT("Focus Info includes the integrated Operations panel"),
				FocusInfo->GetWidgetFromName(TEXT("BodyOperationsContainer")));
			Test.TestNotNull(TEXT("Celestial Overview includes the scrollable system list"),
				Overview->GetWidgetFromName(TEXT("StarSystemScrollBox")));
			Test.TestNotNull(TEXT("Celestial Overview exposes the strategic network status"),
				Overview->GetWidgetFromName(TEXT("StrategicStatusBadge")));
			Test.TestNotNull(TEXT("Celestial Overview exposes the strategic issue detail"),
				Overview->GetWidgetFromName(TEXT("StrategicDetailTextBlock")));
			Test.TestNotNull(TEXT("Celestial Overview exposes one direct bottleneck focus action"),
				Overview->GetWidgetFromName(TEXT("StrategicFocusButton")));
			Test.TestNotNull(TEXT("Celestial Overview can toggle world Route topology"),
				Overview->GetWidgetFromName(TEXT("StrategyOverlayToggleButton")));
			Test.TestFalse(TEXT("Celestial Overview always provides a network summary"),
				Overview->GetStrategicSummaryLabel().IsEmpty());
			Test.TestTrue(TEXT("World Route topology is visible by default"),
				Overview->IsStrategyOverlayVisible());
			Test.TestNotNull(TEXT("Guidance uses the responsive viewport wrapper"),
				Guidance->GetWidgetFromName(TEXT("PlayerGuidanceBannerScaleBox")));
			Test.TestNotNull(TEXT("Guidance exposes one direct milestone Action button"),
				Guidance->GetWidgetFromName(TEXT("PlayerGuidanceActionButton")));
			Test.TestNotNull(TEXT("Guidance includes the shared Resource Glyph host"),
				Guidance->GetWidgetFromName(TEXT("PlayerGuidanceResourceGlyph")));
			int32 ViewportWidth = 0;
			int32 ViewportHeight = 0;
			PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
			const FGeometry ViewportGeometry =
				UWidgetLayoutLibrary::GetViewportWidgetGeometry(PlayerController);
			FVector2D LogicalViewportSize = ViewportGeometry.GetLocalSize();
			const bool bHasRenderedViewportGeometry =
				LogicalViewportSize.X > UE_SMALL_NUMBER
				&& LogicalViewportSize.Y > UE_SMALL_NUMBER;
			const bool bUsesNullRHI =
				FParse::Param(FCommandLine::Get(), TEXT("nullrhi"));
			if (!bUsesNullRHI)
			{
				Test.TestTrue(TEXT("Rendered PIE exposes viewport geometry for HUD readability checks"),
					bHasRenderedViewportGeometry);
			}
			if (LogicalViewportSize.X <= UE_SMALL_NUMBER
				|| LogicalViewportSize.Y <= UE_SMALL_NUMBER)
			{
				LogicalViewportSize = FVector2D(
					static_cast<float>(ViewportWidth),
					static_cast<float>(ViewportHeight));
			}
			if (LogicalViewportSize.X <= UE_SMALL_NUMBER
				|| LogicalViewportSize.Y <= UE_SMALL_NUMBER)
			{
				LogicalViewportSize = FVector2D(
					FSRUILayoutPolicy::DefaultValidationViewportWidth,
					FSRUILayoutPolicy::DefaultValidationViewportHeight);
			}
			const FSRUITopCenterLaneLayout GuidanceLane =
				Guidance->GetResolvedCommandLaneLayout();
			Test.AddInfo(FString::Printf(
				TEXT("HUD readability viewport %.0fx%.0f | rendered geometry: %s | command lane %.0fx%.0f at %.0f,%.0f"),
				LogicalViewportSize.X,
				LogicalViewportSize.Y,
				bHasRenderedViewportGeometry ? TEXT("yes") : TEXT("no (validation fallback)"),
				GuidanceLane.DesignSize.X * GuidanceLane.Scale,
				GuidanceLane.DesignSize.Y * GuidanceLane.Scale,
				GuidanceLane.Insets.Left,
				GuidanceLane.Insets.Top));
			Test.TestTrue(TEXT("Guidance resolves a non-zero command lane in PIE"),
				GuidanceLane.DesignSize.X > 0.0f && GuidanceLane.DesignSize.Y > 0.0f);
			Test.TestTrue(TEXT("Guidance starts below the top survival and cycle rails"),
				GuidanceLane.Insets.Top
					> LogicalViewportSize.Y
						* FSRUILayoutPolicy::DefaultTopHUDHeightRatio);
			if (LogicalViewportSize.X >= 1280.0f && LogicalViewportSize.Y >= 720.0f)
			{
				Test.TestTrue(TEXT("Supported PIE resolutions preserve both side HUD panels"),
					GuidanceLane.bPreservesSidePanels);
				Test.TestTrue(TEXT("Guidance remains inside its center lane"),
					GuidanceLane.Insets.Left
						+ GuidanceLane.DesignSize.X * GuidanceLane.Scale
						<= LogicalViewportSize.X - GuidanceLane.Insets.Right
							+ UE_KINDA_SMALL_NUMBER);
			}
			if (UWidget* GuidanceDetail = Guidance->GetWidgetFromName(
				TEXT("PlayerGuidanceDetailTextBlock")))
			{
				Test.TestEqual(TEXT("Guidance detail visibility follows the resolved density mode"),
					GuidanceDetail->GetVisibility(),
					Guidance->IsCompactCommandLane()
						? ESlateVisibility::Collapsed
						: ESlateVisibility::HitTestInvisible);
			}
			if (bHasRenderedViewportGeometry)
			{
				if (UWidget* GuidanceBanner = Guidance->GetWidgetFromName(
					TEXT("PlayerGuidanceBannerDesignSizeBox")))
				{
					const FGeometry BannerGeometry = GuidanceBanner->GetCachedGeometry();
					const FVector2D BannerTopLeft = ViewportGeometry.AbsoluteToLocal(
						BannerGeometry.GetAbsolutePosition());
					const FVector2D BannerBottomRight = ViewportGeometry.AbsoluteToLocal(
						BannerGeometry.LocalToAbsolute(BannerGeometry.GetLocalSize()));
					Test.TestTrue(TEXT("The rendered Guidance banner starts below the top HUD"),
						BannerTopLeft.Y + UE_KINDA_SMALL_NUMBER >= GuidanceLane.Insets.Top);
					if (GuidanceLane.bPreservesSidePanels)
					{
						Test.TestTrue(TEXT("The rendered Guidance banner clears the left Overview"),
							BannerTopLeft.X + UE_KINDA_SMALL_NUMBER >= GuidanceLane.Insets.Left);
						Test.TestTrue(TEXT("The rendered Guidance banner clears right Operations"),
							BannerBottomRight.X <= LogicalViewportSize.X
								- GuidanceLane.Insets.Right + UE_KINDA_SMALL_NUMBER);
					}
				}
			}
			Test.TestNotNull(TEXT("Augment Choice uses the responsive modal wrapper"),
				AugmentChoice->GetWidgetFromName(TEXT("AugmentChoicePanelScaleBox")));
			Test.TestNotNull(TEXT("Facility Inspector uses the responsive panel wrapper"),
				FacilityInspector->GetWidgetFromName(TEXT("FacilityControlPanelScaleBox")));
			Test.TestNotNull(TEXT("Game Over uses the responsive content wrapper"),
				GameOver->GetWidgetFromName(TEXT("GameOverContentScaleBox")));
			Test.TestNotNull(TEXT("Build Dock exposes all seven Family navigation slots"),
				BuildDock->GetWidgetFromName(TEXT("StructureCategoryButton7")));
			Test.TestNotNull(TEXT("Build Dock cards use the shared Family Glyph"),
				BuildDock->GetWidgetFromName(TEXT("StructureFacilityButtonFamilyGlyph1")));
			Test.TestNotNull(TEXT("Build Dock detail uses the same shared Family Glyph"),
				BuildDock->GetWidgetFromName(TEXT("StructureDetailFamilyGlyph")));
			Test.TestEqual(TEXT("Project Build Dock retains seven semantic Family tabs"),
				BuildDock->GetBuildDockFamilyTabs().Num(),
				7);
			Test.TestTrue(TEXT("Project Build Dock contains the authored construction catalog"),
				!BuildDock->GetBuildOptions().IsEmpty());
			Test.TestNotNull(TEXT("Time Control exposes the always-visible stellar survival rail"),
				TimeControl->GetWidgetFromName(TEXT("StellarSurvivalRailScaleBox")));
			const TArray<FName> SurvivalBadgeNames = {
				TEXT("StellarSurvivalTimeBadge"),
				TEXT("StellarObjectiveBadge"),
				TEXT("StellarIncomeBadge"),
				TEXT("StellarConsumptionBadge"),
				TEXT("StellarNetBadge"),
				TEXT("StellarInboundBadge"),
			};
			for (const FName BadgeName : SurvivalBadgeNames)
			{
				USRStatusBadgeWidget* Badge = Cast<USRStatusBadgeWidget>(
					TimeControl->GetWidgetFromName(BadgeName));
				Test.TestNotNull(
					*FString::Printf(TEXT("Survival rail creates %s"), *BadgeName.ToString()),
					Badge);
				if (IsValid(Badge))
				{
					Test.TestFalse(
						*FString::Printf(TEXT("%s has a glanceable metric label"), *BadgeName.ToString()),
						Badge->GetLabel().IsEmpty());
				}
			}

			USRTimeControlSubsystem* TimeControlSubsystem =
				PIEWorld->GetSubsystem<USRTimeControlSubsystem>();
			Test.TestNotNull(TEXT("PIE creates the shared Time Control subsystem"), TimeControlSubsystem);
			if (IsValid(TimeControlSubsystem))
			{
				Test.TestTrue(
					TEXT("A new run waits in planning mode until the player explicitly starts simulation"),
					TimeControlSubsystem->IsSimulationPaused());
			}
			USRRunTelemetrySubsystem* RunTelemetrySubsystem =
				PIEWorld->GetSubsystem<USRRunTelemetrySubsystem>();
			Test.TestNotNull(TEXT("PIE creates the bounded Run Telemetry subsystem"),
				RunTelemetrySubsystem);
			if (IsValid(RunTelemetrySubsystem))
			{
				Test.TestTrue(TEXT("PIE can capture an authoritative Run telemetry snapshot"),
					RunTelemetrySubsystem->CaptureSnapshotNow());
				FSRRunTelemetrySnapshot TelemetrySnapshot;
				Test.TestTrue(TEXT("PIE telemetry exposes its latest snapshot"),
					RunTelemetrySubsystem->GetLatestSnapshot(TelemetrySnapshot));
				Test.TestTrue(TEXT("PIE telemetry resolves the generated Star and celestial system"),
					TelemetrySnapshot.bHasPrimaryStar
						&& TelemetrySnapshot.CelestialBodyCount > 0
						&& TelemetrySnapshot.ConstructibleBodyCount > 0);
				Test.TestTrue(TEXT("PIE telemetry reads the same paused Simulation contract"),
					TelemetrySnapshot.bSimulationPaused
						&& FMath::IsNearlyZero(TelemetrySnapshot.SimulationSeconds));
				Test.TestTrue(TEXT("PIE starts with the shipped bounded stellar pressure reserve"),
					FMath::IsNearlyEqual(TelemetrySnapshot.StoredStellarFuel, 20000.0)
						&& FMath::IsNearlyEqual(TelemetrySnapshot.StellarConsumptionPerSecond, 50.0)
						&& TelemetrySnapshot.StellarDemandPhase == ESRStellarDemandPhaseV2::Grace
						&& FMath::IsNearlyZero(TelemetrySnapshot.StellarFuelPressureRatio));
				Test.TestTrue(TEXT("PIE telemetry builds a valid diagnostic summary"),
					RunTelemetrySubsystem->GetSummary().bIsValid);
				FSRRunBalanceResult FlatProjection;
				Test.TestTrue(TEXT("PIE telemetry can project the current observed supply"),
					RunTelemetrySubsystem->BuildCurrentFlatSupplyProjection(FlatProjection));
				Test.TestTrue(TEXT("The live projection emits a deterministic timeline"),
					!FlatProjection.Timeline.IsEmpty()
						&& FlatProjection.ScenarioId == FName(TEXT("CurrentFlatSupply")));
				Test.TestTrue(TEXT("Doing nothing now gives a readable ten-minute failure baseline"),
					FlatProjection.Outcome == ESRStellarRunOutcome::Defeat
						&& FlatProjection.CompletionSeconds >= 10.0 * 60.0
						&& FlatProjection.CompletionSeconds <= 11.0 * 60.0
						&& FlatProjection.PeakDemandPerSecond <= 100.0);
				Test.AddInfo(FString::Printf(
					TEXT("Current flat-supply projection | outcome %d | until %.0fs | completion %.0fs | delivered %.0f | peak demand %.1f/s"),
					static_cast<int32>(FlatProjection.Outcome),
					FlatProjection.SimulatedUntilSeconds,
					FlatProjection.CompletionSeconds,
					FlatProjection.TotalDeliveredFuel,
					FlatProjection.PeakDemandPerSecond));
			}

			Guidance->EvaluateCurrentContext();
			const FSRPlayerGuidanceMessage FirstFuelMessage =
				Guidance->GetDisplayedGuidanceMessage();
			Test.TestEqual(TEXT("A fresh PIE Run starts with the first extraction milestone"),
				FirstFuelMessage.MessageId,
				FName(TEXT("FirstFuel.PlaceExtractor")));
			Test.TestEqual(TEXT("The first milestone owns a direct Extractor placement action"),
				FirstFuelMessage.ActionKind,
				ESRPlayerGuidanceActionKind::BuildExtractor);
			Test.TestTrue(TEXT("The fresh Run presents its first action as a compact System Scan"),
				FirstFuelMessage.CategoryText.ToString().Contains(TEXT("SYSTEM SCAN")));
			const FSRPlayerGuidanceSnapshot FirstFuelActionSnapshot = Guidance->BuildCurrentSnapshot();
			const FSRSystemScanSnapshot& InitialSystemScan =
				FirstFuelActionSnapshot.FirstFuelMilestone.InitialSystemScan;
			const FSRSystemScanCandidate* RecommendedScanCandidate =
				InitialSystemScan.GetRecommendedCandidate();
			Test.TestTrue(TEXT("The completed scan found at least one viable concrete deposit"),
				InitialSystemScan.bScanComplete
					&& InitialSystemScan.ScannedConstructibleBodyCount > 0
					&& InitialSystemScan.ScannedCardDepositCount > 0
					&& InitialSystemScan.MineableCardDepositCount > 0
					&& InitialSystemScan.DepletedCardDepositCount == 0
					&& InitialSystemScan.ViableCandidateCount > 0
					&& RecommendedScanCandidate != nullptr
					&& RecommendedScanCandidate->DepositRemainingAmount > 0);
			Test.TestTrue(TEXT("The generated opening scan exposes the complete five-Card resource portfolio"),
				InitialSystemScan.HasCompleteFuelPortfolio());
			Test.TestTrue(TEXT("The compact recommendation names its body and resource"),
				RecommendedScanCandidate
					&& FirstFuelMessage.TitleText.ToString().Contains(
						RecommendedScanCandidate->BodyDisplayName.ToString())
					&& FirstFuelMessage.TitleText.ToString().Contains(
						RecommendedScanCandidate->ResourceDisplayName.ToString()));
			Test.TestTrue(TEXT("The live recommendation exposes the scanned resource through the shared glyph"),
				RecommendedScanCandidate
					&& FirstFuelMessage.bShowResourceGlyph
					&& FirstFuelMessage.ResourceGlyph.ResourceId == RecommendedScanCandidate->ResourceId
					&& FirstFuelMessage.ResourceGlyph.Family == RecommendedScanCandidate->Family
					&& FirstFuelMessage.ResourceGlyph.Spectrum == RecommendedScanCandidate->Spectrum
					&& FirstFuelMessage.ResourceGlyph.Grade == RecommendedScanCandidate->Grade);
			AActor* GuidedBody = FirstFuelActionSnapshot.FirstFuelMilestone.RecommendedBodyActor.Get();
			Test.TestNotNull(TEXT("The first milestone resolves a resource-bearing construction world"),
				GuidedBody);
			Test.TestTrue(TEXT("The guided first-fuel world accepts construction"),
				IsValid(GuidedBody)
					&& USRCelestialBodyRuntimeLibrary::GetCelestialCanConstruct(GuidedBody));
			Test.TestTrue(TEXT("The Overview and world Nameplate model mark the same recommended body"),
				Overview->IsBodyInitialSystemScanRecommendation(GuidedBody));
			const TArray<FSRStructureBuildOption> PreActionBuildOptions = BuildDock->GetBuildOptions();
			int32 ExtractionOptionCount = 0;
			int32 SelectableExtractionOptionCount = 0;
			for (const FSRStructureBuildOption& BuildOption : PreActionBuildOptions)
			{
				if (BuildOption.Role == ESRStructureBuildRole::Extraction)
				{
					++ExtractionOptionCount;
					SelectableExtractionOptionCount += BuildOption.IsSelectable() ? 1 : 0;
				}
			}
			Test.TestTrue(TEXT("The Foundation catalog contains an Extraction option"),
				ExtractionOptionCount > 0);
			Test.TestTrue(TEXT("At least one Foundation Extraction option is selectable"),
				SelectableExtractionOptionCount > 0);
			TArray<AActor*> ScanBodies;
			if (IsValid(RegistrySubsystem))
			{
				RegistrySubsystem->GetCelestialBodies(ScanBodies);
			}
			AActor* AlternateConstructibleBody = nullptr;
			for (AActor* BodyActor : ScanBodies)
			{
				if (IsValid(BodyActor)
					&& BodyActor != GuidedBody
					&& USRCelestialBodyRuntimeLibrary::GetCelestialCanConstruct(BodyActor))
				{
					AlternateConstructibleBody = BodyActor;
					break;
				}
			}
			if (IsValid(AlternateConstructibleBody))
			{
				PlayerController->RequestActorFocus(AlternateConstructibleBody, true);
				Test.TestTrue(TEXT("The test begins the command from a different constructible body"),
					PlayerController->GetSelectedActor() == AlternateConstructibleBody);
			}
			Test.TestTrue(TEXT("The first milestone Action focuses a resource world and selects a Miner"),
				Guidance->ExecuteDisplayedAction());
			Test.TestTrue(TEXT("The System Scan command overrides an unrelated current body"),
				PlayerController->GetSelectedActor() == GuidedBody);
			Test.TestTrue(TEXT("The direct construction Action enters Assembly mode"),
				PlayerController->IsAssemblyModeActive());
			const FSRCelestialBodyFocusInfo& GuidedFocusInfo =
				PlayerController->GetSelectedActorFocusInfo();
			Test.TestTrue(TEXT("The direct Action centers and selects the exact recommended deposit"),
				RecommendedScanCandidate
					&& GuidedFocusInfo.bHasSelectedSurfaceStructure
					&& GuidedFocusInfo.SelectedSurfaceStructureInfo.bNaturalStructure
					&& GuidedFocusInfo.SelectedSurfaceStructureInfo.OccupantId
						== RecommendedScanCandidate->DepositOccupantId);
			const FName GuidedStructureId = BuildDock->GetSelectedStructureId();
			const TArray<FSRStructureBuildOption> GuidedBuildOptions = BuildDock->GetBuildOptions();
			const FSRStructureBuildOption* GuidedBuildOption =
				GuidedBuildOptions.FindByPredicate(
					[GuidedStructureId](const FSRStructureBuildOption& BuildOption)
					{
						return BuildOption.StructureId == GuidedStructureId;
					});
			Test.TestNotNull(TEXT("The direct construction Action selects a catalog option"),
				GuidedBuildOption);
			if (GuidedBuildOption)
			{
				Test.TestEqual(TEXT("The selected option is an Extraction workflow"),
					GuidedBuildOption->Role,
					ESRStructureBuildRole::Extraction);
				Test.TestEqual(TEXT("Guidance and the Build Dock recommend the same exact Facility"),
					BuildDock->GetRecommendedBuildOptionId(),
					GuidedBuildOption->StructureId);
			}

			USRPlanetSurfaceGrid* GuidedSurfaceGrid =
				USRCelestialBodyRuntimeLibrary::FindPlanetSurfaceGrid(GuidedBody);
			USRStructureInstanceManagerComponent* GuidedStructureManager = IsValid(GuidedBody)
				? GuidedBody->FindComponentByClass<USRStructureInstanceManagerComponent>()
				: nullptr;
			FSRPlanetSurfaceGridCellNeighbors DepositNeighbors;
			const bool bHasDepositNeighbors = RecommendedScanCandidate
				&& IsValid(GuidedSurfaceGrid)
				&& GuidedSurfaceGrid->GetCellNeighbors(
					RecommendedScanCandidate->DepositCellId,
					DepositNeighbors);
			Test.TestTrue(TEXT("The recommended deposit exposes an adjacent Miner test cell"),
				bHasDepositNeighbors);
			if (IsValid(GuidedSurfaceGrid)
				&& IsValid(GuidedStructureManager)
				&& RecommendedScanCandidate)
			{
				FSRResourceDepositInstance BeforeHarvest;
				FSRResourceDepositInstance AfterHarvest;
				FSRResourceInstance HarvestedResource;
				const bool bHasFiniteTarget =
					GuidedStructureManager->GetResourceDepositInstance(
						RecommendedScanCandidate->DepositOccupantId,
						BeforeHarvest)
					&& !FSRResourceDepositAmountModel::IsInfinite(
						BeforeHarvest.TotalAmount)
					&& BeforeHarvest.RemainingAmount > 0;
				Test.TestTrue(TEXT("The recommended Card deposit has a finite harvest contract"),
					bHasFiniteTarget);
				if (bHasFiniteTarget)
				{
					Test.TestTrue(TEXT("A real surface-grid harvest succeeds"),
						GuidedStructureManager->TryHarvestResourceDeposit(
							GuidedSurfaceGrid,
							RecommendedScanCandidate->DepositOccupantId,
							HarvestedResource,
							AfterHarvest));
					Test.TestEqual(TEXT("A real harvest consumes exactly one finite Card"),
						AfterHarvest.RemainingAmount,
						BeforeHarvest.RemainingAmount - 1);
					Test.TestEqual(TEXT("The harvested Card matches the scanned resource"),
						HarvestedResource.ResourceId,
						RecommendedScanCandidate->ResourceId);

					FSRResourceDepositInstance RestoredDeposit;
					Test.TestTrue(TEXT("The deterministic test restores the original deposit amount"),
						GuidedStructureManager->TryConfigureResourceDepositAmount(
							BeforeHarvest.OccupantId,
							BeforeHarvest.TotalAmount,
							BeforeHarvest.RemainingAmount,
							RestoredDeposit));
				}
			}
			if (GuidedBuildOption
				&& IsValid(GuidedBuildOption->StructureDataAsset)
				&& IsValid(GuidedStructureManager)
				&& RecommendedScanCandidate
				&& bHasDepositNeighbors)
			{
				Test.TestTrue(TEXT("Selecting the Miner card in the Build Dock immediately highlights its recommended deposit"),
					GuidedStructureManager->IsMiningResourceDepositHighlighted(
						RecommendedScanCandidate->DepositOccupantId));

				TArray<FSRPlacedStructureInstance> PlacedStructures;
				GuidedStructureManager->GetPlacedStructures(PlacedStructures);
				int32 RegisteredDepositCount = 0;
				int32 FiniteDepositCount = 0;
				int32 HighlightedDepositCount = 0;
				int32 RegisteredFuelCardDepositCount = 0;
				int32 HighlightedFuelCardDepositCount = 0;
				for (const FSRPlacedStructureInstance& PlacedStructure : PlacedStructures)
				{
					FSRResourceDepositInstance Deposit;
					if (!GuidedStructureManager->GetResourceDepositInstance(
						PlacedStructure.OccupantId,
						Deposit))
					{
						continue;
					}

					++RegisteredDepositCount;
					FiniteDepositCount +=
						!FSRResourceDepositAmountModel::IsInfinite(Deposit.TotalAmount)
							&& Deposit.TotalAmount > 0
							&& Deposit.RemainingAmount == Deposit.TotalAmount
							? 1
							: 0;
					const bool bHighlighted =
						GuidedStructureManager->IsMiningResourceDepositHighlighted(
							PlacedStructure.OccupantId);
					HighlightedDepositCount += bHighlighted ? 1 : 0;
					if (IsValid(Deposit.ResourceDataAsset)
						&& Deposit.ResourceDataAsset->ResourceClass == ESRResourceClass::Card)
					{
						++RegisteredFuelCardDepositCount;
						HighlightedFuelCardDepositCount += bHighlighted ? 1 : 0;
					}
				}
				Test.TestTrue(TEXT("The selected Miner exposes at least one mineable deposit"),
					RegisteredDepositCount > 0);
				Test.TestEqual(TEXT("Every authored Resource V2 deposit starts with its finite DA amount"),
					FiniteDepositCount,
					RegisteredDepositCount);
				Test.TestEqual(TEXT("Selecting the Miner card highlights every mineable deposit on its body"),
					HighlightedDepositCount,
					RegisteredDepositCount);
				Test.TestTrue(TEXT("The scan body contains at least one stellar-fuel Card deposit"),
					RegisteredFuelCardDepositCount > 0);
				Test.TestEqual(TEXT("Stellar-fuel Card deposits are included in the Miner highlight set"),
					HighlightedFuelCardDepositCount,
					RegisteredFuelCardDepositCount);

				FSRFocusedSurfaceStructureInfo MinerFocusInfo;
				MinerFocusInfo.bIsValid = true;
				MinerFocusInfo.OccupantId = TEXT("Test.Miner.Highlight");
				MinerFocusInfo.StructureId = GuidedBuildOption->StructureId;
				MinerFocusInfo.DisplayName = GuidedBuildOption->DisplayName;
				MinerFocusInfo.StructureDataAsset = GuidedBuildOption->StructureDataAsset;
				MinerFocusInfo.FootprintCellIds.Add(DepositNeighbors.NegativeU);
				MinerFocusInfo.bHasFacilityDataAsset = true;
				PlayerController->SetSelectedActorSurfaceStructureInfo(GuidedBody, MinerFocusInfo);

				Test.TestTrue(TEXT("Selecting a Miner highlights its adjacent deposit"),
					GuidedStructureManager->IsMiningResourceDepositHighlighted(
						RecommendedScanCandidate->DepositOccupantId));
				Test.TestTrue(TEXT("The adjacent deposit receives the stronger TARGET state"),
					GuidedStructureManager->IsMiningResourceDepositTarget(
						RecommendedScanCandidate->DepositOccupantId));

				TArray<UTextRenderComponent*> MiningScanLabels;
				GuidedBody->GetComponents<UTextRenderComponent>(MiningScanLabels);
				int32 ResourceScanLabelCount = 0;
				bool bFoundTargetLabel = false;
				for (const UTextRenderComponent* Label : MiningScanLabels)
				{
					if (!IsValid(Label))
					{
						continue;
					}
					const FString LabelText = Label->Text.ToString();
					if (LabelText.StartsWith(TEXT("RESOURCE"))
						|| LabelText.StartsWith(TEXT("TARGET")))
					{
						++ResourceScanLabelCount;
					}
					bFoundTargetLabel |= LabelText.StartsWith(TEXT("TARGET"))
						&& LabelText.Contains(RecommendedScanCandidate->ResourceDisplayName.ToString());
				}
				Test.TestEqual(TEXT("Every highlighted deposit receives a world-space scan label"),
					ResourceScanLabelCount,
					RegisteredDepositCount);
				Test.TestTrue(TEXT("The adjacent deposit label is promoted to TARGET"),
					bFoundTargetLabel);

				PlayerController->SetSelectedSurfaceStructureInfo(
					false,
					FSRFocusedSurfaceStructureInfo());
				Test.TestTrue(TEXT("The Build Dock Miner keeps the scan active after its Inspector selection clears"),
					GuidedStructureManager->IsMiningResourceDepositHighlighted(
						RecommendedScanCandidate->DepositOccupantId));
				PlayerController->SetAssemblyModeActive(false);
				Test.TestFalse(TEXT("Leaving Assembly mode restores deposit visuals"),
					GuidedStructureManager->IsMiningResourceDepositHighlighted(
						RecommendedScanCandidate->DepositOccupantId));
				MiningScanLabels.Reset();
				GuidedBody->GetComponents<UTextRenderComponent>(MiningScanLabels);
				const bool bRetainedMiningScanLabel = MiningScanLabels.ContainsByPredicate(
					[](const UTextRenderComponent* Label)
					{
						if (!IsValid(Label))
						{
							return false;
						}
						const FString LabelText = Label->Text.ToString();
						return LabelText.StartsWith(TEXT("RESOURCE"))
							|| LabelText.StartsWith(TEXT("TARGET"));
					});
				Test.TestFalse(TEXT("Leaving Assembly mode removes temporary scan labels"),
					bRetainedMiningScanLabel);
				PlayerController->RequestSurfaceStructureFocus(
					GuidedBody,
					RecommendedScanCandidate->DepositOccupantId,
					false);
			}
			Test.TestNotNull(TEXT("Survival rail resolves the primary Star"), PrimaryStar);
			if (IsValid(PrimaryStar))
			{
				const FSRStellarFuelState BeforeDelivery = PrimaryStar->GetStellarFuelState();
				PrimaryStar->AddStellarFuel(300.0);
				const FSRStellarFuelState AfterDelivery = PrimaryStar->GetStellarFuelState();
				Test.TestTrue(TEXT("The Star records actual delivered fuel for the observed-income rail"),
					FMath::IsNearlyEqual(
						AfterDelivery.TotalDeliveredFuel - BeforeDelivery.TotalDeliveredFuel,
						300.0));
				Test.TestTrue(TEXT("A delivery contributes to the transparent thirty-second income window"),
					FMath::IsNearlyEqual(
						AfterDelivery.RecentFuelIncomePerSecond - BeforeDelivery.RecentFuelIncomePerSecond,
						10.0));
				Test.TestTrue(TEXT("A full survival reserve does not stockpile past its cap"),
					FMath::IsNearlyEqual(
						AfterDelivery.StoredFuel,
						BeforeDelivery.StoredFuel)
						&& FMath::IsNearlyEqual(AfterDelivery.LastFuelReserveOverflow, 300.0));
			}

			Guidance->SetAutomaticContextEvaluationEnabled(false);
			FSRAugmentChoice PreviewChoice;
			PreviewChoice.ChoiceKind = ESRAugmentChoiceKind::ResourceV2Package;
			PreviewChoice.PackageId = TEXT("StateResonator");
			PreviewChoice.DisplayName = FText::FromString(TEXT("State Resonator"));
			AugmentChoice->SetAugmentChoices({ PreviewChoice }, 1);
			AugmentChoice->SetVisibility(ESlateVisibility::Visible);
			Test.TestTrue(TEXT("A project Augment modal can focus its first choice"),
				AugmentChoice->FocusDefaultChoice());
			Test.TestTrue(TEXT("Guidance snapshot recognizes a visible blocking choice"),
				Guidance->BuildCurrentSnapshot().bBlockingChoiceVisible);
			Guidance->EvaluateCurrentContext();
			Test.TestFalse(TEXT("Non-modal Guidance is suppressed while Augment Choice is visible"),
				Guidance->GetDisplayedGuidanceMessage().IsVisible());
			AugmentChoice->ClearAugmentChoices();
			AugmentChoice->SetVisibility(ESlateVisibility::Collapsed);
			Guidance->SetAutomaticContextEvaluationEnabled(true);

			GameOver->SetVisibility(ESlateVisibility::Visible);
			Test.TestTrue(TEXT("A visible project Game Over overlay blocks world interaction"),
				PlayerController->IsPointerOverBlockingUI());
			Test.TestTrue(TEXT("Game Over also blocks the Enhanced Input world-click routing path"),
				FSRPlayerControllerPointerUIRouter::RouteLeftClick(
					StarRovers::PlayerControllerUI::MakeDefaultWidgetLayerOrder(),
					0.0f,
					0.0f,
					false,
					FacilityInspector,
					HubShortcut,
					FocusInfo,
					Overview,
					TimeControl,
					AugmentChoice,
					BuildDock,
					GameOver));
			GameOver->SetVisibility(ESlateVisibility::Collapsed);
			return true;
		}

	private:
		FAutomationTestBase& Test;
		double StartTimeSeconds = 0.0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRProjectHUDContractPIETest,
	"StarRovers.UI.Integration.PIE.ProjectHUDContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRProjectHUDContractPIETest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Levels/SolarSystem")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(
		StarRovers::PlayerUIIntegrationTests::FWaitForProjectHUDContractCommand(*this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif

#endif
