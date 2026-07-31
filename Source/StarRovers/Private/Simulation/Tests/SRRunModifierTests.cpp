#if WITH_DEV_AUTOMATION_TESTS

#include "Pattern/SRStellarPatternContract.h"
#include "Simulation/SRRunModifierDataAssets.h"
#include "Simulation/SRRunModifierSubsystem.h"
#include "Simulation/SRRunModifierTypes.h"

#include "Engine/World.h"
#include "Misc/AutomationTest.h"

namespace StarRovers::RunModifierTests
{
	FSRRunModifierEffect MakeEffect(
		const TCHAR* EffectId,
		ESRRunModifierEffectKind EffectKind,
		double Magnitude,
		ESRRunModifierFacilityScope FacilityScope = ESRRunModifierFacilityScope::Any,
		ESRGlyphType AffectedGlyph = ESRGlyphType::Empty)
	{
		FSRRunModifierEffect Effect;
		Effect.EffectId = FName(EffectId);
		Effect.EffectKind = EffectKind;
		Effect.Magnitude = Magnitude;
		Effect.FacilityScope = FacilityScope;
		Effect.AffectedGlyph = AffectedGlyph;
		return Effect;
	}

	FSRRunModifierSource MakeSource(
		const TCHAR* SourceId,
		ESRRunModifierSourceKind SourceKind,
		int32 StackCount,
		std::initializer_list<FSRRunModifierEffect> Effects)
	{
		FSRRunModifierSource Source;
		Source.SourceId = FName(SourceId);
		Source.SourceKind = SourceKind;
		Source.StackCount = StackCount;
		for (const FSRRunModifierEffect& Effect : Effects)
		{
			Source.Effects.Add(Effect);
		}
		return Source;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRRunModifierCanonicalResolutionTest,
	"StarRovers.RunModifiers.CanonicalResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRRunModifierCanonicalResolutionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace StarRovers::RunModifierTests;

	FSRRunModifierSource Technology = MakeSource(
		TEXT("FastMetalTech"),
		ESRRunModifierSourceKind::Technology,
		1,
		{ MakeEffect(TEXT("MetalTransformTime"), ESRRunModifierEffectKind::FacilityProcessTimeMultiplier, 0.8, ESRRunModifierFacilityScope::Transform, ESRGlyphType::Metal) });
	FSRRunModifierSource Augment = MakeSource(
		TEXT("OrganicDrive"),
		ESRRunModifierSourceKind::Augment,
		2,
		{
			MakeEffect(TEXT("AllTransformTime"), ESRRunModifierEffectKind::FacilityProcessTimeMultiplier, 0.9, ESRRunModifierFacilityScope::Transform),
			MakeEffect(TEXT("OrganicGrowth"), ESRRunModifierEffectKind::TransformOrganicGrowthDelta, 1.0, ESRRunModifierFacilityScope::Transform),
		});
	FSRRunModifierSource Trial = MakeSource(
		TEXT("ViolentOrbit"),
		ESRRunModifierSourceKind::Trial,
		1,
		{
			MakeEffect(TEXT("Environment"), ESRRunModifierEffectKind::EnvironmentIntensityDelta, 2.0),
			MakeEffect(TEXT("Demand"), ESRRunModifierEffectKind::StellarRequiredScoreMultiplier, 1.5),
		});

	FSRRunModifierContext Context;
	FString FailureReason;
	TestTrue(TEXT("A valid context builds from reverse registration order."),
		FSRRunModifierResolver::BuildContext({ Trial, Augment, Technology }, 7, Context, FailureReason));
	TestEqual(TEXT("The context retains its requested revision."), Context.Revision, 7);
	TestEqual(TEXT("Every active source remains present."), Context.ActiveSources.Num(), 3);
	if (Context.ActiveSources.Num() == 3)
	{
		TestEqual(TEXT("Technology is canonical first."), Context.ActiveSources[0].SourceId, FName(TEXT("FastMetalTech")));
		TestEqual(TEXT("Augment is canonical second."), Context.ActiveSources[1].SourceId, FName(TEXT("OrganicDrive")));
		TestEqual(TEXT("Trial is canonical third."), Context.ActiveSources[2].SourceId, FName(TEXT("ViolentOrbit")));
	}

	FSRRunModifierQuery MetalTransformQuery;
	MetalTransformQuery.FacilityScope = ESRRunModifierFacilityScope::Transform;
	MetalTransformQuery.DominantGlyph = ESRGlyphType::Metal;
	const FSRResolvedRunModifiers MetalModifiers = FSRRunModifierResolver::Resolve(Context, MetalTransformQuery);
	TestTrue(TEXT("Matching multiplicative effects stack deterministically."),
		FMath::IsNearlyEqual(MetalModifiers.FacilityProcessTimeMultiplier, 0.8 * 0.9 * 0.9));
	TestEqual(TEXT("Stacked integer deltas add once per stack."), MetalModifiers.TransformOrganicGrowthDelta, 2);
	TestEqual(TEXT("Wildcard Trial environment pressure applies."), MetalModifiers.EnvironmentIntensityDelta, 2);
	TestTrue(TEXT("Stellar demand multiplier shares the same context."),
		FMath::IsNearlyEqual(MetalModifiers.StellarRequiredScoreMultiplier, 1.5));

	MetalTransformQuery.DominantGlyph = ESRGlyphType::Organic;
	const FSRResolvedRunModifiers OrganicModifiers = FSRRunModifierResolver::Resolve(Context, MetalTransformQuery);
	TestTrue(TEXT("A glyph-specific Technology does not affect another dominant glyph."),
		FMath::IsNearlyEqual(OrganicModifiers.FacilityProcessTimeMultiplier, 0.9 * 0.9));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRRunModifierBoundsTest,
	"StarRovers.RunModifiers.Bounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRRunModifierBoundsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace StarRovers::RunModifierTests;
	const FSRRunModifierSource Source = MakeSource(
		TEXT("ExtremeStack"),
		ESRRunModifierSourceKind::Augment,
		FSRRunModifierResolver::MaximumSourceStacks,
		{
			MakeEffect(TEXT("Fast"), ESRRunModifierEffectKind::FacilityProcessTimeMultiplier, 0.01),
			MakeEffect(TEXT("Growth"), ESRRunModifierEffectKind::TransformOrganicGrowthDelta, 4.0),
			MakeEffect(TEXT("SlowTravel"), ESRRunModifierEffectKind::LogisticsTravelTimeMultiplier, 10.0),
		});
	FSRRunModifierContext Context;
	FString FailureReason;
	TestTrue(TEXT("A maximum legal stack builds."), FSRRunModifierResolver::BuildContext({ Source }, 1, Context, FailureReason));
	const FSRResolvedRunModifiers Modifiers = FSRRunModifierResolver::Resolve(Context);
	TestTrue(TEXT("Facility speed never escapes its lower balance bound."),
		FMath::IsNearlyEqual(Modifiers.FacilityProcessTimeMultiplier, FSRRunModifierResolver::MinimumGeneralMultiplier));
	TestEqual(TEXT("Discrete Pattern deltas stay within board bounds."), Modifiers.TransformOrganicGrowthDelta, FSRRunModifierResolver::MaximumDelta);
	TestTrue(TEXT("Travel time never escapes its upper balance bound."),
		FMath::IsNearlyEqual(Modifiers.LogisticsTravelTimeMultiplier, FSRRunModifierResolver::MaximumLogisticsTravelTimeMultiplier));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRRunModifierStellarContractProjectionTest,
	"StarRovers.RunModifiers.StellarContractProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRRunModifierStellarContractProjectionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FSRStellarPatternContract Contract;
	Contract.ContractId = FName(TEXT("ModifierContract"));
	Contract.RequiredPattern.Reset();
	Contract.RequiredPattern.SetGlyph(2, 2, ESRGlyphType::Metal);
	Contract.RequiredMask.Reset(false);
	Contract.RequiredMask.SetCellActive(2, 2, true);
	Contract.BaseScorePerPattern = 10;
	Contract.RequiredScorePerCycle = 20;
	Contract.RequiredScoreGrowthPerCycle = 0;
	Contract.StellarHealthMaximum = 100.0;
	Contract.StartingStellarHealth = 100.0;
	Contract.InitialStellarHealthDecreasePerSecond = 2.0;
	Contract.StellarHealthDecreaseMultiplierPerPeriod = 1.5;
	Contract.StellarHealthRestoredPerPatternScore = 1.0;
	Contract.BonusRules.Reset();

	FSRPatternHandRule Hand;
	Hand.RuleId = FName(TEXT("CenterMetal"));
	Hand.RuleKind = ESRPatternHandRuleKind::SameGlyphShape;
	Hand.ShapeMask.Reset(false);
	Hand.ShapeMask.SetCellActive(2, 2, true);
	Hand.ShapeMask.SetCellActive(2, 3, true);
	Hand.TransformPolicy = ESRPatternHandTransformPolicy::Fixed;
	Hand.BonusScore = 5;
	Contract.BonusRules.Add(Hand);

	FSRStellarPatternContractModifiers Modifiers;
	Modifiers.BaseScoreMultiplier = 1.5;
	Modifiers.BonusScoreMultiplier = 2.0;
	Modifiers.RequiredScoreMultiplier = 1.5;
	Modifiers.HealthDamageMultiplier = 2.0;
	Modifiers.HealthRecoveryMultiplier = 0.5;

	FSRPattern Pattern;
	Pattern.SetGlyph(2, 2, ESRGlyphType::Metal);
	Pattern.SetGlyph(2, 3, ESRGlyphType::Metal);
	const FSRStellarPatternScoreResult Score = FSRStellarPatternContractResolver::ScorePattern(Pattern, 1, Contract, Modifiers);
	TestEqual(TEXT("The base score projection is rounded deterministically."), Score.BaseScorePerPattern, 15);
	TestEqual(TEXT("The bonus score projection uses its independent channel."), Score.BonusScorePerPattern, static_cast<int64>(10));
	TestEqual(TEXT("The projected total score uses both channels."), Score.TotalScore, static_cast<int64>(25));
	TestEqual(TEXT("The projected Cycle requirement uses the same resolver."),
		FSRStellarPatternContractResolver::GetRequiredScoreForCycle(Contract, 0, Modifiers),
		static_cast<int64>(30));

	const FSRStellarContractCycleSettlement Settlement =
		FSRStellarPatternContractResolver::SettleCycle(Contract, 0, 30, Modifiers);
	TestTrue(TEXT("The score target remains independent from the health clock."), Settlement.bRequirementMet);
	TestTrue(
		TEXT("Health-damage modifiers scale the compounded per-second decrease."),
		FMath::IsNearlyEqual(
			FSRStellarPatternContractResolver::GetStellarHealthDecreasePerSecondForPeriod(Contract, 2, Modifiers),
			9.0));
	TestTrue(
		TEXT("Health-recovery modifiers scale accepted Pattern score restoration."),
		FMath::IsNearlyEqual(
			FSRStellarPatternContractResolver::GetStellarHealthRestorationForScore(Contract, 40, Modifiers),
			20.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRRunModifierProgressionAuthorityTest,
	"StarRovers.RunModifiers.ProgressionAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRRunModifierProgressionAuthorityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace StarRovers::RunModifierTests;
	UWorld* TestWorld = NewObject<UWorld>(GetTransientPackage());
	USRRunModifierSubsystem* Subsystem = NewObject<USRRunModifierSubsystem>(TestWorld);

	USRTechnologyDataAsset* FoundationTechnology = NewObject<USRTechnologyDataAsset>();
	FoundationTechnology->TechnologyId = FName(TEXT("PatternFoundation"));
	FoundationTechnology->UnlockedStructureIds.Add(FName(TEXT("TransformFacility")));
	FoundationTechnology->Effects.Add(MakeEffect(
		TEXT("FoundationSpeed"),
		ESRRunModifierEffectKind::FacilityProcessTimeMultiplier,
		0.9,
		ESRRunModifierFacilityScope::Transform));
	USRTechnologyDataAsset* AdvancedTechnology = NewObject<USRTechnologyDataAsset>();
	AdvancedTechnology->TechnologyId = FName(TEXT("PatternSynthesis"));
	AdvancedTechnology->PrerequisiteTechnologyIds.Add(FoundationTechnology->TechnologyId);
	AdvancedTechnology->UnlockedStructureIds.Add(FName(TEXT("SynthesisFacility")));
	TArray<USRTechnologyDataAsset*> Technologies = { AdvancedTechnology, FoundationTechnology };
	Subsystem->RegisterTechnologyDataAssets(Technologies);
	TestFalse(TEXT("A Technology cannot bypass its prerequisite."), Subsystem->UnlockTechnology(AdvancedTechnology->TechnologyId));
	TestTrue(TEXT("The foundation Technology unlocks."), Subsystem->UnlockTechnology(FoundationTechnology->TechnologyId));
	TestTrue(TEXT("The prerequisite now permits the advanced Technology."), Subsystem->UnlockTechnology(AdvancedTechnology->TechnologyId));
	TestTrue(TEXT("Technology is the authority for guaranteed facility access."),
		Subsystem->IsStructureUnlockedByTechnology(FName(TEXT("TransformFacility")))
		&& Subsystem->IsStructureUnlockedByTechnology(FName(TEXT("SynthesisFacility"))));

	USRRunAugmentDataAsset* Augment = NewObject<USRRunAugmentDataAsset>();
	Augment->AugmentId = FName(TEXT("CompactOrganicLine"));
	Augment->MaximumStacks = 2;
	Augment->Effects.Add(MakeEffect(
		TEXT("CompactGrowth"),
		ESRRunModifierEffectKind::TransformOrganicGrowthDelta,
		1.0,
		ESRRunModifierFacilityScope::Transform));
	TArray<USRRunAugmentDataAsset*> Augments = { Augment };
	Subsystem->RegisterAugmentDataAssets(Augments);
	TestTrue(TEXT("A true Augment applies its first stack."), Subsystem->ApplyAugment(Augment->AugmentId));
	TestTrue(TEXT("A stackable Augment applies its second stack."), Subsystem->ApplyAugment(Augment->AugmentId));
	TestFalse(TEXT("An Augment cannot exceed its authored stack cap."), Subsystem->ApplyAugment(Augment->AugmentId));
	TestEqual(TEXT("The Augment stack count is authoritative."), Subsystem->GetAugmentStackCount(Augment->AugmentId), 2);

	USRTrialDataAsset* Trial = NewObject<USRTrialDataAsset>();
	Trial->TrialId = FName(TEXT("MagneticStorm"));
	Trial->DurationCycles = 2;
	Trial->Effects.Add(MakeEffect(
		TEXT("StormIntensity"),
		ESRRunModifierEffectKind::EnvironmentIntensityDelta,
		1.0));
	TArray<USRTrialDataAsset*> Trials = { Trial };
	Subsystem->RegisterTrialDataAssets(Trials);
	TestTrue(TEXT("A registered Trial activates at an explicit Cycle boundary."), Subsystem->ActivateTrial(Trial->TrialId, 5));
	const TArray<FSRActiveTrialState> ActiveTrials = Subsystem->GetActiveTrials();
	TestEqual(TEXT("Exactly one Trial is active."), ActiveTrials.Num(), 1);
	if (ActiveTrials.Num() == 1)
	{
		TestEqual(TEXT("Trial start is recorded."), ActiveTrials[0].StartCycleIndex, 5);
		TestEqual(TEXT("Trial expiry is an exclusive deterministic Cycle boundary."), ActiveTrials[0].EndCycleIndexExclusive, 7);
	}

	const FSRRunModifierContext Context = Subsystem->GetRunModifierContext();
	TestEqual(TEXT("Technology, stacked Augment, and Trial share one context."), Context.ActiveSources.Num(), 3);
	TestTrue(TEXT("Deactivating the Trial removes it from the shared context."), Subsystem->DeactivateTrial(Trial->TrialId));
	TestEqual(TEXT("Only Technology and Augment remain after Trial deactivation."),
		Subsystem->GetRunModifierContext().ActiveSources.Num(),
		2);
	return true;
}

#endif
