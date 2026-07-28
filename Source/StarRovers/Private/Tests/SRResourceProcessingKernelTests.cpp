#include "Automation/SRResourceInstanceOperations.h"
#include "Automation/SRResourceProcessingKernel.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FSRResourceInstance MakeCard(ESRResourceFamily Family, double CurrentEnergy)
	{
		FSRResourceInstance Resource;
		Resource.ResourceSchemaVersion = StarRovers::Resources::CurrentResourceSchemaVersion;
		Resource.ResourceInstanceId = FName(TEXT("KernelTestInstance"));
		Resource.ResourceId = FName(*FString::Printf(TEXT("KernelCard_%d"), static_cast<int32>(Family)));
		Resource.ResourceClass = ESRResourceClass::Card;
		Resource.Family = Family;
		Resource.Spectrum = ESRResourceSpectrum::Red;
		Resource.Grade = 2;
		Resource.CurrentEnergy = CurrentEnergy;
		Resource.EnergyValue = CurrentEnergy;
		Resource.RemainingProcessLimit = 0;
		Resource.StackCount = 1;
		return Resource;
	}

	FSRResourceProcessSpec MakeProcess(
		FName Archetype,
		double FacilityEnergyDelta,
		ESRResourceProcessTemperatureState Temperature = ESRResourceProcessTemperatureState::Normal,
		ESRResourceFamilyAction FamilyAction = ESRResourceFamilyAction::None,
		FName BodyId = NAME_None)
	{
		FSRResourceProcessSpec Spec;
		Spec.ProcessArchetype = Archetype;
		Spec.FacilityEnergyDelta = FacilityEnergyDelta;
		Spec.Temperature = Temperature;
		Spec.FamilyAction = FamilyAction;
		Spec.ProcessingBodyId = BodyId;
		return Spec;
	}

	bool HasState(const FSRResourceInstance& Resource, ESRResourceFamilyState State)
	{
		return (Resource.ActiveFamilyStateFlags & StarRovers::Resources::GetFamilyStateBit(State)) != 0;
	}

	FSRResourceProcessResult ApplyProcess(
		FSRResourceInstance& Resource,
		const FSRResourceProcessSpec& Spec)
	{
		const FSRResourceProcessResult Result = FSRResourceProcessingKernel::Evaluate(Resource, Spec);
		if (Result.IsSuccess())
		{
			Resource = Result.OutputResource;
		}
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourceKernelAdditiveUnlimitedTest,
	"StarRovers.ResourceSystem.ProcessingKernel.AdditiveAndUnlimited",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourceKernelAdditiveUnlimitedTest::RunTest(const FString& Parameters)
{
	FSRResourceInstance Resource = MakeCard(ESRResourceFamily::Crystal, 5.0);
	for (int32 ProcessIndex = 0; ProcessIndex < 100; ++ProcessIndex)
	{
		const FName Archetype = ProcessIndex % 2 == 0 ? FName(TEXT("Pulse")) : FName(TEXT("Compression"));
		const FSRResourceProcessResult Result = ApplyProcess(
			Resource,
			MakeProcess(Archetype, 1.0, ESRResourceProcessTemperatureState::Normal));
		if (!TestTrue(TEXT("Every process succeeds despite RemainingProcessLimit being zero"), Result.IsSuccess()))
		{
			return false;
		}
		TestEqual(
			TEXT("The trace is strictly additive"),
			Result.OutputEnergy,
			Result.InputEnergy
				+ Result.FacilityEnergyDelta
				+ Result.FamilyEnergyDelta
				+ Result.ProcessTagEnergyDelta
				+ Result.ClampEnergyDelta);
	}

	TestEqual(TEXT("One hundred additive processes accumulate exactly"), Resource.CurrentEnergy, 105.0);
	TestEqual(TEXT("No V2 process consumes the Legacy process limit"), Resource.RemainingProcessLimit, 0);
	TestEqual(TEXT("Diagnostic Process Count records all completed processes"), Resource.ProcessingMemory.ProcessCount, 100);
	TestEqual(TEXT("Temporary Legacy Energy mirrors Current Energy"), Resource.EnergyValue, Resource.CurrentEnergy);
	TestEqual(TEXT("Temporary Legacy Process Count mirrors V2 memory"), Resource.ProcessCount, 100);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourceKernelMetalTest,
	"StarRovers.ResourceSystem.ProcessingKernel.Family.Metal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourceKernelMetalTest::RunTest(const FString& Parameters)
{
	FSRResourceInstance Resource = MakeCard(ESRResourceFamily::Metal, 5.0);
	FSRResourceProcessResult Result = ApplyProcess(
		Resource,
		MakeProcess(TEXT("Forge"), 4.0, ESRResourceProcessTemperatureState::Hot));
	TestTrue(TEXT("Initial Hot Forge succeeds"), Result.IsSuccess());
	TestEqual(TEXT("Initial Hot Forge is base addition only"), Resource.CurrentEnergy, 9.0);
	TestEqual(TEXT("The first Metal operation records one Work Strain"), Resource.ProcessingMemory.GeneralProcessesSinceReset, 1);

	Result = ApplyProcess(Resource, MakeProcess(TEXT("Press"), 3.0, ESRResourceProcessTemperatureState::Cold));
	TestEqual(TEXT("Hot to Cold activates the Tempered bonus on the current process"), Resource.CurrentEnergy, 17.0);
	TestTrue(TEXT("Tempered remains visible on the output"), HasState(Resource, ESRResourceFamilyState::Tempered));
	TestTrue(TEXT("The result exposes positive State activation for future Tags"), Result.bPositiveFamilyStateActivated);
	TestEqual(TEXT("Tempered contributes an additive five"), Result.FamilyEnergyDelta, 5.0);
	TestEqual(TEXT("The second Metal operation records two Work Strain"), Resource.ProcessingMemory.GeneralProcessesSinceReset, 2);

	Result = ApplyProcess(Resource, MakeProcess(TEXT("Forge"), 4.0, ESRResourceProcessTemperatureState::Hot));
	TestEqual(TEXT("The third Metal operation immediately pays Fatigued despite changing Archetype"), Resource.CurrentEnergy, 13.0);
	TestTrue(TEXT("Fatigued is active"), HasState(Resource, ESRResourceFamilyState::Fatigued));
	TestFalse(TEXT("Fatigued blocks a lingering Tempered state"), HasState(Resource, ESRResourceFamilyState::Tempered));
	TestEqual(TEXT("Fatigued contributes additive minus eight"), Result.FamilyEnergyDelta, -8.0);
	TestEqual(TEXT("Work Strain reaches its threshold"), Resource.ProcessingMemory.GeneralProcessesSinceReset, 3);

	Result = ApplyProcess(Resource, MakeProcess(TEXT("Press"), 3.0, ESRResourceProcessTemperatureState::Cold));
	TestEqual(TEXT("Fatigued persists across another Archetype and blocks Tempered"), Resource.CurrentEnergy, 8.0);
	TestTrue(TEXT("Changing Archetype alone cannot clear Fatigued"), HasState(Resource, ESRResourceFamilyState::Fatigued));
	TestFalse(TEXT("A Fatigued Hot-to-Cold transition cannot become Tempered"), HasState(Resource, ESRResourceFamilyState::Tempered));

	Result = ApplyProcess(Resource, MakeProcess(
		TEXT("Anneal"),
		0.0,
		ESRResourceProcessTemperatureState::Normal,
		ESRResourceFamilyAction::Anneal));
	TestEqual(TEXT("Anneal is an explicit zero-Energy recovery"), Resource.CurrentEnergy, 8.0);
	TestFalse(TEXT("Anneal clears Fatigued"), HasState(Resource, ESRResourceFamilyState::Fatigued));
	TestFalse(TEXT("Anneal clears Tempered"), HasState(Resource, ESRResourceFamilyState::Tempered));
	TestTrue(TEXT("Anneal exposes negative State recovery"), Result.bNegativeFamilyStateCleared);
	TestEqual(TEXT("Anneal resets Work Strain"), Resource.ProcessingMemory.GeneralProcessesSinceReset, 0);

	ApplyProcess(Resource, MakeProcess(TEXT("Forge"), 4.0, ESRResourceProcessTemperatureState::Hot));
	ApplyProcess(Resource, MakeProcess(TEXT("Press"), 3.0, ESRResourceProcessTemperatureState::Cold));
	TestEqual(TEXT("A post-Anneal Hot-to-Cold cycle earns Tempered again"), Resource.CurrentEnergy, 20.0);
	TestEqual(TEXT("Post-Anneal cycle rebuilds Work Strain from zero"), Resource.ProcessingMemory.GeneralProcessesSinceReset, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourceKernelCrystalTest,
	"StarRovers.ResourceSystem.ProcessingKernel.Family.Crystal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourceKernelCrystalTest::RunTest(const FString& Parameters)
{
	FSRResourceInstance Resource = MakeCard(ESRResourceFamily::Crystal, 4.0);
	ApplyProcess(Resource, MakeProcess(TEXT("Resonance"), 3.0));
	TestEqual(TEXT("First Crystal process receives base Energy"), Resource.CurrentEnergy, 7.0);
	ApplyProcess(Resource, MakeProcess(TEXT("Resonance"), 3.0));
	TestEqual(TEXT("Second repeat activates Resonant"), Resource.CurrentEnergy, 14.0);
	TestTrue(TEXT("Resonant is active"), HasState(Resource, ESRResourceFamilyState::Resonant));
	ApplyProcess(Resource, MakeProcess(TEXT("Resonance"), 3.0));
	TestEqual(TEXT("Third repeat remains in the profitable Resonant block"), Resource.CurrentEnergy, 21.0);
	ApplyProcess(Resource, MakeProcess(TEXT("Resonance"), 3.0));
	TestEqual(TEXT("Fourth repeat combines Resonant and larger Fractured penalty"), Resource.CurrentEnergy, 18.0);
	TestTrue(TEXT("Resonant can coexist with Fractured"), HasState(Resource, ESRResourceFamilyState::Resonant));
	TestTrue(TEXT("Fractured is active"), HasState(Resource, ESRResourceFamilyState::Fractured));

	const FSRResourceProcessResult Recovery = ApplyProcess(Resource, MakeProcess(TEXT("Facet"), 2.0));
	TestEqual(TEXT("Changing Crystal Archetype recovers before applying the current base gain"), Resource.CurrentEnergy, 20.0);
	TestFalse(TEXT("Resonant clears on Archetype change"), HasState(Resource, ESRResourceFamilyState::Resonant));
	TestFalse(TEXT("Fractured clears on Archetype change"), HasState(Resource, ESRResourceFamilyState::Fractured));
	TestTrue(TEXT("Crystal recovery is exposed to Tag evaluation"), Recovery.bNegativeFamilyStateCleared);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourceKernelOrganicTest,
	"StarRovers.ResourceSystem.ProcessingKernel.Family.Organic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourceKernelOrganicTest::RunTest(const FString& Parameters)
{
	FSRResourceInstance Resource = MakeCard(ESRResourceFamily::Organic, 3.0);
	ApplyProcess(Resource, MakeProcess(TEXT("GrowthVat"), 0.0, ESRResourceProcessTemperatureState::Normal, ESRResourceFamilyAction::Growth));
	TestEqual(TEXT("Growth is an explicit zero-Energy action"), Resource.CurrentEnergy, 3.0);
	TestTrue(TEXT("Growth activates Matured"), HasState(Resource, ESRResourceFamilyState::Matured));

	ApplyProcess(Resource, MakeProcess(TEXT("Loom"), 3.0));
	TestEqual(TEXT("The next general process consumes Matured for plus six"), Resource.CurrentEnergy, 12.0);
	TestFalse(TEXT("Matured is consumed"), HasState(Resource, ESRResourceFamilyState::Matured));
	ApplyProcess(Resource, MakeProcess(TEXT("Press"), 4.0));
	TestEqual(TEXT("Second process without Growth immediately receives Depleted penalty"), Resource.CurrentEnergy, 9.0);
	TestTrue(TEXT("Depleted is active"), HasState(Resource, ESRResourceFamilyState::Depleted));

	const FSRResourceProcessResult GrowthRecovery = ApplyProcess(
		Resource,
		MakeProcess(TEXT("GrowthVat"), 0.0, ESRResourceProcessTemperatureState::Normal, ESRResourceFamilyAction::Growth));
	TestTrue(TEXT("Growth clears Depleted"), GrowthRecovery.bNegativeFamilyStateCleared);
	TestTrue(TEXT("Growth prepares Matured again"), HasState(Resource, ESRResourceFamilyState::Matured));
	ApplyProcess(Resource, MakeProcess(TEXT("Loom"), 3.0));
	TestEqual(TEXT("The recovered Growth cycle is profitable again"), Resource.CurrentEnergy, 18.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourceKernelPlasmaTest,
	"StarRovers.ResourceSystem.ProcessingKernel.Family.Plasma",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourceKernelPlasmaTest::RunTest(const FString& Parameters)
{
	FSRResourceInstance Resource = MakeCard(ESRResourceFamily::Plasma, 6.0);
	ApplyProcess(Resource, MakeProcess(TEXT("Amplifier"), 4.0, ESRResourceProcessTemperatureState::Normal, ESRResourceFamilyAction::Amplification));
	TestEqual(TEXT("First Amplification primes Energized without consuming its bonus"), Resource.CurrentEnergy, 10.0);
	TestTrue(TEXT("Energized is active"), HasState(Resource, ESRResourceFamilyState::Energized));
	ApplyProcess(Resource, MakeProcess(TEXT("Amplifier"), 4.0, ESRResourceProcessTemperatureState::Normal, ESRResourceFamilyAction::Amplification));
	TestEqual(TEXT("Second Amplification receives Energized plus five"), Resource.CurrentEnergy, 19.0);
	ApplyProcess(Resource, MakeProcess(TEXT("Amplifier"), 4.0, ESRResourceProcessTemperatureState::Normal, ESRResourceFamilyAction::Amplification));
	TestEqual(TEXT("Third Amplification combines Energized and larger Overloaded penalty"), Resource.CurrentEnergy, 18.0);
	TestTrue(TEXT("Energized and Overloaded can coexist"), HasState(Resource, ESRResourceFamilyState::Energized));
	TestTrue(TEXT("Overloaded is active"), HasState(Resource, ESRResourceFamilyState::Overloaded));

	const FSRResourceProcessResult Discharge = ApplyProcess(
		Resource,
		MakeProcess(TEXT("Grounding"), 1.0, ESRResourceProcessTemperatureState::Normal, ESRResourceFamilyAction::Discharge));
	TestEqual(TEXT("Discharge clears the cycle before its additive base gain"), Resource.CurrentEnergy, 19.0);
	TestFalse(TEXT("Discharge clears Energized"), HasState(Resource, ESRResourceFamilyState::Energized));
	TestFalse(TEXT("Discharge clears Overloaded"), HasState(Resource, ESRResourceFamilyState::Overloaded));
	TestTrue(TEXT("Discharge exposes negative State recovery"), Discharge.bNegativeFamilyStateCleared);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourceKernelVoidTest,
	"StarRovers.ResourceSystem.ProcessingKernel.Family.Void",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourceKernelVoidTest::RunTest(const FString& Parameters)
{
	FSRResourceInstance Resource = MakeCard(ESRResourceFamily::Void, 2.0);
	FSRResourceProcessResult Result = ApplyProcess(
		Resource,
		MakeProcess(TEXT("NullSink"), -3.0, ESRResourceProcessTemperatureState::Normal, ESRResourceFamilyAction::VoidSacrifice));
	TestEqual(TEXT("Void Sacrifice cannot remove more Energy than is available"), Result.FacilityEnergyDelta, -2.0);
	TestEqual(TEXT("Void Sacrifice clamps at zero before later Tag hooks"), Resource.CurrentEnergy, 0.0);
	TestEqual(TEXT("Actual sacrificed Energy is stored"), Resource.ProcessingMemory.StoredFamilyMagnitude, 2.0);
	TestTrue(TEXT("A nonzero sacrifice activates Echoing"), HasState(Resource, ESRResourceFamilyState::Echoing));

	ApplyProcess(Resource, MakeProcess(TEXT("Echo"), 4.0, ESRResourceProcessTemperatureState::Normal, ESRResourceFamilyAction::EnergyGain));
	TestEqual(TEXT("Echoing returns twice the actual sacrifice"), Resource.CurrentEnergy, 8.0);
	TestFalse(TEXT("Echoing is consumed by the gain"), HasState(Resource, ESRResourceFamilyState::Echoing));
	ApplyProcess(Resource, MakeProcess(TEXT("Echo"), 4.0, ESRResourceProcessTemperatureState::Normal, ESRResourceFamilyAction::EnergyGain));
	TestEqual(TEXT("Second gain without Sacrifice immediately receives Collapsed penalty"), Resource.CurrentEnergy, 4.0);
	TestTrue(TEXT("Collapsed is active"), HasState(Resource, ESRResourceFamilyState::Collapsed));

	Result = ApplyProcess(
		Resource,
		MakeProcess(TEXT("NullSink"), -3.0, ESRResourceProcessTemperatureState::Normal, ESRResourceFamilyAction::VoidSacrifice));
	TestEqual(TEXT("A new Sacrifice records its actual magnitude"), Resource.ProcessingMemory.StoredFamilyMagnitude, 3.0);
	TestFalse(TEXT("Sacrifice clears Collapsed"), HasState(Resource, ESRResourceFamilyState::Collapsed));
	TestTrue(TEXT("Sacrifice exposes negative State recovery"), Result.bNegativeFamilyStateCleared);
	ApplyProcess(Resource, MakeProcess(TEXT("Echo"), 4.0, ESRResourceProcessTemperatureState::Normal, ESRResourceFamilyAction::EnergyGain));
	TestEqual(TEXT("The renewed Echo cycle adds six"), Resource.CurrentEnergy, 11.0);

	FSRResourceInstance CappedEcho = MakeCard(ESRResourceFamily::Void, 20.0);
	ApplyProcess(CappedEcho, MakeProcess(TEXT("DeepSink"), -5.0, ESRResourceProcessTemperatureState::Normal, ESRResourceFamilyAction::VoidSacrifice));
	ApplyProcess(CappedEcho, MakeProcess(TEXT("Echo"), 4.0, ESRResourceProcessTemperatureState::Normal, ESRResourceFamilyAction::EnergyGain));
	TestEqual(TEXT("Echo bonus is capped at eight"), CappedEcho.CurrentEnergy, 27.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourceKernelTransactionalFailureTest,
	"StarRovers.ResourceSystem.ProcessingKernel.TransactionalFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourceKernelTransactionalFailureTest::RunTest(const FString& Parameters)
{
	const FSRResourceInstance Input = MakeCard(ESRResourceFamily::Metal, 5.0);
	FSRResourceProcessResult Result = FSRResourceProcessingKernel::Evaluate(Input, MakeProcess(NAME_None, 4.0));
	TestEqual(TEXT("Missing Archetype is rejected"), Result.Outcome, ESRResourceProcessOutcome::MissingProcessArchetype);
	TestTrue(
		TEXT("A rejected process returns an unchanged output snapshot"),
		StarRovers::Resources::AreResourceV2RuntimeFieldsEquivalent(Input, Result.OutputResource));
	TestEqual(TEXT("Rejected processing does not consume Legacy limit"), Result.OutputResource.RemainingProcessLimit, 0);

	Result = FSRResourceProcessingKernel::Evaluate(
		Input,
		MakeProcess(TEXT("WrongAction"), 0.0, ESRResourceProcessTemperatureState::Normal, ESRResourceFamilyAction::Growth));
	TestEqual(TEXT("A Family-specific action cannot leak into another Family"), Result.Outcome, ESRResourceProcessOutcome::InvalidFamilyAction);

	Result = FSRResourceProcessingKernel::Evaluate(
		Input,
		MakeProcess(TEXT("BadAnneal"), 1.0, ESRResourceProcessTemperatureState::Normal, ESRResourceFamilyAction::Anneal));
	TestEqual(TEXT("Anneal cannot hide an Energy gain"), Result.Outcome, ESRResourceProcessOutcome::InvalidEnergy);

	FSRResourceInstance LegacyInput = Input;
	LegacyInput.ResourceSchemaVersion = StarRovers::Resources::LegacyResourceSchemaVersion;
	Result = FSRResourceProcessingKernel::Evaluate(LegacyInput, MakeProcess(TEXT("Forge"), 4.0));
	TestEqual(TEXT("Migration is an explicit boundary before processing"), Result.Outcome, ESRResourceProcessOutcome::UnsupportedSchema);

	FSRResourceInstance StellarFuel = Input;
	StellarFuel.ResourceClass = ESRResourceClass::StellarFuel;
	StellarFuel.Family = ESRResourceFamily::None;
	Result = FSRResourceProcessingKernel::Evaluate(StellarFuel, MakeProcess(TEXT("Forge"), 4.0));
	TestEqual(TEXT("Stellar Fuel cannot re-enter general processing"), Result.Outcome, ESRResourceProcessOutcome::TerminalResource);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourceKernelDeterministicPreviewTest,
	"StarRovers.ResourceSystem.ProcessingKernel.DeterministicPreview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourceKernelDeterministicPreviewTest::RunTest(const FString& Parameters)
{
	FSRResourceInstance Input = MakeCard(ESRResourceFamily::Metal, 12.0);
	Input.LogisticsMetadata.OriginBodyId = FName(TEXT("Cinder"));
	Input.ProcessingMemory.LastProcessArchetype = FName(TEXT("Forge"));
	Input.ProcessingMemory.LastTemperature = ESRResourceProcessTemperatureState::Hot;
	Input.ProcessingMemory.ConsecutiveSameArchetypeCount = 1;
	Input.ProcessingMemory.GeneralProcessesSinceReset = 1;
	const FSRResourceInstance BeforePreview = Input;
	const FSRResourceProcessSpec Spec = MakeProcess(
		TEXT("CryoPress"),
		3.0,
		ESRResourceProcessTemperatureState::Cold,
		ESRResourceFamilyAction::None,
		TEXT("Prism"));

	const FSRResourceProcessResult First = FSRResourceProcessingKernel::Evaluate(Input, Spec);
	const FSRResourceProcessResult Second = FSRResourceProcessingKernel::Evaluate(Input, Spec);
	TestTrue(TEXT("Both evaluations succeed"), First.IsSuccess() && Second.IsSuccess());
	TestEqual(TEXT("Equivalent inputs produce identical Output Energy"), First.OutputEnergy, Second.OutputEnergy);
	TestEqual(TEXT("Equivalent inputs produce identical transition flags"), First.ActivatedFamilyStateFlags, Second.ActivatedFamilyStateFlags);
	TestTrue(
		TEXT("Equivalent inputs produce identical future-relevant output state"),
		StarRovers::Resources::AreResourceV2RuntimeFieldsEquivalent(First.OutputResource, Second.OutputResource));
	TestTrue(
		TEXT("Preview evaluation never mutates its input"),
		StarRovers::Resources::AreResourceV2RuntimeFieldsEquivalent(Input, BeforePreview));
	TestEqual(TEXT("The reference preview is 12 + 3 + 5"), First.OutputEnergy, 20.0);
	TestEqual(TEXT("Processing body is recorded only in the output"), First.OutputResource.LogisticsMetadata.LastProcessedBodyId, FName(TEXT("Prism")));
	TestTrue(TEXT("Processing outside Origin becomes durable output metadata"), First.OutputResource.LogisticsMetadata.bHasBeenProcessedOutsideOrigin);
	TestTrue(TEXT("The input still has no Last Processed Body"), Input.LogisticsMetadata.LastProcessedBodyId.IsNone());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourceKernelClampAndInvariantTest,
	"StarRovers.ResourceSystem.ProcessingKernel.ClampAndInvariants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourceKernelClampAndInvariantTest::RunTest(const FString& Parameters)
{
	const FSRResourceInstance Input = MakeCard(ESRResourceFamily::Metal, 1.0);
	FSRResourceProcessResult Result = FSRResourceProcessingKernel::Evaluate(
		Input,
		MakeProcess(TEXT("Compression"), -10.0));
	TestTrue(TEXT("A finite negative additive process is valid"), Result.IsSuccess());
	TestEqual(TEXT("The unclamped additive result is exposed"), Result.UnclampedOutputEnergy, -9.0);
	TestEqual(TEXT("Default prototype rules clamp Current Energy at zero"), Result.OutputEnergy, 0.0);
	TestEqual(TEXT("Clamp is represented as an additive correction"), Result.ClampEnergyDelta, 9.0);
	TestTrue(TEXT("Preview can explain that clamping occurred"), Result.bEnergyClamped);

	FSRResourceProcessingRules UnclampedRules;
	UnclampedRules.bClampCurrentEnergyAtZero = false;
	Result = FSRResourceProcessingKernel::Evaluate(
		Input,
		MakeProcess(TEXT("Compression"), -10.0),
		UnclampedRules);
	TestEqual(TEXT("The unresolved negative-Energy design can be prototyped by configuration"), Result.OutputEnergy, -9.0);

	FSRResourceInstance InvalidStates = Input;
	InvalidStates.ActiveFamilyStateFlags = StarRovers::Resources::GetFamilyStateBit(ESRResourceFamilyState::Echoing);
	Result = FSRResourceProcessingKernel::Evaluate(InvalidStates, MakeProcess(TEXT("Forge"), 4.0));
	TestEqual(TEXT("A Metal card cannot carry a Void State"), Result.Outcome, ESRResourceProcessOutcome::InvalidResource);

	FSRResourceInstance FutureSchema = Input;
	FutureSchema.ResourceSchemaVersion = StarRovers::Resources::CurrentResourceSchemaVersion + 1;
	Result = FSRResourceProcessingKernel::Evaluate(FutureSchema, MakeProcess(TEXT("Forge"), 4.0));
	TestEqual(TEXT("A future schema must be handled explicitly before processing"), Result.Outcome, ESRResourceProcessOutcome::UnsupportedSchema);
	return true;
}

#endif
