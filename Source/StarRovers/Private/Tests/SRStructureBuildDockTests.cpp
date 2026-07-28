#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Assembly/SRStructureBuildDockModel.h"
#include "Assembly/SRAssemblyStructurePlacementPreview.h"
#include "Automation/SRFacilityNetworkComponent.h"
#include "Camera/SRPlayerController.h"
#include "Components/Button.h"
#include "Misc/AutomationTest.h"
#include "Structure/SRStructureDataAsset.h"
#include "Surface/SRPlanetSurfaceGrid.h"
#include "UI/SRStructureSelectionWidget.h"

#if WITH_EDITOR
#include "Engine/Engine.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/UObjectIterator.h"
#endif

namespace StarRovers::StructureBuildDockTests
{
	FSRStructureBuildOption MakeOption(
		const TCHAR* StructureId,
		ESRResourceFamily ResourceFamily,
		ESRStructureBuildRole Role,
		bool bSelectable = true,
		ESRFacilityRarity Rarity = ESRFacilityRarity::Basic)
	{
		USRStructureDataAsset* StructureDataAsset = NewObject<USRStructureDataAsset>(GetTransientPackage());
		StructureDataAsset->StructureId = FName(StructureId);
		StructureDataAsset->DisplayName = FText::FromString(StructureId);
		StructureDataAsset->bAvailableForConstruction = true;
		StructureDataAsset->bIsResourceDeposit = false;

		FSRStructureBuildOption Option;
		Option.StructureId = StructureDataAsset->StructureId;
		Option.DisplayName = StructureDataAsset->DisplayName;
		Option.StructureDataAsset = StructureDataAsset;
		Option.ResourceFamily = ResourceFamily;
		Option.Role = Role;
		Option.Rarity = Rarity;
		Option.bEnabled = bSelectable;
		Option.bUnlocked = bSelectable;
		Option.Availability = bSelectable
			? ESRStructureBuildAvailability::Available
			: ESRStructureBuildAvailability::LockedByAugment;
		Option.BlockReason = bSelectable
			? ESRStructureBuildBlockReason::None
			: ESRStructureBuildBlockReason::RequiresAugment;
		if (!bSelectable)
		{
			Option.BlockReasonText = FText::FromString(TEXT("Requires an Augment unlock."));
		}
		return Option;
	}

	TArray<FSRStructureBuildOption> MakeReferenceCatalog()
	{
		return {
			MakeOption(TEXT("CrystalMill"), ESRResourceFamily::Crystal, ESRStructureBuildRole::FamilyProcessing),
			MakeOption(TEXT("MetalLockedPress"), ESRResourceFamily::Metal, ESRStructureBuildRole::FamilyProcessing, false),
			MakeOption(TEXT("FuelImprinter"), ESRResourceFamily::None, ESRStructureBuildRole::FuelImprinting),
			MakeOption(TEXT("ServiceCore"), ESRResourceFamily::None, ESRStructureBuildRole::Infrastructure),
			MakeOption(TEXT("MetalForge"), ESRResourceFamily::Metal, ESRStructureBuildRole::FamilyProcessing),
			MakeOption(TEXT("Conveyor"), ESRResourceFamily::None, ESRStructureBuildRole::Logistics, true, ESRFacilityRarity::Starting),
			MakeOption(TEXT("StellarFuelFabricator"), ESRResourceFamily::None, ESRStructureBuildRole::StellarFuelFabrication),
			MakeOption(TEXT("TagImprinter"), ESRResourceFamily::None, ESRStructureBuildRole::TagProcessing),
			MakeOption(TEXT("UniversalProcessor"), ESRResourceFamily::None, ESRStructureBuildRole::FamilyProcessing),
			MakeOption(TEXT("Miner"), ESRResourceFamily::None, ESRStructureBuildRole::Extraction, true, ESRFacilityRarity::Starting),
		};
	}

