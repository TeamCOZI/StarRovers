#if WITH_DEV_AUTOMATION_TESTS

#include "Save/SRRunSaveGame.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"

namespace StarRovers::RunSaveTests
{
	FSRRunModifierEffect MakeEffect(
		const TCHAR* EffectId,
		ESRRunModifierEffectKind EffectKind,
		double Magnitude)
	{
		FSRRunModifierEffect Effect;
		Effect.EffectId = FName(EffectId);
		Effect.EffectKind = EffectKind;
		Effect.Magnitude = Magnitude;
		return Effect;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRRunSaveMemoryRoundTripTest,
	"StarRovers.Save.Unified.MemoryRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRRunSaveMemoryRoundTripTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	USRRunSaveGame* SourceSave = NewObject<USRRunSaveGame>();
	SourceSave->RunData.SaveId = FGuid::NewGuid();
	SourceSave->RunData.Generation.GeneratorActorName = FName(TEXT("SaveRoundTripGenerator"));
	SourceSave->RunData.Generation.RuntimeGenerationSeed = 73021;
	SourceSave->RunData.TimeControl.CurrentCycleIndex = 17;
	SourceSave->RunData.TimeControl.CycleProgressSeconds = 4.25f;
	SourceSave->RunData.RunModifiers.ContextRevision = 9;
	SourceSave->RunData.RunModifiers.NextContextRevision = 10;

	FSRRunCelestialBodySaveData& Body = SourceSave->RunData.CelestialBodies.AddDefaulted_GetRef();
	Body.BodyKey.ActorName = FName(TEXT("SaveRoundTripPlanet"));
	Body.BodyKey.VariableName = TEXT("Save Round Trip Planet");
	Body.BodyKey.BodyCategory = ESRCelestialBodyCategory::Planet;
	Body.BodyKey.GenerationSeed = 4107;
	Body.bHasSurfaceState = true;
	FSRConveyorItem& Item = Body.Conveyors.Items.AddDefaulted_GetRef();
	Item.ResourceInstance.ResourceId = FName(TEXT("PatternCargo"));
	Item.ResourceInstance.StackCount = 3;
	Item.ResourceInstance.Pattern.SetGlyph(0, 0, ESRGlyphType::Metal);
	Item.ResourceInstance.Pattern.SetGlyph(2, 2, ESRGlyphType::Organic);
	Item.ResourceInstance.Pattern.SetGlyph(4, 4, ESRGlyphType::Plasma);
	Item.CurrentLane.Layer = 2;
	Item.CurrentLane.CellId.CellX = 3;
	Item.CurrentLane.CellId.CellY = 7;
	Item.Progress = 0.375f;

	TArray<uint8> Bytes;
	TestTrue(TEXT("The unified USaveGame serializes to memory."), UGameplayStatics::SaveGameToMemory(SourceSave, Bytes));
	TestTrue(TEXT("The serialized payload is not empty."), !Bytes.IsEmpty());
	USRRunSaveGame* LoadedSave = Cast<USRRunSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	TestNotNull(TEXT("The memory payload restores the unified save class."), LoadedSave);
	if (!LoadedSave)
	{
		return false;
	}
	TestEqual(TEXT("Run save version survives serialization."), LoadedSave->RunData.Version, StarRovers::Save::Run::CurrentVersion);
	TestEqual(TEXT("Save identity survives serialization."), LoadedSave->RunData.SaveId, SourceSave->RunData.SaveId);
	TestEqual(TEXT("Topology content version survives serialization."), LoadedSave->RunData.Generation.ContentVersion, StarRovers::Save::Run::CurrentTopologyContentVersion);
	TestEqual(TEXT("Generator identity survives serialization."), LoadedSave->RunData.Generation.GeneratorActorName, FName(TEXT("SaveRoundTripGenerator")));
	TestEqual(TEXT("Runtime generation Seed survives serialization."), LoadedSave->RunData.Generation.RuntimeGenerationSeed, 73021);
	TestEqual(TEXT("Cycle index survives serialization."), LoadedSave->RunData.TimeControl.CurrentCycleIndex, 17);
	TestEqual(TEXT("Context revision survives serialization."), LoadedSave->RunData.RunModifiers.ContextRevision, 9);
	TestEqual(TEXT("Celestial body count survives serialization."), LoadedSave->RunData.CelestialBodies.Num(), 1);
	const FSRConveyorItem& LoadedItem = LoadedSave->RunData.CelestialBodies[0].Conveyors.Items[0];
	TestEqual(TEXT("Pattern cargo stack survives serialization."), LoadedItem.ResourceInstance.StackCount, 3);
	TestTrue(TEXT("All 25 Pattern cells survive serialization."), LoadedItem.ResourceInstance.Pattern == Item.ResourceInstance.Pattern);
	TestTrue(TEXT("In-flight conveyor progress survives serialization."), FMath::IsNearlyEqual(LoadedItem.Progress, 0.375f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRRunGenerationPayloadValidationTest,
	"StarRovers.Save.Unified.GenerationPayloadValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRRunGenerationPayloadValidationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FSRRunGenerationSaveData Payload;
	TestFalse(TEXT("A generation payload without a Generator identity is invalid."), Payload.IsValid());

	Payload.GeneratorActorName = FName(TEXT("RuntimeGenerator"));
	Payload.RuntimeGenerationSeed = 99173;
	TestTrue(TEXT("A current content version, Generator identity, and non-negative Seed form a valid payload."), Payload.IsValid());

	Payload.ContentVersion = StarRovers::Save::Run::CurrentTopologyContentVersion + 1;
	TestFalse(TEXT("A topology content-version mismatch is rejected."), Payload.IsValid());
	Payload.ContentVersion = StarRovers::Save::Run::CurrentTopologyContentVersion;
	Payload.RuntimeGenerationSeed = -1;
	TestFalse(TEXT("A negative runtime generation Seed is rejected."), Payload.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRTimeControlSaveAtomicValidationTest,
	"StarRovers.Save.TimeControl.AtomicValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRTimeControlSaveAtomicValidationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* TestWorld = NewObject<UWorld>(GetTransientPackage());
	USRTimeControlSubsystem* TimeControl = NewObject<USRTimeControlSubsystem>(TestWorld);
	FSRTimeControlSaveData ValidSave;
	ValidSave.TimeScale = 2.0f;
	ValidSave.SecondsPerPeriod = 30.0f;
	ValidSave.CycleProgressSeconds = 12.0f;
	ValidSave.CurrentCycleIndex = 8;
	ValidSave.bSimulationPaused = true;
	TestTrue(TEXT("A valid simulation clock imports."), TimeControl->ImportSaveData(ValidSave));

	FSRTimeControlSaveData InvalidSave = ValidSave;
	InvalidSave.Version = 99;
	InvalidSave.CurrentCycleIndex = 500;
	TestFalse(TEXT("An unsupported clock version is rejected."), TimeControl->ImportSaveData(InvalidSave));
	FSRTimeControlSaveData Snapshot;
	TimeControl->ExportSaveData(Snapshot);
	TestEqual(TEXT("Rejected clock import leaves the Cycle unchanged."), Snapshot.CurrentCycleIndex, 8);
	TestTrue(TEXT("Rejected clock import leaves progress unchanged."), FMath::IsNearlyEqual(Snapshot.CycleProgressSeconds, 12.0f));
	TestTrue(TEXT("Rejected clock import leaves pause state unchanged."), Snapshot.bSimulationPaused);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRRunModifierSaveAtomicValidationTest,
	"StarRovers.Save.RunModifiers.AtomicValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRRunModifierSaveAtomicValidationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace StarRovers::RunSaveTests;
	UWorld* TestWorld = NewObject<UWorld>(GetTransientPackage());
	USRRunModifierSubsystem* RunModifiers = NewObject<USRRunModifierSubsystem>(TestWorld);

	USRTechnologyDataAsset* Technology = NewObject<USRTechnologyDataAsset>();
	Technology->TechnologyId = FName(TEXT("SaveTechnology"));
	Technology->Effects.Add(MakeEffect(TEXT("TechnologySpeed"), ESRRunModifierEffectKind::FacilityProcessTimeMultiplier, 0.9));
	RunModifiers->RegisterTechnologyDataAssets({ Technology });
	TestTrue(TEXT("Test Technology unlocks."), RunModifiers->UnlockTechnology(Technology->TechnologyId));

	USRRunAugmentDataAsset* Augment = NewObject<USRRunAugmentDataAsset>();
	Augment->AugmentId = FName(TEXT("SaveAugment"));
	Augment->MaximumStacks = 3;
	Augment->Effects.Add(MakeEffect(TEXT("AugmentBonus"), ESRRunModifierEffectKind::StellarBonusScoreMultiplier, 1.2));
	RunModifiers->RegisterAugmentDataAssets({ Augment });
	TestTrue(TEXT("First saved Augment stack applies."), RunModifiers->ApplyAugment(Augment->AugmentId));
	TestTrue(TEXT("Second saved Augment stack applies."), RunModifiers->ApplyAugment(Augment->AugmentId));

	USRTrialDataAsset* Trial = NewObject<USRTrialDataAsset>();
	Trial->TrialId = FName(TEXT("SaveTrial"));
	Trial->DurationCycles = 3;
	Trial->Effects.Add(MakeEffect(TEXT("TrialDemand"), ESRRunModifierEffectKind::StellarRequiredScoreMultiplier, 1.1));
	RunModifiers->RegisterTrialDataAssets({ Trial });
	TestTrue(TEXT("Saved Trial activates at an explicit boundary."), RunModifiers->ActivateTrial(Trial->TrialId, 4));

	FSRRunModifierSaveData SavedState;
	RunModifiers->ExportSaveData(SavedState);
	TestTrue(TEXT("A third runtime stack can diverge after the snapshot."), RunModifiers->ApplyAugment(Augment->AugmentId));
	TestTrue(TEXT("The valid snapshot restores the prior modifier state."), RunModifiers->ImportSaveData(SavedState));
	TestEqual(TEXT("Saved Augment stack count is restored exactly."), RunModifiers->GetAugmentStackCount(Augment->AugmentId), 2);
	TestEqual(TEXT("Saved context revision is restored exactly."), RunModifiers->GetRunModifierContext().Revision, SavedState.ContextRevision);

	FSRRunModifierSaveData InvalidState = SavedState;
	const FSRRunModifierAugmentStackSaveData DuplicateStack = InvalidState.AugmentStacks[0];
	InvalidState.AugmentStacks.Add(DuplicateStack);
	TestFalse(TEXT("Duplicate Augment stacks reject the entire import."), RunModifiers->ImportSaveData(InvalidState));
	TestEqual(TEXT("Rejected modifier import leaves stacks intact."), RunModifiers->GetAugmentStackCount(Augment->AugmentId), 2);
	TestEqual(TEXT("Rejected modifier import leaves context revision intact."), RunModifiers->GetRunModifierContext().Revision, SavedState.ContextRevision);
	return true;
}

#endif
