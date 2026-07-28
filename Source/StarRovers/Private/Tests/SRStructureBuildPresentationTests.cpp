#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Assembly/SRAssemblyStructurePlacementPreview.h"
#include "Misc/AutomationTest.h"
#include "Structure/SRStructureDataAsset.h"
#include "UI/SRStructureBuildPresentation.h"

namespace StarRovers::StructureBuildPresentationTests
{
	FSRStructureBuildOption MakeDetailedOption()
	{
		USRStructureDataAsset* StructureDataAsset = NewObject<USRStructureDataAsset>(GetTransientPackage());
		StructureDataAsset->StructureId = FName(TEXT("MetalInductionForge"));
		StructureDataAsset->bAvailableForConstruction = true;

		FSRStructureBuildOption Option;
		Option.StructureId = StructureDataAsset->StructureId;
		Option.DisplayName = FText::FromString(TEXT("Induction Forge"));
		Option.StructureDataAsset = StructureDataAsset;
		Option.Description = FText::FromString(TEXT("Alternates field polarity while processing Metal."));
		Option.ResourceFamily = ESRResourceFamily::Metal;
		Option.Role = ESRStructureBuildRole::FamilyProcessing;
		Option.OperationKind = ESRFacilityOperationKind::Process;
		Option.ProcessRole = ESRFacilityProcessRoleV2::FamilyProcess;
		Option.LineRole = ESRFacilityLineRoleV2::Primer;
		Option.ProcessArchetype = FName(TEXT("Forge"));
		Option.FacilityEnergyDelta = 4.0;
		Option.Rarity = ESRFacilityRarity::Advanced;
		Option.FootprintCellsX = 2;
		Option.FootprintCellsY = 3;
		Option.InputPortCount = 2;
		Option.OutputPortCount = 1;
		Option.OperationalLoad = 7;
		Option.OperationalPriority = ESROperationalPriorityV2::Background;
		Option.BaseProcessSeconds = 4.5f;
		Option.bEnabled = true;
		Option.bUnlocked = true;
		Option.Availability = ESRStructureBuildAvailability::Available;
		return Option;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStructureBuildCardPresentationTest,
	"StarRovers.UI.BuildDock.Presentation.CardAndDetail",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStructureBuildCardPresentationTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::StructureBuildPresentationTests;

	FSRStructureBuildOption Option = MakeDetailedOption();
	const FSRStructureBuildCardPresentation Card = FSRStructureBuildPresentationBuilder::BuildCard(Option);
	TestEqual(TEXT("Card identity is stable"), Card.StructureId, Option.StructureId);
	TestTrue(TEXT("Card exposes its workflow role"), Card.RoleText.ToString().Contains(TEXT("FAMILY")));
	TestTrue(TEXT("Card exposes its Family"), Card.MetadataText.ToString().Contains(TEXT("METAL")));
	TestTrue(TEXT("Card exposes its footprint"), Card.MetadataText.ToString().Contains(TEXT("2x3")));
	TestTrue(TEXT("Card exposes Operational Load"), Card.MetadataText.ToString().Contains(TEXT("LOAD 7")));
	TestTrue(TEXT("Available cards remain selectable"), Card.bSelectable);

	const FSRStructureBuildDetailPresentation Detail = FSRStructureBuildPresentationBuilder::BuildDetail(Option);
	TestTrue(TEXT("Detail exposes port topology"), Detail.SpecificationText.ToString().Contains(TEXT("2 in / 1 out")));
	TestTrue(TEXT("Detail exposes base cycle time"), Detail.SpecificationText.ToString().Contains(TEXT("4.5")));
	TestTrue(TEXT("Detail exposes operational priority"), Detail.SpecificationText.ToString().Contains(TEXT("Background")));

	const FSRStructureBuildFlowPresentation Flow =
		FSRStructureBuildPresentationBuilder::BuildFlow(Option);
	TestTrue(TEXT("The transform diagram names the accepted Family card"),
		Flow.InputText.ToString().Contains(TEXT("METAL")));
	TestTrue(TEXT("The transform diagram names the stable process archetype"),
		Flow.ProcessText.ToString().Contains(TEXT("FORGE")));
	TestTrue(TEXT("Normal processing advertises an additive Energy change"),
		Flow.EffectText.ToString().Contains(TEXT("+4")));
	TestTrue(TEXT("The transform diagram exposes the Facility's Line role"),
		Flow.EffectText.ToString().Contains(TEXT("PRIMER")));
	TestTrue(TEXT("The diagram warns that runtime history remains authoritative"),
		Flow.ToolTipText.ToString().Contains(TEXT("history")));

	FSRStructureBuildOption Bridge = Option;
	Bridge.ResourceFamily = ESRResourceFamily::None;
	Bridge.LineRole = ESRFacilityLineRoleV2::UniversalBridge;
	const FSRStructureBuildFlowPresentation BridgeFlow =
		FSRStructureBuildPresentationBuilder::BuildFlow(Bridge);
	TestTrue(TEXT("Universal processing is labeled as a Bridge at a glance"),
		BridgeFlow.EffectText.ToString().Contains(TEXT("BRIDGE")));
	TestTrue(TEXT("A Bridge explicitly disclaims positive Family merit"),
		BridgeFlow.EffectText.ToString().Contains(TEXT("NO FAMILY MERIT"))
			&& BridgeFlow.ToolTipText.ToString().Contains(TEXT("negative Family pressure")));

	FSRStructureBuildOption FuelFabricator = Option;
	FuelFabricator.Role = ESRStructureBuildRole::StellarFuelFabrication;
	FuelFabricator.ResourceFamily = ESRResourceFamily::None;
	FuelFabricator.OperationKind = ESRFacilityOperationKind::Synthesize;
	FuelFabricator.SynthesisRole = ESRFacilitySynthesisRoleV2::StellarFuelFabricator;
	const FSRStructureBuildFlowPresentation FuelFlow =
		FSRStructureBuildPresentationBuilder::BuildFlow(FuelFabricator);
	TestTrue(TEXT("The final Fabricator visibly requires five Cards"),
		FuelFlow.InputText.ToString().Contains(TEXT("5")));
	TestTrue(TEXT("The final Fabricator alone names the B x C operation"),
		FuelFlow.ProcessText.ToString().Contains(TEXT("B x C")));

	FSRStructureBuildOption TagImprinter = Option;
	TagImprinter.Role = ESRStructureBuildRole::TagProcessing;
	TagImprinter.ResourceFamily = ESRResourceFamily::None;
	TagImprinter.ProcessRole = ESRFacilityProcessRoleV2::ApplyProcessTag;
	TagImprinter.ProcessTagId = FName(TEXT("Overtone"));
	const FSRStructureBuildFlowPresentation TagFlow =
		FSRStructureBuildPresentationBuilder::BuildFlow(TagImprinter);
	TestTrue(TEXT("A Tag Facility distinguishes a tagged output Card"),
		TagFlow.OutputText.ToString().Contains(TEXT("TAGGED")));
	TestTrue(TEXT("A Tag Facility labels its default recipe payload"),
		TagFlow.EffectText.ToString().Contains(TEXT("OVERTONE")));

	FSRStructureBuildOption ServiceCore = Option;
	ServiceCore.Role = ESRStructureBuildRole::Infrastructure;
	ServiceCore.ResourceFamily = ESRResourceFamily::None;
	ServiceCore.OperationKind = ESRFacilityOperationKind::Synthesize;
	ServiceCore.SynthesisRole = ESRFacilitySynthesisRoleV2::ServiceCore;
	const FSRStructureBuildFlowPresentation ServiceFlow =
		FSRStructureBuildPresentationBuilder::BuildFlow(ServiceCore);
	TestTrue(TEXT("Infrastructure makes its Industrial Supply input explicit"),
		ServiceFlow.InputText.ToString().Contains(TEXT("SUPPLY")));
	TestTrue(TEXT("Infrastructure makes Capacity the output of its contract"),
		ServiceFlow.OutputText.ToString().Contains(TEXT("CAPACITY")));

	FSRStructureBuildRecommendationContext RecommendationContext;
	RecommendationContext.bActive = true;
	RecommendationContext.RecommendedStructureId = Option.StructureId;
	RecommendationContext.CurrentStep = 3;
	RecommendationContext.TotalSteps = 9;
	RecommendationContext.ObjectiveText = FText::FromString(TEXT("PROCESS THE FIRST METAL CARD"));
	const FSRStructureBuildRecommendationPresentation Recommendation =
		FSRStructureBuildPresentationBuilder::BuildRecommendation(Option, RecommendationContext);
	TestTrue(TEXT("Only the exact objective option exposes a recommendation"), Recommendation.bVisible);
	TestTrue(TEXT("Recommendation badges expose milestone progress"),
		Recommendation.BadgeText.ToString().Contains(TEXT("3/9")));
	TestTrue(TEXT("Recommendation reason remains an action, not a generic score"),
		Recommendation.ReasonText.ToString().Contains(TEXT("METAL")));

	Option.bUnlocked = false;
	Option.Availability = ESRStructureBuildAvailability::LockedByAugment;
	Option.BlockReason = ESRStructureBuildBlockReason::RequiresAugment;
	Option.UnlockHintText = FText::FromString(TEXT("Acquire the Metal Control Package."));
	const FSRStructureBuildDetailPresentation LockedDetail =
		FSRStructureBuildPresentationBuilder::BuildDetail(Option);
	TestFalse(TEXT("Locked detail cannot be selected"), LockedDetail.bSelectable);
	TestTrue(TEXT("Locked detail preserves the unlock explanation"),
		LockedDetail.AvailabilityDetailText.ToString().Contains(TEXT("Metal Control Package")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStructurePlacementPreviewStateTest,
	"StarRovers.UI.BuildDock.Presentation.PlacementStates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStructurePlacementPreviewStateTest::RunTest(const FString& Parameters)
{
	const FSRStructurePlacementPreview Inactive =
		FSRAssemblyStructurePlacementPreviewEvaluator::Evaluate(nullptr, nullptr, 0).Preview;
	TestEqual(TEXT("No selection is inactive"), Inactive.Status, ESRStructurePlacementPreviewStatus::Inactive);

	USRStructureDataAsset* Structure = NewObject<USRStructureDataAsset>(GetTransientPackage());
	Structure->StructureId = FName(TEXT("PreviewStructure"));
	Structure->bAvailableForConstruction = true;
	Structure->bIsResourceDeposit = false;
	Structure->FootprintCellsX = 2;
	Structure->FootprintCellsY = 3;
	const FSRStructurePlacementPreview Awaiting =
		FSRAssemblyStructurePlacementPreviewEvaluator::Evaluate(nullptr, Structure, 1).Preview;
	TestEqual(TEXT("A selected structure waits for a surface"),
		Awaiting.Status,
		ESRStructurePlacementPreviewStatus::AwaitingSurface);
	TestEqual(TEXT("Rotation is reflected in preview width"), Awaiting.FootprintCellsX, 3);
	TestEqual(TEXT("Rotation is reflected in preview height"), Awaiting.FootprintCellsY, 2);

	FSRStructureBuildOption BuildOption;
	BuildOption.StructureId = Structure->StructureId;
	BuildOption.StructureDataAsset = Structure;
	BuildOption.FootprintCellsX = 2;
	BuildOption.FootprintCellsY = 3;
	BuildOption.OperationalLoad = 7;
	BuildOption.bEnabled = true;
	BuildOption.bUnlocked = true;
	BuildOption.Availability = ESRStructureBuildAvailability::Available;
	const FSRStructureBuildPlacementPresentation BrowsePlacement =
		FSRStructureBuildPresentationBuilder::BuildPlacement(BuildOption, nullptr);
	TestTrue(TEXT("A browsed Facility asks for selection before claiming a live target"),
		BrowsePlacement.TargetText.ToString().Contains(TEXT("SELECT")));
	TestTrue(TEXT("A browsed Facility still exposes its authored footprint"),
		BrowsePlacement.FootprintText.ToString().Contains(TEXT("2x3")));
	TestTrue(TEXT("A browsed Facility exposes its projected load"),
		BrowsePlacement.CapacityText.ToString().Contains(TEXT("+7")));

	FSRStructurePlacementPreview CapacityPreview = Awaiting;
	CapacityPreview.Status = ESRStructurePlacementPreviewStatus::Ready;
	CapacityPreview.bHasTarget = true;
	CapacityPreview.bCanPlace = true;
	CapacityPreview.bHasCapacityData = true;
	CapacityPreview.bCapacityWarning = true;
	CapacityPreview.CurrentDemand = 18;
	CapacityPreview.ProjectedDemand = 25;
	CapacityPreview.TotalCapacity = 20;
	const FSRStructureBuildPlacementPresentation LivePlacement =
		FSRStructureBuildPresentationBuilder::BuildPlacement(BuildOption, &CapacityPreview);
	TestTrue(TEXT("A clear world target is visible without reading its description"),
		LivePlacement.TargetText.ToString().Contains(TEXT("CLEAR")));
	TestTrue(TEXT("Rotation is preserved in the compact live footprint"),
		LivePlacement.FootprintText.ToString().Contains(TEXT("3x2")));
	TestTrue(TEXT("Capacity reports current, projected, and total demand together"),
		LivePlacement.CapacityText.ToString().Contains(TEXT("18"))
		&& LivePlacement.CapacityText.ToString().Contains(TEXT("25"))
		&& LivePlacement.CapacityText.ToString().Contains(TEXT("20")));
	TestEqual(TEXT("Projected overload uses the warning semantic"),
		LivePlacement.CapacityVisualState,
		ESRUIVisualState::Warning);

	Structure->BuildKind = ESRStructureBuildKind::Conveyor;
	const FSRStructurePlacementPreview Conveyor =
		FSRAssemblyStructurePlacementPreviewEvaluator::Evaluate(nullptr, Structure, 0).Preview;
	TestEqual(TEXT("Conveyors expose their distinct path interaction"),
		Conveyor.Status,
		ESRStructurePlacementPreviewStatus::ConveyorPath);

	Structure->BuildKind = ESRStructureBuildKind::Structure;
	Structure->bAvailableForConstruction = false;
	const FSRStructurePlacementPreview Disabled =
		FSRAssemblyStructurePlacementPreviewEvaluator::Evaluate(nullptr, Structure, 0).Preview;
	TestEqual(TEXT("Disabled authored content is diagnosed before target selection"),
		Disabled.Status,
		ESRStructurePlacementPreviewStatus::InvalidDefinition);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStructureBuildEmptyStateTest,
	"StarRovers.UI.BuildDock.Presentation.EmptyStates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStructureBuildEmptyStateTest::RunTest(const FString& Parameters)
{
	const FSRStructureBuildEmptyStatePresentation EmptyCatalog =
		FSRStructureBuildPresentationBuilder::BuildEmptyState(0, 0, 0);
	TestTrue(TEXT("An unregistered catalog produces a visible explanation"), EmptyCatalog.bVisible);
	TestEqual(TEXT("An empty catalog is a warning"), EmptyCatalog.VisualState, ESRUIVisualState::Warning);
	TestTrue(TEXT("The empty catalog gives a recovery action"),
		EmptyCatalog.ActionText.ToString().Contains(TEXT("NEXT")));

	const FSRStructureBuildEmptyStatePresentation NoMatches =
		FSRStructureBuildPresentationBuilder::BuildEmptyState(12, 0, 0);
	TestTrue(TEXT("A zero-result Family filter is distinguished from missing content"),
		NoMatches.BadgeText.ToString().Contains(TEXT("NO MATCHES")));
	TestTrue(TEXT("A zero-result filter points to another Family tab"),
		NoMatches.ActionText.ToString().Contains(TEXT("Family")));

	const FSRStructureBuildEmptyStatePresentation AllLocked =
		FSRStructureBuildPresentationBuilder::BuildEmptyState(12, 4, 0);
	TestEqual(TEXT("A visible-but-locked catalog uses the locked semantic state"),
		AllLocked.VisualState,
		ESRUIVisualState::Locked);
	TestTrue(TEXT("The locked state explains the Augment dependency"),
		AllLocked.ActionText.ToString().Contains(TEXT("AUGMENT LOCK")));

	const FSRStructureBuildEmptyStatePresentation Available =
		FSRStructureBuildPresentationBuilder::BuildEmptyState(12, 4, 1);
	TestFalse(TEXT("At least one selectable card suppresses the empty state"), Available.bVisible);
	return true;
}

#endif
