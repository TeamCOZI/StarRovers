#if WITH_DEV_AUTOMATION_TESTS

#include "../SRFacilityOutputResourceBuilder.h"
#include "../SRFacilityProcessingRuleEvaluator.h"
#include "../SRFacilityProcessingStepExecutor.h"
#include "Automation/SRFacilityDataAsset.h"
#include "Automation/SRResourceDataAsset.h"

#include "Misc/AutomationTest.h"

namespace StarRovers::FacilityPatternOperatorTests
{
	FSRResourceInstance MakeResource(
		USRResourceDataAsset* ResourceDataAsset,
		FName InstanceId,
		FName SourceId,
		const FSRPattern& Pattern)
	{
		FSRResourceInstance Resource = ResourceDataAsset->BuildInstanceFromPattern(Pattern, 17, SourceId);
		Resource.ResourceInstanceId = InstanceId;
		return Resource;
	}

	USRFacilityDataAsset* MakeFacility(ESRFacilityOperationKind OperationKind)
	{
		USRFacilityDataAsset* FacilityDataAsset = NewObject<USRFacilityDataAsset>();
		FacilityDataAsset->OperationKind = OperationKind;
		return FacilityDataAsset;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRFacilityPatternTransformPipelineTest,
	"StarRovers.Facility.Pattern.TransformPipeline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRFacilityPatternTransformPipelineTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	USRResourceDataAsset* ResourceDataAsset = NewObject<USRResourceDataAsset>();
	ResourceDataAsset->ResourceId = FName(TEXT("StarIron"));
	FSRPattern InputPattern;
	InputPattern.SetGlyph(2, 0, ESRGlyphType::Metal);
	const FSRResourceInstance InputResource = StarRovers::FacilityPatternOperatorTests::MakeResource(
		ResourceDataAsset,
		FName(TEXT("TransformInput")),
		FName(TEXT("StarIronDeposit")),
		InputPattern);

	USRFacilityDataAsset* FacilityDataAsset = StarRovers::FacilityPatternOperatorTests::MakeFacility(
		ESRFacilityOperationKind::Process);
	FacilityDataAsset->TransformOperator.SelectionMask.Reset(false);
	FacilityDataAsset->TransformOperator.SelectionMask.SetCellActive(2, 0, true);
	FacilityDataAsset->TransformOperator.Direction = ESRPatternDirection::Right;
	FacilityDataAsset->TransformOperator.OrganicGrowthsPerComponent = 0;
	FSRFacilityInstance FacilityInstance;
	FacilityInstance.FacilityDataAsset = FacilityDataAsset;

	TArray<FSRResourceInstance> Inputs;
	Inputs.Add(InputResource);
	TestTrue(
		TEXT("The facility pipeline accepts a valid Transform input."),
		FSRFacilityOutputResourceBuilder::DoesInputSetMatchOperation(
			FacilityDataAsset,
			Inputs,
			ESRFacilityTemperatureState::Normal));

	TArray<FSRResourceInstance> Outputs;
	FSRFacilityOutputResourceBuilder::BuildOutputResources(FacilityInstance, Inputs, Outputs);
	TestEqual(TEXT("Transform creates one pipeline output."), Outputs.Num(), 1);
	if (Outputs.Num() == 1)
	{
		const FSRResourceInstance& Output = Outputs[0];
		TestTrue(TEXT("Transform preserves resource identity."), Output.ResourceId == InputResource.ResourceId);
		TestTrue(TEXT("Transform preserves source provenance."), Output.SourcePatternId == InputResource.SourcePatternId);
		TestTrue(TEXT("Transform assigns a fresh instance identity."), !Output.ResourceInstanceId.IsNone() && Output.ResourceInstanceId != InputResource.ResourceInstanceId);
		TestTrue(TEXT("Transform stores the resolver output Pattern."), Output.Pattern.GetGlyph(2, 1) == ESRGlyphType::Metal);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRFacilityPatternSynthesisPipelineTest,
	"StarRovers.Facility.Pattern.SynthesisPipeline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRFacilityPatternSynthesisPipelineTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	USRResourceDataAsset* BaseDataAsset = NewObject<USRResourceDataAsset>();
	BaseDataAsset->ResourceId = FName(TEXT("BaseOre"));
	USRResourceDataAsset* OverlayDataAsset = NewObject<USRResourceDataAsset>();
	OverlayDataAsset->ResourceId = FName(TEXT("OverlayOre"));
	USRResourceDataAsset* ProductDataAsset = NewObject<USRResourceDataAsset>();
	ProductDataAsset->ResourceId = FName(TEXT("Alloy"));

	FSRPattern BasePattern;
	BasePattern.SetGlyph(0, 0, ESRGlyphType::Organic);
	FSRPattern OverlayPattern;
	OverlayPattern.SetGlyph(0, 0, ESRGlyphType::Metal);
	OverlayPattern.SetGlyph(4, 4, ESRGlyphType::Plasma);
	TArray<FSRResourceInstance> Inputs;
	Inputs.Add(StarRovers::FacilityPatternOperatorTests::MakeResource(
		BaseDataAsset,
		FName(TEXT("BaseInput")),
		FName(TEXT("BaseDeposit")),
		BasePattern));
	Inputs.Add(StarRovers::FacilityPatternOperatorTests::MakeResource(
		OverlayDataAsset,
		FName(TEXT("OverlayInput")),
		FName(TEXT("OverlayDeposit")),
		OverlayPattern));

	USRFacilityDataAsset* FacilityDataAsset = StarRovers::FacilityPatternOperatorTests::MakeFacility(
		ESRFacilityOperationKind::Synthesize);
	FacilityDataAsset->SynthesisOutputResource = ProductDataAsset;
	FSRFacilityInstance FacilityInstance;
	FacilityInstance.FacilityDataAsset = FacilityDataAsset;

	TArray<FSRResourceInstance> Outputs;
	FSRFacilityOutputResourceBuilder::BuildOutputResources(FacilityInstance, Inputs, Outputs);
	TestEqual(TEXT("Synthesis creates one pipeline output."), Outputs.Num(), 1);
	if (Outputs.Num() == 1)
	{
		const FSRResourceInstance& Output = Outputs[0];
		TestTrue(TEXT("Synthesis uses its configured product resource."), Output.ResourceDataAsset == ProductDataAsset && Output.ResourceId == ProductDataAsset->ResourceId);
		TestTrue(TEXT("Synthesis clears single-source provenance."), Output.SourcePatternId.IsNone() && Output.SourcePatternSeed == 0);
		TestTrue(TEXT("Synthesis stores the winning collision glyph."), Output.Pattern.GetGlyph(0, 0) == ESRGlyphType::Metal);
		TestTrue(TEXT("Synthesis copies non-overlapping overlay glyphs."), Output.Pattern.GetGlyph(4, 4) == ESRGlyphType::Plasma);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRFacilityPatternSeparationPipelineTest,
	"StarRovers.Facility.Pattern.SeparationPipeline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRFacilityPatternSeparationPipelineTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	USRResourceDataAsset* ResourceDataAsset = NewObject<USRResourceDataAsset>();
	ResourceDataAsset->ResourceId = FName(TEXT("MixedOre"));
	FSRPattern InputPattern;
	InputPattern.SetGlyph(0, 0, ESRGlyphType::Metal);
	InputPattern.SetGlyph(4, 4, ESRGlyphType::Organic);
	const FSRResourceInstance InputResource = StarRovers::FacilityPatternOperatorTests::MakeResource(
		ResourceDataAsset,
		FName(TEXT("SeparationInput")),
		FName(TEXT("MixedDeposit")),
		InputPattern);
	TArray<FSRResourceInstance> Inputs;
	Inputs.Add(InputResource);

	USRFacilityDataAsset* FacilityDataAsset = StarRovers::FacilityPatternOperatorTests::MakeFacility(
		ESRFacilityOperationKind::Separate);
	FacilityDataAsset->SeparationOperator.PrimaryOutputMask.Reset(false);
	FacilityDataAsset->SeparationOperator.PrimaryOutputMask.SetCellActive(0, 0, true);
	FSRFacilityInstance FacilityInstance;
	FacilityInstance.FacilityDataAsset = FacilityDataAsset;

	TestEqual(
		TEXT("The pipeline reserves two outputs for Separation."),
		FSRFacilityOutputResourceBuilder::CountProducedOutputResources(FacilityDataAsset),
		2);
	TArray<FSRResourceInstance> Outputs;
	FSRFacilityOutputResourceBuilder::BuildOutputResources(FacilityInstance, Inputs, Outputs);
	TestEqual(TEXT("Separation creates two pipeline outputs."), Outputs.Num(), 2);
	if (Outputs.Num() == 2)
	{
		TestTrue(TEXT("Both outputs preserve the input resource identity."), Outputs[0].ResourceId == InputResource.ResourceId && Outputs[1].ResourceId == InputResource.ResourceId);
		TestTrue(TEXT("Each Separation output has a distinct instance identity."), !Outputs[0].ResourceInstanceId.IsNone() && Outputs[0].ResourceInstanceId != Outputs[1].ResourceInstanceId);
		TestEqual(
			TEXT("Separation does not duplicate occupied glyph cells."),
			Outputs[0].Pattern.GetOccupiedCellCount() + Outputs[1].Pattern.GetOccupiedCellCount(),
			InputPattern.GetOccupiedCellCount());
		TestTrue(TEXT("The primary mask routes its glyph to output zero."), Outputs[0].Pattern.GetGlyph(0, 0) == ESRGlyphType::Metal);
		TestTrue(TEXT("The complement routes its glyph to output one."), Outputs[1].Pattern.GetGlyph(4, 4) == ESRGlyphType::Organic);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRFacilityPatternPhysicalTemperatureTest,
	"StarRovers.Facility.Pattern.PhysicalTemperature",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRFacilityPatternPhysicalTemperatureTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	USRFacilityDataAsset* FacilityDataAsset = StarRovers::FacilityPatternOperatorTests::MakeFacility(
		ESRFacilityOperationKind::Process);
	FacilityDataAsset->BaseProcessSeconds = 2.75f;

	USRResourceDataAsset* ResourceDataAsset = NewObject<USRResourceDataAsset>();
	ResourceDataAsset->ResourceId = FName(TEXT("PatternResource"));
	FSRPattern Pattern;
	Pattern.SetGlyph(2, 2, ESRGlyphType::Metal);
	FSRResourceInstance Resource = StarRovers::FacilityPatternOperatorTests::MakeResource(
		ResourceDataAsset,
		FName(TEXT("PhysicalTemperatureInput")),
		FName(TEXT("PatternDeposit")),
		Pattern);

	FSRFacilityInstance FacilityInstance;
	FacilityInstance.FacilityDataAsset = FacilityDataAsset;
	FacilityInstance.TemperatureState = ESRFacilityTemperatureState::Normal;
	FacilityInstance.ProcessingInventory.Add(Resource);
	TestTrue(
		TEXT("A Pattern operation advances at a valid physical temperature."),
		FSRFacilityProcessingRuleEvaluator::CanAdvanceProcessing(FacilityInstance));
	TestEqual(
		TEXT("The facility base time determines Pattern process time."),
		FSRFacilityProcessingRuleEvaluator::ResolveProcessSeconds(FacilityInstance),
		2.75f);

	FacilityInstance.TemperatureState = ESRFacilityTemperatureState::Frozen;
	TestFalse(
		TEXT("The physical facility temperature still blocks a frozen Pattern operation."),
		FSRFacilityProcessingRuleEvaluator::CanAdvanceProcessing(FacilityInstance));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRFacilityPatternSynthesisFullCycleTest,
	"StarRovers.Facility.Pattern.SynthesisFullCycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRFacilityPatternSynthesisFullCycleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	USRResourceDataAsset* BaseDataAsset = NewObject<USRResourceDataAsset>();
	BaseDataAsset->ResourceId = FName(TEXT("CycleBase"));
	USRResourceDataAsset* OverlayDataAsset = NewObject<USRResourceDataAsset>();
	OverlayDataAsset->ResourceId = FName(TEXT("CycleOverlay"));
	USRResourceDataAsset* ProductDataAsset = NewObject<USRResourceDataAsset>();
	ProductDataAsset->ResourceId = FName(TEXT("CycleProduct"));

	FSRPattern BasePattern;
	BasePattern.SetGlyph(2, 2, ESRGlyphType::Organic);
	FSRPattern OverlayPattern;
	OverlayPattern.SetGlyph(2, 2, ESRGlyphType::Metal);
	const FSRResourceInstance BaseInput = StarRovers::FacilityPatternOperatorTests::MakeResource(
		BaseDataAsset,
		FName(TEXT("CycleBaseInput")),
		FName(TEXT("CycleBaseDeposit")),
		BasePattern);
	const FSRResourceInstance OverlayInput = StarRovers::FacilityPatternOperatorTests::MakeResource(
		OverlayDataAsset,
		FName(TEXT("CycleOverlayInput")),
		FName(TEXT("CycleOverlayDeposit")),
		OverlayPattern);

	USRFacilityDataAsset* FacilityDataAsset = StarRovers::FacilityPatternOperatorTests::MakeFacility(
		ESRFacilityOperationKind::Synthesize);
	FacilityDataAsset->SynthesisOutputResource = ProductDataAsset;
	FSRFacilityInstance FacilityInstance;
	FacilityInstance.FacilityDataAsset = FacilityDataAsset;
	FSRFacilityPortInventory BaseInputPort;
	BaseInputPort.Capacity = 1;
	BaseInputPort.Inventory.Add(BaseInput);
	FacilityInstance.InputPortInventories.Add(MoveTemp(BaseInputPort));
	FSRFacilityPortInventory OverlayInputPort;
	OverlayInputPort.Capacity = 1;
	OverlayInputPort.Inventory.Add(OverlayInput);
	FacilityInstance.InputPortInventories.Add(MoveTemp(OverlayInputPort));
	FSRFacilityPortInventory OutputPort;
	OutputPort.Capacity = 1;
	OutputPort.PortKind = ESRFacilityPortKind::Output;
	FacilityInstance.OutputPortInventories.Add(MoveTemp(OutputPort));

	FSRFacilityProcessingStartResult StartResult;
	TestTrue(
		TEXT("A Synthesis facility starts after gathering exactly one resource from each of two input ports."),
		FSRFacilityProcessingStepExecutor::TryStartProcessing(nullptr, FacilityInstance, &StartResult));
	TestTrue(TEXT("The running step is a standard Pattern operation."), StartResult.StepKind == ESRFacilityProcessingStepKind::Standard);
	TestEqual(TEXT("Both Synthesis inputs move into the processing inventory."), FacilityInstance.ProcessingInventory.Num(), 2);
	TestTrue(TEXT("The two source input ports are consumed."), FacilityInstance.InputPortInventories[0].Inventory.IsEmpty() && FacilityInstance.InputPortInventories[1].Inventory.IsEmpty());

	FSRFacilityProcessingCompletionResult CompletionResult;
	TestTrue(
		TEXT("The Synthesis facility completes through the shared processing step executor."),
		FSRFacilityProcessingStepExecutor::TryCompleteProcessing(nullptr, FacilityInstance, &CompletionResult));
	TestEqual(TEXT("The completed cycle stores exactly one product."), CompletionResult.OutputCount, 1);
	TestFalse(TEXT("The completed cycle clears the processing state."), FacilityInstance.bProcessing);
	TestTrue(TEXT("The completed cycle clears consumed resources."), FacilityInstance.ProcessingInventory.IsEmpty());
	TestEqual(TEXT("The product is stored in the output port."), FacilityInstance.OutputPortInventories[0].Inventory.Num(), 1);
	if (FacilityInstance.OutputPortInventories[0].Inventory.Num() == 1)
	{
		const FSRResourceInstance& Product = FacilityInstance.OutputPortInventories[0].Inventory[0];
		TestTrue(TEXT("The stored product uses the configured synthesis identity."), Product.ResourceId == ProductDataAsset->ResourceId);
		TestTrue(TEXT("The stored product contains the resolved Pattern."), Product.Pattern.GetGlyph(2, 2) == ESRGlyphType::Metal);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRFacilityPatternEnvironmentPipelineTest,
	"StarRovers.Facility.Pattern.EnvironmentPipeline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRFacilityPatternEnvironmentPipelineTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	USRResourceDataAsset* ResourceDataAsset = NewObject<USRResourceDataAsset>();
	ResourceDataAsset->ResourceId = FName(TEXT("EnvironmentOre"));
	FSRPattern InputPattern;
	InputPattern.SetGlyph(2, 0, ESRGlyphType::Metal);
	TArray<FSRResourceInstance> Inputs;
	Inputs.Add(StarRovers::FacilityPatternOperatorTests::MakeResource(
		ResourceDataAsset,
		FName(TEXT("EnvironmentInput")),
		FName(TEXT("EnvironmentDeposit")),
		InputPattern));

	USRFacilityDataAsset* FacilityDataAsset = StarRovers::FacilityPatternOperatorTests::MakeFacility(
		ESRFacilityOperationKind::Process);
	FacilityDataAsset->TransformOperator.SelectionMask.Reset(false);
	FacilityDataAsset->TransformOperator.SelectionMask.SetCellActive(2, 0, true);
	FacilityDataAsset->TransformOperator.Direction = ESRPatternDirection::Right;
	FacilityDataAsset->TransformOperator.OrganicGrowthsPerComponent = 0;
	FSRFacilityInstance FacilityInstance;
	FacilityInstance.FacilityDataAsset = FacilityDataAsset;
	FacilityInstance.PatternEnvironment.EnvironmentId = FName(TEXT("HighGravity"));
	FSRPatternEnvironmentEffectSpec GravityEffect;
	GravityEffect.EffectKind = ESRPatternEnvironmentEffectKind::DirectionalPull;
	GravityEffect.Direction = ESRPatternDirection::Down;
	FacilityInstance.PatternEnvironment.Effects.Add(GravityEffect);

	TArray<FSRResourceInstance> Outputs;
	FSRFacilityOutputResourceBuilder::BuildOutputResources(FacilityInstance, Inputs, Outputs);
	TestEqual(TEXT("The environment pipeline still creates one Transform output."), Outputs.Num(), 1);
	if (Outputs.Num() == 1)
	{
		TestTrue(TEXT("The facility Transform runs before the environment cycle."), Outputs[0].Pattern.GetGlyph(3, 1) == ESRGlyphType::Metal);
		TestTrue(TEXT("The pre-environment facility destination is no longer occupied."), Outputs[0].Pattern.GetGlyph(2, 1) == ESRGlyphType::Empty);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRFacilityPatternRunModifierPipelineTest,
	"StarRovers.Facility.Pattern.RunModifierPipeline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRFacilityPatternRunModifierPipelineTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	USRResourceDataAsset* ResourceDataAsset = NewObject<USRResourceDataAsset>();
	ResourceDataAsset->ResourceId = FName(TEXT("ModifierOrganic"));
	FSRPattern InputPattern;
	InputPattern.SetGlyph(2, 2, ESRGlyphType::Organic);
	TArray<FSRResourceInstance> Inputs;
	Inputs.Add(StarRovers::FacilityPatternOperatorTests::MakeResource(
		ResourceDataAsset,
		FName(TEXT("ModifierInput")),
		FName(TEXT("ModifierDeposit")),
		InputPattern));

	USRFacilityDataAsset* FacilityDataAsset = StarRovers::FacilityPatternOperatorTests::MakeFacility(
		ESRFacilityOperationKind::Process);
	FacilityDataAsset->BaseProcessSeconds = 8.0f;
	FacilityDataAsset->TransformOperator.SelectionMask.Reset(false);
	FacilityDataAsset->TransformOperator.SelectionMask.SetCellActive(2, 2, true);
	FacilityDataAsset->TransformOperator.Direction = ESRPatternDirection::Right;
	FacilityDataAsset->TransformOperator.OrganicGrowthsPerComponent = 0;

	FSRRunModifierSource Source;
	Source.SourceId = FName(TEXT("OrganicLineAugment"));
	Source.SourceKind = ESRRunModifierSourceKind::Augment;
	Source.StackCount = 1;
	FSRRunModifierEffect ProcessTimeEffect;
	ProcessTimeEffect.EffectId = FName(TEXT("OrganicTransformTime"));
	ProcessTimeEffect.EffectKind = ESRRunModifierEffectKind::FacilityProcessTimeMultiplier;
	ProcessTimeEffect.Magnitude = 0.5;
	ProcessTimeEffect.FacilityScope = ESRRunModifierFacilityScope::Transform;
	ProcessTimeEffect.AffectedGlyph = ESRGlyphType::Organic;
	Source.Effects.Add(ProcessTimeEffect);
	FSRRunModifierEffect GrowthEffect;
	GrowthEffect.EffectId = FName(TEXT("OrganicTransformGrowth"));
	GrowthEffect.EffectKind = ESRRunModifierEffectKind::TransformOrganicGrowthDelta;
	GrowthEffect.Magnitude = 1.0;
	GrowthEffect.FacilityScope = ESRRunModifierFacilityScope::Transform;
	GrowthEffect.AffectedGlyph = ESRGlyphType::Organic;
	Source.Effects.Add(GrowthEffect);

	FSRFacilityInstance FacilityInstance;
	FacilityInstance.FacilityDataAsset = FacilityDataAsset;
	FacilityInstance.ProcessingInventory = Inputs;
	FString FailureReason;
	TestTrue(TEXT("The facility receives a valid modifier snapshot."),
		FSRRunModifierResolver::BuildContext({ Source }, 3, FacilityInstance.RunModifierContext, FailureReason));
	TestTrue(TEXT("Process duration uses the same dominant-glyph query as output resolution."),
		FMath::IsNearlyEqual(FSRFacilityProcessingRuleEvaluator::ResolveProcessSeconds(FacilityInstance), 4.0f));

	TArray<FSRResourceInstance> Outputs;
	FSRFacilityOutputResourceBuilder::BuildOutputResources(FacilityInstance, Inputs, Outputs);
	TestEqual(TEXT("The modifier pipeline creates one Transform output."), Outputs.Num(), 1);
	if (Outputs.Num() == 1)
	{
		TestEqual(TEXT("The snapshotted Organic growth delta is applied by the production resolver."),
			Outputs[0].Pattern.CountGlyph(ESRGlyphType::Organic),
			2);
	}
	return true;
}

#endif
