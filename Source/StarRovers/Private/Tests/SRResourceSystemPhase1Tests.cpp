#include "Automation/SRResourceDataAsset.h"
#include "Automation/SRResourceInstanceOperations.h"
#include "Automation/SRFacilityResourceOperations.h"
#include "Logistics/SRSpaceLogisticsTypes.h"
#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	USRResourceDataAsset* MakeTransientMetalCard()
	{
		USRResourceDataAsset* ResourceDataAsset = NewObject<USRResourceDataAsset>(GetTransientPackage());
		if (!ResourceDataAsset)
		{
			return nullptr;
		}

		ResourceDataAsset->ResourceDefinitionVersion = StarRovers::Resources::CurrentResourceDefinitionVersion;
		ResourceDataAsset->ResourceId = FName(TEXT("HeliosIron"));
		ResourceDataAsset->ResourceClass = ESRResourceClass::Card;
		ResourceDataAsset->Family = ESRResourceFamily::Metal;
		ResourceDataAsset->SeedEnergy = 5.0;
		ResourceDataAsset->NativeSpectrum = ESRResourceSpectrum::Red;
		ResourceDataAsset->NativeGrade = 2;
		return ResourceDataAsset;
	}

	FSRResourceInstance MakeRichResourceInstance()
	{
		FSRResourceInstance ResourceInstance;
		ResourceInstance.ResourceInstanceId = FName(TEXT("ResourceInstance_Phase1"));
		ResourceInstance.ResourceId = FName(TEXT("HeliosIron"));
		ResourceInstance.ResourceSchemaVersion = StarRovers::Resources::CurrentResourceSchemaVersion;
		ResourceInstance.ResourceClass = ESRResourceClass::Card;
		ResourceInstance.Family = ESRResourceFamily::Metal;
		ResourceInstance.CurrentEnergy = 34.0;
		ResourceInstance.SeedEnergySnapshot = 5.0;
		ResourceInstance.bHasSeedEnergySnapshot = true;
		ResourceInstance.EnergyValue = 34.0;
		ResourceInstance.Spectrum = ESRResourceSpectrum::Red;
		ResourceInstance.Grade = 2;
		ResourceInstance.ActiveFamilyStateFlags =
			StarRovers::Resources::GetFamilyStateBit(ESRResourceFamilyState::Tempered)
			| StarRovers::Resources::GetFamilyStateBit(ESRResourceFamilyState::Fatigued);
		ResourceInstance.ProcessTagSlot.TagId = FName(TEXT("Overtone"));
		ResourceInstance.ProcessTagSlot.Lifecycle = ESRResourceSlotLifecycle::Primed;
		ResourceInstance.ProcessTagSlot.RemainingTriggers = 1;
		ResourceInstance.FuelImprintSlot.ImprintId = FName(TEXT("TwinSeal"));
		ResourceInstance.ProcessingMemory.LastProcessArchetype = FName(TEXT("CryoPress"));
		ResourceInstance.ProcessingMemory.LastTemperature = ESRResourceProcessTemperatureState::Cold;
		ResourceInstance.ProcessingMemory.LastFamilyAction = ESRResourceFamilyAction::EnergyGain;
		ResourceInstance.ProcessingMemory.ConsecutiveSameArchetypeCount = 1;
		ResourceInstance.ProcessingMemory.ConsecutiveSameFamilyActionCount = 2;
		ResourceInstance.ProcessingMemory.GeneralProcessesSinceReset = 4;
		ResourceInstance.ProcessingMemory.StoredFamilyMagnitude = 3.5;
		ResourceInstance.ProcessingMemory.TransitCountAtLastEnergyChange = 2;
		ResourceInstance.ProcessingMemory.ProcessCount = 4;
		ResourceInstance.ProcessingMemory.EnergyChangeCount = 4;
		ResourceInstance.ProcessCount = 4;
		ResourceInstance.EnergyChangeCount = 4;
		ResourceInstance.LogisticsMetadata.OriginBodyId = FName(TEXT("Cinder"));
		ResourceInstance.LogisticsMetadata.LastProcessedBodyId = FName(TEXT("Prism"));
		ResourceInstance.LogisticsMetadata.LastTransitSourceBodyId = FName(TEXT("Cinder"));
		ResourceInstance.LogisticsMetadata.LastTransitDestinationBodyId = FName(TEXT("Prism"));
		ResourceInstance.LogisticsMetadata.TransitCount = 2;
		ResourceInstance.LogisticsMetadata.bHasBeenProcessedOutsideOrigin = true;
		FSRResourceTagStack LegacyTag;
		LegacyTag.Tag = ESRResourceProcessTag::Responsive;
		LegacyTag.StackCount = 1;
		LegacyTag.RemainingCycles = 2;
		ResourceInstance.Tags.Add(LegacyTag);
		ResourceInstance.RemainingProcessLimit = 7;
		ResourceInstance.StackCount = 3;
		return ResourceInstance;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourceV2DefaultInstanceTest,
	"StarRovers.ResourceSystem.ResourceV2.DefaultInstance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourceV2DefaultInstanceTest::RunTest(const FString& Parameters)
{
	USRResourceDataAsset* ResourceDataAsset = MakeTransientMetalCard();
	TestNotNull(TEXT("A transient Resource V2 definition was created"), ResourceDataAsset);
	if (!ResourceDataAsset)
	{
		return false;
	}

	const FSRResourceInstance ResourceInstance = ResourceDataAsset->BuildDefaultInstance();
	TestEqual(TEXT("The current instance schema is used"), ResourceInstance.ResourceSchemaVersion, 3);
	TestEqual(TEXT("Resource Class is copied"), ResourceInstance.ResourceClass, ESRResourceClass::Card);
	TestEqual(TEXT("Family is copied"), ResourceInstance.Family, ESRResourceFamily::Metal);
	TestEqual(TEXT("Seed Energy initializes Current Energy"), ResourceInstance.CurrentEnergy, 5.0);
	TestTrue(TEXT("A new Card captures an immutable Seed Energy snapshot"), ResourceInstance.bHasSeedEnergySnapshot);
	TestEqual(TEXT("The captured Seed matches the authored definition"), ResourceInstance.SeedEnergySnapshot, 5.0);
	TestEqual(TEXT("Legacy Energy mirrors Seed Energy during migration"), ResourceInstance.EnergyValue, 5.0);
	TestEqual(TEXT("Native Spectrum is copied"), ResourceInstance.Spectrum, ESRResourceSpectrum::Red);
	TestEqual(TEXT("Native Grade is copied"), ResourceInstance.Grade, 2);
	TestEqual(TEXT("A new resource has no active Family State"), ResourceInstance.ActiveFamilyStateFlags, 0);
	TestTrue(TEXT("A new Process Tag slot is empty"), ResourceInstance.ProcessTagSlot.TagId.IsNone());
	TestTrue(TEXT("A new Fuel Imprint slot is empty"), ResourceInstance.FuelImprintSlot.ImprintId.IsNone());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourceV1PayloadMigrationTest,
	"StarRovers.ResourceSystem.ResourceV2.LegacyPayloadMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourceV1PayloadMigrationTest::RunTest(const FString& Parameters)
{
	USRResourceDataAsset* ResourceDataAsset = MakeTransientMetalCard();
	TestNotNull(TEXT("A migration definition was created"), ResourceDataAsset);
	if (!ResourceDataAsset)
	{
		return false;
	}

	FSRResourceInstance ResourceInstance;
	ResourceInstance.ResourceDataAsset = ResourceDataAsset;
	ResourceInstance.ResourceId = ResourceDataAsset->ResourceId;
	ResourceInstance.EnergyValue = 42.0;
	ResourceInstance.ProcessCount = 3;
	ResourceInstance.EnergyChangeCount = 2;
	// Simulate tagged-property loading: a newly-added schema field can have its C++
	// default even when the source save itself is version 1.
	ResourceInstance.ResourceSchemaVersion = StarRovers::Resources::CurrentResourceSchemaVersion;
	ResourceInstance.CurrentEnergy = -999.0;

	StarRovers::Resources::UpgradeResourceInstanceToCurrentSchema(ResourceInstance, true);

	TestEqual(TEXT("The payload is promoted to schema 3"), ResourceInstance.ResourceSchemaVersion, 3);
	TestEqual(TEXT("Legacy Energy becomes authoritative Current Energy"), ResourceInstance.CurrentEnergy, 42.0);
	TestEqual(TEXT("Resource Class is hydrated from the definition"), ResourceInstance.ResourceClass, ESRResourceClass::Card);
	TestEqual(TEXT("Family is hydrated from the definition"), ResourceInstance.Family, ESRResourceFamily::Metal);
	TestEqual(TEXT("Spectrum is hydrated from the definition"), ResourceInstance.Spectrum, ESRResourceSpectrum::Red);
	TestEqual(TEXT("Grade is hydrated from the definition"), ResourceInstance.Grade, 2);
	TestEqual(TEXT("Process Count moves into hidden V2 memory"), ResourceInstance.ProcessingMemory.ProcessCount, 3);
	TestEqual(TEXT("Energy Change Count moves into hidden V2 memory"), ResourceInstance.ProcessingMemory.EnergyChangeCount, 2);
	TestTrue(TEXT("Legacy migration captures a stable Seed snapshot"), ResourceInstance.bHasSeedEnergySnapshot);
	TestEqual(TEXT("Legacy migration resolves Seed from the definition"), ResourceInstance.SeedEnergySnapshot, 5.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourceV2SeedSnapshotMigrationTest,
	"StarRovers.ResourceSystem.Phase12.ResourceSeed.Schema2Migration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourceV2SeedSnapshotMigrationTest::RunTest(const FString& Parameters)
{
	FSRResourceInstance ResourceInstance;
	ResourceInstance.ResourceSchemaVersion = StarRovers::Resources::InitialResourceV2SchemaVersion;
	ResourceInstance.ResourceId = FName(TEXT("HeliosIron"));
	ResourceInstance.ResourceClass = ESRResourceClass::Card;
	ResourceInstance.Family = ESRResourceFamily::Metal;
	ResourceInstance.CurrentEnergy = 34.0;
	ResourceInstance.EnergyValue = 34.0;
	ResourceInstance.Spectrum = ESRResourceSpectrum::Red;
	ResourceInstance.Grade = 2;

	StarRovers::Resources::UpgradeResourceInstanceToCurrentSchema(ResourceInstance);

	TestEqual(TEXT("Schema 2 Card migrates without using the Legacy energy bridge"),
		ResourceInstance.ResourceSchemaVersion,
		StarRovers::Resources::CurrentResourceSchemaVersion);
	TestEqual(TEXT("Schema 2 Current Energy remains authoritative"), ResourceInstance.CurrentEnergy, 34.0);
	TestTrue(TEXT("Reference ResourceId supplies the missing Seed snapshot"), ResourceInstance.bHasSeedEnergySnapshot);
	TestEqual(TEXT("The migrated Helios Iron Seed remains five"), ResourceInstance.SeedEnergySnapshot, 5.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourceV2StackEquivalenceTest,
	"StarRovers.ResourceSystem.ResourceV2.StackEquivalence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourceV2StackEquivalenceTest::RunTest(const FString& Parameters)
{
	const FSRResourceInstance Reference = MakeRichResourceInstance();
	FSRResourceInstance Candidate = Reference;
	Candidate.ResourceInstanceId = FName(TEXT("A_Different_Instance_Id"));
	TestTrue(
		TEXT("Instance identity does not prevent otherwise identical resources from stacking"),
		StarRovers::FacilityResources::AreResourceInstancesStackEquivalent(Reference, Candidate));

	Candidate.ProcessingMemory.ConsecutiveSameArchetypeCount += 1;
	TestFalse(
		TEXT("Different hidden processing memory splits a stack"),
		StarRovers::FacilityResources::AreResourceInstancesStackEquivalent(Reference, Candidate));

	Candidate = Reference;
	Candidate.LogisticsMetadata.OriginBodyId = FName(TEXT("Nadir"));
	TestFalse(
		TEXT("Different rule-affecting origin metadata splits a stack"),
		StarRovers::FacilityResources::AreResourceInstancesStackEquivalent(Reference, Candidate));

	Candidate = Reference;
	Candidate.FuelImprintSlot.ImprintId = FName(TEXT("FoundrySeal"));
	TestFalse(
		TEXT("Different Fuel Imprints split a stack"),
		StarRovers::FacilityResources::AreResourceInstancesStackEquivalent(Reference, Candidate));

	Candidate = Reference;
	Candidate.SeedEnergySnapshot = 6.0;
	TestFalse(
		TEXT("Different immutable Seed snapshots split a stack"),
		StarRovers::FacilityResources::AreResourceInstancesStackEquivalent(Reference, Candidate));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourceV2CargoSerializationTest,
	"StarRovers.ResourceSystem.ResourceV2.LogisticsCargoSerialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourceV2CargoSerializationTest::RunTest(const FString& Parameters)
{
	FSRSpaceLogisticsSaveData SourceSaveData;
	FSRSpaceLogisticsHubRouteSaveData& SourceRoute = SourceSaveData.HubRoutes.AddDefaulted_GetRef();
	SourceRoute.RouteId = FName(TEXT("Route_Phase1"));
	SourceRoute.Cargo = MakeRichResourceInstance();
	StarRovers::Resources::PrepareResourceInstanceForSave(SourceRoute.Cargo);

	TArray<uint8> SerializedBytes;
	FMemoryWriter MemoryWriter(SerializedBytes, true);
	FObjectAndNameAsStringProxyArchive WriterArchive(MemoryWriter, false);
	FSRSpaceLogisticsSaveData::StaticStruct()->SerializeItem(WriterArchive, &SourceSaveData, nullptr);
	WriterArchive.Close();

	FSRSpaceLogisticsSaveData LoadedSaveData;
	FMemoryReader MemoryReader(SerializedBytes, true);
	FObjectAndNameAsStringProxyArchive ReaderArchive(MemoryReader, true);
	FSRSpaceLogisticsSaveData::StaticStruct()->SerializeItem(ReaderArchive, &LoadedSaveData, nullptr);
	ReaderArchive.Close();

	TestEqual(TEXT("Serialized save keeps schema version 5"), LoadedSaveData.Version, 5);
	TestEqual(TEXT("One cargo route was restored"), LoadedSaveData.HubRoutes.Num(), 1);
	if (LoadedSaveData.HubRoutes.Num() != 1)
	{
		return false;
	}

	const FSRResourceInstance& LoadedCargo = LoadedSaveData.HubRoutes[0].Cargo;
	TestTrue(
		TEXT("All Legacy and Resource V2 rule fields survive cargo serialization"),
		StarRovers::FacilityResources::AreResourceInstancesStackEquivalent(SourceRoute.Cargo, LoadedCargo));
	TestEqual(TEXT("Cargo stack count survives serialization"), LoadedCargo.StackCount, 3);
	TestTrue(TEXT("Cargo keeps its Seed snapshot flag"), LoadedCargo.bHasSeedEnergySnapshot);
	TestEqual(TEXT("Cargo keeps its immutable Seed value"), LoadedCargo.SeedEnergySnapshot, 5.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourceV2LocationMetadataTest,
	"StarRovers.ResourceSystem.ResourceV2.LocationMetadataTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourceV2LocationMetadataTest::RunTest(const FString& Parameters)
{
	FSRResourceInstance ResourceInstance = MakeRichResourceInstance();
	ResourceInstance.LogisticsMetadata = FSRResourceLogisticsMetadata();
	const double InitialEnergy = ResourceInstance.CurrentEnergy;
	const int32 InitialFamilyStateFlags = ResourceInstance.ActiveFamilyStateFlags;

	StarRovers::Resources::InitializeResourceOrigin(ResourceInstance, FName(TEXT("Cinder")));
	StarRovers::Resources::RecordResourceProcessedOnBody(ResourceInstance, FName(TEXT("Cinder")));
	StarRovers::Resources::RecordResourceTransit(
		ResourceInstance,
		FName(TEXT("Cinder")),
		FName(TEXT("Prism")));

	TestEqual(TEXT("Mining/creation fixes the Origin Body"), ResourceInstance.LogisticsMetadata.OriginBodyId, FName(TEXT("Cinder")));
	TestEqual(TEXT("Local processing records its body"), ResourceInstance.LogisticsMetadata.LastProcessedBodyId, FName(TEXT("Cinder")));
	TestEqual(TEXT("Transit records its source"), ResourceInstance.LogisticsMetadata.LastTransitSourceBodyId, FName(TEXT("Cinder")));
	TestEqual(TEXT("Transit records its destination"), ResourceInstance.LogisticsMetadata.LastTransitDestinationBodyId, FName(TEXT("Prism")));
	TestEqual(TEXT("Transit count increments once per completed leg"), ResourceInstance.LogisticsMetadata.TransitCount, 1);
	TestFalse(TEXT("Transport alone is not processing outside Origin"), ResourceInstance.LogisticsMetadata.bHasBeenProcessedOutsideOrigin);

	StarRovers::Resources::RecordResourceProcessedOnBody(ResourceInstance, FName(TEXT("Prism")));
	TestTrue(TEXT("Processing away from Origin becomes a durable fact"), ResourceInstance.LogisticsMetadata.bHasBeenProcessedOutsideOrigin);
	TestEqual(TEXT("The latest processing body is retained"), ResourceInstance.LogisticsMetadata.LastProcessedBodyId, FName(TEXT("Prism")));
	TestEqual(TEXT("Ordinary transit never changes Energy"), ResourceInstance.CurrentEnergy, InitialEnergy);
	TestEqual(TEXT("Ordinary transit never changes Family State"), ResourceInstance.ActiveFamilyStateFlags, InitialFamilyStateFlags);
	return true;
}

#endif
