#if WITH_DEV_AUTOMATION_TESTS

#include "Pattern/SRPatternEnvironmentResolver.h"

#include "Misc/AutomationTest.h"

namespace StarRovers::PatternEnvironmentTests
{
	FSRPatternEnvironmentEffectSpec MakeMovementEffect(
		ESRPatternEnvironmentEffectKind EffectKind,
		ESRPatternDirection Direction)
	{
		FSRPatternEnvironmentEffectSpec EffectSpec;
		EffectSpec.EffectKind = EffectKind;
		EffectSpec.Direction = Direction;
		return EffectSpec;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRPatternEnvironmentNeutralAndPullTest,
	"StarRovers.Pattern.Environment.NeutralAndPull",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRPatternEnvironmentNeutralAndPullTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FSRPattern InputPattern;
	InputPattern.SetGlyph(0, 0, ESRGlyphType::Metal);
	InputPattern.SetGlyph(0, 2, ESRGlyphType::Crystal);
	const FSRPatternEnvironmentSpec NeutralEnvironment;
	const FSRPatternEnvironmentResolveResult NeutralResult = FSRPatternEnvironmentResolver::Resolve(
		InputPattern,
		NeutralEnvironment);
	TestTrue(TEXT("The default Neutral environment resolves."), NeutralResult.bSucceeded);
	TestTrue(TEXT("A Neutral environment preserves every Pattern cell."), NeutralResult.OutputPattern == InputPattern);
	TestTrue(TEXT("A Neutral environment emits no movement trace."), NeutralResult.TraceEvents.IsEmpty());

	FSRPatternEnvironmentSpec GravityEnvironment;
	GravityEnvironment.EnvironmentId = FName(TEXT("HighGravity"));
	GravityEnvironment.Effects.Add(StarRovers::PatternEnvironmentTests::MakeMovementEffect(
		ESRPatternEnvironmentEffectKind::DirectionalPull,
		ESRPatternDirection::Down));
	const FSRPatternEnvironmentResolveResult GravityResult = FSRPatternEnvironmentResolver::Resolve(
		InputPattern,
		GravityEnvironment);
	TestTrue(TEXT("A Directional Pull environment resolves."), GravityResult.bSucceeded);
	TestTrue(TEXT("Metal receives one downward movement input."), GravityResult.OutputPattern.GetGlyph(1, 0) == ESRGlyphType::Metal);
	TestTrue(TEXT("Crystal keeps its glyph rule and slides to the directional edge."), GravityResult.OutputPattern.GetGlyph(4, 2) == ESRGlyphType::Crystal);
	for (const FSRPatternTraceEvent& TraceEvent : GravityResult.TraceEvents)
	{
		TestEqual(TEXT("Environment traces identify their source effect."), TraceEvent.EnvironmentEffectIndex, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRPatternEnvironmentDriftAndBloomTest,
	"StarRovers.Pattern.Environment.DriftAndBloom",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRPatternEnvironmentDriftAndBloomTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FSRPattern DriftInput;
	DriftInput.SetGlyph(2, 0, ESRGlyphType::Metal);
	DriftInput.SetGlyph(0, 0, ESRGlyphType::Organic);
	FSRPatternEnvironmentSpec DriftEnvironment;
	DriftEnvironment.EnvironmentId = FName(TEXT("MagneticGale"));
	FSRPatternEnvironmentEffectSpec DriftEffect = StarRovers::PatternEnvironmentTests::MakeMovementEffect(
		ESRPatternEnvironmentEffectKind::ContinuousDrift,
		ESRPatternDirection::Right);
	DriftEffect.AffectedGlyph = ESRGlyphType::Metal;
	DriftEffect.MaxDriftSteps = 4;
	DriftEnvironment.Effects.Add(DriftEffect);
	const FSRPatternEnvironmentResolveResult DriftResult = FSRPatternEnvironmentResolver::Resolve(
		DriftInput,
		DriftEnvironment);
	TestTrue(TEXT("Continuous Drift resolves."), DriftResult.bSucceeded);
	TestTrue(TEXT("Continuous Drift repeatedly moves the selected glyph to the edge."), DriftResult.OutputPattern.GetGlyph(2, 4) == ESRGlyphType::Metal);
	TestTrue(TEXT("A glyph outside the environment filter remains in place."), DriftResult.OutputPattern.GetGlyph(0, 0) == ESRGlyphType::Organic);

	FSRPattern BloomInput;
	BloomInput.SetGlyph(2, 2, ESRGlyphType::Organic);
	FSRPatternEnvironmentSpec BloomEnvironment;
	BloomEnvironment.EnvironmentId = FName(TEXT("LivingWorld"));
	FSRPatternEnvironmentEffectSpec BloomEffect;
	BloomEffect.EffectKind = ESRPatternEnvironmentEffectKind::OrganicBloom;
	BloomEffect.Direction = ESRPatternDirection::Down;
	BloomEffect.OrganicGrowthsPerComponent = 1;
	BloomEnvironment.Effects.Add(BloomEffect);
	const FSRPatternEnvironmentResolveResult BloomResult = FSRPatternEnvironmentResolver::Resolve(
		BloomInput,
		BloomEnvironment);
	TestTrue(TEXT("Organic Bloom resolves."), BloomResult.bSucceeded);
	TestEqual(TEXT("Organic Bloom grows once without moving the source glyph."), BloomResult.OutputPattern.CountGlyph(ESRGlyphType::Organic), 2);
	TestTrue(TEXT("Organic Bloom keeps the existing component anchor."), BloomResult.OutputPattern.GetGlyph(2, 2) == ESRGlyphType::Organic);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRPatternEnvironmentAtomicValidationTest,
	"StarRovers.Pattern.Environment.AtomicValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRPatternEnvironmentAtomicValidationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FSRPattern InputPattern;
	InputPattern.SetGlyph(2, 2, ESRGlyphType::Metal);
	FSRPatternEnvironmentSpec InvalidEnvironment;
	InvalidEnvironment.EnvironmentId = FName(TEXT("TooManyEffects"));
	const FSRPatternEnvironmentEffectSpec EffectSpec =
		StarRovers::PatternEnvironmentTests::MakeMovementEffect(
			ESRPatternEnvironmentEffectKind::DirectionalPull,
			ESRPatternDirection::Down);
	for (int32 EffectIndex = 0; EffectIndex <= FSRPatternEnvironmentResolver::MaxEnvironmentEffects; ++EffectIndex)
	{
		InvalidEnvironment.Effects.Add(EffectSpec);
	}

	const FSRPatternEnvironmentResolveResult Result = FSRPatternEnvironmentResolver::Resolve(
		InputPattern,
		InvalidEnvironment);
	TestFalse(TEXT("An invalid environment fails."), Result.bSucceeded);
	TestTrue(TEXT("An invalid environment reports its validation failure."), Result.Failure == ESRPatternEnvironmentResolveFailure::InvalidEnvironmentSpec);
	TestTrue(TEXT("An invalid environment emits no partial trace."), Result.TraceEvents.IsEmpty());
	TestTrue(TEXT("A failed environment does not expose a partial output Pattern."), Result.OutputPattern.IsEmpty());
	return true;
}

#endif
