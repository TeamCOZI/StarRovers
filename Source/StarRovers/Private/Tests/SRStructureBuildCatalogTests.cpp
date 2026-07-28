#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Assembly/SRStructureBuildCatalog.h"
#include "Misc/AutomationTest.h"

namespace StarRovers::StructureBuildCatalogTests
{
	USRStructureDataAsset* MakeStructure(const TCHAR* StructureId)
	{
		USRStructureDataAsset* Structure = NewObject<USRStructureDataAsset>(GetTransientPackage());
		Structure->StructureId = FName(StructureId);
		Structure->DisplayName = FText::FromString(StructureId);
		Structure->Description = FText::FromString(TEXT("Catalog test structure"));
		Structure->bAvailableForConstruction = true;
		Structure->bIsResourceDeposit = false;
		return Structure;
	}

	USRFacilityDataAsset* MakeMetalProcessor()
	{
		USRFacilityDataAsset* Facility = NewObject<USRFacilityDataAsset>(GetTransientPackage());
		Facility->FacilityKind = ESRFacilityKind::Standard;
		Facility->Rarity = ESRFacilityRarity::Advanced;
		Facility->OperationKind = ESRFacilityOperationKind::Process;
		Facility->FacilityDefinitionVersion = StarRovers::Facilities::CurrentFacilityDefinitionVersion;
		Facility->ResourceV2ContentId = FName(TEXT("InductionForge"));
		Facility->ResourceV2Process.ProcessRole = ESRFacilityProcessRoleV2::FamilyProcess;
		Facility->ResourceV2Process.LineRole = ESRFacilityLineRoleV2::Primer;
		Facility->ResourceV2Process.ProcessArchetype = FName(TEXT("Forge"));
		Facility->ResourceV2Process.AcceptedFamily = ESRResourceFamily::Metal;
		Facility->ResourceV2Process.FacilityEnergyDelta = 4.0;
		Facility->OperationalLoad = 7;
		Facility->DefaultOperationalPriority = ESROperationalPriorityV2::Background;
		Facility->BaseProcessSeconds = 4.5f;
		return Facility;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStructureBuildCatalogMetadataTest,
	"StarRovers.UI.BuildCatalog.MetadataAndAvailability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStructureBuildCatalogMetadataTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::StructureBuildCatalogTests;

	USRStructureDataAsset* Structure = MakeStructure(TEXT("MetalCatalogProcessor"));
	Structure->FacilityDataAsset = MakeMetalProcessor();
	Structure->FootprintCellsX = 2;
	Structure->FootprintCellsY = 3;
	Structure->InputPorts.AddDefaulted(2);
	Structure->OutputPorts.AddDefaulted(1);

	FSRStructureBuildOption AvailableOption;
	TestTrue(TEXT("A valid constructible asset produces a catalog option"),
		FSRStructureBuildCatalogBuilder::TryBuildOption(Structure, true, AvailableOption));
	TestTrue(TEXT("An unlocked constructible option is selectable"), AvailableOption.IsSelectable());
	TestEqual(TEXT("Family metadata comes from the Resource V2 process definition"),
		AvailableOption.ResourceFamily,
		ESRResourceFamily::Metal);
	TestEqual(TEXT("The process role is normalized for family-first UI grouping"),
		AvailableOption.Role,
		ESRStructureBuildRole::FamilyProcessing);
	TestEqual(TEXT("The Build Dock receives the Facility V2 operation contract"),
		AvailableOption.OperationKind,
		ESRFacilityOperationKind::Process);
	TestEqual(TEXT("The Build Dock receives the stable process archetype"),
		AvailableOption.ProcessArchetype,
		FName(TEXT("Forge")));
	TestTrue(TEXT("The Build Dock receives the additive Energy delta"),
		FMath::IsNearlyEqual(AvailableOption.FacilityEnergyDelta, 4.0));
	TestEqual(TEXT("The Build Dock receives the Facility Line role"),
		AvailableOption.LineRole,
		ESRFacilityLineRoleV2::Primer);
	TestEqual(TEXT("Operational Load is available without opening the facility runtime"),
		AvailableOption.OperationalLoad,
		7);
	TestEqual(TEXT("Operational priority is available to construction UI"),
		AvailableOption.OperationalPriority,
		ESROperationalPriorityV2::Background);
	TestEqual(TEXT("Base process time is available to construction UI"),
		AvailableOption.BaseProcessSeconds,
		4.5f);
	TestEqual(TEXT("Footprint X is copied into the catalog"), AvailableOption.FootprintCellsX, 2);
	TestEqual(TEXT("Footprint Y is copied into the catalog"), AvailableOption.FootprintCellsY, 3);
	TestEqual(TEXT("Input port count is copied into the catalog"), AvailableOption.InputPortCount, 2);
	TestEqual(TEXT("Output port count is copied into the catalog"), AvailableOption.OutputPortCount, 1);

	FSRStructureBuildOption LockedOption;
	TestTrue(TEXT("A locked structure remains in the catalog"),
		FSRStructureBuildCatalogBuilder::TryBuildOption(Structure, false, LockedOption));
	TestFalse(TEXT("A locked option cannot be selected"), LockedOption.IsSelectable());
	TestEqual(TEXT("A locked option exposes its availability"),
		LockedOption.Availability,
		ESRStructureBuildAvailability::LockedByAugment);
	TestEqual(TEXT("A locked option exposes a stable block reason"),
		LockedOption.BlockReason,
		ESRStructureBuildBlockReason::RequiresAugment);
	TestFalse(TEXT("A locked option provides a player-facing unlock hint"),
		LockedOption.UnlockHintText.IsEmpty());

	Structure->bAvailableForConstruction = false;
	FSRStructureBuildOption DisabledOption;
	TestTrue(TEXT("An authored disabled option remains diagnosable"),
		FSRStructureBuildCatalogBuilder::TryBuildOption(Structure, false, DisabledOption));
	TestEqual(TEXT("Construction-disabled takes precedence over Augment lock"),
		DisabledOption.Availability,
		ESRStructureBuildAvailability::ConstructionDisabled);
	TestEqual(TEXT("Construction-disabled has a distinct reason"),
		DisabledOption.BlockReason,
		ESRStructureBuildBlockReason::ConstructionDisabled);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRStructureBuildCatalogFilteringTest,
	"StarRovers.UI.BuildCatalog.FilteringAndIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRStructureBuildCatalogFilteringTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::StructureBuildCatalogTests;

	USRStructureDataAsset* ValidStructure = MakeStructure(TEXT("UniqueCatalogStructure"));
	USRStructureDataAsset* NaturalDeposit = MakeStructure(TEXT("NaturalCatalogDeposit"));
	NaturalDeposit->bIsResourceDeposit = true;
	USRStructureDataAsset* InvalidStructure = MakeStructure(TEXT("TemporaryId"));
	InvalidStructure->StructureId = NAME_None;

	const TArray<USRStructureDataAsset*> ConfiguredAssets = {
		ValidStructure,
		ValidStructure,
		NaturalDeposit,
		InvalidStructure,
	};
	FSRStructureBuildCatalog Catalog;
	FSRStructureBuildCatalogBuilder::BuildCatalog(ConfiguredAssets, nullptr, Catalog);

	TestEqual(TEXT("All configured references are counted"), Catalog.ConfiguredAssetCount, 4);
	TestEqual(TEXT("Only one unique buildable definition is exposed"), Catalog.BuildOptions.Num(), 1);
	TestEqual(TEXT("Duplicate ids are reported"), Catalog.ExcludedDuplicateIdCount, 1);
	TestEqual(TEXT("Natural deposits never appear in the construction catalog"),
		Catalog.ExcludedNaturalDepositCount,
		1);
	TestEqual(TEXT("Invalid definitions are reported"), Catalog.ExcludedInvalidAssetCount, 1);
	return true;
}

#endif
