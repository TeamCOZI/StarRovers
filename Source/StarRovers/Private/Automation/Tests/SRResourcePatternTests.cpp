#if WITH_DEV_AUTOMATION_TESTS

#include "Automation/SRResourceDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"

#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	TArray<FSRSourceGlyphCount> MakeStarIronGlyphCounts()
	{
		FSRSourceGlyphCount MetalCount;
		MetalCount.Glyph = ESRGlyphType::Metal;
		MetalCount.Count = 2;

		FSRSourceGlyphCount CrystalCount;
		CrystalCount.Glyph = ESRGlyphType::Crystal;
		CrystalCount.Count = 2;

		return {MetalCount, CrystalCount};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourceSourcePatternGenerationTest,
	"StarRovers.ResourceSystem.PatternSource.Generation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourceSourcePatternGenerationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const TArray<FSRSourceGlyphCount> GlyphCounts = MakeStarIronGlyphCounts();
	FSRPattern FirstPattern;
	FSRPattern SecondPattern;
	TestTrue(
		TEXT("A valid source specification generates a Pattern."),
		StarRovers::ResourcePatterns::TryGenerateSourcePattern(GlyphCounts, 1729, FirstPattern));
	TestTrue(
		TEXT("The same source specification and seed can be generated again."),
		StarRovers::ResourcePatterns::TryGenerateSourcePattern(GlyphCounts, 1729, SecondPattern));
	TestTrue(TEXT("Source generation is deterministic."), FirstPattern == SecondPattern);
	TestEqual(TEXT("The source has exactly two Metal glyphs."), FirstPattern.CountGlyph(ESRGlyphType::Metal), 2);
	TestEqual(TEXT("The source has exactly two Crystal glyphs."), FirstPattern.CountGlyph(ESRGlyphType::Crystal), 2);
	TestEqual(TEXT("The source has exactly four occupied cells."), FirstPattern.GetOccupiedCellCount(), 4);

	TArray<FSRSourceGlyphCount> ReversedGlyphCounts = GlyphCounts;
	ReversedGlyphCounts.Swap(0, 1);
	FSRPattern ReorderedPattern;
	TestTrue(
		TEXT("Reordering source count entries remains valid."),
		StarRovers::ResourcePatterns::TryGenerateSourcePattern(ReversedGlyphCounts, 1729, ReorderedPattern));
	TestTrue(TEXT("Entry order does not change a rolled Pattern."), ReorderedPattern == FirstPattern);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourceSourcePatternValidationTest,
	"StarRovers.ResourceSystem.PatternSource.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourceSourcePatternValidationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FSRPattern OutputPattern(ESRGlyphType::Plasma);
	TArray<FSRSourceGlyphCount> DuplicateGlyphCounts = MakeStarIronGlyphCounts();
	const FSRSourceGlyphCount DuplicateMetalCount = DuplicateGlyphCounts[0];
	DuplicateGlyphCounts.Add(DuplicateMetalCount);
	TestFalse(
		TEXT("Duplicate glyph definitions are rejected."),
		StarRovers::ResourcePatterns::TryGenerateSourcePattern(DuplicateGlyphCounts, 1, OutputPattern));
	TestTrue(TEXT("A rejected specification clears the output Pattern."), OutputPattern.IsEmpty());

	FSRSourceGlyphCount EmptyCount;
	EmptyCount.Glyph = ESRGlyphType::Empty;
	EmptyCount.Count = 1;
	TestFalse(
		TEXT("Empty cannot be configured as a source glyph."),
		StarRovers::ResourcePatterns::TryGenerateSourcePattern(TConstArrayView<FSRSourceGlyphCount>(&EmptyCount, 1), 1, OutputPattern));

	TArray<FSRSourceGlyphCount> OverflowCounts = MakeStarIronGlyphCounts();
	OverflowCounts[0].Count = 24;
	OverflowCounts[1].Count = 2;
	TestFalse(
		TEXT("A source cannot occupy more than 25 cells."),
		StarRovers::ResourcePatterns::TryGenerateSourcePattern(OverflowCounts, 1, OutputPattern));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRResourceDepositPatternCopyTest,
	"StarRovers.ResourceSystem.PatternSource.DepositCopy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRResourceDepositPatternCopyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	USRResourceDataAsset* ResourceDataAsset = NewObject<USRResourceDataAsset>();
	ResourceDataAsset->ResourceId = FName(TEXT("StarIron"));
	ResourceDataAsset->SourceGlyphCounts = MakeStarIronGlyphCounts();

	FSRResourceDepositInstance ResourceDeposit;
	ResourceDeposit.OccupantId = FName(TEXT("Deposit_42"));
	ResourceDeposit.ResourceDataAsset = ResourceDataAsset;
	ResourceDeposit.ResourceId = ResourceDataAsset->ResourceId;
	ResourceDeposit.SourcePatternId = ResourceDeposit.OccupantId;
	ResourceDeposit.SourcePatternSeed = StarRovers::ResourcePatterns::MakeStableSourcePatternSeed(
		ResourceDeposit.SourcePatternId,
		ResourceDeposit.ResourceId,
		ResourceDataAsset->SourcePatternSeedSalt);
	ResourceDataAsset->TryGenerateSourcePattern(ResourceDeposit.SourcePatternSeed, ResourceDeposit.SourcePattern);
	ResourceDeposit.TotalAmount = 10;
	ResourceDeposit.RemainingAmount = 10;

	TestTrue(TEXT("The configured deposit is a valid Pattern source."), ResourceDeposit.IsPatternSourceValid());
	TestTrue(TEXT("A non-empty configured deposit can be harvested."), ResourceDeposit.CanHarvestResource());
	const FSRResourceInstance FirstResource = ResourceDeposit.BuildResourceInstance(FName(TEXT("Resource_A")));
	FSRResourceInstance SecondResource = ResourceDeposit.BuildResourceInstance(FName(TEXT("Resource_B")));
	TestTrue(TEXT("Every harvest copies the deposit's one-time Pattern roll."), FirstResource.Pattern == ResourceDeposit.SourcePattern);
	TestTrue(TEXT("Repeated harvest previews have identical Pattern payloads."), FirstResource.Pattern == SecondResource.Pattern);
	TestEqual(TEXT("The source Pattern ID is preserved."), FirstResource.SourcePatternId, ResourceDeposit.SourcePatternId);
	TestEqual(TEXT("The source seed is preserved."), FirstResource.SourcePatternSeed, ResourceDeposit.SourcePatternSeed);
	TestNotEqual(TEXT("Individual resource instance IDs remain distinct."), FirstResource.ResourceInstanceId, SecondResource.ResourceInstanceId);

	SecondResource.SourcePatternId = FName(TEXT("AnotherDeposit"));
	SecondResource.SourcePatternSeed += 1;
	TestTrue(
		TEXT("Stack identity is defined by the resource and exact Pattern, not provenance."),
		StarRovers::ResourcePatterns::ArePatternPayloadsEquivalent(FirstResource, SecondResource));

	const ESRGlyphType ReplacementGlyph = SecondResource.Pattern.Cells[0] == ESRGlyphType::Empty
		? ESRGlyphType::Metal
		: ESRGlyphType::Empty;
	SecondResource.Pattern.Cells[0] = ReplacementGlyph;
	TestFalse(
		TEXT("Changing one Pattern cell prevents stacking."),
		StarRovers::ResourcePatterns::ArePatternPayloadsEquivalent(FirstResource, SecondResource));

	FSRResourceInstance HarvestedResource;
	TestTrue(
		TEXT("Harvest succeeds while a finite deposit has remaining units."),
		ResourceDeposit.TryHarvestResource(FName(TEXT("Harvested_A")), HarvestedResource));
	TestEqual(TEXT("Harvest consumes exactly one finite unit."), ResourceDeposit.RemainingAmount, 9);
	TestTrue(TEXT("Harvest copies the stored deposit Pattern."), HarvestedResource.Pattern == ResourceDeposit.SourcePattern);

	ResourceDeposit.RemainingAmount = 0;
	TestFalse(TEXT("A depleted finite deposit cannot be harvested."), ResourceDeposit.CanHarvestResource());
	TestFalse(
		TEXT("A depleted finite deposit rejects harvest atomically."),
		ResourceDeposit.TryHarvestResource(FName(TEXT("Harvested_B")), HarvestedResource));
	TestTrue(TEXT("A rejected harvest clears its output."), HarvestedResource.ResourceId.IsNone());

	ResourceDeposit.RemainingAmount = MAX_int32;
	ResourceDeposit.TotalAmount = MAX_int32;
	TestTrue(
		TEXT("An infinite deposit can be harvested."),
		ResourceDeposit.TryHarvestResource(FName(TEXT("Harvested_Infinite")), HarvestedResource));
	TestEqual(TEXT("An infinite deposit does not decrement."), ResourceDeposit.RemainingAmount, MAX_int32);
	return true;
}

#endif