	TArray<FSRStructureBuildOption> MakeLargeCatalog(int32 OptionCount)
	{
		TArray<FSRStructureBuildOption> Options;
		Options.Reserve(FMath::Max(0, OptionCount));
		for (int32 OptionIndex = 0; OptionIndex < OptionCount; ++OptionIndex)
		{
			const FString StructureId = FString::Printf(TEXT("LargeMetalProcessor%03d"), OptionIndex + 1);
			Options.Add(MakeOption(
				*StructureId,
				ESRResourceFamily::Metal,
				ESRStructureBuildRole::FamilyProcessing));
		}
		return Options;
	}

	bool ContainsStructureId(const TArray<FName>& StructureIds, const TCHAR* StructureId)
	{
		return StructureIds.Contains(FName(StructureId));
	}

	bool ContainsBuildOption(const TArray<FSRStructureBuildOption>& BuildOptions, const TCHAR* StructureId)
	{
		return BuildOptions.ContainsByPredicate(
			[StructureId](const FSRStructureBuildOption& BuildOption)
			{
				return BuildOption.StructureId == FName(StructureId);
			});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStructureBuildDockFamilyFilteringTest,
	"StarRovers.UI.BuildDock.FamilyFiltering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStructureBuildDockFamilyFilteringTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::StructureBuildDockTests;

	const TArray<FSRStructureBuildOption> BuildOptions = MakeReferenceCatalog();
	TArray<FName> MetalIds;
	FSRStructureBuildDockModel::QueryOptions(
		BuildOptions,
		ESRStructureBuildFamilyFilter::Metal,
		true,
		MetalIds);

	TestTrue(TEXT("A Metal workspace includes its Family processor"), ContainsStructureId(MetalIds, TEXT("MetalForge")));
	TestTrue(TEXT("Locked Family processors remain visible"), ContainsStructureId(MetalIds, TEXT("MetalLockedPress")));
	TestTrue(TEXT("Shared extraction remains available inside a Family workflow"), ContainsStructureId(MetalIds, TEXT("Miner")));
	TestTrue(TEXT("Shared logistics remains available inside a Family workflow"), ContainsStructureId(MetalIds, TEXT("Conveyor")));
	TestTrue(TEXT("Shared generic processors remain available inside a Family workflow"), ContainsStructureId(MetalIds, TEXT("UniversalProcessor")));
	TestTrue(TEXT("Shared Tag facilities remain available inside a Family workflow"), ContainsStructureId(MetalIds, TEXT("TagImprinter")));
	TestTrue(TEXT("Shared Fuel Imprint facilities remain available inside a Family workflow"), ContainsStructureId(MetalIds, TEXT("FuelImprinter")));
	TestFalse(TEXT("A Metal workspace excludes Crystal-only processors"), ContainsStructureId(MetalIds, TEXT("CrystalMill")));
	TestFalse(TEXT("A Family workspace excludes global infrastructure"), ContainsStructureId(MetalIds, TEXT("ServiceCore")));
	TestFalse(TEXT("A Family workspace excludes the final multi-Family fabricator"), ContainsStructureId(MetalIds, TEXT("StellarFuelFabricator")));
	TestEqual(TEXT("Extraction is consistently ordered before logistics"), MetalIds[0], FName(TEXT("Miner")));
	TestEqual(TEXT("Logistics is consistently ordered before processors"), MetalIds[1], FName(TEXT("Conveyor")));

	TArray<FName> StrictMetalIds;
	FSRStructureBuildDockModel::QueryOptions(
		BuildOptions,
		ESRStructureBuildFamilyFilter::Metal,
		false,
		StrictMetalIds);
	TestEqual(TEXT("Strict filtering returns only exact Family entries"), StrictMetalIds.Num(), 2);
	TestTrue(TEXT("Strict filtering still keeps locked exact-Family entries"), ContainsStructureId(StrictMetalIds, TEXT("MetalLockedPress")));

	TArray<FName> SharedIds;
	FSRStructureBuildDockModel::QueryOptions(
		BuildOptions,
		ESRStructureBuildFamilyFilter::Shared,
		true,
		SharedIds);
	TestTrue(TEXT("Shared navigation includes infrastructure"), ContainsStructureId(SharedIds, TEXT("ServiceCore")));
	TestTrue(TEXT("Shared navigation includes stellar fuel fabrication"), ContainsStructureId(SharedIds, TEXT("StellarFuelFabricator")));
	TestFalse(TEXT("Shared navigation excludes exact-Family processors"), ContainsStructureId(SharedIds, TEXT("MetalForge")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStructureBuildDockTabSummaryTest,
	"StarRovers.UI.BuildDock.TabSummaryAndSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStructureBuildDockTabSummaryTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::StructureBuildDockTests;

	const TArray<FSRStructureBuildOption> BuildOptions = MakeReferenceCatalog();
	TArray<FSRStructureBuildFamilyTab> Tabs;
	FSRStructureBuildDockModel::BuildFamilyTabs(BuildOptions, Tabs);
	TestEqual(TEXT("The Build Dock exposes all seven primary navigation tabs"), Tabs.Num(), 7);
	TestEqual(TEXT("All is the stable first tab"), Tabs[0].Filter, ESRStructureBuildFamilyFilter::All);
	TestEqual(TEXT("Shared is the stable final tab"), Tabs.Last().Filter, ESRStructureBuildFamilyFilter::Shared);

	const FSRStructureBuildFamilyTab* MetalTab = Tabs.FindByPredicate(
		[](const FSRStructureBuildFamilyTab& Tab)
		{
			return Tab.Filter == ESRStructureBuildFamilyFilter::Metal;
		});
	TestNotNull(TEXT("A Metal tab summary is produced"), MetalTab);
	if (MetalTab)
	{
		TestEqual(TEXT("The Metal tab reports Family-owned facilities without inflating counts from shared helpers"),
			MetalTab->TotalOptionCount,
			2);
		TestEqual(TEXT("Locked entries are excluded only from the selectable Family count"),
			MetalTab->SelectableOptionCount,
			1);
	}
	const FSRStructureBuildFamilyTab* OrganicTab = Tabs.FindByPredicate(
		[](const FSRStructureBuildFamilyTab& Tab)
		{
			return Tab.Filter == ESRStructureBuildFamilyFilter::Organic;
		});
	TestNotNull(TEXT("An empty Family still has a stable navigation slot"), OrganicTab);
	if (OrganicTab)
	{
		TestFalse(TEXT("Shared helpers alone do not make an unauthored Family tab available"), OrganicTab->HasOptions());
	}

	TestEqual(TEXT("Family assets resolve to their preferred workspace"),
		FSRStructureBuildDockModel::ResolvePreferredFilter(BuildOptions[4]),
		ESRStructureBuildFamilyFilter::Metal);
	TestEqual(TEXT("Shared assets resolve to the Shared workspace"),
		FSRStructureBuildDockModel::ResolvePreferredFilter(BuildOptions[5]),
		ESRStructureBuildFamilyFilter::Shared);
	TestEqual(TEXT("The recommendation model resolves the selectable Foundation extractor"),
		FSRStructureBuildDockModel::FindRecommendedOptionId(
			BuildOptions,
			ESRStructureBuildRole::Extraction),
		FName(TEXT("Miner")));
	TestEqual(TEXT("A Family objective skips locked and off-Family processors"),
		FSRStructureBuildDockModel::FindRecommendedOptionId(
			BuildOptions,
			ESRStructureBuildRole::FamilyProcessing,
			ESRResourceFamily::Metal),
		FName(TEXT("MetalForge")));
	TestTrue(TEXT("A missing Family objective never fabricates a recommendation"),
		FSRStructureBuildDockModel::FindRecommendedOptionId(
			BuildOptions,
			ESRStructureBuildRole::FamilyProcessing,
			ESRResourceFamily::Void).IsNone());

	USRStructureSelectionWidget* Widget = NewObject<USRStructureSelectionWidget>(GetTransientPackage());
	TestNotNull(TEXT("The native Build Dock widget can be instantiated"), Widget);
	if (!Widget)
	{
		return false;
	}

	Widget->SetBuildOptions(BuildOptions);
	TestEqual(TEXT("The Build Dock defaults to a complete catalog view"),
		Widget->GetSelectedBuildDockFamily(),
		ESRStructureBuildFamilyFilter::All);
	TestEqual(TEXT("The complete view retains every catalog entry"),
		Widget->GetVisibleBuildOptions().Num(),
		BuildOptions.Num());
	TestTrue(TEXT("A populated Family can be selected directly"),
		Widget->SelectBuildDockFamily(ESRStructureBuildFamilyFilter::Metal));
	TestTrue(TEXT("The Metal view exposes the Metal processor"),
		ContainsBuildOption(Widget->GetVisibleBuildOptions(), TEXT("MetalForge")));

	Widget->DispatchBuildOptionSelected(FName(TEXT("MetalLockedPress")));
	TestFalse(TEXT("A locked visible option still cannot enter placement mode"), Widget->HasSelectedStructureId());
	Widget->DispatchBuildOptionSelected(FName(TEXT("MetalForge")));
	TestEqual(TEXT("An available filtered option enters placement mode"),
		Widget->GetSelectedStructureId(),
		FName(TEXT("MetalForge")));
	return true;
}

#if WITH_EDITOR

namespace StarRovers::StructureBuildDockTests
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

	bool ValidateProjectPlacementPreview(
		UWorld* PIEWorld,
		USRStructureSelectionWidget* ProjectBuildDock,
		FAutomationTestBase& Test)
	{
		if (!IsValid(PIEWorld) || !IsValid(ProjectBuildDock))
		{
			return false;
		}

		const TArray<FSRStructureBuildOption> ProjectOptions = ProjectBuildDock->GetBuildOptions();
		const FSRStructureBuildOption* PlacementOption = ProjectOptions.FindByPredicate(
			[](const FSRStructureBuildOption& Option)
			{
				USRStructureDataAsset* StructureDataAsset = Option.StructureDataAsset.Get();
				return Option.IsSelectable()
					&& IsValid(StructureDataAsset)
					&& StructureDataAsset->BuildData().BuildKind == ESRStructureBuildKind::Structure;
			});
		if (!PlacementOption)
		{
			return false;
		}

		for (TObjectIterator<USRPlanetSurfaceGrid> GridIt; GridIt; ++GridIt)
		{
			USRPlanetSurfaceGrid* SurfaceGrid = *GridIt;
			if (!IsValid(SurfaceGrid)
				|| SurfaceGrid->HasAnyFlags(RF_ClassDefaultObject)
				|| SurfaceGrid->GetWorld() != PIEWorld)
			{
				continue;
			}

			if (SurfaceGrid->GetCellsRef().IsEmpty())
			{
				// Production follows this same lazy initialization path when a body
				// enters assembly focus; SolarSystem does not eagerly allocate every grid.
				SurfaceGrid->PrepareGridForAssembly();
			}
			const TArray<FSRPlanetSurfaceGridCell>& Cells = SurfaceGrid->GetCellsRef();
			const int32 SampleStride = FMath::Max(1, Cells.Num() / 4096);
			for (int32 CellIndex = 0; CellIndex < Cells.Num(); CellIndex += SampleStride)
			{
				TArray<FSRPlanetSurfaceGridCellId> FootprintCellIds;
				if (!SurfaceGrid->GetFootprintCellIds(
					Cells[CellIndex].CellId,
					PlacementOption->FootprintCellsX,
					PlacementOption->FootprintCellsY,
					FootprintCellIds)
					|| !SurfaceGrid->SetHoveredCell(Cells[CellIndex].CellId))
				{
					continue;
				}

				const FSRStructurePlacementEvaluation Evaluation =
					FSRAssemblyStructurePlacementPreviewEvaluator::Evaluate(
						SurfaceGrid,
						PlacementOption->StructureDataAsset.Get(),
						0);
				Test.TestTrue(TEXT("An authored project structure resolves a targeted world preview"),
					Evaluation.Preview.bHasTarget);
				Test.TestTrue(TEXT("The targeted preview is either placeable or explains its world-space block"),
					Evaluation.Preview.bCanPlace
					|| Evaluation.Preview.Status == ESRStructurePlacementPreviewStatus::OutsideSurface
					|| Evaluation.Preview.Status == ESRStructurePlacementPreviewStatus::BlockedTerrain
					|| Evaluation.Preview.Status == ESRStructurePlacementPreviewStatus::BlockedOccupancy);
				Test.TestEqual(TEXT("The world preview reports every footprint cell"),
					Evaluation.FootprintCellIds.Num(),
					PlacementOption->FootprintCellsX * PlacementOption->FootprintCellsY);
				if (AActor* SurfaceOwner = SurfaceGrid->GetOwner();
					IsValid(SurfaceOwner)
					&& IsValid(SurfaceOwner->FindComponentByClass<USRFacilityNetworkComponent>()))
				{
					Test.TestTrue(TEXT("Planet placement preview includes projected Operational Capacity"),
						Evaluation.Preview.bHasCapacityData);
				}
				SurfaceGrid->ClearHoveredCell();
				return Evaluation.Preview.bHasTarget;
			}
		}

		return false;
	}

	class FWaitForFamilyBuildDockCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FWaitForFamilyBuildDockCommand(FAutomationTestBase& InTest)
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
				Test.AddError(TEXT("A PIE world was not available for Build Dock validation."));
				return true;
			}

			USRStructureSelectionWidget* Widget = CreateWidget<USRStructureSelectionWidget>(
				PIEWorld,
				USRStructureSelectionWidget::StaticClass());
			Test.TestNotNull(TEXT("The Family Build Dock is constructible through CreateWidget"), Widget);
			if (IsValid(Widget))
			{
				Widget->SetBuildOptions(MakeLargeCatalog(60));
				const TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();
				Test.TestTrue(TEXT("The Build Dock builds a cached Slate hierarchy"), Widget->GetCachedWidget().IsValid());
				Test.TestNotNull(TEXT("The seventh Family tab exists in the runtime widget tree"),
					Widget->GetWidgetFromName(FName(TEXT("StructureCategoryButton7"))));
				Test.TestNotNull(TEXT("The paged structure selection row exists in the runtime widget tree"),
					Widget->GetWidgetFromName(FName(TEXT("StructureFacilityButton1"))));
				Test.TestNotNull(TEXT("A structure card exposes its role label"),
					Widget->GetWidgetFromName(FName(TEXT("StructureFacilityButtonRole1"))));
				Test.TestNotNull(TEXT("A structure card exposes compact metadata"),
					Widget->GetWidgetFromName(FName(TEXT("StructureFacilityButtonMetadata1"))));
				Test.TestNotNull(TEXT("A structure card exposes semantic availability"),
					Widget->GetWidgetFromName(FName(TEXT("StructureFacilityButtonStatus1"))));
				Test.TestNotNull(TEXT("The Build Dock exposes a structure detail panel"),
					Widget->GetWidgetFromName(FName(TEXT("StructureDetailBorder"))));
				Test.TestNotNull(TEXT("The detail panel exposes live placement feedback"),
					Widget->GetWidgetFromName(FName(TEXT("StructureDetailPlacement"))));
				Test.TestNotNull(TEXT("The detail panel exposes an input-process-output diagram"),
					Widget->GetWidgetFromName(FName(TEXT("StructureBuildFlowPanel"))));
				Test.TestNotNull(TEXT("The transform diagram has an explicit input node"),
					Widget->GetWidgetFromName(FName(TEXT("StructureBuildFlowInput"))));
				Test.TestNotNull(TEXT("The transform diagram has an explicit process node"),
					Widget->GetWidgetFromName(FName(TEXT("StructureBuildFlowProcess"))));
				Test.TestNotNull(TEXT("The transform diagram has an explicit output node"),
					Widget->GetWidgetFromName(FName(TEXT("StructureBuildFlowOutput"))));
				Test.TestNotNull(TEXT("The placement strip exposes target validity"),
					Widget->GetWidgetFromName(FName(TEXT("StructurePlacementTargetBadge"))));
				Test.TestNotNull(TEXT("The placement strip exposes rotated footprint"),
					Widget->GetWidgetFromName(FName(TEXT("StructurePlacementFootprintBadge"))));
				Test.TestNotNull(TEXT("The placement strip exposes projected Capacity"),
					Widget->GetWidgetFromName(FName(TEXT("StructurePlacementCapacityBadge"))));
				Test.TestNotNull(TEXT("Large catalogs expose an exact current-page label"),
					Widget->GetWidgetFromName(FName(TEXT("StructurePageTextBlock"))));
				Test.TestEqual(TEXT("Sixty facilities are split into twelve five-card pages"),
					Widget->GetBuildDockPageCount(),
					12);
				Test.TestEqual(TEXT("A large Build Dock never creates more than seven page indicators"),
					Widget->GetVisibleBuildDockPageIndicatorCount(),
					7);
				Test.TestTrue(TEXT("Build Dock next-page input advances the page"),
					Widget->AdvanceStructureSelectionTab());
				Test.TestEqual(TEXT("The Build Dock reports the advanced page"), Widget->GetBuildDockPageIndex(), 1);
				Test.TestTrue(TEXT("Build Dock previous-page input retreats the page"),
					Widget->RetreatStructureSelectionTab());
				Test.TestEqual(TEXT("The Build Dock returns to its first page"), Widget->GetBuildDockPageIndex(), 0);
				Test.TestTrue(TEXT("PIE can switch to the Metal Family workspace"),
					Widget->SelectBuildDockFamily(ESRStructureBuildFamilyFilter::Metal));
				Test.TestEqual(TEXT("PIE Metal filtering retains the complete large Family catalog"),
					Widget->GetVisibleBuildOptions().Num(),
					60);
			}

			bool bPlacementPreviewReady = false;
			ASRPlayerController* PlayerController = Cast<ASRPlayerController>(
				PIEWorld->GetFirstPlayerController());
			Test.TestNotNull(TEXT("SolarSystem PIE uses the Star Rovers player controller"), PlayerController);
			if (IsValid(PlayerController))
			{
				USRStructureSelectionWidget* ProjectBuildDock = PlayerController->GetStructureSelectionWidget();
				Test.TestNotNull(TEXT("The project player controller creates its configured Build Dock"), ProjectBuildDock);
				if (IsValid(ProjectBuildDock))
				{
					Test.TestTrue(TEXT("The project Build Dock receives the authored construction catalog"),
						!ProjectBuildDock->GetBuildOptions().IsEmpty());
					Test.TestEqual(TEXT("The project Build Dock exposes all Family navigation tabs"),
						ProjectBuildDock->GetBuildDockFamilyTabs().Num(),
						7);
					Test.TestNotNull(TEXT("The configured project widget is upgraded to the seventh Family tab"),
						ProjectBuildDock->GetWidgetFromName(FName(TEXT("StructureCategoryButton7"))));
					Test.TestNotNull(TEXT("The configured project widget is upgraded to structure cards"),
						ProjectBuildDock->GetWidgetFromName(FName(TEXT("StructureFacilityButtonMetadata1"))));
					Test.TestNotNull(TEXT("The configured project widget is upgraded to the detail panel"),
						ProjectBuildDock->GetWidgetFromName(FName(TEXT("StructureDetailBorder"))));
					Test.TestNotNull(TEXT("The configured project widget is upgraded to the transform diagram"),
						ProjectBuildDock->GetWidgetFromName(FName(TEXT("StructureBuildFlowProcess"))));
					Test.TestNotNull(TEXT("The configured project widget is upgraded to placement metrics"),
						ProjectBuildDock->GetWidgetFromName(FName(TEXT("StructurePlacementCapacityBadge"))));
					bPlacementPreviewReady = ValidateProjectPlacementPreview(PIEWorld, ProjectBuildDock, Test);
				}
			}
			if (!bPlacementPreviewReady)
			{
				if (FPlatformTime::Seconds() - StartTimeSeconds < 30.0)
				{
					return false;
				}
				Test.AddError(TEXT("No authored surface footprint became ready for placement-preview validation."));
			}
			return true;
		}

	private:
		FAutomationTestBase& Test;
		double StartTimeSeconds = 0.0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStructureBuildDockPIETest,
	"StarRovers.UI.BuildDock.PIE.NativeFamilyFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStructureBuildDockPIETest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Levels/SolarSystem")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(
		StarRovers::StructureBuildDockTests::FWaitForFamilyBuildDockCommand(*this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif

#endif
