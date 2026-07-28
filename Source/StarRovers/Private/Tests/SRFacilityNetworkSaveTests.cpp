#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Automation/SRFacilityNetworkComponent.h"
#include "Automation/SRResourceSystemContent.h"
#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Simulation/SRSimulationSettings.h"
#include "Structure/SRStructureDataAsset.h"

namespace StarRovers::FacilityNetworkSaveTests
{
	USRStructureDataAsset* MakeForgeStructure()
	{
		USRFacilityDataAsset* FacilityDataAsset = NewObject<USRFacilityDataAsset>(GetTransientPackage());
		FSRResourceSystemContent::ApplyFacilityPreset(
			*FacilityDataAsset,
			ESRFacilityContentPresetV2::InductionForge);
		USRStructureDataAsset* StructureDataAsset = NewObject<USRStructureDataAsset>(GetTransientPackage());
		StructureDataAsset->StructureId = FName(TEXT("SaveTestForge"));
		StructureDataAsset->FacilityDataAsset = FacilityDataAsset;
		StructureDataAsset->bProcessReady = true;
		StructureDataAsset->bDeliveryReady = true;
		return StructureDataAsset;
	}

	FSRResourceInstance MakeHeliosIron()
	{
		FSRResourceInstance Resource;
		FSRResourceSystemContent::MakeReferenceResourceInstance(
			ESRResourceContentPresetV2::HeliosIron,
			FName(TEXT("Cinder")),
			Resource);
		return Resource;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRFacilityNetworkSaveRoundTripTest,
	"StarRovers.ResourceSystem.Phase14.FacilitySave.InFlightRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRFacilityNetworkSaveRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace StarRovers::FacilityNetworkSaveTests;
	USRSimulationSettings* Settings = GetMutableDefault<USRSimulationSettings>();
	if (!TestNotNull(TEXT("Simulation settings exist"), Settings))
	{
		return false;
	}
	TGuardValue<ESRResourceRulesetVersion> RulesetGuard(
		Settings->ResourceRulesetVersion,
		ESRResourceRulesetVersion::ResourceV2);

	USRStructureDataAsset* StructureDataAsset = MakeForgeStructure();
	USRFacilityNetworkComponent* SourceNetwork = NewObject<USRFacilityNetworkComponent>(GetTransientPackage());
	SourceNetwork->SetFacilityDebugLoggingEnabled(false);
	const FName OccupantId(TEXT("Facility_Save_Forge"));
	TestTrue(TEXT("Source Facility registers"),
		SourceNetwork->RegisterFacility(
			OccupantId,
			StructureDataAsset,
			FSRPlanetSurfaceGridCellId(),
			TArray<FSRPlanetSurfaceGridCellId>({FSRPlanetSurfaceGridCellId()}),
			0));
	TestTrue(TEXT("Source input is accepted"), SourceNetwork->AddInputResource(OccupantId, MakeHeliosIron()));
	TestTrue(TEXT("One debug second starts and advances the Forge"), SourceNetwork->DebugStepFacilities(1.0f, 1));

	FSRFacilityInstance SourceFacility;
	TestTrue(TEXT("Source runtime can be inspected"), SourceNetwork->GetFacilityInstance(OccupantId, SourceFacility));
	TestTrue(TEXT("Source operation is in flight"), SourceFacility.bProcessing);
	TestEqual(TEXT("Source progress is one second"), SourceFacility.ProcessProgressSeconds, 1.0f);
	TestEqual(TEXT("Source duration snapshot is four seconds"), SourceFacility.ResolvedProcessSeconds, 4.0f);

	FSRFacilityNetworkSaveData SourceSaveData;
	SourceNetwork->ExportSaveData(SourceSaveData);
	TestEqual(TEXT("Facility save schema starts at version one"), SourceSaveData.Version, 1);
	TestEqual(TEXT("Exactly one Facility is exported"), SourceSaveData.Facilities.Num(), 1);

	TArray<uint8> SerializedBytes;
	FMemoryWriter MemoryWriter(SerializedBytes, true);
	FObjectAndNameAsStringProxyArchive WriterArchive(MemoryWriter, false);
	FSRFacilityNetworkSaveData::StaticStruct()->SerializeItem(WriterArchive, &SourceSaveData, nullptr);
	WriterArchive.Close();

	FSRFacilityNetworkSaveData LoadedSaveData;
	FMemoryReader MemoryReader(SerializedBytes, true);
	FObjectAndNameAsStringProxyArchive ReaderArchive(MemoryReader, true);
	FSRFacilityNetworkSaveData::StaticStruct()->SerializeItem(ReaderArchive, &LoadedSaveData, nullptr);
	ReaderArchive.Close();
	TestEqual(TEXT("Binary round trip keeps one Facility"), LoadedSaveData.Facilities.Num(), 1);

	USRFacilityNetworkComponent* LoadedNetwork = NewObject<USRFacilityNetworkComponent>(GetTransientPackage());
	LoadedNetwork->SetFacilityDebugLoggingEnabled(false);
	TestTrue(TEXT("A fresh Network imports the serialized DTO transactionally"),
		LoadedNetwork->ImportSaveData(LoadedSaveData));
	FSRFacilityInstance LoadedFacility;
	TestTrue(TEXT("Imported Facility exists"), LoadedNetwork->GetFacilityInstance(OccupantId, LoadedFacility));
	TestTrue(TEXT("In-flight state is restored"), LoadedFacility.bProcessing);
	TestEqual(TEXT("Progress survives Save/Load"), LoadedFacility.ProcessProgressSeconds, 1.0f);
	TestEqual(TEXT("Resolved duration survives Save/Load"), LoadedFacility.ResolvedProcessSeconds, 4.0f);
	TestEqual(TEXT("Exactly one reserved input survives Save/Load"), LoadedFacility.ProcessingInventory.Num(), 1);
	if (!LoadedFacility.ProcessingInventory.IsEmpty())
	{
		TestTrue(TEXT("Reserved Card keeps its Seed snapshot"),
			LoadedFacility.ProcessingInventory[0].bHasSeedEnergySnapshot);
		TestEqual(TEXT("Reserved Card keeps Seed five"),
			LoadedFacility.ProcessingInventory[0].SeedEnergySnapshot,
			5.0);
	}

	FSRFacilityNetworkSaveData CorruptSaveData = LoadedSaveData;
	const FSRFacilityInstanceSaveData DuplicateFacility = CorruptSaveData.Facilities[0];
	CorruptSaveData.Facilities.Add(DuplicateFacility);
	TestFalse(TEXT("Duplicate OccupantIds reject the entire import"),
		LoadedNetwork->ImportSaveData(CorruptSaveData));
	FSRFacilityInstance AfterRejectedImport;
	TestTrue(TEXT("Rejected import leaves the previous runtime intact"),
		LoadedNetwork->GetFacilityInstance(OccupantId, AfterRejectedImport));
	TestEqual(TEXT("Rejected import cannot overwrite progress"),
		AfterRejectedImport.ProcessProgressSeconds,
		1.0f);

	TestTrue(TEXT("Three more seconds complete the restored operation"),
		LoadedNetwork->DebugStepFacilities(3.0f, 1));
	FSRResourceInstance OutputResource;
	TestTrue(TEXT("Restored operation produces a real output"),
		LoadedNetwork->ExtractOutputResource(OccupantId, OutputResource));
	TestEqual(TEXT("Restored Forge output has Energy nine"), OutputResource.CurrentEnergy, 9.0);
	return true;
}

#endif
