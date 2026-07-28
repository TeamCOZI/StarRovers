#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Simulation/SRAugmentPackageContent.h"
#include "Simulation/SRAugmentSubsystem.h"
#include "Simulation/SRResourceV2RunSaveSubsystem.h"
#include "Simulation/SRTimeControlSubsystem.h"
#include "Structure/SRStructureInstanceSaveData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRDepositSaveMigrationTest,
	"StarRovers.ResourceSystem.Phase21.SaveMigration.DepositAmounts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRDepositSaveMigrationTest::RunTest(const FString& Parameters)
{
	FSRResourceDepositSaveData Placeholder;
	Placeholder.OccupantId = TEXT("Deposit.Placeholder");
	Placeholder.TotalAmount = 50000;
	Placeholder.RemainingAmount = 25000;
	FSRResourceDepositSaveData Migrated;
	bool bMigratedPlaceholder = false;
	bool bMigratedInfinite = false;
	FString FailureReason;
	TestTrue(TEXT("A schema-one 50,000 placeholder migrates"),
		FSRResourceDepositSaveMigration::MigrateAmount(
			1,
			Placeholder,
			true,
			120,
			Migrated,
			bMigratedPlaceholder,
			bMigratedInfinite,
			&FailureReason));
	TestTrue(TEXT("Placeholder migration is reported"), bMigratedPlaceholder);
	TestFalse(TEXT("Finite placeholder is not an infinite migration"), bMigratedInfinite);
	TestEqual(TEXT("Catalog total replaces 50,000"), Migrated.TotalAmount, 120);
	TestEqual(TEXT("Half-depleted ratio remains half depleted"), Migrated.RemainingAmount, 60);

	FSRResourceDepositSaveData Infinite = Placeholder;
	Infinite.TotalAmount = 0;
	Infinite.RemainingAmount = 0;
	TestTrue(TEXT("A schema-one implicit infinite V2 deposit migrates"),
		FSRResourceDepositSaveMigration::MigrateAmount(
			1,
			Infinite,
			true,
			180,
			Migrated,
			bMigratedPlaceholder,
			bMigratedInfinite,
			&FailureReason));
	TestTrue(TEXT("Implicit infinite-to-finite migration is reported"), bMigratedInfinite);
	TestEqual(TEXT("V2 utility deposit receives the current authored total"),
		Migrated.TotalAmount, 180);
	TestEqual(TEXT("Legacy infinite starts as a full finite V2 deposit"),
		Migrated.RemainingAmount, 180);

	TestTrue(TEXT("A true Legacy resource keeps explicit infinite persistence"),
		FSRResourceDepositSaveMigration::MigrateAmount(
			1,
			Infinite,
			false,
			0,
			Migrated,
			bMigratedPlaceholder,
			bMigratedInfinite,
			&FailureReason));
	TestEqual(TEXT("Legacy infinite total uses the runtime sentinel"),
		Migrated.TotalAmount, MAX_int32);
	TestEqual(TEXT("Legacy infinite kind remains explicit"),
		Migrated.PersistenceKind,
		ESRResourceDepositPersistenceKind::LegacyInfinite);

	FSRResourceDepositSaveData Emergency;
	Emergency.OccupantId = TEXT("Deposit.Emergency");
	Emergency.PersistenceKind = ESRResourceDepositPersistenceKind::Finite;
	Emergency.TotalAmount = 25;
	Emergency.RemainingAmount = 7;
	TestTrue(TEXT("A current emergency deposit remains exact"),
		FSRResourceDepositSaveMigration::MigrateAmount(
			FSRStructureInstanceManagerSaveData::CurrentVersion,
			Emergency,
			true,
			120,
			Migrated,
			bMigratedPlaceholder,
			bMigratedInfinite,
			&FailureReason));
	TestEqual(TEXT("Emergency total is not silently normalized"), Migrated.TotalAmount, 25);
	TestEqual(TEXT("Emergency depletion is exact"), Migrated.RemainingAmount, 7);

	Emergency.RemainingAmount = 26;
	TestFalse(TEXT("Corrupt finite amounts are rejected"),
		FSRResourceDepositSaveMigration::MigrateAmount(
			FSRStructureInstanceManagerSaveData::CurrentVersion,
			Emergency,
			true,
			120,
			Migrated,
			bMigratedPlaceholder,
			bMigratedInfinite,
			&FailureReason));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRRunSaveCodecTest,
	"StarRovers.ResourceSystem.Phase21.SaveMigration.CheckedCodec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRRunSaveCodecTest::RunTest(const FString& Parameters)
{
	FSRResourceV2RunSaveData Source;
	Source.PrimaryStarBodyId = TEXT("Helios");
	FSRResourceV2BodySaveData& Body = Source.Bodies.AddDefaulted_GetRef();
	Body.BodyId = TEXT("Cinder");
	Body.ActorName = TEXT("BP_Planet_Cinder");
	Body.bHasStructureManager = true;
	FSRPlacedStructureSaveData& Structure =
		Body.StructureManager.PlacedStructures.AddDefaulted_GetRef();
	Structure.OccupantId = TEXT("Structure_Test_1");
	Structure.StructureId = TEXT("TestStructure");

	TArray<uint8> Payload;
	uint32 Checksum = 0;
	FString FailureReason;
	TestTrue(TEXT("Run DTO encodes to a checked payload"),
		FSRResourceV2RunSaveCodec::Encode(
			Source,
			Payload,
			Checksum,
			FailureReason));
	TestTrue(TEXT("Run payload is non-empty"), !Payload.IsEmpty());

	FSRResourceV2RunSaveData Decoded;
	TestTrue(TEXT("Checked payload decodes"),
		FSRResourceV2RunSaveCodec::Decode(
			Payload,
			Checksum,
			Decoded,
			FailureReason));
	TestEqual(TEXT("Parent schema survives"), Decoded.Version, Source.Version);
	TestEqual(TEXT("Primary Star identity survives"),
		Decoded.PrimaryStarBodyId, Source.PrimaryStarBodyId);
	TestEqual(TEXT("Body topology survives"), Decoded.Bodies.Num(), 1);
	if (!Decoded.Bodies.IsEmpty())
	{
		TestEqual(TEXT("Structure topology survives"),
			Decoded.Bodies[0].StructureManager.PlacedStructures.Num(), 1);
	}

	Payload[Payload.Num() / 2] ^= 0x5A;
	TestFalse(TEXT("A one-byte payload mutation is rejected before import"),
		FSRResourceV2RunSaveCodec::Decode(
			Payload,
			Checksum,
			Decoded,
			FailureReason));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRAugmentProgressSaveTest,
	"StarRovers.ResourceSystem.Phase21.SaveMigration.AugmentProgress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRAugmentProgressSaveTest::RunTest(const FString& Parameters)
{
	TArray<FSRAugmentPackageDefinitionV2> Definitions;
	FSRAugmentPackageContentV2::GetAllDefinitions(Definitions);
	if (!TestTrue(TEXT("Augment catalog has at least two Packages"), Definitions.Num() >= 2))
	{
		return false;
	}

	FSRAugmentSaveData Source;
	Source.PreviousOfferPackageIdsV2.Add(Definitions[1].PackageId);
	FSRAugmentChoiceSaveData& Pending = Source.CurrentChoices.AddDefaulted_GetRef();
	Pending.ChoiceKind = ESRAugmentChoiceKind::ResourceV2Package;
	Pending.PackageId = Definitions[0].PackageId;
	Pending.StructureId = Definitions[0].PackageId;
	Pending.OfferRole = ESRAugmentOfferRoleV2::Immediate;
	Source.CurrentChoiceCycleIndex = 3;
	Source.bPausedSimulationForCurrentChoice = true;

	USRAugmentSubsystem* Augments = NewObject<USRAugmentSubsystem>();
	FString FailureReason;
	TestTrue(TEXT("Augment progress imports transactionally"),
		Augments->ImportSaveData(Source, FailureReason));
	FSRAugmentSaveData RoundTrip;
	Augments->ExportSaveData(RoundTrip);
	TestEqual(TEXT("Pending choice cycle survives"),
		RoundTrip.CurrentChoiceCycleIndex, 3);
	TestEqual(TEXT("Pending choice survives"), RoundTrip.CurrentChoices.Num(), 1);
	TestEqual(TEXT("Previous Offer diversity memory survives"),
		RoundTrip.PreviousOfferPackageIdsV2.Num(), 1);

	FSRAugmentSaveData Corrupt = Source;
	Corrupt.PreviousOfferPackageIdsV2.Add(Definitions[1].PackageId);
	TestFalse(TEXT("Duplicate diversity memory rejects the entire import"),
		Augments->ImportSaveData(Corrupt, FailureReason));
	FSRAugmentSaveData AfterRejected;
	Augments->ExportSaveData(AfterRejected);
	TestEqual(TEXT("Rejected import leaves pending choice intact"),
		AfterRejected.CurrentChoiceCycleIndex, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRTimeControlSaveTest,
	"StarRovers.ResourceSystem.Phase21.SaveMigration.TimeControl",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRTimeControlSaveTest::RunTest(const FString& Parameters)
{
	FSRTimeControlSaveData Source;
	Source.TimeScale = 4.0f;
	Source.SecondsPerPeriod = 20.0f;
	Source.CycleProgressSeconds = 7.5f;
	Source.CurrentCycleIndex = 12;
	Source.bSimulationPaused = true;
	USRTimeControlSubsystem* TimeControl = NewObject<USRTimeControlSubsystem>();
	FString FailureReason;
	TestTrue(TEXT("Time Control clock imports"),
		TimeControl->ImportSaveData(Source, FailureReason));
	FSRTimeControlSaveData RoundTrip;
	TimeControl->ExportSaveData(RoundTrip);
	TestEqual(TEXT("Cycle index survives"), RoundTrip.CurrentCycleIndex, 12);
	TestEqual(TEXT("Fractional cycle boundary survives"),
		RoundTrip.CycleProgressSeconds, 7.5f);
	TestEqual(TEXT("Selected speed survives while paused"), RoundTrip.TimeScale, 4.0f);
	TestTrue(TEXT("Pause survives"), RoundTrip.bSimulationPaused);

	Source.CycleProgressSeconds = Source.SecondsPerPeriod;
	TestTrue(TEXT("An exact pending cycle boundary remains loadable"),
		TimeControl->ImportSaveData(Source, FailureReason));

	Source.CycleProgressSeconds = 21.0f;
	TestFalse(TEXT("Out-of-period clock progress is rejected"),
		TimeControl->ImportSaveData(Source, FailureReason));
	return true;
}

#endif
